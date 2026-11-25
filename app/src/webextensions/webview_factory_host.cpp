#define ENABLE_VERBOSE_LOGS 0
#define ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT FALSE
#define ENABLE_CONTENT_BLOCK_FILTERS 1

#include "webview_factory_host.h"
#include "webextensions/webview_host.h"
#include "util/file_util.h"
#include "common/src/log.h"
#include "common/src/memory.h"
#include "core/core_api.h"
#include "common/src/uri.h"
#include "common/src/string_util.h"
#include "app.h"

#include "content_block/embedded_filter_lists.h"

#include <glib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#   include "window/orion_webview_tabview_widgets/orion_webkit_webview.h"
#endif

#include <map>
#include <chrono>

#include <core/core_api.h>
#include <core/webkit_bridge.h>
#include <core/downloader.h>
#include <core/settings.h>
#include <core/webextensions/runtime.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/web_view.h>

#include <common/src/defer.h>
#include <common/src/memory.h>
#include <common/src/uri.h>
#include <common/src/string_util.h>
#include <common/src/time_util.h>
#include <common/src/log.h>

#include "util/downloads_util.h"
#include "util/file_util.h"
#include "util/cookies_util.h"


namespace WebExtensions {

std::atomic<int> global_webview_factory_count = 0;

}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT


using namespace std::literals::chrono_literals;

namespace WebExtensions {

typedef std::function<void(WebExtensionWebViewFactory*)> InitializeReply;

class WebExtensionWebViewFactoryImpl
    : public WebExtensionWebViewFactory {
public:
    explicit WebExtensionWebViewFactoryImpl(
        orion::bridge::WebViewFactoryBridge& bridge);

    ~WebExtensionWebViewFactoryImpl();
    void initialize_platform() override;
    void web_view_for_content(WebViewReply reply) override;
    WebViewRef web_view_for_content_synchronous(WebViewRef related_webview) override;
    void web_view_for_extension_popup(orion::ExtensionId extension_id, WebViewReply reply) override;
    bool show_extension_background_web_inspector(orion::ExtensionId extension_id) override;
    void update_background_pages(const WebExtensionBackgroundPagesChange& change) override;
    void register_scheme(
        const WebExtensionSchemeRegistration& registration,
        WebExtensionSchemeRequestHandler handler
    ) override;
    OpaqueNetworkSession* network_session() override;
    std::vector<orion::gtk::WeakRefWrapper> web_views() override;
    orion::Observable<WebViewMapChange>& web_views_change() const override;
    void set_web_views_observer(orion::ObserverId observer_id) override;
    orion::ObserverId web_views_observer() const override;
    ContentFilterStore* filter_store() override;
public:
    void on_gresource_font_scheme_request(WebKitURISchemeRequest* request);
protected:
    WebKitNetworkSession* sync_network_session();
    WebKitWebContext* sync_web_context();
    WebViewRef sync_web_view_common(WebViewRef related_view);
    WebViewRef sync_web_view_for_content(WebViewRef related_view = nullptr);
    WebViewRef sync_web_view_for_extension_popup(orion::ExtensionId extension_id, WebViewRef related_view = nullptr);
private:
    UNIQUE_PTR(WebKitWebContext, g_object_unref) m_context = nullptr;
    UNIQUE_PTR(WebKitNetworkSession, g_object_unref) m_network_session = nullptr;
    std::vector<orion::gtk::WeakRefWrapper> m_orion_webkit_webviews; // managers of individual webviews
    const std::unique_ptr<orion::Observable<WebViewMapChange>> m_orion_webkit_webviews_change;
    orion::ObserverId m_web_views_observer{};
    ContentFilterStore m_filter_store{};
    std::map<orion::ExtensionId, WebViewRef> m_background_web_views;
};

WebExtensionWebViewFactoryImpl::WebExtensionWebViewFactoryImpl(
    orion::bridge::WebViewFactoryBridge& bridge) :
      WebExtensionWebViewFactory(bridge)
    , m_orion_webkit_webviews_change(std::make_unique<orion::Observable<WebViewMapChange>>())
{
    global_webview_factory_count.fetch_add(1, std::memory_order_relaxed);

    orion::log_verbose(
        "WebExtensionWebViewFactoryImpl: profile_id=%lld private=%d ptr=%p",
        profile_id(),
        static_cast<int>(private_mode()),
        this
    );
}

WebExtensionWebViewFactoryImpl::~WebExtensionWebViewFactoryImpl()
{
    // Stop core dispatch before releasing any native state.
    detach();
    global_webview_factory_count.fetch_sub(1, std::memory_order_relaxed);

    m_background_web_views.clear();

    // Must be called only when no GAsyncReadyCallback (of each individual store) is pending because we must not
    // unref the store if these callbacks (save, load etc.) are still pending
    orion::contentblock::free_embedded_filter_lists(this);

    orion::check_observers(*m_orion_webkit_webviews_change, "webview map");
    
    orion::log_verbose(
        "~WebExtensionWebViewFactoryImpl: ptr=%p", this
    );
}

void
WebExtensionWebViewFactoryImpl::initialize_platform()
{
#if ENABLE_CONTENT_BLOCK_FILTERS
    orion::contentblock::setup_filter_lists(this);
#endif
}

void
WebExtensionWebViewFactoryImpl::update_background_pages(const WebExtensionBackgroundPagesChange& change)
{
    for(const orion::ExtensionId& extension_id : change.deleted) {
        m_background_web_views.erase(extension_id);
    }
    for(const WebExtensionBackgroundPage& background_page : change.created) {
        WebViewRef web_view = sync_web_view_common(nullptr);
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view.get()), background_page.uri.c_str());
        m_background_web_views[background_page.extension_id] = std::move(web_view);
        orion::log_debug(
            "WebExtensionWebView> Started background page/worker for extension {%s} in profile %lld",
            background_page.extension_id.value.c_str(),
            profile_id().value
        );
    }
}

