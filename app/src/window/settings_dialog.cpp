#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <glib/gi18n.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <core/settings.h>
#include <common/src/defer.h>
#include <common/src/format_util.h>

#include "app.h"
#include "window/main_window/main_window.h"

#include "settings_dialog.h"

#include "util/settings_util.h"

#include "filter_lists_dialog.h"
#include "manage_search_engines_dialog.h"
#include "manage_website_data_dialog.h"
#include "bookmarks_dialog.h"
#include "reset_orion_dialog.h"
#include "get_orion_plus_dialog.h"
#include "log_in_to_kagi_dialog.h"
#include "import_browser_data_dialog.h"
#include "default_font_dialog.h"
#include "add_website_dialog.h"
#include "customize_theme_popover.h"

#include "window/settings_dialog/dropdown_settings.h"
#include "window/settings_dialog/entry_settings.h"
#include "window/settings_dialog/check_button_settings.h"
#include "window/settings_dialog/radio_button_settings.h"
#include "content_block/embedded_filter_lists.h"
#include "content_block/filter_store.h"

struct _SettingsDialog
{
	AdwDialog parent;

    GtkStack* stack;
    GtkButton* manage_filter_lists_button;
    GtkPopover* password_provider_help_popover;
    GtkListBox* websites_settings_sidebar_list_box;
    GtkStack* websites_default_settings_stack;
    GtkListView* configured_websites_list_view;
    orion::ObserverId websites_settings_observer_id;
    orion::ObserverId filter_lists_observer_id;
    orion::ObserverId filter_lists_sync_observer_id;
    guint configured_websites_selected_index;
    guint content_blocker_progress_tick_id;
    GtkStack* content_blocker_status_stack;
    GtkProgressBar* content_blocker_sync_progress_bar;
    GtkButton* content_blocker_update_now_button;
    GtkLabel* content_blocker_last_updated_label;
    GtkLabel* content_blocker_active_rules_label;
    GtkLabel* content_blocker_active_lists_label;
    bool content_blocker_update_pending;
};

G_DEFINE_TYPE(SettingsDialog, settings_dialog, ADW_TYPE_DIALOG)

namespace {
static void settings_dialog_present_add_website_dialog(SettingsDialog* self, GtkWidget* parent);
}

void
settings_dialog_present_from_last_selected(MainWindow* parent_window)
{
    orion::log_verbose("%s: parent=%p", __func__, parent_window);
    orion::log_verbose(
        "%s: observers before open profile=%zu window=%zu tab=%zu setting=%zu single_setting=%zu extension=%zu",
        __func__,
        global_core_api->profiles_change().observer_count(),
        global_core_api->windows_change().observer_count(),
        global_core_api->tabs_change().observer_count(),
        global_core_api->settings_change().observer_count(),
        global_core_api->single_setting_change().observer_count(),
        global_core_api->extensions_change().observer_count()
    );

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(parent_window);
    global_core_api->get_setting(main_window_get_current_profile_id(parent_window), orion::settings::general::selected_settings_tab_index,
        [wrapped_ref](orion::GetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
            MainWindow* win = MAIN_WINDOW(wrapped_ref->get());
            if(win)
            {
                SettingsDialog* dialog = SETTINGS_DIALOG(g_object_new(SETTINGS_DIALOG_TYPE, NULL));
                GtkSelectionModel* model = gtk_stack_get_pages(dialog->stack);
                defer([model]{ g_object_unref(model); });
                gtk_selection_model_select_item(model, reply.value, false);

                // mark settings dialog as open on the main window so title binding can reflect it
                main_window_set_settings_dialog_open(win, TRUE);
                g_signal_connect_data(dialog, "closed", G_CALLBACK(+[](AdwDialog* d, gpointer user_data){
                    MainWindow* win = MAIN_WINDOW(user_data);
                    orion::log_verbose("settings_dialog closed: dialog=%p parent=%p", d, win);
                    main_window_set_settings_dialog_open(win, FALSE);

                    orion::log_verbose(
                        "settings_dialog closed: observers after close profile=%zu window=%zu tab=%zu setting=%zu single_setting=%zu extension=%zu",
                        global_core_api->profiles_change().observer_count(),
                        global_core_api->windows_change().observer_count(),
                        global_core_api->tabs_change().observer_count(),
                        global_core_api->settings_change().observer_count(),
                        global_core_api->single_setting_change().observer_count(),
                        global_core_api->extensions_change().observer_count()
                    );
                }), g_object_ref(win),
                +[](gpointer user_data, GClosure*) {
                    g_object_unref(user_data);
                },
                G_CONNECT_DEFAULT);
                adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
                g_object_unref(win);
            }
        });
}

void
settings_dialog_present_with_page_selected(MainWindow* parent_window, int page)
{
    orion::log_debug("settings_dialog_present_with_page_selected: parent=%p page=%d", parent_window, page);
    SettingsDialog* dialog = SETTINGS_DIALOG(g_object_new(SETTINGS_DIALOG_TYPE, NULL));
    GtkSelectionModel* model = gtk_stack_get_pages(dialog->stack);
    defer([model]{ g_object_unref(model); });
    gtk_selection_model_select_item(model, page, false);
    
    // mark settings dialog as open on the main window so title binding can reflect it
    main_window_set_settings_dialog_open(parent_window, TRUE);
    g_signal_connect_data(dialog, "closed", G_CALLBACK(+[](AdwDialog* d, gpointer user_data){
        MainWindow* win = MAIN_WINDOW(user_data);
        orion::log_debug("settings_dialog closed: dialog=%p parent=%p", d, win);
        main_window_set_settings_dialog_open(win, FALSE);

        orion::log_debug(
            "settings_dialog closed: observers after close profile=%zu window=%zu tab=%zu setting=%zu single_setting=%zu extension=%zu",
            global_core_api->profiles_change().observer_count(),
            global_core_api->windows_change().observer_count(),
            global_core_api->tabs_change().observer_count(),
            global_core_api->settings_change().observer_count(),
            global_core_api->single_setting_change().observer_count(),
            global_core_api->extensions_change().observer_count()
        );
    }), g_object_ref(parent_window),
    +[](gpointer user_data, GClosure*) {
        g_object_unref(user_data);
    },
    G_CONNECT_DEFAULT);
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(parent_window));
}

void
settings_dialog_present(MainWindow* parent_window, int page)
{
    if (page < 0) {
        settings_dialog_present_from_last_selected(parent_window);
    } else {
        settings_dialog_present_with_page_selected(parent_window, page);
    }
}


void
settings_dialog_select_page(SettingsDialog* self, int page_number)
{
    GtkSelectionModel* model = gtk_stack_get_pages(self->stack);
    defer([model]{ g_object_unref(model); });
    gtk_selection_model_select_item(model, page_number, false);
}

struct FileReadData {
    orion::ProfileId profile_id;
    GtkWidget* parent{};
    std::string file_path;
};

