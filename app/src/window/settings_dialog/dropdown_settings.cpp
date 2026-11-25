#include <core/settings.h>
#include <core/settings_types.h>
#include <common/src/defer.h>

#include "util/file_util.h"

#include "dropdown_settings.h"
#include "app.h"
#include "window/main_window/main_window.h"
#include "data_item/profiles_columnview_item.h"


static orion::ProfileId
widget_get_profile_id(GtkWidget* widget)
{
    return main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(widget)));
}

#define on_drop_down_notify_selected_define(widget_name, enum_type, setting_type) \
void \
on_##widget_name##_notify_selected(GtkDropDown* self, GParamSpec* pspec, gpointer user_data) \
{ \
    auto val = (enum_type)(gtk_drop_down_get_selected(self)+1); \
    orion::ProfileId profile_id = widget_get_profile_id(GTK_WIDGET(self)); \
    global_core_api->set_setting(profile_id, setting_type, val, [](orion::SetSettingReply<enum_type> reply){ \
        VERIFY_LOG(reply.err); \
    }); \
}

#define on_drop_down_map_define(widget_name, enum_type, setting_type) \
void \
on_##widget_name##_map(GtkDropDown* self, gpointer user_data) \
{ \
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self); \
    orion::ProfileId profile_id = widget_get_profile_id(GTK_WIDGET(self)); \
    global_core_api->get_setting(profile_id, setting_type, [wrapped_ref](orion::GetSettingReply<enum_type> reply) \
    { \
        VERIFY_LOG(reply.err); \
        GtkDropDown* dropdown = GTK_DROP_DOWN(wrapped_ref->get()); \
        if(dropdown) \
        { \
            gtk_drop_down_set_selected(dropdown, (uint)reply.value-1); \
            g_object_unref(dropdown); \
        } \
    }); \
}

#define drop_down_funcs_define(widget_name, enum_type, settings_type) \
    on_drop_down_notify_selected_define(widget_name, enum_type, settings_type) \
    on_drop_down_map_define(widget_name, enum_type, settings_type)

