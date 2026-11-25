#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include <common/src/log.h>
#include <common/src/defer.h>

#include "bookmarks_dialog.h"
#include "app.h"
#include "util/action_util.h"
#include "util/click_util.h"
#include "util/dialog_util.h"
#include "util/text_util.h"
#include "util/menu_util.h"
#include "window/columnview/bookmarks_columnview.h"
#include "window/columnview/history_columnview.h"
#include "window/columnview/reading_list_columnview.h"
#include "window/columnview/profiles_columnview.h"
#include "window/columnview/passwords_columnview.h"
#include "window/columnview/extensions_listview.h"
#include "window/profile_dialog.h"
#include "window/add_password_dialog.h"
#include "window/extensions_keyboard_shortcuts_dialog.h"
#include "window/profiles_icon_selection_box.h"
#include "window/main_window/main_window.h"
#include "data_item/profiles_columnview_item.h"
#include "data_item/passwords_columnview_item.h"
#include "window/url_image/url_image.h"

#define BOOKMARKS_ROOT_LIST_ITEM (bookmarks_root_list_item_get_type())

struct _BookmarksDialog
{
	AdwDialog parent;

	GtkListBox* sidebar_list_box;

	GtkBox* history;
	GtkBox* bookmarks;
	GtkBox* reading_list;
	GtkBox* passwords;
	GtkBox* extensions;
	GtkBox* profiles;

	GtkStack* content_stack;

	GtkStackPage* history_page;
	GtkStackPage* bookmarks_page;
	GtkStackPage* reading_list_page;
	GtkStackPage* passwords_page;
	GtkStackPage* extensions_page;
	GtkStackPage* profiles_page;

	GtkColumnView* history_column_view;
	GtkColumnView* bookmarks_column_view;
	GtkColumnView* reading_column_view;
	GtkColumnView* passwords_column_view;
	GtkListView* extensions_list_view;
	GtkColumnView* profiles_column_view;

    GtkPopoverMenu* bookmarks_internal_folder_popover_menu;
    GtkPopoverMenu* bookmarks_folder_popover_menu;
    GtkPopoverMenu* bookmarks_bookmark_popover_menu;
    GtkPopoverMenu* bookmarks_column_view_popover_menu;
    GtkPopoverMenu* history_folder_popover_menu;
    GtkPopoverMenu* history_address_popover_menu;
    GtkPopoverMenu* reading_list_folder_popover_menu;
    GtkPopoverMenu* reading_list_address_popover_menu;
    GtkPopoverMenu* passwords_popover_menu;
    GtkPopoverMenu* profiles_popover_menu;
    GtkPopoverMenu* internal_profile_popover_menu;

    GtkPasswordEntry* login_password_entry;
    GtkEntry* bookmarks_search_entry;
    GtkEntry* history_search_entry;
    GtkLabel* history_items_label;

    std::optional<orion::BookmarkFolderId>* selected_bookmark_folder;
    orion::ObserverId history_entries_change_observer_id;

    // std::set would be more suitable but it would not be embeddable in this struct
    // due to "‘offsetof’ within non-standard-layout type" compile warning (error)
    std::vector<orion::ObserverId> history_entries_observer_ids;

    GtkButton* remove_password_button;
    GtkEntry* passwords_search_entry;
};

G_DEFINE_TYPE(BookmarksDialog, bookmarks_dialog, ADW_TYPE_DIALOG)

static void
on_profiles_context_menu_icon_selection_box_map(ProfilesIconSelectionBox* self, gpointer user_data);

AdwDialog*
bookmarks_dialog_new()
{
	return ADW_DIALOG(g_object_new(BOOKMARKS_DIALOG_TYPE, NULL));
}

void
bookmarks_dialog_sidebar_select(BookmarksDialog* win, int i) //used to initialize dialog on correct page since it is accessible through multiple sources
{
	gtk_list_box_select_row(win->sidebar_list_box, gtk_list_box_get_row_at_index(win->sidebar_list_box, i));
}

void
bookmarks_dialog_set_selected_bookmark_folder(BookmarksDialog* self, std::optional<orion::BookmarkFolderId> id)
{
    *(self->selected_bookmark_folder) = id;
}

std::optional<orion::BookmarkFolderId>
bookmarks_dialog_get_selected_bookmark_folder(BookmarksDialog* self)
{
    return *(self->selected_bookmark_folder);
}