static void
on_sync_import_from_file_file_read(GObject* object, GAsyncResult* result, gpointer user_data)
{
    FileReadData* read_data = static_cast<FileReadData*>(user_data);
    defer([read_data]{ delete read_data; });
    
    orion::ProfileId profile_id = read_data->profile_id;
    std::string file_path = read_data->file_path;

    GError* error = NULL;
    gchar* contents = NULL;
    gsize length = 0;

    gboolean success = g_file_load_contents_finish(
        G_FILE(object),
        result,
        &contents,
        &length,
        NULL, // etag_out
        &error
    );
    defer([contents]{ g_free(contents); });
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_sync_import_from_file_file_read: failed to read file contents: %s (GError code %d)",
            error->message, error->code
        );
        return;
    }
    orion::log_info("on_sync_import_from_file_file_read: read %zu bytes, success: %d", length, success);

    // TODO: implement data import from the read contents
}

static void
on_sync_import_from_file_file_chosen(GObject* object, GAsyncResult* result, gpointer user_data)
{
    if (user_data == NULL) return;
    MainWindow* main_window = MAIN_WINDOW(user_data);

    GError* error = NULL;
	GFile* file = gtk_file_dialog_open_finish(GTK_FILE_DIALOG(object), result, &error);
	if (error) {
        defer([error]{ g_error_free(error); });
		if (error->code == GTK_DIALOG_ERROR_DISMISSED) {
			orion::log_info("on_sync_import_from_file_file_chosen: user dismissed the dialog by clicking Cancel button");
		}
		orion::log_error("on_sync_import_from_file_file_chosen: failed to open file: %s (GError code %d)",
            error->message, error->code
        );
        return;
	}
    defer([file]{ g_object_unref(file); });

    orion::log_info("on_sync_import_from_file_file_chosen: %s", g_file_peek_path(file));

    FileReadData* read_data = new FileReadData{
        .profile_id = main_window_get_current_profile_id(main_window),
        .parent = GTK_WIDGET(main_window),
        .file_path = g_file_peek_path(file),
    };

    g_file_load_contents_async(
        file,
        NULL, // cancellable
        on_sync_import_from_file_file_read,
        read_data // user_data
    );
}

static void
on_sync_export_to_file_finished(GObject* object, GAsyncResult* result, gpointer user_data)
{
    GtkFileDialog* dialog = GTK_FILE_DIALOG(object);
    GError* error = nullptr;
    GFile* file = gtk_file_dialog_save_finish(dialog, result, &error);
    if (error) {
        defer([error]{ g_error_free(error); });
        orion::log_error("on_sync_export_to_file_finished: error exporting to file: %s", error->message);
        return;
    }
    // do not unref file in this scope, it is needed in callbacks

    // TODO: export data to the file and unref the file when done
    g_object_unref(file);
}

static void
on_sync_export_to_file_clicked(GtkButton* button, gpointer user_data)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(button));
    if (!root) {
        orion::log_warning("on_sync_export_to_file_clicked: failed to get root widget from button");
        return;
    }
    MainWindow* main_window = MAIN_WINDOW(root);
    GtkFileDialog* file_dialog = gtk_file_dialog_new();
    defer([file_dialog]{ g_object_unref(file_dialog); });

    gint64 now_us = g_get_real_time();
    gint64 epoch_unix_timestamp = now_us / G_USEC_PER_SEC;
    gchar* initial_name = g_strdup_printf("sync_export_%ld.zip", epoch_unix_timestamp);
    defer([initial_name]{ g_free(initial_name); });
    
    gtk_file_dialog_set_initial_name(file_dialog, initial_name);

    gtk_file_dialog_save(
        file_dialog,
        GTK_WINDOW(main_window),
        NULL, /* cancellable */
        on_sync_export_to_file_finished,
        main_window /* user_data */
    );
}

static void
on_sync_import_from_file_clicked(GtkButton* button, gpointer user_data)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(button));
    if (!root) {
        orion::log_warning("on_sync_import_from_file_clicked: failed to get root widget from button");
        return;
    }
    MainWindow* main_window = MAIN_WINDOW(root);
    GtkFileDialog* file_dialog = gtk_file_dialog_new();

	gtk_file_dialog_open(
        file_dialog,
        GTK_WINDOW(main_window),
        NULL, // cancellable
        on_sync_import_from_file_file_chosen,
        main_window // user_data
    );
}

static gboolean
on_search_learn_more_link_activated(GtkLabel* label, gchar* uri, gpointer user_data)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(label));
    if (!root) {
        orion::log_warning("on_search_learn_more_link_activated: failed to get root widget from label");
        return FALSE;
    }
    MainWindow* main_window = MAIN_WINDOW(root);

    g_action_group_activate_action(
        G_ACTION_GROUP(main_window),
        "open-uri-in-new-tab",
        g_variant_new_string(uri)
    );

    adw_dialog_close(ADW_DIALOG(user_data));
    return TRUE;
}

static void
on_manage_filter_lists_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = filter_lists_dialog_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_manage_search_engines_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = manage_search_engines_dialog_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_manage_website_data_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = manage_website_data_dialog_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_manage_profiles_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 5);
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_manage_passwords_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 3);
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_reset_orion_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = reset_orion_dialog_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_manage_extensions_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(dialog), 4);
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_add_website_clicked(GtkButton* button, gpointer user_data)
{
    settings_dialog_present_add_website_dialog(SETTINGS_DIALOG(user_data), GTK_WIDGET(button));
}

static void
on_customize_theme_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = customize_theme_popover_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_password_provider_help_clicked(GtkButton* button, gpointer user_data)
{
    SettingsDialog* dialog = SETTINGS_DIALOG(user_data);

    if (!dialog->password_provider_help_popover) {
        dialog->password_provider_help_popover = GTK_POPOVER(gtk_popover_new());
        gtk_widget_set_parent(GTK_WIDGET(dialog->password_provider_help_popover), GTK_WIDGET(button));
    }

    GtkLabel* label = GTK_LABEL(gtk_label_new("— Passwords saved in Orion’s keychain can also be used in macOS / iOS Safari.\n\n— Passwords saved in Safari or 3rd party password providers can be accessed, but not updated.\n\nFor the best Password Autofill experience, import passwords into Orion’s keychain."));

    gtk_label_set_wrap(label, TRUE);
    gtk_label_set_max_width_chars(label, 40);
    gtk_label_set_xalign(label, 0);

    gtk_popover_set_child(dialog->password_provider_help_popover, GTK_WIDGET(label));
    gtk_popover_set_has_arrow(dialog->password_provider_help_popover, TRUE);
    gtk_popover_set_position(dialog->password_provider_help_popover, GTK_POS_RIGHT);
    gtk_popover_set_pointing_to(dialog->password_provider_help_popover, NULL);
    gtk_popover_popup(dialog->password_provider_help_popover);
}

static void
on_stack_selection_changed(GtkSelectionModel* selection, guint position, guint n_items, gpointer user_data)
{
    GtkBitset* selection_bitset = gtk_selection_model_get_selection_in_range(selection, position, n_items);
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });
    if(gtk_bitset_get_size(selection_bitset))
    {
        int64_t page_num = gtk_bitset_get_nth(selection_bitset, 0);
        GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(user_data));
        if(MAIN_IS_WINDOW(root))
        {
            global_core_api->set_setting(main_window_get_current_profile_id(MAIN_WINDOW(root)), orion::settings::general::selected_settings_tab_index, page_num,
                [](orion::SetSettingReply<int64_t> reply){VERIFY_LOG(reply.err);});
        }
    }
}