drop_down_funcs_define(orion_opens_drop_down, orion::settings::AppOpensWith, orion::settings::general::app_opens_with)
drop_down_funcs_define(window_opens_drop_down, orion::settings::NewWindowsOpenWith, orion::settings::general::new_windows_open_with)
drop_down_funcs_define(tab_opens_drop_down, orion::settings::NewTabsOpenWith, orion::settings::general::new_tabs_open_with)
drop_down_funcs_define(remove_downloads_drop_down, orion::settings::RemoveDownloadItemsMode, orion::settings::general::remove_download_items_mode)
drop_down_funcs_define(open_external_links_drop_down, orion::settings::OpenExternalLinksIn, orion::settings::general::open_external_links_in)
drop_down_funcs_define(bookmarks_bar_style_drop_down, orion::settings::BookmarksBarStyle, orion::settings::appearance::bookmarks_bar_style)
drop_down_funcs_define(theme_drop_down, orion::settings::Theme, orion::settings::appearance::theme)
drop_down_funcs_define(tab_switch_order_drop_down, orion::settings::TabSwitchOrder, orion::settings::tabs::tab_switch_order)
drop_down_funcs_define(remove_trackers_mode_drop_down, orion::settings::RemoveTrackersFromUrlsMode, orion::settings::privacy::remove_trackers_from_urls_mode)
drop_down_funcs_define(remove_history_mode_drop_down, orion::settings::RemoveHistoryItemsMode, orion::settings::privacy::remove_history_items_mode)
drop_down_funcs_define(delete_cookies_mode_drop_down, orion::settings::DeleteCookiesMode, orion::settings::privacy::delete_cookies_mode)
drop_down_funcs_define(language_drop_down, orion::settings::Language, orion::settings::general::language)
drop_down_funcs_define(search_engine_drop_down, orion::settings::SearchEngine, orion::settings::search::search_engine);
drop_down_funcs_define(websites_default_reader_mode_drop_down, orion::settings::ReaderModeWebsiteSetting, orion::settings::websites::enable_reader_mode_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_autoplay_drop_down, orion::settings::AutoplayWebsiteSetting, orion::settings::websites::autoplay_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_page_zoom_drop_down, orion::settings::PageZoom, orion::settings::websites::page_zoom_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_popup_windows_drop_down, orion::settings::PopupWindowsWebsiteSetting, orion::settings::websites::popup_windows_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_notifications_drop_down, orion::settings::NotificationsWebsiteSetting, orion::settings::websites::notifications_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_downloads_drop_down, orion::settings::DownloadsWebsiteSetting, orion::settings::websites::downloads_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_camera_drop_down, orion::settings::CameraWebsiteSetting, orion::settings::websites::camera_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_microphone_drop_down, orion::settings::MicrophoneWebsiteSetting, orion::settings::websites::microphone_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_screen_sharing_drop_down, orion::settings::ScreenSharingWebsiteSetting, orion::settings::websites::screen_sharing_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_content_blockers_drop_down, orion::settings::ContentBlockersWebsiteSetting, orion::settings::websites::content_blockers_setting_for_all_websites)
drop_down_funcs_define(websites_default_location_drop_down, orion::settings::LocationWebsiteSetting, orion::settings::websites::location_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_tracking_prevention_drop_down, orion::settings::TrackingPreventionWebsiteSetting, orion::settings::websites::tracking_prevention_setting_for_all_websites)
drop_down_funcs_define(websites_default_cookies_drop_down, orion::settings::CookiesWebsiteSetting, orion::settings::websites::cookies_setting_for_all_websites)
drop_down_funcs_define(websites_default_javascript_drop_down, orion::settings::JavaScriptWebsiteSetting, orion::settings::websites::javascript_setting_for_all_websites)
drop_down_funcs_define(websites_default_web_fonts_drop_down, orion::settings::WebFontsWebsiteSetting, orion::settings::websites::web_fonts_setting_for_all_websites)
drop_down_funcs_define(websites_default_ssl_certificate_validation_drop_down, orion::settings::SslCertificateValidationWebsiteSetting, orion::settings::websites::ssl_certificate_validation_setting_for_all_websites)
drop_down_funcs_define(websites_default_user_agent_drop_down, orion::settings::UserAgentWebsiteSetting, orion::settings::websites::user_agent_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_compatibility_mode_drop_down, orion::settings::CompatibilityModeWebsiteSetting, orion::settings::websites::compatibility_mode_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_external_apps_drop_down, orion::settings::ExternalAppsWebsiteSetting, orion::settings::websites::external_apps_default_setting_for_all_websites)
drop_down_funcs_define(websites_default_picture_in_picture_drop_down, orion::settings::PictureInPictureWebsiteSetting, orion::settings::websites::picture_in_picture_default_setting_for_all_websites)

static void
canonical_path_download_location_handler(GtkDropDown* self, std::string_view path)
{
    GtkStringList* list = GTK_STRING_LIST(gtk_drop_down_get_model(self));
    guint selected = gtk_drop_down_get_selected(self);

    g_signal_handlers_block_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)on_file_download_location_drop_down_notify_selected, NULL);
    {
        if(g_list_model_get_n_items(G_LIST_MODEL(list)) < 4)
        {
            gtk_string_list_append(list, path.data());
        }
        else if(0 != g_strcmp0(path.data(), gtk_string_list_get_string(list, 3)))
        {
            const char* strings_added[] = {path.data(), NULL};
            gtk_string_list_splice(list, 3, 1, strings_added);
        }
        
        gtk_drop_down_set_selected(self, selected);
    }
    g_signal_handlers_unblock_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)on_file_download_location_drop_down_notify_selected, NULL);
}