std::string
bookmarks_dialog_get_history_search_filter(BookmarksDialog* self)
{
    const char* text = gtk_editable_get_text(GTK_EDITABLE(self->history_search_entry));
    return std::string(text ? text : "");
}

std::string
bookmarks_dialog_get_passwords_search_filter(BookmarksDialog* self)
{
    const char* text = gtk_editable_get_text(GTK_EDITABLE(self->passwords_search_entry));
    return std::string(text ? text : "");
}

static void
on_new_profile_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_present(profile_dialog_new_create(), GTK_WIDGET(user_data));
}

static void
on_uninstall_confirmation(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
    orion::log_debug("on_uninstall_confirmation: %s", response);
	adw_dialog_close(ADW_DIALOG(self));
}

static void
on_uninstall_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);
    GtkWidget* win = GTK_WIDGET(user_data);

	adw_alert_dialog_format_body_markup(
		ADW_ALERT_DIALOG(dialog),
        // TODO: fill the name dynamically
		_("<b>This extension will be unistalled immediately. You can’t undo this action.</b>\n\nAre you sure you want to uninstall ‘1Password – Password Manager (example)’")
    );

	adw_alert_dialog_add_responses(
		ADW_ALERT_DIALOG(dialog),
		"cancel", _("_Cancel"),
		"uninstall", _("_Uninstall"),
		NULL
    );

	adw_alert_dialog_set_response_appearance(
		ADW_ALERT_DIALOG(dialog),
		"uninstall",
		ADW_RESPONSE_DESTRUCTIVE
    );

	adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "uninstall");
	adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

	g_signal_connect(dialog, "response", G_CALLBACK(on_uninstall_confirmation), win);
	adw_dialog_present(dialog, GTK_WIDGET(win));
}

static void
on_add_password_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_present(add_password_dialog_new(), GTK_WIDGET(user_data));
}

static void
delete_selected_passwords_dialog_on_response(AdwAlertDialog* dialog, gchar* response, gpointer user_data)
{
    orion::log_debug("delete_selected_passwords_dialog_on_response: %s", response);
    if (g_strcmp0(response, "remove") != 0) {
        orion::log_debug("delete_selected_passwords_dialog_on_response: user cancelled");
	    return;
    }

    BookmarksDialog* bookmarks_dialog = BOOKMARKS_DIALOG(user_data);

    GtkSelectionModel* selection = gtk_column_view_get_model(bookmarks_dialog->passwords_column_view);
    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    guint64 n_selected = gtk_bitset_get_size(bitset);

    GtkMultiSelection* multi_selection = GTK_MULTI_SELECTION(selection);
    GListModel* list_model = gtk_multi_selection_get_model(multi_selection);
    std::vector<orion::PasswordItemId> ids_to_delete;

    for (guint64 i = 0; i < n_selected; i++) {
        gpointer list_item = g_list_model_get_item(list_model, gtk_bitset_get_nth(bitset, static_cast<guint>(i)));
        defer([list_item]{ g_object_unref(list_item); });

        if (!PASSWORDS_COLUMNVIEW_IS_ITEM(list_item)) {
            orion::log_warning("delete_selected_passwords_dialog_on_response: selected item is not a PasswordsColumnviewItem");
            continue;
        }

        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(list_item);
        ids_to_delete.push_back(passwords_columnview_item_get_id(item));
    }

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->delete_password_items(ids_to_delete, [dialog_weak](orion::Error err){
        VERIFY_LOG(err);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        AdwDialog* dialog = ADW_DIALOG(dialog_strong.get());
        
        adw_dialog_close(ADW_DIALOG(dialog));
    });
}