static void
forbidden_response(WebKitURISchemeRequest* request)
{
    static constexpr char kNotFoundBody[] = "403 Forbidden\n";
    UNIQUE_PTR(GInputStream, g_object_unref) stream(g_memory_input_stream_new_from_data(kNotFoundBody, sizeof(kNotFoundBody) - 1, nullptr));
    UNIQUE_PTR(WebKitURISchemeResponse, g_object_unref) response(webkit_uri_scheme_response_new(stream.get(), sizeof(kNotFoundBody) - 1));
    webkit_uri_scheme_response_set_status(response.get(), 403, "Forbidden");
    webkit_uri_scheme_response_set_content_type(response.get(), "text/plain");
    webkit_uri_scheme_request_finish_with_response(request, response.get());
}

static void
not_found_response(WebKitURISchemeRequest* request)
{
    static constexpr char kNotFoundBody[] = "404 Not Found\n";
    UNIQUE_PTR(GInputStream, g_object_unref) stream(g_memory_input_stream_new_from_data(kNotFoundBody, sizeof(kNotFoundBody) - 1, nullptr));
    UNIQUE_PTR(WebKitURISchemeResponse, g_object_unref) response(webkit_uri_scheme_response_new(stream.get(), sizeof(kNotFoundBody) - 1));
    webkit_uri_scheme_response_set_status(response.get(), 404, "Not Found");
    webkit_uri_scheme_response_set_content_type(response.get(), "text/plain");
    webkit_uri_scheme_request_finish_with_response(request, response.get());
}

static WebExtensionRequestSecurityOrigin
request_security_origin(WebKitURISchemeRequest* request)
{
    UNIQUE_PTR(WebKitSecurityOrigin, webkit_security_origin_unref) web_view_security_origin(
        webkit_security_origin_new_for_uri(
            webkit_web_view_get_uri(
                webkit_uri_scheme_request_get_web_view(request)
            )
        )
    );
    UNIQUE_PTR(WebKitSecurityOrigin, webkit_security_origin_unref) request_security_origin(
        webkit_security_origin_new_for_uri(
            webkit_uri_scheme_request_get_uri(request)
        )
    );

    std::string web_view_security_origin_host = coalesce(webkit_security_origin_get_host(web_view_security_origin.get()), "");
    std::string request_security_origin_host = coalesce(webkit_security_origin_get_host(request_security_origin.get()), "");
    return {
        .request_host = request_security_origin_host,
        .web_view_host = web_view_security_origin_host,
    };
}

