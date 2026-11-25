#include <core/settings.h>

#include "entry_settings.h"
#include "app.h"
#include "window/main_window/main_window.h"

#define widget_get_profile_id(widget) main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget))))

void
on_homepage_entry_map(GtkEntry* self, gpointer user_data)
{
    orion::ProfileId profile_id = widget_get_profile_id(self);
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);
    global_core_api->get_setting(profile_id, orion::settings::general::homepage,
    [wrapped_ref](orion::GetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
        GtkEditable* entry = GTK_EDITABLE(wrapped_ref->get());
        if(entry)
        {
            gtk_editable_set_text(entry, reply.value.c_str());
            g_object_unref(entry);
        }
    });
}

void
on_homepage_entry_notify_text(GtkEntry* self, GParamSpec* pspec, gpointer user_data)
{
    orion::ProfileId profile_id = widget_get_profile_id(self);
    std::string val = gtk_editable_get_text(GTK_EDITABLE(self));
    global_core_api->set_setting(profile_id, orion::settings::general::homepage, val,
        [](orion::SetSettingReply<std::string> reply){VERIFY_LOG(reply.err);});
}

void
on_custom_user_agent_entry_map(GtkEntry* self, gpointer user_data)
{
    orion::ProfileId profile_id = widget_get_profile_id(self);
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);
    global_core_api->get_setting(profile_id, orion::settings::privacy::custom_user_agent,
    [wrapped_ref](orion::GetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
        GtkEditable* entry = GTK_EDITABLE(wrapped_ref->get());
        if(entry)
        {
            gtk_editable_set_text(entry, reply.value.c_str());
            g_object_unref(entry);
        }
    });
}

void
on_custom_user_agent_entry_notify_text(GtkEntry* self, GParamSpec* pspec, gpointer user_data)
{
    orion::ProfileId profile_id = widget_get_profile_id(self);
    std::string val = gtk_editable_get_text(GTK_EDITABLE(self));
    global_core_api->set_setting(profile_id, orion::settings::privacy::custom_user_agent, val,
        [](orion::SetSettingReply<std::string> reply){VERIFY_LOG(reply.err);});
}