#define ENABLE_VERBOSE_LOGS 0

#include "app.h"

#include "cookies_util.h"

#include <common/src/defer.h>

#include <core/settings.h>

#include "util/debounce_util.h"
#include "util/ref_util.h"


namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static void
on_cookies_changed(WebKitCookieManager* mgr, orion::gtk::Batcher* batcher)
{
    batcher->run();
}

static void
cookie_free_wrapper(gpointer data) {
    soup_cookie_free(static_cast<SoupCookie*>(data));
};

static void
finish_get_all_cookies_cb(GObject* source, GAsyncResult* res, gpointer user_data)
{
    auto* val = static_cast<orion::settings::DeleteCookiesMode*>(user_data);
    defer([val]{ delete val; });

    WebKitCookieManager* mgr = WEBKIT_COOKIE_MANAGER(source);

    
    GError* err = NULL;
    GList* cookies = webkit_cookie_manager_get_all_cookies_finish(mgr, res, &err);
    if(err)
    {
        orion::log_error("%s", err->message);
        g_free(err);
        return;
    }
    defer([cookies]{ g_list_free_full(cookies, cookie_free_wrapper); }); //captures by value, meaning we can write into the cookies variable

    GDateTime* max_expires = NULL;
    {
        GDateTime* now = g_date_time_new_now_utc(); //basing on utc to avoid DST change issues
        defer([now]{ g_date_time_unref(now); });

        switch(*val)
        {
            case orion::settings::DeleteCookiesMode::not_set:
            case orion::settings::DeleteCookiesMode::manually:
                return;
            case orion::settings::DeleteCookiesMode::after_1_day:
                max_expires = g_date_time_add_days(now, 1);
                break;
            case orion::settings::DeleteCookiesMode::after_1_week:
                max_expires = g_date_time_add_days(now, 7);
                break;
            case orion::settings::DeleteCookiesMode::after_2_weeks:
                max_expires = g_date_time_add_days(now, 14);
                break;
            case orion::settings::DeleteCookiesMode::after_1_month:
                max_expires = g_date_time_add_months(now, 1);
                break;
            case orion::settings::DeleteCookiesMode::after_3_months:
                max_expires = g_date_time_add_months(now, 3);
                break;
            case orion::settings::DeleteCookiesMode::after_6_months:
                max_expires = g_date_time_add_months(now, 6);
                break;
            case orion::settings::DeleteCookiesMode::after_1_year:
                max_expires = g_date_time_add_years(now, 1);
                break;
            case orion::settings::DeleteCookiesMode::after_app_exits:
                //a NULL expires value sets the soup cookie as current session bound
                break;
        }
    }
    defer([max_expires]{ if(max_expires) g_date_time_unref(max_expires); });
    

    for(; cookies; cookies = cookies->next)
    {
        SoupCookie* cookie = static_cast<SoupCookie*>(cookies->data);

        GDateTime* cookie_expires = soup_cookie_get_expires(cookie);

        //cookies that were originally set as session only should be ignored by us
        if(cookie_expires)
        {
            if (!max_expires ||
                0 < g_date_time_compare(cookie_expires, max_expires))
            {
                soup_cookie_set_expires(cookie, max_expires);
                webkit_cookie_manager_add_cookie(mgr, cookie, NULL, NULL, NULL);

                if(max_expires)
                {
                    gchar* date = g_date_time_format_iso8601(max_expires);
                    orion::log_verbose("clamping lifetime of cookie for website %s: expires %s", soup_cookie_get_domain(cookie), date);
                    g_free(date);
                }
                else
                {
                    orion::log_verbose("making cookie for website %s bound to current session only", soup_cookie_get_domain(cookie));
                }
            }
            else
            {
                gchar* date = g_date_time_format_iso8601(cookie_expires);
                orion::log_verbose("keeping current cookie expires with domain %s: expires %s", soup_cookie_get_domain(cookie), date);
                g_free(date);
            }
        }
        else
        {
            orion::log_verbose("preserving cookie for website %s as bound to current session only", soup_cookie_get_domain(cookie));
        }
    }
}

void
register_cookie_max_age_clamp(WebKitCookieManager* mgr, orion::ProfileId profile_id)
{
    orion::gtk::WeakRefWrapper mgr_weak(mgr);

    auto batch_cb = [mgr_weak, profile_id](){
        global_core_api->get_setting(profile_id, orion::settings::privacy::delete_cookies_mode,
            [mgr_weak](orion::GetSettingReply<orion::settings::DeleteCookiesMode> reply)
        {
            VERIFY_LOG(reply.err);
            switch(reply.value)
            {
                case orion::settings::DeleteCookiesMode::not_set:
                case orion::settings::DeleteCookiesMode::manually:
                    return;
                case orion::settings::DeleteCookiesMode::after_1_day:
                case orion::settings::DeleteCookiesMode::after_1_week:
                case orion::settings::DeleteCookiesMode::after_2_weeks:
                case orion::settings::DeleteCookiesMode::after_1_month:
                case orion::settings::DeleteCookiesMode::after_3_months:
                case orion::settings::DeleteCookiesMode::after_6_months:
                case orion::settings::DeleteCookiesMode::after_1_year:
                case orion::settings::DeleteCookiesMode::after_app_exits:
                    {
                        auto mgr_strong = mgr_weak.lock();
                        if(!mgr_strong) return;

                        auto* val = new orion::settings::DeleteCookiesMode(reply.value);

                        webkit_cookie_manager_get_all_cookies(
                            WEBKIT_COOKIE_MANAGER(mgr_strong.get()),
                            NULL,
                            finish_get_all_cookies_cb,
                            val
                        );
                    }
                    break;
            }
        });
    };

    //lifetime of batcher is tied to WebKitCookieManager
    auto* batcher = new orion::gtk::Batcher(batch_cb, 100000); //100 ms

    auto delete_batcher = [](gpointer data, GClosure* closure){ delete static_cast<orion::gtk::Batcher*>(data); };

    g_signal_connect_data(mgr, "changed", G_CALLBACK(on_cookies_changed), batcher, delete_batcher, G_CONNECT_DEFAULT);

    auto* observer_id = new orion::ObserverId(global_core_api->settings_change().add_observer(
        [profile_id, batcher](const orion::SettingsChange& change){ //lifetime of this callback needs to be at most equal to batcher (same thread)
            for(auto& updated : change.updated)
            {
                if (updated.profile_id == profile_id &&
                    updated.key == orion::settings::privacy::delete_cookies_mode.key())
                {
                    batcher->run();
                }
            }
        }
    ));

    auto remove_setting_observer = [](gpointer data, GObject* deleted){
        auto* observer_id = static_cast<orion::ObserverId*>(data);
        global_core_api->settings_change().remove_observer(*observer_id);
        delete observer_id;
    };

    g_object_weak_ref(G_OBJECT(mgr), remove_setting_observer, observer_id);
}

#endif

} //namespace orion