static WebExtensionHttpHeaders
request_headers(WebKitURISchemeRequest* request)
{
    WebExtensionHttpHeaders headers;
    SoupMessageHeaders* incoming_headers = webkit_uri_scheme_request_get_http_headers(request);
    if(!incoming_headers) {
        return headers;
    }

    SoupMessageHeadersIter iter;
    soup_message_headers_iter_init(&iter, incoming_headers);
    const char* name = nullptr;
    const char* value = nullptr;
    while(soup_message_headers_iter_next(&iter, &name, &value)) {
        headers.emplace_back(name, value);
    }
    return headers;
}

static std::string
header_value(const WebExtensionHttpHeaders& headers, const std::string& name)
{
    for(const auto& header : headers) {
        if(orion::str::equals_case_insensitive(header.first, name)) {
            return header.second;
        }
    }
    return "";
}

static std::optional<WebExtensionRequestBody>
request_body(WebKitURISchemeRequest* request, const WebExtensionHttpHeaders& headers)
{
    WebExtensionRequestBody body;
    body.content_type = header_value(headers, "Content-Type");
    UNIQUE_PTR(GInputStream, g_object_unref) body_stream(webkit_uri_scheme_request_get_http_body(request));
    if(!body_stream) {
        return body;
    }

    std::vector<char> chunk(8192);
    while(true) {
        gssize bytes_read = g_input_stream_read(body_stream.get(), chunk.data(), chunk.size(), nullptr, nullptr);
        if(bytes_read > 0) {
            body.content.append(chunk.data(), bytes_read);
            continue;
        }
        if(bytes_read == 0) {
            break;
        }
        orion::log_warning("WebExtensionWebView> Failed reading request body");
        return std::nullopt;
    }
    body.total_content_length = body.content.size();
    return body;
}

static void
set_extension_scheme_response_content_type(
    WebKitURISchemeRequest* request,
    WebKitURISchemeResponse* response,
    SoupMessageHeaders* headers,
    const WebExtensionSchemeResponse& scheme_response)
{
    std::string content_type = header_value(scheme_response.headers, "Content-Type");
    if(content_type.empty()) {
        content_type = scheme_response.content_type;
    }
    if(content_type.empty()) {
        const char* request_uri = webkit_uri_scheme_request_get_uri(request);
        const std::string path = request_uri
            ? orion::Uri::parse_strict(request_uri).path()
            : std::string();
        UNIQUE_PTR(gchar, g_free) type(
            g_content_type_guess(path.c_str(), nullptr, 0, nullptr));
        if(type) {
            UNIQUE_PTR(gchar, g_free) mime_type(g_content_type_get_mime_type(type.get()));
            content_type = mime_type ? mime_type.get() : type.get();
        }
    }
    if(content_type.empty()) {
        return;
    }

    webkit_uri_scheme_response_set_content_type(response, content_type.c_str());
    if(!soup_message_headers_get_one(headers, "Content-Type")) {
        soup_message_headers_append(headers, "Content-Type", content_type.c_str());
    }
}