static void
on_remove_password_clicked(GtkButton* button, gpointer user_data)
{
    BookmarksDialog* bookmarks_dialog = BOOKMARKS_DIALOG(user_data);
    
    GtkSelectionModel* selection = gtk_column_view_get_model(bookmarks_dialog->passwords_column_view);
    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    guint64 n_selected = gtk_bitset_get_size(bitset);
    orion::log_debug("on_remove_password_clicked: %lu items selected", n_selected);

    if (n_selected == 0) {
        orion::log_warning("on_remove_password_clicked: no items selected");
        return;
    }

    AdwDialog* dialog = adw_alert_dialog_new(NULL, NULL);

    if (n_selected == 1) {
        GtkMultiSelection* multi_selection = GTK_MULTI_SELECTION(selection);
        GListModel* list_model = gtk_multi_selection_get_model(multi_selection);
        gpointer list_item = g_list_model_get_item(list_model, gtk_bitset_get_nth(bitset, 0));
        defer([list_item]{ g_object_unref(list_item); });

        if (!PASSWORDS_COLUMNVIEW_IS_ITEM(list_item)) {
            orion::log_warning("on_remove_password_clicked: selected item is not a PasswordsColumnviewItem");
            return;
        }

        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(list_item);
        const char* username = passwords_columnview_item_get_username(item)->str;
        const char* website = passwords_columnview_item_get_url(item)->str;
     
        adw_alert_dialog_format_heading(ADW_ALERT_DIALOG(dialog),
            _("Are you sure you want to remove the password for “%s” on “%s”?"),
            username, website
        );
        adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
            _("This password will be removed from all your devices.")
        );
    } else {
        adw_alert_dialog_format_heading(ADW_ALERT_DIALOG(dialog),
            _("Are you sure you want to remove these passwords?")
        );
        adw_alert_dialog_set_body(ADW_ALERT_DIALOG(dialog),
            _("These passwords will be removed from all your devices.")
        );
    }

    adw_alert_dialog_add_responses(
        ADW_ALERT_DIALOG(dialog),
        "cancel", _("_Cancel"),
        "remove", _("_Remove"),
        NULL
    );

    adw_alert_dialog_set_response_appearance(
        ADW_ALERT_DIALOG(dialog),
        "remove",
        ADW_RESPONSE_DESTRUCTIVE
    );

    adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(dialog), "cancel");
    adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(dialog), "cancel");

    g_signal_connect(dialog, "response", G_CALLBACK(delete_selected_passwords_dialog_on_response), bookmarks_dialog);

    adw_dialog_present(dialog, GTK_WIDGET(bookmarks_dialog));
}

static void
on_passwords_export_clicked(GtkButton* button, gpointer user_data)
{
    GtkWindow* parent = orion::gtk::get_parent_window(GTK_WIDGET(button));
    GActionMap* map = G_ACTION_MAP(parent);
    g_action_activate(g_action_map_lookup_action(map, "export-passwords"), NULL);
}

static void
on_login_password_entry_activate(GtkPasswordEntry* entry, gpointer user_data)
{
    gtk_stack_set_visible_child_name(GTK_STACK(user_data), "passwords-page");
}

