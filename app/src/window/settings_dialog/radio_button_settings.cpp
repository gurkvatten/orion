#include <core/settings.h>


#include "radio_button_settings.h"
#include "app.h"
#include "window/main_window/main_window.h"

void
on_share_crash_radio_buttons_map(GtkCheckButton* button_a, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    GActionMap* action_map = G_ACTION_MAP(win);
    GAction* action = g_action_map_lookup_action(action_map, "share-crash-reports-setting");
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(action);
    global_core_api->get_setting(main_window_get_current_profile_id(win), orion::settings::privacy::share_crash_reports,
        [wrapped_ref](orion::GetSettingReply<orion::settings::ShareCrashReports> reply){
            VERIFY_LOG(reply.err);
            GAction* action = G_ACTION(wrapped_ref->get());
            if(action)
            {
                GVariant* var = g_variant_new_int32((int32_t)reply.value);
                g_action_change_state(action, var);
                g_object_unref(action);
            }
        });
}

void
on_app_icon_radio_buttons_map(GtkCheckButton* button_a, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    GActionMap* action_map = G_ACTION_MAP(win);
    GAction* action = g_action_map_lookup_action(action_map, "app-icon-setting");
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(action);
    global_core_api->get_setting(main_window_get_current_profile_id(win), orion::settings::appearance::app_icon,
        [wrapped_ref](orion::GetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            GAction* action = G_ACTION(wrapped_ref->get());
            if(action)
            {
                GVariant* var = g_variant_new_string(reply.value.c_str());
                g_action_change_state(action, var);
                g_object_unref(action);
            }
        });
}