static void
finish_extension_scheme_response(WebKitURISchemeRequest* request, const WebExtensionSchemeResponse& scheme_response)
{
    if(scheme_response.status == WebExtensionRequestStatus::forbidden) {
        forbidden_response(request);
        return;
    }
    if(scheme_response.status == WebExtensionRequestStatus::not_found) {
        not_found_response(request);
        return;
    }
    if(scheme_response.status == WebExtensionRequestStatus::failed) {
        UNIQUE_PTR(GError, g_error_free) error(g_error_new_literal(
            g_quark_from_static_string("extension-scheme"),
            0,
            scheme_response.error_message.empty() ? "Failed to load resource" : scheme_response.error_message.c_str()
        ));
        webkit_uri_scheme_request_finish_error(request, error.get());
        return;
    }

    UNIQUE_PTR(GBytes, g_bytes_unref) bytes(g_bytes_new(scheme_response.body.data(), scheme_response.body.size()));
    UNIQUE_PTR(GInputStream, g_object_unref) stream(g_memory_input_stream_new_from_bytes(bytes.get()));
    UNIQUE_PTR(WebKitURISchemeResponse, g_object_unref) response(
        webkit_uri_scheme_response_new(stream.get(), scheme_response.body.size())
    );
    UNIQUE_PTR(SoupMessageHeaders, soup_message_headers_unref) headers(
        soup_message_headers_new(SOUP_MESSAGE_HEADERS_RESPONSE));
    for(const auto& header : scheme_response.headers) {
        soup_message_headers_append(headers.get(), header.first.c_str(), header.second.c_str());
    }
    set_extension_scheme_response_content_type(
        request, response.get(), headers.get(), scheme_response);
    webkit_uri_scheme_response_set_http_headers(response.get(), headers.release());
    webkit_uri_scheme_response_set_status(
        response.get(),
        scheme_response.http_status_code,
        scheme_response.reason_phrase.c_str()
    );
    webkit_uri_scheme_request_finish_with_response(request, response.get());
}

static void
on_extension_scheme_request(WebKitURISchemeRequest* request, const WebExtensionSchemeRequestHandler& handler)
{
    WebExtensionHttpHeaders headers = request_headers(request);
    std::optional<WebExtensionRequestBody> body = request_body(request, headers);
    if(!body) {
        UNIQUE_PTR(GError, g_error_free) error(g_error_new_literal(
            g_quark_from_static_string("extension-scheme"),
            0,
            "Failed to read request body"
        ));
        webkit_uri_scheme_request_finish_error(request, error.get());
        return;
    }
    const gchar* method = webkit_uri_scheme_request_get_http_method(request);
    orion::gtk::WeakRefWrapper request_weak(request);
    handler(
        {
            .uri = webkit_uri_scheme_request_get_uri(request),
            .security_origin = request_security_origin(request),
            .method = method ? method : "GET",
            .headers = std::move(headers),
            .body = std::move(*body),
        },
        [request_weak = std::move(request_weak)](WebExtensionSchemeResponse response) {
            auto request_ref = request_weak.lock();
            if(!request_ref) {
                return;
            }
            finish_extension_scheme_response(
                WEBKIT_URI_SCHEME_REQUEST(request_ref.get()),
                response
            );
        }
    );
}

void
WebExtensionWebViewFactoryImpl::on_gresource_font_scheme_request(WebKitURISchemeRequest* request)
{
    const gchar *path = webkit_uri_scheme_request_get_path(request);
    
    bool is_allowed = false;
    if(0 == g_strcmp0(path, "/OpenDyslexic3-Regular.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Charter-Bold-Italic.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Charter-Bold.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Charter-Italic.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Charter-Regular.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Gelasio-Italic-VariableFont_wght.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Gelasio-VariableFont_wght.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/LiberationSerif-BoldItalic.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/LiberationSerif-Bold.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/LiberationSerif-Italic.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/LiberationSerif-Regular.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Merriweather-Italic-VariableFont_opsz,wdth,wght.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Merriweather-VariableFont_opsz,wdth,wght.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/texgyrepagella-bolditalic.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/texgyrepagella-bold.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/texgyrepagella-italic.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/texgyrepagella-regular.otf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Vollkorn-Italic-VariableFont_wght.ttf")) {
        is_allowed = true;
    } else if(0 == g_strcmp0(path, "/Vollkorn-VariableFont_wght.ttf")) {
        is_allowed = true;
    }

    if(!is_allowed)
    {
        GError* error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_ACCES, "Access denied to internal resource");
        orion::log_error("invalid gresource font path: %s", path);
        webkit_uri_scheme_request_finish_error(request, error);
        g_error_free(error);
        return;
    }

    std::string complete_path = std::string(GRESOURCE_PREFIX "/fonts") + path;
    
    GError* err = NULL;
    gsize size;
    
    if(g_resources_get_info(complete_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, &size, NULL, &err))
    {
        GInputStream *stream = g_resources_open_stream(complete_path.c_str(), G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
        
        //TODO: dynamic mime type?
        webkit_uri_scheme_request_finish(request, stream, size, "font/ttf");
        g_object_unref(stream);
    }
    else
    {
        webkit_uri_scheme_request_finish_error(request, err);
        g_error_free(err);
    }
}