GtkPopoverMenu*
bookmarks_dialog_get_bookmarks_internal_folder_popover_menu(BookmarksDialog* self)
{
    return self->bookmarks_internal_folder_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_bookmarks_folder_popover_menu(BookmarksDialog* self)
{
    return self->bookmarks_folder_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_bookmarks_bookmark_popover_menu(BookmarksDialog* self)
{
    return self->bookmarks_bookmark_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_bookmarks_column_view_popover_menu(BookmarksDialog* self)
{
    return self->bookmarks_column_view_popover_menu;
}



GtkColumnView*
bookmarks_dialog_get_bookmarks_columnview(BookmarksDialog* self)
{
    return self->bookmarks_column_view;
}

GtkColumnView*
bookmarks_dialog_get_history_columnview(BookmarksDialog* self)
{
    return self->history_column_view;
}

GtkColumnView*
bookmarks_dialog_get_reading_list_columnview(BookmarksDialog* self)
{
    return self->reading_column_view;
}

GtkPopoverMenu*
bookmarks_dialog_get_history_folder_popover_menu(BookmarksDialog* self)
{
    return self->history_folder_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_reading_list_folder_popover_menu(BookmarksDialog* self)
{
    return self->reading_list_folder_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_reading_list_address_popover_menu(BookmarksDialog* self)
{
    return self->reading_list_address_popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_passwords_popover_menu(BookmarksDialog* self)
{
    return self->passwords_popover_menu;
}

void
bookmarks_dialog_set_passwords_popover_menu(BookmarksDialog* self, GtkPopoverMenu* menu)
{
    if (self->passwords_popover_menu) {
        gtk_widget_unparent(GTK_WIDGET(self->passwords_popover_menu));
    }
    self->passwords_popover_menu = menu;
    gtk_widget_set_parent(GTK_WIDGET(menu), GTK_WIDGET(self));
}

GtkPopoverMenu*
bookmarks_dialog_get_profiles_popover_menu(BookmarksDialog* self)
{
    GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/bookmarks_dialog_profiles_popover_menu.ui");
    defer([builder]{ g_object_unref(builder); });

    GtkPopoverMenu* popover_menu{};

    orion::gtk::setup_context_popover_menu(builder, GTK_WIDGET(self), &popover_menu, "bookmarks_dialog_profiles_popover_menu");
    GtkWidget* profile_icon_selection_widget = GTK_WIDGET(profiles_icon_selection_box_new());
    g_signal_connect(profile_icon_selection_widget, "map", G_CALLBACK(on_profiles_context_menu_icon_selection_box_map), NULL);
    gtk_popover_menu_add_child(popover_menu, profile_icon_selection_widget,"profile_icon_selection");
    return popover_menu;
}

GtkPopoverMenu*
bookmarks_dialog_get_internal_profile_popover_menu(BookmarksDialog* self)
{
    GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/bookmarks_dialog_profiles_popover_menu.ui");
    defer([builder]{ g_object_unref(builder); });

    GtkPopoverMenu* popover_menu{};

    orion::gtk::setup_context_popover_menu(builder, GTK_WIDGET(self), &popover_menu, "bookmarks_dialog_internal_profile_popover_menu");
    GtkWidget* profile_icon_selection_widget = GTK_WIDGET(profiles_icon_selection_box_new());
    g_signal_connect(profile_icon_selection_widget, "map", G_CALLBACK(on_profiles_context_menu_icon_selection_box_map), NULL);
    gtk_popover_menu_add_child(popover_menu, profile_icon_selection_widget,"profile_icon_selection");
    return popover_menu;
}

static void
on_items_label_unmap(GtkLabel* label, gpointer user_data)
{
    g_signal_handlers_disconnect_by_data(label, user_data);
}

static void
on_history_items_label_map(GtkLabel* label, gpointer user_data)
{
    orion::log_verbose("on_history_items_label_map");

    if (!BOOKMARKS_IS_DIALOG(user_data)) {
        orion::log_warning("on_history_items_label_map: user_data is not a BookmarksDialog");
        return;
    }

    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
    std::string search_filter = gtk_editable_get_text(GTK_EDITABLE(dialog->history_search_entry));

    if(!g_object_get_data(G_OBJECT(label), "n-items"))
    {
        uint* n_items = g_new(uint, 1);
        *n_items = 8;
        g_object_set_data_full(G_OBJECT(label), "n-items", n_items, [](gpointer data){g_free(data);});
    }
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(label);

    orion::GetNonemptyHistoryDaysQuery query{
        .profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(label)))),
        .search_filter = search_filter,
    };

    global_core_api->get_nonempty_history_days(query,
        [wrapped_ref](orion::GetNonemptyHistoryDaysReply reply){
            VERIFY_LOG(reply.err);
            GtkLabel* label = GTK_LABEL(wrapped_ref->get());
            if(label)
            {

                uint n = 0;
                for(orion::HistoryEntryDay day : reply.days)
                {
                    n += day.visit_count;
                }
                char* text = orion::text::n_items_get_text(n);
                gtk_label_set_text(label, text);
                g_free(text);

                uint* n_items = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
                *n_items = n;
    

                g_object_unref(label);
            }
        });

    if (!dialog->history_entries_change_observer_id.value) {
        dialog->history_entries_change_observer_id = global_core_api->history_entries_change().add_observer(
            [label](const orion::HistoryEntriesChange& change){
                uint* n = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
                *n+=change.created.size();
                *n-=change.deleted.size();
                char* text = orion::text::n_items_get_text(*n);
                gtk_label_set_text(label, text);
                g_free(text);
            });

        g_signal_connect_data(label, "unmap", G_CALLBACK(on_items_label_unmap), dialog,
            [](gpointer data, GClosure* closure){
                BookmarksDialog* dialog = BOOKMARKS_DIALOG(data);
                global_core_api->history_entries_change().clear_observer(&dialog->history_entries_change_observer_id);
            }, G_CONNECT_DEFAULT);
    }
}

static void
on_bookmarks_items_label_map(GtkLabel* label, gpointer user_data)
{
    if(!g_object_get_data(G_OBJECT(label), "n-items"))
    {
        uint* n_items = g_new(uint, 1);
        *n_items = 8;
        g_object_set_data_full(G_OBJECT(label), "n-items", n_items, [](gpointer data){g_free(data);});
    }
    //TODO: filter handling
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(label);

    orion::GetBookmarkIdsQuery query{};
    query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(label))));
    global_core_api->get_bookmark_ids(query,
        [wrapped_ref](orion::GetBookmarkIdsReply reply){
            VERIFY_LOG(reply.err);

            GtkLabel* label = GTK_LABEL(wrapped_ref->get());
            if(label)
            {
                char* text = orion::text::n_items_get_text(reply.ids.size());
                gtk_label_set_text(label, text);
                g_free(text);
                uint* n_items = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
                *n_items = reply.ids.size();
            }
        });

    orion::ObserverId* id = new orion::ObserverId;
    *id = global_core_api->bookmarks_change().add_observer([label](const orion::BookmarksChange& change){
        uint* n = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
        *n+=change.created.size();
        *n-=change.deleted.size();
        char* text = orion::text::n_items_get_text(*n);
        gtk_label_set_text(label, text);
        g_free(text);
    });

    g_signal_connect_data(label, "unmap", G_CALLBACK(on_items_label_unmap), id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->bookmarks_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);
}