static void
no_kagi_account_on_restore_subscription_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));

    adw_dialog_present(log_in_to_kagi_dialog_new(), GTK_WIDGET(user_data));
}

static void
no_kagi_account_on_subscribe_to_orion_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));

    adw_dialog_present(get_orion_plus_dialog_new(), GTK_WIDGET(user_data));
}

static void
no_kagi_account_on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_restore_purchase_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = adw_dialog_new();
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 16);
    gtk_widget_set_margin_end(box, 16);
    gtk_widget_set_margin_bottom(box, 16);
    gtk_widget_set_margin_start(box, 16);

    GtkWidget* image = gtk_image_new_from_icon_name("orion-logo");
    gtk_image_set_pixel_size(GTK_IMAGE(image), 64);

    GtkWidget* label1 = gtk_label_new(_("No Kagi account is logged in"));
    const char* label1_classes[] = {"bold-text", NULL};
    gtk_widget_set_css_classes(label1, label1_classes);

    GtkWidget* label2 = gtk_label_new(_("You are not currently logged in to Kagi. If you have a Kagi account with an Orion+ log in to restore your purchase. Otherwise click “Subscribe to Orion+”"));
    gtk_label_set_wrap(GTK_LABEL(label2), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(label2), 40);
    gtk_label_set_xalign(GTK_LABEL(label2), 0.5);
    gtk_widget_set_hexpand(label2, TRUE);
    gtk_widget_set_halign(label2, GTK_ALIGN_CENTER);

    GtkWidget* button1 = gtk_button_new_with_label(_("Log in to Restore Subscription"));
    GtkWidget* button2 = gtk_button_new_with_label(_("Subscribe to Orion+"));
    GtkWidget* button3 = gtk_button_new_with_label(_("Cancel"));

    const char* button1_classes[] = {"suggested-action", NULL};
    gtk_widget_set_css_classes(button1, button1_classes);
    gtk_widget_set_margin_top(button1, 8);
    gtk_widget_set_margin_top(button3, 8);

    gtk_box_append(GTK_BOX(box), image);
    gtk_box_append(GTK_BOX(box), label1);
    gtk_box_append(GTK_BOX(box), label2);
    gtk_box_append(GTK_BOX(box), button1);
    gtk_box_append(GTK_BOX(box), button2);
    gtk_box_append(GTK_BOX(box), button3);

    g_signal_connect(button1, "clicked", G_CALLBACK(no_kagi_account_on_restore_subscription_clicked), dialog);
    g_signal_connect(button2, "clicked", G_CALLBACK(no_kagi_account_on_subscribe_to_orion_clicked), dialog);
    g_signal_connect(button3, "clicked", G_CALLBACK(no_kagi_account_on_cancel_clicked), dialog);

    adw_dialog_set_child(dialog, box);
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_import_browser_data_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_present(import_browser_data_dialog_new(), GTK_WIDGET(user_data));
}

static void
on_customize_default_font_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_present(default_font_dialog_new(), GTK_WIDGET(user_data));
}

static gboolean
remove_downloads_drop_down_get_separator_visible(GtkListHeader* header, uint start)
{
    return start>0;
}

namespace {

struct ConfiguredWebsiteSettingInfo {
    std::string key_name;
    bool is_bool;
    bool inverted_bool;
};

struct ConfiguredWebsiteRow {
    std::string website;
    std::string key_name;
    std::string type;
    std::string value;
};

struct ConfiguredWebsiteSettingOption {
    std::string value;
    std::string label;
};

static const ConfiguredWebsiteSettingInfo configured_website_setting_infos[] = {
    {orion::settings::website::specific::enable_reader_mode.key_name(), false, false},
    {orion::settings::website::specific::autoplay_default_setting.key_name(), false, false},
    {orion::settings::website::specific::page_zoom.key_name(), false, false},
    {orion::settings::website::specific::popup_windows_default_setting.key_name(), false, false},
    {orion::settings::website::specific::notifications_default_setting.key_name(), false, false},
    {orion::settings::website::specific::downloads_default_setting.key_name(), false, false},
    {orion::settings::website::specific::camera_default_setting.key_name(), false, false},
    {orion::settings::website::specific::microphone_default_setting.key_name(), false, false},
    {orion::settings::website::specific::screen_sharing_default_setting.key_name(), false, false},
    {orion::settings::website::specific::enable_content_blockers.key_name(), true, false},
    {orion::settings::website::specific::location_default_setting.key_name(), false, false},
    {orion::settings::website::specific::enable_intelligent_tracking_prevention.key_name(), true, false},
    {orion::settings::website::specific::enable_cookies.key_name(), true, false},
    {orion::settings::website::specific::enable_javascript.key_name(), true, false},
    {orion::settings::website::specific::block_web_fonts.key_name(), true, true},
    {orion::settings::website::specific::ssl_certificate_validation_setting.key_name(), false, false},
    {orion::settings::website::specific::user_agent.key_name(), false, false},
    {orion::settings::website::specific::enable_compatibility_mode.key_name(), true, false}, // Compatibility Mode applies multiple settings
    {orion::settings::website::specific::external_apps_default_setting.key_name(), false, false},
    {orion::settings::website::specific::picture_in_picture_default_setting.key_name(), false, false},
    {"", false, false}, // Extension Settings: not stored in settings table
};

static const ConfiguredWebsiteSettingInfo*
configured_website_setting_info_for_index(guint index)
{
    if (index >= std::size(configured_website_setting_infos)) return nullptr;
    return &configured_website_setting_infos[index];
}

static const ConfiguredWebsiteSettingInfo*
configured_website_setting_info_for_key_name(const std::string& key_name)
{
    for (const ConfiguredWebsiteSettingInfo& info : configured_website_setting_infos) {
        if (!info.key_name.empty() && key_name == info.key_name) return &info;
    }
    return nullptr;
}

static std::string
configured_website_row_to_string(const ConfiguredWebsiteRow& row)
{
    return row.website + "\t" + row.key_name + "\t" + row.type + "\t" + row.value;
}

static bool
configured_website_row_from_string(const char* encoded_row, ConfiguredWebsiteRow* row)
{
    if (!encoded_row || !row) return false;

    std::string value(encoded_row);
    size_t first = value.find('\t');
    size_t second = first == std::string::npos ? std::string::npos : value.find('\t', first + 1);
    size_t third = second == std::string::npos ? std::string::npos : value.find('\t', second + 1);
    if (first == std::string::npos || second == std::string::npos || third == std::string::npos) return false;

    row->website = value.substr(0, first);
    row->key_name = value.substr(first + 1, second - first - 1);
    row->type = value.substr(second + 1, third - second - 1);
    row->value = value.substr(third + 1);
    return !row->website.empty() && !row->key_name.empty() && !row->type.empty();
}

static std::string
configured_website_label_from_bool(const ConfiguredWebsiteSettingInfo& info, const std::string& value)
{
    bool bool_value{};
    if (orion::settings::unmarshal_value(value, &bool_value)) return value;
    if (info.inverted_bool) bool_value = !bool_value;
    return bool_value ? _("On") : _("Off");
}

static std::string
configured_website_label_for_ask_deny_allow(int64_t value)
{
    switch (value) {
    case 0: return _("Default");
    case 1: return _("Ask");
    case 2: return _("Don't Allow");
    case 3: return _("Allow");
    default: return std::to_string(value);
    }
}

static std::string
configured_website_label_from_integer(const char* key_name, const std::string& value)
{
    int64_t int_value{};
    if (orion::settings::parse_int64(value, &int_value)) return value;

    if (g_strcmp0(key_name, "autoplay_default_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Allow All Auto-Play");
        case 2: return _("Stop Media with Sound");
        case 3: return _("Never Auto-Play");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "page_zoom") == 0) {
        switch (int_value) {
        case 1: return _("50%");
        case 2: return _("75%");
        case 3: return _("80%");
        case 4: return _("85%");
        case 5: return _("90%");
        case 6: return _("95%");
        case 7: return _("100%");
        case 8: return _("115%");
        case 9: return _("125%");
        case 10: return _("150%");
        case 11: return _("175%");
        case 12: return _("200%");
        case 13: return _("250%");
        case 14: return _("300%");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "popup_windows_default_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Allow");
        case 2: return _("Block");
        case 3: return _("Block and Notify");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "notifications_default_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Ask");
        case 2: return _("Allow");
        case 3: return _("Don't Allow");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "downloads_default_setting") == 0
        || g_strcmp0(key_name, "location_default_setting") == 0
        || g_strcmp0(key_name, "external_apps_default_setting") == 0) {
        return configured_website_label_for_ask_deny_allow(int_value);
    }

    if (g_strcmp0(key_name, "ssl_certificate_validation_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Strict");
        case 2: return _("Ignore");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "user_agent") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Automatic");
        case 2: return _("Custom");
        case 3: return _("Chrome");
        case 4: return _("Firefox");
        case 5: return _("Safari");
        case 6: return _("Safari iPhone");
        case 7: return _("Safari iPad");
        case 8: return _("Edge");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "picture_in_picture_default_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Automatic");
        case 2: return _("Manual");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "enable_reader_mode") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("On");
        case 2: return _("Off");
        default: return std::to_string(int_value);
        }
    }

    if (g_strcmp0(key_name, "camera_default_setting") == 0
        || g_strcmp0(key_name, "microphone_default_setting") == 0
        || g_strcmp0(key_name, "screen_sharing_default_setting") == 0) {
        switch (int_value) {
        case 0: return _("Default");
        case 1: return _("Ask");
        case 2: return _("Deny");
        case 3: return _("Allow");
        default: return std::to_string(int_value);
        }
    }

    return int_value == 0 ? _("Default") : std::to_string(int_value);
}