WebKitWebContext*
WebExtensionWebViewFactoryImpl::sync_web_context()
{
    if(!m_context) {
        m_context.reset(webkit_web_context_new());
        register_schemes();

        // TODO: Should these gresource-font scheme registrations be made by calling
        // WebExtensionWebViewFactoryImpl::register_scheme instead?
        WebKitSecurityManager* security_manager = webkit_web_context_get_security_manager(m_context.get());
        webkit_security_manager_register_uri_scheme_as_secure(security_manager, "gresource-font");
        webkit_web_context_register_uri_scheme(
            m_context.get(),
            "gresource-font",
            [](WebKitURISchemeRequest* request, gpointer user_data){((WebExtensionWebViewFactoryImpl*)user_data)->on_gresource_font_scheme_request(request);},
            this,
            NULL
        );
    }
    return m_context.get();
}

void
WebExtensionWebViewFactoryImpl::register_scheme(
    const WebExtensionSchemeRegistration& registration,
    WebExtensionSchemeRequestHandler handler
)
{
    WebKitSecurityManager* security_manager = webkit_web_context_get_security_manager(m_context.get());
    if(registration.secure) {
        webkit_security_manager_register_uri_scheme_as_secure(security_manager, registration.scheme.c_str());
    }
    if(registration.cors_enabled) {
        webkit_security_manager_register_uri_scheme_as_cors_enabled(security_manager, registration.scheme.c_str());
    }

    webkit_web_context_register_uri_scheme(
        m_context.get(),
        registration.scheme.c_str(),
        [](WebKitURISchemeRequest* request, gpointer user_data){
            auto* handler = static_cast<WebExtensionSchemeRequestHandler*>(user_data);
            on_extension_scheme_request(request, *handler);
        },
        new WebExtensionSchemeRequestHandler(std::move(handler)),
        [](gpointer user_data){
            delete static_cast<WebExtensionSchemeRequestHandler*>(user_data);
        }
    );
}

WebKitNetworkSession*
WebExtensionWebViewFactoryImpl::sync_network_session()
{
    if(!m_network_session) {
        std::vector<std::string> base_path = WebExtensionWebViewFactory::get_profile_path_parts(
            profile_id(),
            private_mode()
        );

        std::vector<std::string> cookie_path = base_path;
        cookie_path.push_back("cookies.sqlite");

        std::vector<std::string> data_dir = base_path;
        data_dir.push_back("data");

        std::vector<std::string> cache_dir = base_path;
        cache_dir.push_back("cache");

        m_network_session.reset(
            webkit_network_session_new(
                orion::str::join_strings(data_dir, G_DIR_SEPARATOR_S).c_str(),
                orion::str::join_strings(cache_dir, G_DIR_SEPARATOR_S).c_str()
            )
        );
        WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager(m_network_session.get());
        webkit_cookie_manager_set_persistent_storage(
            cookie_manager,
            orion::str::join_strings(cookie_path, G_DIR_SEPARATOR_S).c_str(),
            WebKitCookiePersistentStorage::WEBKIT_COOKIE_PERSISTENT_STORAGE_SQLITE
        );
        orion::register_cookie_max_age_clamp(cookie_manager, profile_id());

        WebKitWebsiteDataManager* website_data_manager = webkit_network_session_get_website_data_manager(m_network_session.get());
        webkit_website_data_manager_set_favicons_enabled(website_data_manager, TRUE);

        // Enable ITP (Intelligent Tracking Prevention) by default
        webkit_network_session_set_itp_enabled(m_network_session.get(), TRUE);

        orion::ProfileId* profile_id =
            new orion::ProfileId(this->profile_id());
        g_signal_connect_data(m_network_session.get(), "download-started", G_CALLBACK(orion::on_download_started), profile_id,
            [](gpointer data, GClosure* closure){ delete static_cast<orion::ProfileId*>(data); }, G_CONNECT_DEFAULT);
    }
    return m_network_session.get();
}