void
on_file_download_location_drop_down_map(GtkDropDown* self, gpointer user_data)
{
    GActionMap* action_map = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(user_data)));
    GAction* file_download_location_action = g_action_map_lookup_action(action_map, "file-download-location");
    GAction* file_download_custom_path_action = g_action_map_lookup_action(action_map, "file-download-custom-path");
    GVariant* file_download_location_state = g_action_get_state(file_download_location_action);
    GVariant* file_download_custom_path_state = g_action_get_state(file_download_custom_path_action);
    size_t file_download_location_string_length = 0;
    const char* file_download_location_string = g_variant_get_string(file_download_location_state, &file_download_location_string_length);
    const char* file_download_custom_path_string = g_variant_get_string(file_download_custom_path_state, NULL);

    int file_download_location_val = strtol(file_download_location_string, nullptr, 10); //the action already does validation for us so we ignore that here
    

    GListModel* model = gtk_drop_down_get_model(self);
    GtkStringList* string_list = GTK_STRING_LIST(model);

    g_signal_handlers_block_by_func(self, (gpointer)on_file_download_location_drop_down_notify_selected, user_data);
    uint n_items = g_list_model_get_n_items(model);
    for(uint i = n_items-1; i>2; i--)
    {
        gtk_string_list_remove(string_list, i);
    }

    
    gtk_string_list_append(string_list, file_download_custom_path_string);
    if(file_download_custom_path_string && *file_download_custom_path_string)
    {
        orion::gtk::WeakRefWrapper drop_down_weak(self);
        orion::get_host_path(file_download_custom_path_string, [drop_down_weak = std::move(drop_down_weak)](std::string_view host_path){
            auto drop_down_strong = drop_down_weak.lock();
            if(drop_down_strong) canonical_path_download_location_handler(GTK_DROP_DOWN(drop_down_strong.get()), host_path);
        });
    }
    if(file_download_location_val<3 && file_download_location_val > 0)
    {
        gtk_drop_down_set_selected(self, file_download_location_val-1);
    }
    else if(file_download_location_val == 3)
    {
        gtk_drop_down_set_selected(self, 3);
    }
    g_signal_handlers_unblock_by_func(self, (gpointer)on_file_download_location_drop_down_notify_selected, user_data);
}

static void
on_download_path_file_dialog_finished(GObject* source, GAsyncResult* res, gpointer user_data)
{
    auto* drop_down_weak_heap = static_cast<orion::gtk::WeakRefWrapper*>(user_data);
    defer([drop_down_weak_heap]{ delete drop_down_weak_heap; });
    auto dropdown_strong = drop_down_weak_heap->lock();
    
    GError* err = NULL;
    GFile* selected_path = gtk_file_dialog_select_folder_finish(GTK_FILE_DIALOG(source), res, &err);
    if(err)
    {
        orion::log_error("%s", err->message);
        defer([err]{ g_error_free(err); });
    }
    else
    {
        defer([selected_path]{ g_object_unref(selected_path); });
        char* path_string = g_file_get_path(selected_path);
        if(!path_string)
        {
            orion::log_error("selected empty path");
            return;
        }
        GtkDropDown* dropdown = GTK_DROP_DOWN(dropdown_strong.get());
        GListModel* model = gtk_drop_down_get_model(dropdown);
        GtkStringList* string_list = GTK_STRING_LIST(model);

        g_signal_handlers_block_matched(dropdown, G_SIGNAL_MATCH_FUNC, 0, 0,
            NULL, (gpointer)on_file_download_location_drop_down_notify_selected, NULL);
        {
            uint n_items = g_list_model_get_n_items(model);
            for(uint i = n_items-1; i>2; i--)
            {
                gtk_string_list_remove(string_list, i);
            }

            gtk_string_list_append(string_list, path_string);
            
            orion::get_host_path(path_string, [drop_down_weak = std::move(*drop_down_weak_heap)](std::string_view host_path){
                auto drop_down_strong = drop_down_weak.lock();
                if(drop_down_strong) canonical_path_download_location_handler(GTK_DROP_DOWN(drop_down_strong.get()), host_path);
            });
            gtk_drop_down_set_selected(dropdown, 3);
        }
        g_signal_handlers_unblock_matched(dropdown, G_SIGNAL_MATCH_FUNC, 0, 0,
            NULL, (gpointer)on_file_download_location_drop_down_notify_selected, NULL);

        //sinking floating ref because gtk_widget_activate_action does not actually consume the variant on failure
        GVariant* var = g_variant_ref_sink(g_variant_new_take_string(path_string));
        defer([var]{ g_variant_unref(var); });
        gtk_widget_activate_action_variant(GTK_WIDGET(dropdown_strong.get()), "win.file-download-custom-path", var);
        
    }
}