static std::vector<ConfiguredWebsiteSettingOption>
configured_website_base_setting_options(const std::string& key_name, const std::string& type)
{
    const ConfiguredWebsiteSettingInfo* info = configured_website_setting_info_for_key_name(key_name);
    std::vector<ConfiguredWebsiteSettingOption> options;

    if ((info && info->is_bool) || type == "boolean") {
        bool inverted = info && info->inverted_bool;
        options.push_back({inverted ? "0" : "1", _("On")});
        options.push_back({inverted ? "1" : "0", _("Off")});
    } else if (type == "integer") {
        std::vector<int64_t> values;

        if (key_name == "page_zoom") {
            values = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
        } else if (key_name == "ssl_certificate_validation_setting"
            || key_name == "picture_in_picture_default_setting"
            || key_name == "enable_reader_mode") {
            values = {1, 2};
        } else if (key_name == "user_agent") {
            values = {1, 2, 3, 4, 5, 6, 7, 8};
        } else {
            values = {1, 2, 3};
        }

        for (int64_t value : values) {
            std::string raw_value = std::to_string(value);
            options.push_back({
                raw_value,
                configured_website_label_from_integer(key_name.c_str(), raw_value),
            });
        }
    }

    return options;
}

static std::vector<ConfiguredWebsiteSettingOption>
configured_website_setting_options(const ConfiguredWebsiteRow& row)
{
    std::vector<ConfiguredWebsiteSettingOption> options =
        configured_website_base_setting_options(row.key_name, row.type);

    auto existing = std::find_if(options.begin(), options.end(), [&row](const ConfiguredWebsiteSettingOption& option) {
        return option.value == row.value;
    });
    if (existing == options.end()) {
        const ConfiguredWebsiteSettingInfo fallback_info{row.key_name.c_str(), row.type == "boolean", false};
        options.push_back({
            row.value,
            row.type == "boolean"
                ? configured_website_label_from_bool(fallback_info, row.value)
                : configured_website_label_from_integer(row.key_name.c_str(), row.value),
        });
    }

    return options;
}

static guint
configured_website_setting_selected_option_index(
    const std::vector<ConfiguredWebsiteSettingOption>& options,
    const std::string& value)
{
    for (size_t index = 0; index < options.size(); ++index) {
        if (options[index].value == value) return static_cast<guint>(index);
    }
    return 0;
}

static std::string
configured_website_setting_static_default_value(const std::string& key_name, const std::string& fallback)
{
    for (const auto& setting : orion::settings::website::specific::all()) {
        if (setting->key_name() == key_name) return setting->default_value_as_string();
    }
    return fallback;
}

static std::string
configured_website_setting_static_type(const std::string& key_name)
{
    for (const auto& setting : orion::settings::website::specific::all()) {
        if (setting->key_name() == key_name) return setting->type();
    }
    return "";
}

static bool
settings_dialog_get_current_profile_id(GtkWidget* widget, orion::ProfileId* profile_id)
{
    GtkRoot* root = gtk_widget_get_root(widget);
    if (!MAIN_IS_WINDOW(root)) return false;

    *profile_id = main_window_get_current_profile_id(MAIN_WINDOW(root));
    return true;
}

static void
settings_dialog_refresh_content_blocker_information(SettingsDialog* self)
{
    orion::ProfileId profile_id;
    if (!settings_dialog_get_current_profile_id(GTK_WIDGET(self), &profile_id)) return;

    orion::GetFilterListsQuery query{
        .profile_id = profile_id,
        .ignore_block_web_fonts = true,
    };
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_filter_lists(query, [self_weak](orion::GetFilterListsReply reply) {
        VERIFY_LOG(reply.err);
        if (reply.err) return;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SettingsDialog* self = SETTINGS_DIALOG(self_strong.get());

        int64_t active_rules_count = 0;
        size_t active_lists_count = 0;
        for (const orion::FilterListDto& filter_list : reply.filter_lists) {
            if (!filter_list.is_enabled) continue;

            ++active_lists_count;
            active_rules_count += filter_list.active_rules_count;
        }

        std::string active_rules_text = orion::format::format_integer_with_non_breaking_spaces(active_rules_count);
        gtk_label_set_text(self->content_blocker_active_rules_label, active_rules_text.c_str());
        std::string active_lists_text = std::to_string(active_lists_count);
        gtk_label_set_text(self->content_blocker_active_lists_label, active_lists_text.c_str());

        auto latest = std::max_element(
            reply.filter_lists.begin(),
            reply.filter_lists.end(),
            [](const orion::FilterListDto& lhs, const orion::FilterListDto& rhs) {
                return lhs.updated_at < rhs.updated_at;
            }
        );
        if (latest == reply.filter_lists.end() || latest->updated_at.value <= 0) {
            gtk_label_set_text(self->content_blocker_last_updated_label, _("never"));
            return;
        }

        GDateTime* date_time = g_date_time_new_from_unix_local(latest->updated_at.value);
        if (!date_time) {
            gtk_label_set_text(self->content_blocker_last_updated_label, _("never"));
            return;
        }
        defer([date_time] { g_date_time_unref(date_time); });

        gchar* formatted = g_date_time_format(date_time, "%d.%m.%Y, %H:%M");
        defer([formatted] { g_free(formatted); });
        gtk_label_set_text(self->content_blocker_last_updated_label, formatted);
    });
}