// static void
// reader_check_cb(WebKitUserContentManager* user_content_manager, JSCValue* value, OrionWebkitWebview* view)
// {
//     if(!jsc_value_is_boolean(value))
//     {
//         orion::log_warning("is_probably_readerable.js script returned non boolean");
//         return;

//     }
//     g_object_set(view, "is-readerable", jsc_value_to_boolean(value), NULL);
// }

// void
// WebExtensionWebViewFactoryImpl::user_content_manager_initialize_readerability_check(WebKitUserContentManager* manager, OrionWebkitWebview* web_view)
// {
//     //this message handler needs to match the one in is_probably_readerable.js
//     webkit_user_content_manager_register_script_message_handler(manager, "readerCheck", READABILITY_SCRIPT_WORLD);
//     g_signal_connect(manager, "script-message-received::readerCheck", G_CALLBACK(reader_check_cb), web_view);

//     if(!m_is_probably_readerable_script)
//     {
//         GError* err = NULL;
//         GBytes* js_bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/is_probably_readerable.js", G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
//         if(err)
//         {
//             orion::log_error("%s", err->message);
//             g_error_free(err);
//             return;
//         }

//         gsize data_size = 0;
//         gconstpointer js_data = g_bytes_get_data(js_bytes, &data_size);
//         gchar* js_string = g_strndup(static_cast<const gchar*>(js_data), data_size);

//         g_bytes_unref(js_bytes);

//         m_is_probably_readerable_script.reset(webkit_user_script_new_for_world(js_string, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
//             WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, READABILITY_SCRIPT_WORLD, NULL, NULL)); 
//         g_free(js_string);
//     }
//     webkit_user_content_manager_add_script(manager, m_is_probably_readerable_script.get());
// }