void
on_file_download_location_drop_down_notify_selected(GtkDropDown* self, GParamSpec* pspec, gpointer user_data)
{
    GtkWidget* dialog_widget = GTK_WIDGET(user_data);
    uint selected = gtk_drop_down_get_selected(self);
    if(selected < 2)
    {
        std::string var_string = std::format("{}", selected+1);
        //sinking floating ref because gtk_widget_activate_action_variant does not actually consume the variant on failure
        GVariant* var = g_variant_ref_sink(g_variant_new_string(var_string.c_str()));
        defer([var]{ g_variant_unref(var); });
        gtk_widget_activate_action_variant(GTK_WIDGET(dialog_widget), "win.file-download-location", var);
    }
    else if(selected==2)
    {
        //sinking floating ref because gtk_widget_activate_action_variant does not actually consume the variant on failure
        GVariant* var = g_variant_ref_sink(g_variant_new_string("3"));
        defer([var]{ g_variant_unref(var); });
        gtk_widget_activate_action_variant(GTK_WIDGET(dialog_widget), "win.file-download-location", var);

        GActionMap* win = G_ACTION_MAP(gtk_widget_get_root(dialog_widget));
        GAction* custom_path_setting_action = NULL;
        if(win)
        {
            custom_path_setting_action = g_action_map_lookup_action(win, "file-download-custom-path");
        }
        GVariant* custom_path_setting_state = NULL;
        if(custom_path_setting_action)
        {
            custom_path_setting_state = g_action_get_state(custom_path_setting_action);
        }
        const char* base_directory = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
        if(custom_path_setting_state)
        {
            base_directory = g_variant_get_string(custom_path_setting_state, NULL);
        }
        GFile* file = g_file_new_for_path(base_directory);
        defer([file]{ g_object_unref(file); });

        
        GtkFileDialog* dialog = gtk_file_dialog_new();
        defer([dialog]{ g_object_unref(dialog); });
        gtk_file_dialog_set_initial_file(dialog, file);

        auto* drop_down_weak_heap = new orion::gtk::WeakRefWrapper(self);

        gtk_file_dialog_select_folder(dialog, GTK_WINDOW(win), NULL, on_download_path_file_dialog_finished, drop_down_weak_heap);
    }
    else if(selected > 2)
    {
        //sinking floating ref because gtk_widget_activate_action_variant does not actually consume the variant on failure
        GVariant* var = g_variant_ref_sink(g_variant_new_string("3"));
        defer([var]{ g_variant_unref(var); });
        gtk_widget_activate_action_variant(GTK_WIDGET(dialog_widget), "win.file-download-location", var);

        GtkStringObject* path = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(self));
        if(path)
        {
            GVariant* var = g_variant_ref_sink(g_variant_new_string(gtk_string_object_get_string(path)));
            defer([var]{ g_variant_unref(var); });
            gtk_widget_activate_action_variant(GTK_WIDGET(dialog_widget), "win.file-download-location", var);
        }
    }
}

char*
default_profile_drop_down_listitem_get_label(GtkListItem* listitem)
{
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        GString* icon = profiles_columnview_item_get_icon(item);
        GString* name = profiles_columnview_item_get_profile(item);

        return g_strdup_printf("%s %s", icon->str, name->str);
    }
    else
    {
        return NULL;
    }
}

void
on_default_profile_drop_down_notify_selected(GtkDropDown* self, GParamSpec* pspec, gpointer user_data)
{
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(gtk_drop_down_get_selected_item(self));
    global_core_api->set_default_profile(profiles_columnview_item_get_id(item), [](orion::Error err){VERIFY_LOG(err);});
}

static void
on_default_profile_drop_down_unmap(GtkDropDown* self, gpointer user_data)
{
    gtk_drop_down_set_model(self, NULL);
    g_signal_handlers_disconnect_by_func(self, (void*)on_default_profile_drop_down_unmap, user_data);
}