static void
on_reading_list_items_label_map(GtkLabel* label, gpointer user_data)
{
    if(!g_object_get_data(G_OBJECT(label), "n-items"))
    {
        uint* n_items = g_new(uint, 1);
        *n_items = 8;
        g_object_set_data_full(G_OBJECT(label), "n-items", n_items, [](gpointer data){g_free(data);});
    }
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(label);

    orion::GetReadingListEntryIdsQuery query;
    query.added_after = orion::UnixTimeUtc64(std::numeric_limits<int64_t>::min());
    query.added_before = orion::UnixTimeUtc64Max;
    query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(label))));
    global_core_api->get_reading_list_entry_ids(query,
        [wrapped_ref](orion::GetReadingListEntryIdsReply reply){
            VERIFY_LOG(reply.err);

            GtkLabel* label = GTK_LABEL(wrapped_ref->get());
            if(label)
            {
                char* text = orion::text::n_items_get_text(reply.ids.size());
                gtk_label_set_text(label, text);
                g_free(text);
                uint* n_items = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
                *n_items = reply.ids.size();
            }
        });

    orion::ObserverId* id = new orion::ObserverId;
    *id = global_core_api->reading_list_entries_change().add_observer([label](const orion::ReadingListEntriesChange& change){
        uint* n = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
        *n+=change.created.size();
        *n-=change.deleted.size();
        char* text = orion::text::n_items_get_text(*n);
        gtk_label_set_text(label, text);
        g_free(text);
    });

    g_signal_connect_data(label, "unmap", G_CALLBACK(on_items_label_unmap), id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->reading_list_entries_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);
}


static void
on_profiles_items_label_map(GtkLabel* label, gpointer user_data)
{
    if(!g_object_get_data(G_OBJECT(label), "n-items"))
    {
        uint* n_items = g_new(uint, 1);
        *n_items = 8;
        g_object_set_data_full(G_OBJECT(label), "n-items", n_items, [](gpointer data){g_free(data);});
    }
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(label);
    
    global_core_api->get_profile_ids(
        [wrapped_ref](orion::GetProfileIdsReply reply){
            VERIFY_LOG(reply.err);

            GtkLabel* label = GTK_LABEL(wrapped_ref->get());
            if(label)
            {
                char* text = orion::text::n_items_get_text(reply.ids.size());
                gtk_label_set_text(label, text);
                g_free(text);
                uint* n_items = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
                *n_items = reply.ids.size();
            }
        });

    orion::ObserverId* id = new orion::ObserverId;
    *id = global_core_api->profiles_change().add_observer([label](const orion::ProfilesChange& change){
        uint* n = (uint*)g_object_get_data(G_OBJECT(label), "n-items");
        *n+=change.created.size();
        *n-=change.deleted.size();
        char* text = orion::text::n_items_get_text(*n);
        gtk_label_set_text(label, text);
        g_free(text);
    });

    g_signal_connect_data(label, "unmap", G_CALLBACK(on_items_label_unmap), id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->profiles_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);
}

static void
on_profiles_context_menu_icon_selection_box_map(ProfilesIconSelectionBox* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(main_window_get_context_menu_data(win)->user_data);
    profiles_icon_selection_bind_to_profile_id(self, profiles_columnview_item_get_id(item));
}