WebViewRef 
WebExtensionWebViewFactoryImpl::sync_web_view_common(WebViewRef related_view)
{
    auto world = parent_world().lock();
    if(!world) return nullptr;
    auto profile_bridge = world->profile_bridge().lock();
    if(!profile_bridge) return nullptr;
    WebKitWebContext* context = sync_web_context();
    WebKitNetworkSession* network_session = sync_network_session();
    WebKitSettings* settings = webkit_settings_new_with_settings(
        "enable-write-console-messages-to-stdout", ENABLE_WRITE_CONSOLE_MESSAGES_TO_STDOUT,
        "enable-developer-extras", TRUE,
        "enable-back-forward-navigation-gestures", FALSE, //since we have a custom go back/go forward, we need to fully reimplement these
        //NOTE: specific known good user agent that avoids issues with cloudflare turnstile (https://www.scholarshipscanada.com/News/9/1415/Scholar-News) and also claude.ai
        //at time of writing, the "Ubuntu" substring seems to be freely replaceable while still passing these checks
        "user-agent", "Mozilla/5.0 (X11; Ubuntu; Linux x86_64) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/60.5 Safari/605.1.15",
        // Disable hardware acceleration by default to avoid issues with GPU on for example `claude.ai` page.
        // Note: We cannot disable hardware acceleration because Speedometer 3.0 test relies on it.
        "hardware-acceleration-policy", WEBKIT_HARDWARE_ACCELERATION_POLICY_ALWAYS,

        // "enable-webaudio", TRUE,
        // "enable-media-stream", TRUE,
        // "enable-mediasource", TRUE,
        // "enable-smooth-scrolling", TRUE,
        "javascript-can-open-windows-automatically", TRUE,
        // "enable-2d-canvas-acceleration", FALSE,
        // "enable-webgl", FALSE,
        NULL
    );

    WebKitUserContentManager* user_content_manager = webkit_user_content_manager_new();
    OrionWebkitWebview* orion_webkit_webview = NULL;
    if(!related_view)
    {
        orion_webkit_webview = (OrionWebkitWebview*)g_object_new(ORION_WEBKIT_WEBVIEW_TYPE,
            "web-context", context,
            "network-session", network_session,
            "settings", settings,
            "user-content-manager", user_content_manager,
            NULL
        );
    }
    else
    {
        g_assert(webkit_web_view_get_context(WEBKIT_WEB_VIEW(related_view.get())) == context);
        g_assert(webkit_web_view_get_network_session(WEBKIT_WEB_VIEW(related_view.get())) == network_session);
        orion_webkit_webview = (OrionWebkitWebview*)g_object_new(ORION_WEBKIT_WEBVIEW_TYPE,
            "related-view", related_view.get(),
            "settings", settings,
            "user-content-manager", user_content_manager,
            NULL
        );
    }

    auto web_view_bridge = create_web_view_bridge().lock();
    if(!web_view_bridge) {
        g_object_ref_sink(orion_webkit_webview);
        g_object_unref(orion_webkit_webview);
        g_object_unref(settings);
        g_object_unref(user_content_manager);
        return nullptr;
    }
    auto web_view_host = orion::create_app_web_view_host(
        *web_view_bridge,
        reinterpret_cast<OpaqueWebView*>(orion_webkit_webview));
    g_object_set_data_full(
        G_OBJECT(orion_webkit_webview), "orion-web-view-host",
        web_view_host.release(),
        [](gpointer data) {
            delete static_cast<orion::bridge::WebViewHost*>(data);
        });

    configure_web_view(*web_view_bridge);

    // Track webkit webviews (with weak refs only) to be able to manage their content filters
    orion::gtk::WeakRefWrapper orion_webkit_webview_weak(orion_webkit_webview);
    // We should occasionally clean up expired weaks from the list (periodically)
    m_orion_webkit_webviews.push_back(orion_webkit_webview_weak);
    m_orion_webkit_webviews_change->notify({.created = {orion_webkit_webview_weak}});

    g_object_unref(settings);
    g_object_unref(user_content_manager);
    return WebViewRef((OpaqueWebView*)g_object_ref_sink(orion_webkit_webview));
}

WebViewRef
WebExtensionWebViewFactoryImpl::sync_web_view_for_content(WebViewRef related_view)
{
    return sync_web_view_common(std::move(related_view));
}

WebViewRef
WebExtensionWebViewFactoryImpl::sync_web_view_for_extension_popup(orion::ExtensionId extension_id, WebViewRef related_view)
{
    std::optional<std::string> popup_uri = document_uri(
        extension_id, WebExtensionDocumentKind::popup);
    orion::log_debug("popup uri: %s", popup_uri.value_or("<null>").c_str());
    if(!popup_uri) {
        return nullptr;
    }

    WebViewRef web_view = sync_web_view_common(std::move(related_view));
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(web_view.get()), popup_uri.value().c_str());

    return web_view;
}

void
WebExtensionWebViewFactoryImpl::web_view_for_content(WebViewReply reply)
{
    when_ready([weak_factory = weak_from_this(), reply = std::move(reply)] {
        auto factory = weak_factory.lock();
        if(!factory) return;
        reply(static_cast<WebExtensionWebViewFactoryImpl&>(*factory)
            .sync_web_view_for_content());
    });
}

WebViewRef
WebExtensionWebViewFactoryImpl::web_view_for_content_synchronous(WebViewRef related_view)
{
    if(is_ready())
    {
        return sync_web_view_for_content(std::move(related_view));
    }
    else
    {
        return NULL;
    }
}

void
WebExtensionWebViewFactoryImpl::web_view_for_extension_popup(orion::ExtensionId extension_id, WebViewReply reply)
{
    when_ready([weak_factory = weak_from_this(), reply = std::move(reply), extension_id] {
        auto factory = weak_factory.lock();
        if(!factory) return;
        reply(static_cast<WebExtensionWebViewFactoryImpl&>(*factory)
            .sync_web_view_for_extension_popup(extension_id));
    });
}

