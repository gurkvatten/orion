#include "webextensions/webview_host.h"
#include "window/main_window/main_window.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

#include <common/src/coalesce.h>
#include <common/src/memory.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <jsc/jsc.h>
#include <webkit/webkit.h>
#endif

namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {

struct ScriptMessageHandlerData {
    bridge::WebViewHost::WebViewMessageHandler message_handler;
    void* context;
    void(*destroy_context)(void*);
};

static void
message_handler_callback(WebKitUserContentManager* self, JSCValue* value, ScriptMessageHandlerData* data)
{
    char* str = ::jsc_value_to_string(value);
    std::string message_json = str ? str : "";
    g_free(str);
    data->message_handler(message_json, data->context);
}

class GtkWebViewHost final : public bridge::WebViewHost {
public:
    GtkWebViewHost(
        bridge::WebViewBridge& bridge,
        OpaqueWebView* webview)
        : m_attachment(bridge.attach(*this))
        , m_bridge(bridge.weak_from_this())
        , m_web_view(webview)
    {
    }

    std::weak_ptr<bridge::WebViewBridge> bridge() const
    {
        return m_bridge;
    }

    ~GtkWebViewHost() override
    {
        for(const auto& [manager, signal_handler] : m_signal_handlers) {
            g_signal_handler_disconnect(manager, signal_handler);
            g_object_unref(manager);
        }
    }

    void
    web_view_call(
        const std::string& body, const std::string& argument0_json,
        const std::string& script_world
    ) override
    {
        OpaqueWebView* webview = m_web_view;
        GVariantDict arguments_dict;
        g_variant_dict_init(&arguments_dict, NULL);
        g_variant_dict_insert_value(&arguments_dict, "arg0", g_variant_new_string(argument0_json.c_str()));
        GVariant* arguments = g_variant_dict_end(&arguments_dict);
        webkit_web_view_call_async_javascript_function(
            (WebKitWebView*)webview,
            body.c_str(),
            -1,
            arguments,
            script_world.empty() ? NULL : script_world.c_str(),
            NULL,
            NULL,
            NULL,
            NULL
        );
        g_variant_unref(arguments);
    }

    void
    web_view_add_script(
        const std::vector<std::string>& matches,
        const std::vector<std::string>& exclude_matches, const std::string& script,
        const std::string& script_world, WebViewScriptInjectionTime injection_time,
        WebViewScriptFrames injected_frames
    ) override
    {
        OpaqueWebView* webview = m_web_view;
        if(matches.empty()) {
            // Empty matches restricts use to no access; use an explicit wildcard to inject everywhere.
            return;
        }
        WebKitUserContentManager* user_content_manager =
            webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(webview));
        std::vector<const char*> matches_ptrs;
        for(const auto& match : matches) {
            matches_ptrs.push_back(match.c_str());
        }
        matches_ptrs.push_back(nullptr);
        std::vector<const char*> exclude_matches_ptrs;
        for(const auto& exclude_match : exclude_matches) {
            exclude_matches_ptrs.push_back(exclude_match.c_str());
        }
        exclude_matches_ptrs.push_back(nullptr);
        const auto* exclude_matches_data = exclude_matches.empty()
            ? nullptr
            : exclude_matches_ptrs.data();
        const auto webkit_frames = injected_frames == WebViewScriptFrames::all_frames
            ? WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES
            : WEBKIT_USER_CONTENT_INJECT_TOP_FRAME;
        const auto webkit_injection_time = injection_time == WebViewScriptInjectionTime::document_start
            ? WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START
            : WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END;
        UNIQUE_PTR(WebKitUserScript, webkit_user_script_unref) webextensions_bridge_script(
            script_world.empty()
                ? webkit_user_script_new(
                    script.c_str(), webkit_frames, webkit_injection_time,
                    matches_ptrs.data(), exclude_matches_data)
                : webkit_user_script_new_for_world(
                    script.c_str(), webkit_frames, webkit_injection_time,
                    script_world.c_str(), matches_ptrs.data(), exclude_matches_data));
        webkit_user_content_manager_add_script(user_content_manager, webextensions_bridge_script.get());
    }

    bool
    web_view_register_script_message_handler(
        const std::string& message_handler_name,
        WebViewMessageHandler message_handler,
        void* context,
        void(*destroy_context)(void*),
        const std::string& script_world
    ) override
    {
        OpaqueWebView* webview = m_web_view;
        WebKitUserContentManager* user_content_manager = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(webview));
        if(!webkit_user_content_manager_register_script_message_handler(
            user_content_manager, message_handler_name.c_str(),
            script_world.empty() ? nullptr : script_world.c_str())) {
            if(destroy_context) destroy_context(context);
            return false;
        }
        auto destroy_data = [](gpointer ptr, GClosure *closure) {
            auto* data = static_cast<ScriptMessageHandlerData*>(ptr);
            if(data->destroy_context) data->destroy_context(data->context);
            delete data;
        };
        ScriptMessageHandlerData* data = new ScriptMessageHandlerData {
            .message_handler = message_handler,
            .context = context,
            .destroy_context = destroy_context,
        };
        const gulong signal_handler = g_signal_connect_data(
            user_content_manager,
            ("script-message-received::" + message_handler_name).c_str(),
            G_CALLBACK(message_handler_callback), data, destroy_data,
            static_cast<GConnectFlags>(0));
        m_signal_handlers.emplace_back(
            WEBKIT_USER_CONTENT_MANAGER(g_object_ref(user_content_manager)),
            signal_handler);
        return true;
    }

    std::string
    web_view_security_origin_get_host() override
    {
        OpaqueWebView* webview = m_web_view;
        const char* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(webview));
        UNIQUE_PTR(WebKitSecurityOrigin, webkit_security_origin_unref) security_origin(webkit_security_origin_new_for_uri(uri));
        std::string security_origin_host = coalesce(webkit_security_origin_get_host(security_origin.get()), "");
        return security_origin_host;
    }

    std::string
    web_view_uri() override
    {
        OpaqueWebView* webview = m_web_view;
        const char* uri = webkit_web_view_get_uri(WEBKIT_WEB_VIEW(webview));
        return uri ? uri : "";
    }

    std::optional<int64_t>
    web_view_tab_id() override
    {
        OpaqueWebView* webview = m_web_view;
        GtkWidget* tab_content = gtk_widget_get_ancestor(
            GTK_WIDGET(webview), ORION_WEBVIEW_TAB_CONTENT_TYPE);
        if(!tab_content) {
            return std::nullopt;
        }
        return orion_webview_tab_content_get_id(
            ORION_WEBVIEW_TAB_CONTENT(tab_content)).value;
    }

    std::optional<int64_t>
    web_view_window_id() override
    {
        OpaqueWebView* webview = m_web_view;
        MainWindow* window = orion::gtk::get_root_main_window(GTK_WIDGET(webview));
        if(!window) {
            return std::nullopt;
        }
        return main_window_get_window_id(window).value;
    }

    std::optional<int64_t>
    web_view_active_tab_id() override
    {
        OpaqueWebView* webview = m_web_view;
        MainWindow* window = orion::gtk::get_root_main_window(GTK_WIDGET(webview));
        if(!window) {
            return std::nullopt;
        }
        return main_window_get_current_tab_id(window).value;
    }

private:
    bridge::HostAttachment m_attachment;
    std::weak_ptr<bridge::WebViewBridge> m_bridge;
    OpaqueWebView* m_web_view;
    std::vector<std::pair<WebKitUserContentManager*, gulong>> m_signal_handlers;
};

} // namespace

std::unique_ptr<bridge::WebViewHost>
create_app_web_view_host(
    bridge::WebViewBridge& web_view_bridge, OpaqueWebView* web_view)
{
    return std::make_unique<GtkWebViewHost>(web_view_bridge, web_view);
}

std::weak_ptr<bridge::WebViewBridge>
app_web_view_bridge(OpaqueWebView* web_view)
{
    auto* host = static_cast<GtkWebViewHost*>(g_object_get_data(
        G_OBJECT(web_view), "orion-web-view-host"));
    return host ? host->bridge() : std::weak_ptr<bridge::WebViewBridge>();
}

#else

std::unique_ptr<bridge::WebViewHost>
create_app_web_view_host(bridge::WebViewBridge&, OpaqueWebView*)
{
    return nullptr;
}

std::weak_ptr<bridge::WebViewBridge>
app_web_view_bridge(OpaqueWebView*)
{
    return {};
}

#endif

} // namespace orion