static void
settings_dialog_present_add_website_dialog(SettingsDialog* self, GtkWidget* parent)
{
    const ConfiguredWebsiteSettingInfo* info =
        configured_website_setting_info_for_index(self->configured_websites_selected_index);
    if (!info || info->key_name.empty()) return;

    orion::ProfileId profile_id;
    if (!settings_dialog_get_current_profile_id(parent, &profile_id)) return;

    std::string type = configured_website_setting_static_type(info->key_name);
    if (type.empty()) return;

    std::vector<AddWebsiteDialogSettingOption> options;
    for (const ConfiguredWebsiteSettingOption& option :
        configured_website_base_setting_options(info->key_name, type)) {
        options.push_back({
            .value = option.value,
            .label = option.label,
        });
    }
    if (options.empty()) return;

    AdwDialog* dialog = add_website_dialog_new(profile_id, info->key_name, type, std::move(options));
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

static void
configured_website_set_specific_value(
    GtkWidget* widget,
    const ConfiguredWebsiteRow& row,
    const std::string& value,
    const std::string& default_value)
{
    orion::ProfileId profile_id;
    if (!settings_dialog_get_current_profile_id(widget, &profile_id)) return;

    orion::SetSettingQuery query{
        .profile_id = profile_id,
        .key = global_core_api->get_website_setting_key(row.website, row.key_name),
        .realm = global_core_api->get_website_setting_realm(),
        .key_name = row.key_name,
        .type = row.type,
        .value = value,
        .default_value = default_value,
    };

    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });
}

static void
on_configured_website_value_drop_down_notify_selected(GtkDropDown* dropdown, GParamSpec*, gpointer)
{
    const char* encoded_row = static_cast<const char*>(
        g_object_get_data(G_OBJECT(dropdown), "configured-website-row"));
    auto* options = static_cast<std::vector<ConfiguredWebsiteSettingOption>*>(
        g_object_get_data(G_OBJECT(dropdown), "configured-website-options"));
    if (!encoded_row || !options) return;

    guint selected = gtk_drop_down_get_selected(dropdown);
    if (selected >= options->size()) return;

    ConfiguredWebsiteRow row;
    if (!configured_website_row_from_string(encoded_row, &row)) return;

    configured_website_set_specific_value(GTK_WIDGET(dropdown), row, (*options)[selected].value, "");
}

static void
on_configured_websites_factory_setup(GtkSignalListItemFactory*, GtkListItem* list_item, gpointer)
{
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 5);
    gtk_widget_set_margin_bottom(box, 5);
    gtk_widget_set_halign(box, GTK_ALIGN_FILL);

    GtkWidget* image = gtk_image_new_from_icon_name("orion-logo");
    gtk_widget_set_halign(image, GTK_ALIGN_START);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 25);
    gtk_box_append(GTK_BOX(box), image);

    GtkWidget* website_label = gtk_label_new(nullptr);
    gtk_widget_set_halign(website_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(website_label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(website_label), 0);
    gtk_label_set_ellipsize(GTK_LABEL(website_label), PANGO_ELLIPSIZE_END);
    gtk_box_append(GTK_BOX(box), website_label);

    GtkWidget* value_drop_down = gtk_drop_down_new(nullptr, nullptr);
    gtk_widget_set_halign(value_drop_down, GTK_ALIGN_END);
    gtk_widget_set_size_request(value_drop_down, 180, -1);
    gtk_box_append(GTK_BOX(box), value_drop_down);

    gtk_list_item_set_child(list_item, box);
    g_object_set_data(G_OBJECT(list_item), "configured-website-label", website_label);
    g_object_set_data(G_OBJECT(list_item), "configured-website-drop-down", value_drop_down);
}

static void
on_configured_websites_factory_bind(GtkSignalListItemFactory*, GtkListItem* list_item, gpointer)
{
    GtkWidget* website_label = GTK_WIDGET(g_object_get_data(G_OBJECT(list_item), "configured-website-label"));
    GtkWidget* value_drop_down = GTK_WIDGET(g_object_get_data(G_OBJECT(list_item), "configured-website-drop-down"));
    GtkStringObject* item = GTK_STRING_OBJECT(gtk_list_item_get_item(list_item));
    if (!website_label || !value_drop_down || !item) return;

    ConfiguredWebsiteRow row;
    if (!configured_website_row_from_string(gtk_string_object_get_string(item), &row)) return;

    gtk_label_set_text(GTK_LABEL(website_label), row.website.c_str());

    auto options = configured_website_setting_options(row);
    GtkStringList* option_list = gtk_string_list_new(nullptr);
    for (const ConfiguredWebsiteSettingOption& option : options) {
        gtk_string_list_append(option_list, option.label.c_str());
    }

    gtk_drop_down_set_model(GTK_DROP_DOWN(value_drop_down), G_LIST_MODEL(option_list));
    g_object_unref(option_list);
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(value_drop_down),
        configured_website_setting_selected_option_index(options, row.value)
    );

    g_object_set_data_full(
        G_OBJECT(value_drop_down),
        "configured-website-row",
        g_strdup(gtk_string_object_get_string(item)),
        g_free
    );
    g_object_set_data_full(
        G_OBJECT(value_drop_down),
        "configured-website-options",
        new std::vector<ConfiguredWebsiteSettingOption>(std::move(options)),
        [](gpointer data) {
            delete static_cast<std::vector<ConfiguredWebsiteSettingOption>*>(data);
        }
    );

    gulong handler = g_signal_connect(
        value_drop_down,
        "notify::selected",
        G_CALLBACK(on_configured_website_value_drop_down_notify_selected),
        nullptr
    );
    g_object_set_data(
        G_OBJECT(list_item),
        "configured-website-drop-down-handler",
        reinterpret_cast<gpointer>(handler)
    );
}

static void
on_configured_websites_factory_unbind(GtkSignalListItemFactory*, GtkListItem* list_item, gpointer)
{
    GtkWidget* value_drop_down = GTK_WIDGET(g_object_get_data(G_OBJECT(list_item), "configured-website-drop-down"));
    if (!value_drop_down) return;

    gulong handler = reinterpret_cast<gulong>(
        g_object_get_data(G_OBJECT(list_item), "configured-website-drop-down-handler"));
    if (handler) {
        g_signal_handler_disconnect(value_drop_down, handler);
        g_object_set_data(G_OBJECT(list_item), "configured-website-drop-down-handler", nullptr);
    }

    g_object_set_data_full(G_OBJECT(value_drop_down), "configured-website-row", nullptr, nullptr);
    g_object_set_data_full(G_OBJECT(value_drop_down), "configured-website-options", nullptr, nullptr);
    gtk_drop_down_set_model(GTK_DROP_DOWN(value_drop_down), nullptr);
}