static void
setup_context_menus(BookmarksDialog* self)
{
    {
        // TODO: move these into /src/window/bookmarks_dialog_context_menus.ui in the future
        GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_window/sidebar/context_menus.ui");
        defer([builder]{ g_object_unref(builder); });

        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_internal_folder_popover_menu, "bookmarks_dialog_bookmark_internal_folder_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_folder_popover_menu, "bookmarks_dialog_bookmark_folder_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_bookmark_popover_menu, "bookmarks_dialog_bookmark_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_column_view_popover_menu, "bookmarks_dialog_bookmarks_column_view_context_menu");

        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->history_folder_popover_menu, "history_folder_context_menu");
        
        // the same context menus as in History
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->reading_list_folder_popover_menu, "reading_list_folder_context_menu");
        orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->reading_list_address_popover_menu, "reading_list_address_context_menu");
    }
}

void
bookmarks_dialog_add_history_entries_observer_id(BookmarksDialog* self, orion::ObserverId id)
{
    self->history_entries_observer_ids.push_back(id);
}

void
bookmarks_dialog_remove_history_entries_observer_id(BookmarksDialog* self, orion::ObserverId id)
{
    auto it = std::find(self->history_entries_observer_ids.begin(), self->history_entries_observer_ids.end(), id);
    if (it == self->history_entries_observer_ids.end()) {
        orion::log_warning("bookmarks_dialog_remove_history_entries_observer_id: id not found");
        return;
    }
    self->history_entries_observer_ids.erase(it);
}

void
bookmarks_dialog_clear_history_entries_observer_ids(BookmarksDialog* self)
{
    self->history_entries_observer_ids.clear();
}

std::vector<orion::ObserverId>
bookmarks_dialog_get_history_entries_observer_ids(BookmarksDialog* self)
{
    return std::vector<orion::ObserverId>(
        self->history_entries_observer_ids.begin(),
        self->history_entries_observer_ids.end()
    );
}

static void
change_tab(GtkListBox* sidebar, GtkListBoxRow* row, GtkStack* content_stack)
{
	if(!GTK_IS_STACK(content_stack)) return; //prevents some errors caused by this getting called during window close
	if (row == gtk_list_box_get_row_at_index(sidebar, 0))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "history-page")));
	}
	else if (row == gtk_list_box_get_row_at_index(sidebar, 1))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "bookmarks-page")));
	}
	else if (row == gtk_list_box_get_row_at_index(sidebar, 2))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "reading-list-page")));
	}
	else if (row == gtk_list_box_get_row_at_index(sidebar, 3))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "passwords-page")));
	}
	else if (row == gtk_list_box_get_row_at_index(sidebar, 4))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "extensions-page")));
	}
	else if (row == gtk_list_box_get_row_at_index(sidebar, 5))
	{
		gtk_stack_set_visible_child(content_stack, GTK_WIDGET(gtk_stack_get_child_by_name(content_stack, "profiles-page")));
	}
}

static void
on_bookmarks_search_entry_changed(GtkEditable* editable, gpointer user_data)
{
    BookmarksDialog* self = BOOKMARKS_DIALOG(user_data);

    const char* text = gtk_editable_get_text(editable);
    std::string text_str(text);
    orion::log_verbose("on_bookmarks_search_entry_changed: text: %s", text);
    if (text_str.length() > 0)
    {
        bookmarks_column_view_map_flat_query(self->bookmarks_column_view, self, text_str);
    }else
    {
        bookmarks_column_view_map(self->bookmarks_column_view, self);
    }
}

static void
on_history_search_entry_changed(GtkEditable* editable, gpointer user_data)
{
    BookmarksDialog* self = BOOKMARKS_DIALOG(user_data);

    const char* text = gtk_editable_get_text(editable);
    orion::log_verbose("on_history_search_entry_changed: text: %s", text);
    history_column_view_map(self->history_column_view, self);
    on_history_items_label_map(self->history_items_label, self);
}

static void
on_passwords_search_entry_changed(GtkEditable* editable, gpointer user_data)
{
    BookmarksDialog* self = BOOKMARKS_DIALOG(user_data);

    const char* text = gtk_editable_get_text(editable);
    orion::log_verbose("on_passwords_search_entry_changed: text: %s", text);
    passwords_column_view_map(GTK_WIDGET(self->passwords_column_view), self);
}