bool
WebExtensionWebViewFactoryImpl::show_extension_background_web_inspector(orion::ExtensionId extension_id)
{
    auto it = m_background_web_views.find(extension_id);
    if(it == m_background_web_views.end()) {
        orion::log_warning(
            "WebExtensionWebView> No background page for extension {%s} in profile %lld",
            extension_id.value.c_str(),
            profile_id().value
        );
        return false;
    }

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(it->second.get()));
    webkit_web_inspector_show(inspector);
    return true;
}

OpaqueNetworkSession*
WebExtensionWebViewFactoryImpl::network_session()
{
    return (OpaqueNetworkSession*)sync_network_session();
}

std::vector<orion::gtk::WeakRefWrapper>
WebExtensionWebViewFactoryImpl::web_views()
{
    return m_orion_webkit_webviews;
}

orion::Observable<WebViewMapChange>&
WebExtensionWebViewFactoryImpl::web_views_change() const
{
    return *m_orion_webkit_webviews_change;
}

void
WebExtensionWebViewFactoryImpl::set_web_views_observer(orion::ObserverId observer_id)
{
    m_web_views_observer = observer_id;
}

orion::ObserverId
WebExtensionWebViewFactoryImpl::web_views_observer() const
{
    return m_web_views_observer;
}

ContentFilterStore*
WebExtensionWebViewFactoryImpl::filter_store()
{
    return &m_filter_store;
}

} // namespace WebExtensions

#else // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace WebExtensions {

class WebExtensionWebViewFactoryImpl
    : public WebExtensionWebViewFactory {
public:
    explicit WebExtensionWebViewFactoryImpl(
        orion::bridge::WebViewFactoryBridge& bridge)
        : WebExtensionWebViewFactory(bridge) {
    }

    void web_view_for_content(WebViewReply reply) override {
        if(reply) reply(nullptr);
    }

    WebViewRef web_view_for_content_synchronous(WebViewRef related_webview) override { return nullptr; }

    void web_view_for_extension_popup(
        orion::ExtensionId extension_id, WebViewReply reply) override {
        if(reply) reply(nullptr);
    }

    bool show_extension_background_web_inspector(orion::ExtensionId extension_id) override { return false; }

    void update_background_pages(const WebExtensionBackgroundPagesChange& change) override {}

    void register_scheme(
        const WebExtensionSchemeRegistration& registration,
        WebExtensionSchemeRequestHandler handler
    ) override {}

    std::vector<orion::gtk::WeakRefWrapper>
    web_views() override
    {
        return {};
    }

    orion::Observable<WebViewMapChange>&
    web_views_change() const override
    {
        static orion::Observable<WebViewMapChange> dummy;
        return dummy;
    }

    void
    set_web_views_observer(orion::ObserverId observer_id) override
    {
    }

    orion::ObserverId
    web_views_observer() const override
    {
        return {};
    }

    OpaqueNetworkSession*
    network_session() override
    {
        return nullptr;
    }

    ContentFilterStore*
    filter_store() override
    {
        return &m_filter_store;
    }

private:
    ContentFilterStore m_filter_store{};
};

} // namespace WebExtensions

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

std::shared_ptr<WebExtensionWebViewFactory>
WebExtensionWebViewFactory::create(
    orion::bridge::WebViewFactoryBridge& bridge)
{
    auto factory =
        std::make_shared<WebExtensionWebViewFactoryImpl>(bridge);
    factory->m_attachment = bridge.attach(*factory);
    return factory;
}

void
web_view_unref(OpaqueWebView* web_view)
{
    g_object_unref(web_view);
}

std::vector<std::string>
WebExtensionWebViewFactory::get_profile_path_parts(orion::ProfileId profile_id, orion::PrivateMode private_mode)
{
    std::vector<std::string> base_path{
        g_get_user_cache_dir(),
        APPLICATION_COMMAND,
        std::to_string(profile_id.value)
    };

    if (private_mode == orion::PrivateMode::yes) {
        base_path.push_back("private");
    }

    return base_path;
}