static void
settings_dialog_configure_configured_websites_list_view(SettingsDialog* self)
{
    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    g_signal_connect(factory, "setup", G_CALLBACK(on_configured_websites_factory_setup), nullptr);
    g_signal_connect(factory, "bind", G_CALLBACK(on_configured_websites_factory_bind), nullptr);
    g_signal_connect(factory, "unbind", G_CALLBACK(on_configured_websites_factory_unbind), nullptr);
    gtk_list_view_set_factory(self->configured_websites_list_view, factory);
    g_object_unref(factory);
}

static void
settings_dialog_set_configured_websites_rows(SettingsDialog* self, const std::vector<ConfiguredWebsiteRow>& rows)
{
    GtkStringList* string_list = gtk_string_list_new(nullptr);
    for (const ConfiguredWebsiteRow& row : rows) {
        std::string encoded_row = configured_website_row_to_string(row);
        gtk_string_list_append(string_list, encoded_row.c_str());
    }

    GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(string_list));
    gtk_list_view_set_model(self->configured_websites_list_view, GTK_SELECTION_MODEL(selection));
    g_object_unref(selection);
}

static void
settings_dialog_refresh_configured_websites(SettingsDialog* self)
{
    const ConfiguredWebsiteSettingInfo* info = configured_website_setting_info_for_index(self->configured_websites_selected_index);
    if (!info || info->key_name.empty()) {
        settings_dialog_set_configured_websites_rows(self, {});
        return;
    }

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (!MAIN_IS_WINDOW(root)) {
        settings_dialog_set_configured_websites_rows(self, {});
        return;
    }

    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(root));
    orion::gtk::WeakRefWrapper self_weak(self);
    ConfiguredWebsiteSettingInfo selected_info = *info;

    orion::GetAllValuesForWebsiteSettingQuery query{
        .profile_id = profile_id,
        .key_name = selected_info.key_name
    };
    global_core_api->get_all_values_for_website_setting(query,
    [self_weak, selected_info](orion::GetAllValuesForWebsiteSettingReply reply) {
        VERIFY_LOG(reply.err);
        if (reply.err) return;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SettingsDialog* self = SETTINGS_DIALOG(self_strong.get());

        const ConfiguredWebsiteSettingInfo* current_info =
            configured_website_setting_info_for_index(self->configured_websites_selected_index);
        if (!current_info || current_info->key_name.empty() || current_info->key_name != selected_info.key_name) return;

        std::vector<ConfiguredWebsiteRow> rows;
        for (const orion::RawSettingDto& setting : reply.settings) {
            std::string website;
            std::string key_name;
            if (!orion::parse_website_specific_setting_key(setting.key, &website, &key_name)) continue;
            if (key_name != selected_info.key_name) continue;

            rows.push_back({
                .website = website,
                .key_name = key_name,
                .type = setting.type,
                .value = setting.value,
            });
        }

        std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) {
            return a.website < b.website;
        });

        settings_dialog_set_configured_websites_rows(self, rows);
    });
}

} // namespace

static gboolean
on_content_blocker_progress_tick(GtkWidget* widget, GdkFrameClock*, gpointer)
{
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(widget));
    return G_SOURCE_CONTINUE;
}

static void
settings_dialog_set_filter_lists_syncing(SettingsDialog* self, bool is_syncing)
{
    gtk_stack_set_visible_child_name(
        self->content_blocker_status_stack, is_syncing ? "syncing" : "counts");
    gtk_widget_set_sensitive(
        GTK_WIDGET(self->content_blocker_update_now_button),
        !is_syncing && !self->content_blocker_update_pending);

    if (is_syncing && self->content_blocker_progress_tick_id == 0) {
        self->content_blocker_progress_tick_id = gtk_widget_add_tick_callback(
            GTK_WIDGET(self->content_blocker_sync_progress_bar),
            on_content_blocker_progress_tick,
            nullptr,
            nullptr);
    } else if (!is_syncing && self->content_blocker_progress_tick_id != 0) {
        gtk_widget_remove_tick_callback(
            GTK_WIDGET(self->content_blocker_sync_progress_bar),
            self->content_blocker_progress_tick_id);
        self->content_blocker_progress_tick_id = 0;
        gtk_progress_bar_set_fraction(self->content_blocker_sync_progress_bar, 0.0);
    }
}

static void
on_content_blocker_update_now_clicked(GtkButton*, SettingsDialog* self)
{
    MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!main_window) return;

    WebExtensionWebViewFactory& webview_factory =
        main_window_get_current_webview_factory(main_window);
    if (orion::contentblock::filter_lists_are_syncing(&webview_factory)) return;

    self->content_blocker_update_pending = true;
    gtk_widget_set_sensitive(GTK_WIDGET(self->content_blocker_update_now_button), FALSE);

    orion::GetFilterListsQuery query{
        .profile_id = webview_factory.profile_id(),
    };
    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_filter_lists(query, [self_weak,query](orion::GetFilterListsReply reply) {
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SettingsDialog* self = SETTINGS_DIALOG(self_strong.get());
        self->content_blocker_update_pending = false;

        MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
        if (!main_window) return;
        WebExtensionWebViewFactory& webview_factory =
            main_window_get_current_webview_factory(main_window);
        if (webview_factory.profile_id() != query.profile_id) {
            settings_dialog_set_filter_lists_syncing(
                self,
                orion::contentblock::filter_lists_are_syncing(&webview_factory));
            return;
        }

        if (!reply.err) {
            std::vector<orion::FilterListDto> filter_lists;
            for (const orion::FilterListDto& filter_list : reply.filter_lists) {
                if (!filter_list.is_enabled || filter_list.url.empty()) continue;
                if (filter_list.filter_type != orion::FilterListType::builtin
                    && filter_list.filter_type != orion::FilterListType::user) {
                    continue;
                }
                filter_lists.push_back(filter_list);
            }
            orion::contentblock::setup_filter_lists_from_urls(
                &webview_factory, filter_lists, true);
        }

        settings_dialog_set_filter_lists_syncing(
            self,
            orion::contentblock::filter_lists_are_syncing(&webview_factory));
    });
}

static void
on_configured_websites_list_view_map(GtkWidget* widget, SettingsDialog* self)
{
    settings_dialog_refresh_configured_websites(self);
    if (self->websites_settings_observer_id.value) return;

    orion::gtk::WeakRefWrapper self_weak(self);
    self->websites_settings_observer_id = global_core_api->settings_change().add_observer(
    [self_weak](const orion::SettingsChange& change) {
        bool should_refresh = false;
        for (const orion::SingleSettingAnyChange& setting : change.updated) {
            if (setting.key.compare(0, std::strlen(orion::website_settings_key_prefix), orion::website_settings_key_prefix) == 0) {
                should_refresh = true;
                break;
            }
        }
        if (!should_refresh) return;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        settings_dialog_refresh_configured_websites(SETTINGS_DIALOG(self_strong.get()));
    }).value;
}