static void
update_remove_password_button_state(BookmarksDialog* dialog, GtkSelectionModel* selection)
{
    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    guint64 n_selected = gtk_bitset_get_size(bitset);
    gboolean is_sensitive = (n_selected > 0);

    gtk_widget_set_sensitive(GTK_WIDGET(dialog->remove_password_button), is_sensitive);
}

static void
passwords_columnview_selection_changed(GtkSelectionModel* selection, guint position, guint n_items, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
    update_remove_password_button_state(dialog, selection);
}

void
bookmarks_dialog_on_passwords_columnview_changed(BookmarksDialog* self)
{
    update_remove_password_button_state(
        self,
        gtk_column_view_get_model(self->passwords_column_view)
    );
}

static void
bookmarks_dialog_passwords_column_view_map(GtkColumnView* column_view, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);

    GtkSelectionModel* passwords_selection = gtk_column_view_get_model(dialog->passwords_column_view);
    g_signal_connect(passwords_selection, "selection-changed",
        G_CALLBACK(passwords_columnview_selection_changed), dialog
    );

    update_remove_password_button_state(dialog, passwords_selection);
}

static void
bookmarks_dialog_passwords_column_view_unmap(GtkColumnView* column_view, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);

    GtkSelectionModel* passwords_selection = gtk_column_view_get_model(dialog->passwords_column_view);
    if (passwords_selection) {
        g_signal_handlers_disconnect_by_func(passwords_selection,
            (gpointer)passwords_columnview_selection_changed, dialog
        );
    }
}

static void
bookmarks_dialog_finalize(GObject* gobject)
{
    BookmarksDialog* self = BOOKMARKS_DIALOG(gobject);

    delete self->selected_bookmark_folder;
    self->selected_bookmark_folder = NULL;

    G_OBJECT_CLASS(bookmarks_dialog_parent_class)->finalize(gobject);

    std::destroy_at(&self->history_entries_change_observer_id);
    std::destroy_at(&self->history_entries_observer_ids);
}

static void
bookmarks_dialog_dispose(GObject* gobject)
{
    BookmarksDialog* self = BOOKMARKS_DIALOG(gobject);
    
    GtkPopoverMenu* menus[] = {
        self->bookmarks_internal_folder_popover_menu,
        self->bookmarks_folder_popover_menu,
        self->bookmarks_bookmark_popover_menu,
        self->bookmarks_column_view_popover_menu,
        self->history_folder_popover_menu,
        self->history_address_popover_menu,
        self->reading_list_folder_popover_menu,
        self->reading_list_address_popover_menu,
        self->passwords_popover_menu,
        self->profiles_popover_menu,
        self->internal_profile_popover_menu,
    };

    for (GtkPopoverMenu* m : menus) {
        if (m) {
            gtk_widget_unparent(GTK_WIDGET(m));
        }
    }

    G_OBJECT_CLASS(bookmarks_dialog_parent_class)->dispose(G_OBJECT(self));
}

static void
bookmarks_dialog_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(bookmarks_dialog_parent_class)->root(widget);
    [[maybe_unused]] MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
    BookmarksDialog* self = BOOKMARKS_DIALOG(widget);

    // this needs to be able to get to the MainWindow by calling gtk_widget_get_root
    // so it has to be done here, not in constructor or init
    setup_context_menus(self);
}