static uint
default_profile_drop_down_profile_created(GListStore* store, const orion::ProfileDto& profile_created)
{
    uint num_windows = 0; // not needed here in Settings dialog
    uint num_tabs = 0;    // not needed here in Settings dialog
    ProfilesColumnviewItem* item = profiles_columnview_item_new_from_dto(profile_created, num_windows, num_tabs);
    uint pos = g_list_store_insert_sorted(store, item, profiles_columnview_item_seq_num_compare, NULL);
    g_object_unref(item);
    return pos;
}

static void
default_profile_drop_down_profile_deleted(GListStore* store, orion::ProfileId profile_deleted)
{
    GListModel* model = G_LIST_MODEL(store);
    uint len = g_list_model_get_n_items(model);
    for(uint i = 0; i<len; i++)
    {
        ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(g_list_model_get_item(model, i));
        defer([item]{ g_object_unref(item); });
        if(profiles_columnview_item_get_id(item)==profile_deleted)
        {
            g_list_store_remove(store, i);
            break;
        }
    }
}

static void
default_profile_drop_down_handle_profile_change(GtkDropDown* self, const orion::ProfilesChange& change)
{
    GListStore* store = G_LIST_STORE(gtk_drop_down_get_model(self));
    if(!store) return;
    uint pos;
    g_signal_handlers_block_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_default_profile_drop_down_notify_selected, NULL);
    for(const orion::ProfileDto& profile_created : change.created)
    {
        pos = default_profile_drop_down_profile_created(store, profile_created);
        if(profile_created.is_default) gtk_drop_down_set_selected(self, pos);
    }
    for(orion::ProfileId profile_deleted : change.deleted)
    {
        default_profile_drop_down_profile_deleted(store, profile_deleted);
    }
    for(const orion::ProfileDto& profile_updated : change.updated)
    {
        default_profile_drop_down_profile_deleted(store, profile_updated.id);
        pos = default_profile_drop_down_profile_created(store, profile_updated);
        if(profile_updated.is_default) gtk_drop_down_set_selected(self, pos);
    }
    g_signal_handlers_unblock_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_default_profile_drop_down_notify_selected, NULL);
}

void
on_default_profile_drop_down_map(GtkDropDown* self, gpointer user_data)
{
    orion::ProfileId profile_id = widget_get_profile_id(GTK_WIDGET(self));
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);
    global_core_api->get_profile_ids([wrapped_ref](orion::GetProfileIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_profiles(reply.ids, [wrapped_ref](orion::GetProfilesReply reply){
            VERIFY_LOG(reply.err);
            GtkDropDown* self = GTK_DROP_DOWN(wrapped_ref->get());
            if(self)
            {
                GListStore* store = g_list_store_new(G_TYPE_OBJECT);
                uint pos = 0;
                for(orion::ProfileDto& profile : reply.profiles)
                {
                    uint num_windows = 0; // not needed here in Settings dialog
                    uint num_tabs = 0;    // not needed here in Settings dialog
                    ProfilesColumnviewItem* item = profiles_columnview_item_new_from_dto(profile, num_windows, num_tabs);
                    if(profile.is_default) pos = g_list_model_get_n_items(G_LIST_MODEL(store));
                    g_list_store_append(store, item);
                    g_object_unref(item);
                }
                gtk_drop_down_set_model(self, G_LIST_MODEL(store));
                gtk_drop_down_set_selected(self, pos);
                g_object_unref(self);
            }
        });
    });
    orion::ObserverId* observer_id = new orion::ObserverId;
    *observer_id = global_core_api->profiles_change().add_observer([wrapped_ref](const orion::ProfilesChange& change){
        GtkDropDown* self = GTK_DROP_DOWN(wrapped_ref->get());
        if(self)
        {
            default_profile_drop_down_handle_profile_change(self, change);
            g_object_unref(self);
        }
    });
    g_signal_connect_data(self, "unmap", G_CALLBACK(on_default_profile_drop_down_unmap), observer_id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->profiles_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);
}
