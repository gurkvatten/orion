#define ENABLE_VERBOSE_LOGS 0

#include "kagi_service.h"
#include "app.h"
#include "util/stop_cancellable.h"

#include <core_module/core/core_api.h>
#include <core_module/core/settings.h>
#include <common/src/string_util.h>
#include <common/src/uuid.h>
#include <common/src/crypto.h>
#include <common/src/defer.h>
#include <libsoup/soup.h>

#include <fstream>

namespace orion {

// See also Kagi/Core/API Handlers/KagiService.swift in https://github.com/kagisearch/browser.git
class KagiServiceImpl : public KagiService {
public: // interface implementations
    void get_or_create_kagi_client_id(std::function<void(std::string, orion::Error)> cb) override;
    bool is_kagi_search_query_url(const orion::Uri& uri) override;
    void try_kagi_search_onboarding(Cb<Error> cb) override;
    void set_onboarding_status(OnboardingStatus status) override { m_onboarding_status = status; }
    OnboardingStatus onboarding_status() const override { return m_onboarding_status; }
    void cancel_all_operations(std::function<void()> cancelled_cb) override;
    
private: // usings
    using WebViewFactory = WebExtensions::WebExtensionWebViewFactory;
    using SignupCb = std::function<void(int32_t status, const std::string& token)>;

public: // public methods
    explicit KagiServiceImpl(
        bridge::ProfileWorld& profile_world);
    ~KagiServiceImpl() override;
    void callback_started(const char* name);
    void callback_finished(const char* name);
    static std::string session_cookie_name() { return "kagi_session"; }

private: // private methods
    void start_onboarding(Cb<Error> cb);
    void check_existing_kagi_session(std::function<void(bool has_session)> cb);
    void signup(SignupCb cb);
    void save_onboarding(const std::string& token, Cb<Error> cb);
    void store_kagi_session_cookie_async(const std::string& token);

private: // static helper functions
    static std::string kagi_base_url() { return "https://kagi.com"; }
    static std::string signup_url() { return kagi_base_url() + "/signup/mobile"; }
    static std::string kagi_search_url() { return kagi_base_url() + "/search"; }
    static void do_http_request(KagiServiceImpl* self, const std::string& client_id, SignupCb cb);
    static void soup_session_send_and_read_cb(GObject* object, GAsyncResult* result, gpointer user_data);
    static void webkit_cookie_manager_add_cookie_finish_cb(GObject* source, GAsyncResult* result, gpointer user_data);

private: // data members
    OnboardingStatus m_onboarding_status = OnboardingStatus::none;
    bool m_is_existing_kagi_session = false;
    std::weak_ptr<bridge::ProfileWorld> m_profile_world;
    ProfileId m_profile_id;
    int32_t m_pending_callbacks = 0;
    std::vector<std::function<void()>> m_cancelled_cbs{};
    std::shared_ptr<orion::StopCancellable> m_stop_cancellable{};

private: // friends
    friend std::shared_ptr<KagiService> KagiService::create(
        bridge::ProfileWorld& profile_world);

private: // enums
    enum class SignupError : int32_t {
        too_many_requests = 429,
        mobile_signups_disabled = 403,
        bad_request = 400,
        something_went_wrong = 500,
    };

private: // structs
    struct SoupSessionSendAndReadData {
        KagiServiceImpl* kagi_service{};
        SoupMessage* msg{};
        KagiServiceImpl::SignupCb signup_cb{};
    };
};

std::shared_ptr<KagiService>
KagiService::create(bridge::ProfileWorld& profile_world)
{
    auto service = std::make_shared<KagiServiceImpl>(profile_world);
    service->m_stop_cancellable = orion::StopCancellable::create();
    return service;
}

KagiServiceImpl::KagiServiceImpl(
    bridge::ProfileWorld& profile_world)
    : m_profile_world(profile_world.weak_from_this())
    , m_profile_id(profile_world.profile_id())
{
    orion::log_verbose(
        "KagiServiceImpl created for profile_id=%lld",
        m_profile_id.value
    );
}

KagiServiceImpl::~KagiServiceImpl()
{
    orion::log_verbose(
        "KagiServiceImpl destroyed for profile_id=%lld",
        m_profile_id.value
    );
}

void
KagiServiceImpl::cancel_all_operations(std::function<void()> cancelled_cb)
{
    m_cancelled_cbs.emplace_back(std::move(cancelled_cb));

    m_stop_cancellable->cancel();

    if (m_pending_callbacks == 0) {
        for (auto& cb : m_cancelled_cbs) {
            if (cb) {
                // Never call the callback synchronously, always dispatch it
                global_core_api->async([cb=std::move(cb)]{ cb(); });
            }
        }
    }
}

// Returns the same machine ID when running native or in Flatpak, as long as the same /etc/machine-id is visible.
// Returns empty string if the machine ID cannot be read for some reason.
static std::string
get_machine_id()
{
    std::ifstream file("/etc/machine-id");
    if (!file.is_open()) {
        return "";
    }

    std::string id;
    std::getline(file, id);
    orion::str::trim(&id);
    return id;
}

void
KagiServiceImpl::get_or_create_kagi_client_id(std::function<void(std::string, orion::Error)> cb)
{
    global_core_api->get_common_setting(orion::settings::common::onboarding::kagi_client_id,
        [cb=std::move(cb)](orion::GetSettingReply<std::string> reply){
            std::string machine_id = get_machine_id();

            if (!machine_id.empty()) {
                // Never generate or return stored UUID when machine ID is available
                cb(orion::crypto::md5_str(machine_id), orion::Error::ok());
                return;
            }

            // Try to fallback to generated UUID

            if (reply.err) {
                cb("", reply.err);
                return;
            }

            if (!reply.value.empty()) {
                // Already stored UUID, return it
                cb(reply.value, orion::Error::ok());
                return;
            }

            std::string uuid = orion::generate_uuid_v4();
            std::string kagi_client_id = orion::crypto::md5_str(uuid);

            global_core_api->set_common_setting(orion::settings::common::onboarding::kagi_client_id, kagi_client_id,
                [cb=std::move(cb)](orion::SetSettingReply<std::string> reply){
                    cb(reply.new_value, reply.err);
                }
            );
        }
    );
}

bool
KagiServiceImpl::is_kagi_search_query_url(const orion::Uri& uri)
{
    return uri.uri().find(kagi_search_url() + "?q=") != std::string::npos;
}

void
KagiServiceImpl::callback_started(const char* name)
{
    m_pending_callbacks++;
    orion::log_verbose("kagi_service: callback_started: pending_callbacks=%d name=%s", m_pending_callbacks, name);
}

void
KagiServiceImpl::callback_finished(const char* name)
{
    m_pending_callbacks--;
    orion::log_verbose("kagi_service: callback_finished: pending_callbacks=%d name=%s", m_pending_callbacks, name);

    if (m_pending_callbacks == 0) {
        for (auto& cb : m_cancelled_cbs) {
            if (cb) {
                // Never call the callback synchronously, always dispatch it
                global_core_api->async([cb=std::move(cb)]{ cb(); });
            }
        }
    }
}

void
KagiServiceImpl::try_kagi_search_onboarding(Cb<Error> cb)
{
    if (m_onboarding_status != OnboardingStatus::pending) {
        cb(Error::ok());
        return;
    }

    m_onboarding_status = OnboardingStatus::in_progress;
    start_onboarding(cb);
}

void
KagiServiceImpl::save_onboarding(const std::string& token, Cb<Error> cb)
{
    global_core_api->set_common_setting(orion::settings::common::onboarding::token_key, token,
        [cb=std::move(cb)](orion::SetSettingReply<std::string> reply){
            if (reply.err) {
                cb(WRAP_ERROR(reply.err));
                return;
            }

            global_core_api->set_common_setting(orion::settings::common::onboarding::is_onboarded, true,
                [cb=std::move(cb)](orion::SetSettingReply<bool> reply){
                    if (reply.err) {
                        cb(WRAP_ERROR(reply.err));
                        return;
                    }
                    cb(Error::ok());
                }
            );
        }
    );
}

void
KagiServiceImpl::soup_session_send_and_read_cb(GObject* object, GAsyncResult* result, gpointer user_data)
{
    SoupSessionSendAndReadData* data = reinterpret_cast<SoupSessionSendAndReadData*>(user_data);
    defer([data]{ delete data; }); // deleter will be called as the last deferred function in this function

    KagiServiceImpl* self = data->kagi_service;
    SoupSession* session = SOUP_SESSION(object);
    SoupMessage* msg = data->msg;

    defer([session]{ g_object_unref(session); });
    defer([msg]{ g_object_unref(msg); });
    defer([self]{ self->callback_finished("soup_session_send_and_read_cb"); });

    GError* error = NULL;
    GBytes* body = soup_session_send_and_read_finish(session, result, &error);
    if (error) {
        defer([error] { g_error_free(error); });
        orion::log_error("soup_session_send_and_read_cb: %s", error->message);
        if (data->signup_cb) {
            data->signup_cb(500, "");
        }
        return;
    }
    defer([body]{ g_bytes_unref(body); });

    int32_t status_code = soup_message_get_status(msg);
    if (status_code != 200) {
        orion::log_error("soup_session_send_and_read_cb: HTTP request failed with status code %d", status_code);
        if (data->signup_cb) {
            data->signup_cb(status_code, "");
        }
        return;
    }

    GSList* cookies = soup_cookies_from_response(msg);
    defer([cookies]{ soup_cookies_free(cookies); });

    std::string kagi_session = ""; // token
    bool has_kagi_session_cookie = false;

    for (GSList* l = cookies; l != NULL; l = l->next) {
        SoupCookie* cookie = reinterpret_cast<SoupCookie*>(l->data);

        orion::log_debug(
            "cookie: %s=%s; domain=%s; path=%s; secure=%d; http_only=%d",
            soup_cookie_get_name(cookie),
            soup_cookie_get_value(cookie),
            soup_cookie_get_domain(cookie),
            soup_cookie_get_path(cookie),
            soup_cookie_get_secure(cookie),
            soup_cookie_get_http_only(cookie)
        );

        if (soup_cookie_get_name(cookie) == KagiServiceImpl::session_cookie_name()) {
            kagi_session = soup_cookie_get_value(cookie);
            has_kagi_session_cookie = true;
            break;
        }
    }

    orion::log_debug(
        "soup_session_send_and_read_cb: response length=%zu status_code=%d cookies=%p kagi_session=%s",
        g_bytes_get_size(body),
        status_code,
        cookies,
        kagi_session.c_str()
    );

    if (has_kagi_session_cookie) {
        orion::log_debug("kagi_session found in cookies: %s", kagi_session.c_str());
        // Store kagi_session token to webkit cookies asynchronously
        self->store_kagi_session_cookie_async(kagi_session);
    } else {
        orion::log_warning("kagi_session cookie not found in response");
    }

    if (data->signup_cb) {
        data->signup_cb(status_code, kagi_session);
    }
}

void
KagiServiceImpl::do_http_request(KagiServiceImpl* self, const std::string& client_id, SignupCb cb)
{
    SoupSession* session = soup_session_new(); // we should ensure this is freed
    SoupMessage* msg = soup_message_new("POST", self->signup_url().c_str()); // we should ensure this is freed

    if (!msg) {
        orion::log_error("do_http_request: failed to create SoupMessage");
        g_object_unref(session);
        cb(500, "");
        return;
    }

    std::string app_version = "0.0.0.0.1";
    std::string passphrase = "8f4b1169-384c-45dc-95dc-2c87446a4ca6";
    std::string body_str =
        "device_id=" + client_id + "&app_version=" + app_version + "&passphrase=" + passphrase + "&orion=1";
    std::string ua_prefix = "Orion v";
        
    soup_message_headers_append(
        soup_message_get_request_headers(msg),
        "user-agent",
        (ua_prefix + app_version).c_str()
    );

    GBytes* body_bytes = g_bytes_new(body_str.data(), body_str.size());
    defer([body_bytes]{ g_bytes_unref(body_bytes); });

    soup_message_set_request_body_from_bytes(msg, "application/x-www-form-urlencoded", body_bytes);

    self->callback_started("soup_session_send_and_read_cb");

    SoupSessionSendAndReadData* data = new SoupSessionSendAndReadData{
        .kagi_service = self,
        .msg = msg,
        .signup_cb = std::move(cb),
    };

    soup_session_send_and_read_async(
        session,
        msg,
        G_PRIORITY_DEFAULT, // io_priority
        self->m_stop_cancellable->get(), // cancellable
        KagiServiceImpl::soup_session_send_and_read_cb,
        data // user_data
    );
}

void
KagiServiceImpl::signup(SignupCb cb)
{
    callback_started("get_or_create_kagi_client_id");
    get_or_create_kagi_client_id([this,cb=std::move(cb)](std::string client_id, orion::Error err){
        defer([this]{ callback_finished("get_or_create_kagi_client_id"); });
        if (err) {
            LOG_ERROR(err);
            cb(500, "");
            return;
        }

        // Override client_id for testing purposes here if needed

        // Issue HTTP signup request
        do_http_request(this, client_id, std::move(cb));
    });
}

void
KagiServiceImpl::start_onboarding(Cb<Error> cb)
{
    callback_started("check_existing_kagi_session");
    check_existing_kagi_session([this,cb=std::move(cb)](bool has_session){
        defer([this]{ callback_finished("check_existing_kagi_session"); });

        if (has_session) {
            orion::log_verbose("kagi session: exists");
        } else {
            orion::log_debug("kagi session: does not exist");
        }

        if (has_session) {
            m_onboarding_status = OnboardingStatus::not_applicable;
            cb(Error::ok());
            return;
        }

        orion::log_debug("kagi signup: creating trial account");

        callback_started("signup");
        signup([this,cb=std::move(cb)](int32_t status, const std::string& token){
            defer([this]{ callback_finished("signup"); });

            if (status == 200 && !token.empty()) {
                callback_started("save_onboarding");
                save_onboarding(token, [this,cb=std::move(cb)](Error err){
                    defer([this]{ callback_finished("save_onboarding"); });
                    if (err) {
                        cb(WRAP_ERROR(err));
                        return;
                    }
                    orion::log_debug("kagi onboarding: signup succeeded");
                    cb(Error::ok());
                });
                return;
            }

            orion::log_error("kagi onboarding: failed to signup with status code %d", status);
            m_onboarding_status = OnboardingStatus::error;
            cb(Error::ok());
        });
    });
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {
    struct CookieManagerGetCookiesData {
        std::function<void(bool has_session)> cb{};
        orion::KagiServiceImpl* kagi_service{};
    };
}

static void
on_cookie_manager_get_cookies_finish(GObject* source, GAsyncResult* result, gpointer user_data)
{
    auto data = reinterpret_cast<CookieManagerGetCookiesData*>(user_data);
    defer([data]{ data->kagi_service->callback_finished("on_cookie_manager_get_cookies_finish"); delete data; });

    GError* err = NULL;
    GList* cookies_list = webkit_cookie_manager_get_cookies_finish(WEBKIT_COOKIE_MANAGER(source), result, &err);
    if (err) {
        defer([err]{ g_error_free(err); });
        orion::log_error("on_cookie_manager_get_cookies_finish: %s", err->message);
        return;
    }

    bool has_session = false;

    // Enumerate the resulting list
    for (GList* l = cookies_list; l != NULL; l = l->next) {
        SoupCookie* cookie = reinterpret_cast<SoupCookie*>(l->data);
        gint64 expires = g_date_time_to_unix(soup_cookie_get_expires(cookie));

        orion::log_verbose(
            "cookie: %s=%s; domain=%s; path=%s; secure=%d; http_only=%d; expires=%lld",
            soup_cookie_get_name(cookie),
            soup_cookie_get_value(cookie),
            soup_cookie_get_domain(cookie),
            soup_cookie_get_path(cookie),
            soup_cookie_get_secure(cookie),
            soup_cookie_get_http_only(cookie),
            expires
        );

        if (g_strcmp0(soup_cookie_get_name(cookie), data->kagi_service->session_cookie_name().c_str()) == 0) {
            // We found the session cookie
            has_session = true;
            break;
        }
    }

    // Notify the result
    data->cb(has_session);

    // Free the list
    for (GList* l = cookies_list; l != NULL; l = l->next) {
        soup_cookie_free(reinterpret_cast<SoupCookie*>(l->data));
    }
    g_list_free(cookies_list);
}

void
KagiServiceImpl::check_existing_kagi_session(std::function<void(bool has_session)> cb)
{
    if (m_is_existing_kagi_session) {
        cb(true);
        return;
    }

    auto profile_world = m_profile_world.lock();
    auto factory = profile_world
        ? orion_app_get_web_view_factory(*profile_world).lock()
        : nullptr;
    if(!factory) {
        cb(false);
        return;
    }
    auto session = reinterpret_cast<WebKitNetworkSession*>(factory->network_session());
    WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager(session);

    auto data = new CookieManagerGetCookiesData{
        .cb = std::move(cb),
        .kagi_service = this, // we must ensure this pointer remains valid until the callback is called
    };

    webkit_cookie_manager_get_cookies(
        cookie_manager,
        kagi_base_url().c_str(),
        m_stop_cancellable->get(), // cancellable
        on_cookie_manager_get_cookies_finish,
        data // user_data
    );
    callback_started("on_cookie_manager_get_cookies_finish");
}

void
KagiServiceImpl::webkit_cookie_manager_add_cookie_finish_cb(GObject* source, GAsyncResult* result, gpointer user_data)
{
    KagiServiceImpl* self = reinterpret_cast<KagiServiceImpl*>(user_data);
    defer([self]{ self->callback_finished("webkit_cookie_manager_add_cookie_finish_cb"); });

    GError* error = NULL;
    if (!webkit_cookie_manager_add_cookie_finish(WEBKIT_COOKIE_MANAGER(source), result, &error)) {
        defer([error] { g_error_free(error); });
        orion::log_error("webkit_cookie_manager_add_cookie_finish_cb: %s", error->message);
        return;
    }

    orion::log_debug("kagi_session cookie successfully added to cookie manager");
}

void
KagiServiceImpl::store_kagi_session_cookie_async(const std::string& token)
{
    auto profile_world = m_profile_world.lock();
    auto factory = profile_world
        ? orion_app_get_web_view_factory(*profile_world).lock()
        : nullptr;
    if(!factory) return;
    auto session = reinterpret_cast<WebKitNetworkSession*>(factory->network_session());
    WebKitCookieManager* cookie_manager = webkit_network_session_get_cookie_manager(session);

    // Must be freed
    SoupCookie* cookie = soup_cookie_new(
        "kagi_session", // name
        token.c_str(), // value
        ".kagi.com", // domain
        "/", // path
        INT_MAX // max_age (distant future)
    );

    soup_cookie_set_secure(cookie, TRUE);

    callback_started("webkit_cookie_manager_add_cookie_finish_cb");

    webkit_cookie_manager_add_cookie(
        cookie_manager,
        cookie,
        m_stop_cancellable->get(), // cancellable
        KagiServiceImpl::webkit_cookie_manager_add_cookie_finish_cb, // callback
        this // user_data
    );

    soup_cookie_free(cookie); // should be safe to free immediately after async webkit call
}

#else // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

void
KagiServiceImpl::check_existing_kagi_session(std::function<void(bool has_session)> cb)
{
    cb(false);
}

void
KagiServiceImpl::store_kagi_session_cookie_async(const std::string& token)
{
}

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

} // namespace orion