static void
bookmarks_dialog_init(BookmarksDialog* self)
{
    g_type_ensure(APP_TYPE_URL_IMAGE_WIDGET);
    std::construct_at(&self->history_entries_change_observer_id);
    std::construct_at(&self->history_entries_observer_ids);

	gtk_widget_init_template(GTK_WIDGET(self));
	//orion::gtk::bookmarks_dialog::register_actions(win); //TODO: find out if this is completely unnecessary

	g_signal_connect_object(self->sidebar_list_box, "row-selected", G_CALLBACK(change_tab), self->content_stack, G_CONNECT_DEFAULT);
	g_signal_connect(self->bookmarks_column_view, "map", G_CALLBACK(bookmarks_column_view_map), self);
	bookmarks_column_view_init(self->bookmarks_column_view, self);

	g_signal_connect(self->history_column_view, "realize", G_CALLBACK(history_column_view_realize), self);
	g_signal_connect(self->history_column_view, "map", G_CALLBACK(history_column_view_map), self);
    g_signal_connect(self->history_column_view, "unmap", G_CALLBACK(history_column_view_unmap), self);

	g_signal_connect(self->reading_column_view, "realize", G_CALLBACK(reading_list_column_view_realize), self);
	g_signal_connect(self->reading_column_view, "map", G_CALLBACK(reading_list_column_view_map), self);
	g_signal_connect(self->profiles_column_view, "realize", G_CALLBACK(profiles_column_view_realize), self);
	g_signal_connect(self->profiles_column_view, "map", G_CALLBACK(profiles_column_view_map), self);

	g_signal_connect(self->passwords_column_view, "realize", G_CALLBACK(passwords_column_view_realize), self);
    g_signal_connect(self->passwords_column_view, "map", G_CALLBACK(passwords_column_view_map), self);
    g_signal_connect(self->passwords_column_view, "unmap", G_CALLBACK(passwords_column_view_unmap), self);

	g_signal_connect(self->login_password_entry, "activate", G_CALLBACK(on_login_password_entry_activate), self->content_stack);

    g_signal_connect(self->bookmarks_search_entry, "changed", G_CALLBACK(on_bookmarks_search_entry_changed), self);
    g_signal_connect(self->history_search_entry, "changed", G_CALLBACK(on_history_search_entry_changed), self);
    g_signal_connect(self->passwords_search_entry, "changed", G_CALLBACK(on_passwords_search_entry_changed), self);

    // needed for selection change handling
    g_signal_connect(self->passwords_column_view, "map", G_CALLBACK(bookmarks_dialog_passwords_column_view_map), self);
    g_signal_connect(self->passwords_column_view, "unmap", G_CALLBACK(bookmarks_dialog_passwords_column_view_unmap), self);
}

static GObject*
bookmarks_dialog_constructor(GType type, guint n_properties, GObjectConstructParam* construct_properties)
{
    GObject* obj = G_OBJECT_CLASS(bookmarks_dialog_parent_class)->constructor(type, n_properties, construct_properties);
    reinterpret_cast<BookmarksDialog*>(obj)->selected_bookmark_folder = new std::optional<orion::BookmarkFolderId>;
    return obj;
}

static void
bookmarks_dialog_class_init(BookmarksDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = bookmarks_dialog_dispose;
    G_OBJECT_CLASS(klass)->finalize = bookmarks_dialog_finalize;
    G_OBJECT_CLASS(klass)->constructor = bookmarks_dialog_constructor;
    GTK_WIDGET_CLASS(klass)->root = bookmarks_dialog_root;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/bookmarks_dialog.ui");
	
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, sidebar_list_box);

	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, history);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, bookmarks);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, reading_list);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, passwords);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, extensions);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, profiles);

	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, content_stack);

	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, history_page);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, bookmarks_page);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, reading_list_page);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, passwords_page);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, extensions_page);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, profiles_page);

	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, history_column_view);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, bookmarks_column_view);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, reading_column_view);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, passwords_column_view);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, extensions_list_view);
	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, profiles_column_view);

	gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, login_password_entry);

    gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, bookmarks_search_entry);
    gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, history_search_entry);
    gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, passwords_search_entry);
    gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, history_items_label);

    gtk_widget_class_bind_template_child(widget_class, BookmarksDialog, remove_password_button);

    gtk_widget_class_bind_template_callback(widget_class, on_new_profile_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_uninstall_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_add_password_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_remove_password_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_passwords_export_clicked);

    gtk_widget_class_bind_template_callback(widget_class, on_history_items_label_map);
    gtk_widget_class_bind_template_callback(widget_class, on_bookmarks_items_label_map);
    gtk_widget_class_bind_template_callback(widget_class, on_reading_list_items_label_map);
    gtk_widget_class_bind_template_callback(widget_class, on_profiles_items_label_map);

    gtk_widget_class_bind_template_callback(widget_class, on_bookmarks_column_view_activate);

    gtk_widget_class_bind_template_callback(widget_class, get_extension_title_label);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_source_label);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_icon_ref);
    gtk_widget_class_bind_template_callback(widget_class, get_extension_keyboard_shortcuts_button_visible);
    gtk_widget_class_bind_template_callback(widget_class, on_extension_keyboard_shortcuts_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_extension_uninstall_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_extension_enable_switch_map);
    gtk_widget_class_bind_template_callback(widget_class, on_extension_enable_switch_state_set);
    gtk_widget_class_bind_template_callback(widget_class, on_extension_menu_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_extensions_list_view_map);
}