static void
on_content_blocker_information_map(GtkWidget*, SettingsDialog* self)
{
    settings_dialog_refresh_content_blocker_information(self);
    MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (main_window) {
        settings_dialog_set_filter_lists_syncing(
            self,
            orion::contentblock::filter_lists_are_syncing(
                &main_window_get_current_webview_factory(main_window)));
    }
    if (self->filter_lists_observer_id.value) return;

    orion::gtk::WeakRefWrapper self_weak(self);
    self->filter_lists_observer_id = global_core_api->filter_lists_change().add_observer(
    [self_weak](const orion::FilterListsChange&) {
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        settings_dialog_refresh_content_blocker_information(
            SETTINGS_DIALOG(self_strong.get()));
    }).value;

    self->filter_lists_sync_observer_id =
        orion::contentblock::filter_lists_sync_change().add_observer(
    [self_weak](const orion::contentblock::FilterListsSyncChange& change) {
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SettingsDialog* self = SETTINGS_DIALOG(self_strong.get());
        MainWindow* main_window = orion::gtk::get_root_main_window(GTK_WIDGET(self));
        if (!main_window) return;

        WebExtensionWebViewFactory& webview_factory =
            main_window_get_current_webview_factory(main_window);
        if (change.profile_id != webview_factory.profile_id()
            || change.private_mode != webview_factory.private_mode()) {
            return;
        }

        settings_dialog_set_filter_lists_syncing(self, change.is_syncing);
        if (!change.is_syncing) {
            settings_dialog_refresh_content_blocker_information(self);
        }
    }).value;
}

static void
on_remove_website_clicked(GtkButton* button, gpointer user_data)
{
    SettingsDialog* self = SETTINGS_DIALOG(user_data);
    GtkSelectionModel* selection = gtk_list_view_get_model(self->configured_websites_list_view);
    if (!selection) return;

    std::vector<ConfiguredWebsiteRow> selected_rows;
    guint count = g_list_model_get_n_items(G_LIST_MODEL(selection));
    for (guint index = 0; index < count; ++index) {
        if (!gtk_selection_model_is_selected(selection, index)) continue;

        gpointer item = g_list_model_get_item(G_LIST_MODEL(selection), index);
        if (!GTK_IS_STRING_OBJECT(item)) {
            if (item) g_object_unref(item);
            continue;
        }

        ConfiguredWebsiteRow row;
        if (configured_website_row_from_string(gtk_string_object_get_string(GTK_STRING_OBJECT(item)), &row)) {
            selected_rows.push_back(std::move(row));
        }
        g_object_unref(item);
    }

    orion::ProfileId profile_id;
    if (!settings_dialog_get_current_profile_id(GTK_WIDGET(button), &profile_id)) return;

    for (const ConfiguredWebsiteRow& row : selected_rows) {
        orion::DeleteSettingForWebsiteQuery query{
            .profile_id = profile_id,
            .website = row.website,
            .key_name = row.key_name,
        };
        global_core_api->delete_setting_for_website(query, [](orion::Error err) {
            VERIFY_LOG(err);
        });
    }
}

static void
on_destroy(SettingsDialog* self, gpointer user_data)
{
    if (self->password_provider_help_popover) {
        gtk_widget_unparent(GTK_WIDGET(self->password_provider_help_popover));
        self->password_provider_help_popover = NULL;
    }

    global_core_api->settings_change().clear_observer(&self->websites_settings_observer_id);
    global_core_api->filter_lists_change().clear_observer(&self->filter_lists_observer_id);
    orion::contentblock::filter_lists_sync_change().clear_observer(
        &self->filter_lists_sync_observer_id);
    if (self->content_blocker_progress_tick_id != 0) {
        gtk_widget_remove_tick_callback(
            GTK_WIDGET(self->content_blocker_sync_progress_bar),
            self->content_blocker_progress_tick_id);
        self->content_blocker_progress_tick_id = 0;
    }
}

static void
on_websites_settings_sidebar_row_selected(GtkListBox* sidebar, GtkListBoxRow* row, SettingsDialog* self)
{
    if (!row || !GTK_IS_STACK(self->websites_default_settings_stack)) return;

    static const char* page_names[] = {
        "reader-mode",
        "autoplay",
        "page-zoom",
        "popup-windows",
        "notifications",
        "downloads",
        "camera",
        "microphone",
        "screen-sharing",
        "content-blockers",
        "location",
        "tracking-prevention",
        "cookies",
        "javascript",
        "web-fonts",
        "ssl-certificate-validation",
        "user-agent",
        "compatibility-mode",
        "external-apps",
        "picture-in-picture",
        "extension-settings",
    };

    int index = gtk_list_box_row_get_index(row);
    if (index < 0 || index >= int(std::size(page_names))) return;

    self->configured_websites_selected_index = index;
    gtk_stack_set_visible_child_name(self->websites_default_settings_stack, page_names[index]);
    settings_dialog_refresh_configured_websites(self);
}

static void
settings_dialog_finalize(GObject* object)
{
    SettingsDialog* self = SETTINGS_DIALOG(object);

    std::destroy_at(&self->filter_lists_observer_id);
    std::destroy_at(&self->filter_lists_sync_observer_id);
    std::destroy_at(&self->websites_settings_observer_id);

    G_OBJECT_CLASS(settings_dialog_parent_class)->finalize(object); // call up
}

static void
settings_dialog_init(SettingsDialog* win)
{
    g_type_ensure(CUSTOMIZE_THEME_POPOVER_TYPE);

    std::construct_at(&win->filter_lists_observer_id);
    std::construct_at(&win->filter_lists_sync_observer_id);
    std::construct_at(&win->websites_settings_observer_id);
    
	gtk_widget_init_template(GTK_WIDGET(win));

    settings_dialog_configure_configured_websites_list_view(win);

    g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(win->manage_filter_lists_button, "clicked", G_CALLBACK(on_manage_filter_lists_button_clicked), win);
    g_signal_connect(win->content_blocker_update_now_button, "clicked", G_CALLBACK(on_content_blocker_update_now_clicked), win);
    g_signal_connect(win->content_blocker_last_updated_label, "map", G_CALLBACK(on_content_blocker_information_map), win);
    g_signal_connect(win->configured_websites_list_view, "map", G_CALLBACK(on_configured_websites_list_view_map), win);

    GtkSelectionModel* selection = gtk_stack_get_pages(win->stack);
    defer([selection]{ g_object_unref(selection); });
    g_signal_connect(selection, "selection-changed", G_CALLBACK(on_stack_selection_changed), win);

    g_signal_connect(
        win->websites_settings_sidebar_list_box,
        "row-selected",
        G_CALLBACK(on_websites_settings_sidebar_row_selected),
        win
    );
    gtk_list_box_select_row(
        win->websites_settings_sidebar_list_box,
        gtk_list_box_get_row_at_index(win->websites_settings_sidebar_list_box, 0)
    );
}

static void
settings_dialog_class_init(SettingsDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/settings_dialog.ui");

    G_OBJECT_CLASS(klass)->finalize = settings_dialog_finalize;

	gtk_widget_class_bind_template_child(widget_class, SettingsDialog, manage_filter_lists_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, stack);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, websites_settings_sidebar_list_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, websites_default_settings_stack);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, configured_websites_list_view);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_status_stack);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_sync_progress_bar);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_update_now_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_last_updated_label);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_active_rules_label);
    gtk_widget_class_bind_template_child(widget_class, SettingsDialog, content_blocker_active_lists_label);


    gtk_widget_class_bind_template_callback(widget_class, on_manage_search_engines_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_manage_website_data_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_manage_profiles_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_manage_passwords_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reset_orion_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_manage_extensions_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_password_provider_help_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_restore_purchase_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_import_browser_data_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_customize_default_font_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_add_website_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_website_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_customize_theme_button_clicked);

    gtk_widget_class_bind_template_callback(widget_class, remove_downloads_drop_down_get_separator_visible);


    gtk_widget_class_bind_template_callback(widget_class, on_file_download_location_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_file_download_location_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_orion_opens_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_orion_opens_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_window_opens_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_window_opens_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_opens_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_opens_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_downloads_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_downloads_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_open_external_links_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_open_external_links_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_bookmarks_bar_style_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_bookmarks_bar_style_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_theme_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_theme_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_switch_order_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_switch_order_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_trackers_mode_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_trackers_mode_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_history_mode_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_history_mode_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_delete_cookies_mode_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_delete_cookies_mode_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, default_profile_drop_down_listitem_get_label);
    gtk_widget_class_bind_template_callback(widget_class, on_default_profile_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_default_profile_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_language_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_language_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_search_engine_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_search_engine_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_reader_mode_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_reader_mode_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_autoplay_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_autoplay_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_page_zoom_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_page_zoom_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_popup_windows_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_popup_windows_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_notifications_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_notifications_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_downloads_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_downloads_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_camera_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_camera_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_microphone_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_microphone_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_screen_sharing_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_screen_sharing_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_content_blockers_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_content_blockers_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_location_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_location_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_tracking_prevention_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_tracking_prevention_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_cookies_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_cookies_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_javascript_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_javascript_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_web_fonts_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_web_fonts_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_ssl_certificate_validation_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_ssl_certificate_validation_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_user_agent_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_user_agent_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_compatibility_mode_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_compatibility_mode_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_external_apps_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_external_apps_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_picture_in_picture_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_websites_default_picture_in_picture_drop_down_notify_selected);



    gtk_widget_class_bind_template_callback(widget_class, on_homepage_entry_notify_text);
    gtk_widget_class_bind_template_callback(widget_class, on_homepage_entry_map);
    gtk_widget_class_bind_template_callback(widget_class, on_custom_user_agent_entry_notify_text);
    gtk_widget_class_bind_template_callback(widget_class, on_custom_user_agent_entry_map);



    gtk_widget_class_bind_template_callback(widget_class, on_open_safe_after_download_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_open_safe_after_download_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_automatically_keep_up_to_date_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_automatically_keep_up_to_date_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_warning_before_quit_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_warning_before_quit_check_button_toggled);

    gtk_widget_class_bind_template_callback(widget_class, on_show_website_color_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_website_color_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_always_show_toolbar_in_fs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_always_show_toolbar_in_fs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_bookmarks_bar_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_bookmarks_bar_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_profile_in_address_bar_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_profile_in_address_bar_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_display_rss_feeds_indicator_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_display_rss_feeds_indicator_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_display_web_app_indicator_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_display_web_app_indicator_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_show_sidebar_in_fs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_show_sidebar_in_fs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_prefer_fast_rendering_page_updates_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_prefer_fast_rendering_page_updates_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_disable_transparency_for_fs_windows_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_disable_transparency_for_fs_windows_check_button_toggled);

    gtk_widget_class_bind_template_callback(widget_class, on_use_mini_toolbar_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_use_mini_toolbar_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_tab_preview_on_hover_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_tab_preview_on_hover_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_close_window_with_last_tab_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_close_window_with_last_tab_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_always_show_website_titles_in_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_always_show_website_titles_in_tabs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_favicons_in_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_favicons_in_tabs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_open_new_tab_next_to_current_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_open_new_tab_next_to_current_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_make_newly_opened_link_tab_active_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_make_newly_opened_link_tab_active_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_return_to_last_active_tab_when_closing_current_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_return_to_last_active_tab_when_closing_current_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_use_ctrl_z_to_undo_close_tab_check_button_map_on_default_profile);
    gtk_widget_class_bind_template_callback(widget_class, on_use_ctrl_z_to_undo_close_tab_check_button_toggled_on_default_profile);
    gtk_widget_class_bind_template_callback(widget_class, on_use_ctrl_num_to_switch_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_use_ctrl_num_to_switch_tabs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_nested_vertical_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_nested_vertical_tabs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_use_tab_switcher_ui_for_ctrl_tab_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_use_tab_switcher_ui_for_ctrl_tab_check_button_toggled);

    gtk_widget_class_bind_template_callback(widget_class, on_automatic_https_upgrade_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_automatic_https_upgrade_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_full_website_address_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_full_website_address_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_open_bookmarks_in_new_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_open_bookmarks_in_new_tabs_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_switch_saved_windows_in_place_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_switch_saved_windows_in_place_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_open_external_links_in_preview_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_open_external_links_in_preview_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_show_standard_window_buttons_in_focus_mode_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_show_standard_window_buttons_in_focus_mode_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_confirm_closing_tab_with_pip_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_confirm_closing_tab_with_pip_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_check_spelling_while_typing_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_check_spelling_while_typing_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_correct_spelling_automatically_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_correct_spelling_automatically_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_minimum_font_size_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_minimum_font_size_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_press_tab_to_highlight_each_item_on_web_page_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_press_tab_to_highlight_each_item_on_web_page_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_prevent_esc_from_exiting_fs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_prevent_esc_from_exiting_fs_check_button_toggled);

    gtk_widget_class_bind_template_callback(widget_class, on_enable_cross_device_sync_of_tabs_bookmarks_reading_list_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_cross_device_sync_of_tabs_bookmarks_reading_list_check_button_toggled);
    
    gtk_widget_class_bind_template_callback(widget_class, on_offer_to_autofill_from_orion_keychain_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_offer_to_autofill_from_orion_keychain_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_offer_to_save_to_orion_keychain_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_offer_to_save_to_orion_keychain_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_submit_form_automatically_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_submit_form_automatically_check_button_toggled);
    
    gtk_widget_class_bind_template_callback(widget_class, on_disable_history_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_disable_history_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_update_content_blockers_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_update_content_blockers_check_button_toggled);
    
    gtk_widget_class_bind_template_callback(widget_class, on_also_use_search_engine_in_private_browsing_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_also_use_search_engine_in_private_browsing_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_top_hits_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_top_hits_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_search_engines_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_search_engines_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_ask_fastgpt_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_ask_fastgpt_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_history_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_history_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_bookmarks_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_bookmarks_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_open_tabs_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_autocomplete_open_tabs_check_button_toggled);

    gtk_widget_class_bind_template_callback(widget_class, on_enable_json_formatting_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_json_formatting_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_allow_installation_of_3rd_party_chrome_extensions_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_allow_installation_of_3rd_party_chrome_extensions_check_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_allow_installation_of_3rd_party_firefox_extensions_check_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_allow_installation_of_3rd_party_firefox_extensions_check_button_toggled);

    //NOTE: unlike all others, grouped check buttons (radio buttons essentially) are controlled using actions
    //only initialization is done in map signals
    gtk_widget_class_bind_template_callback(widget_class, on_share_crash_radio_buttons_map);
    gtk_widget_class_bind_template_callback(widget_class, on_app_icon_radio_buttons_map);

    gtk_widget_class_bind_template_callback(widget_class, on_sync_export_to_file_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_sync_import_from_file_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_search_learn_more_link_activated);
}
