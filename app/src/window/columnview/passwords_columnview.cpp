#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <common/src/log.h>
#include <common/src/defer.h>

#include "window/bookmarks_dialog.h"
#include "app.h"
#include "data_item/passwords_columnview_item.h"
#include "passwords_columnview.h"
#include "window/main_window/main_window.h"
#include "window/edit_password_dialog.h"
#include "util/menu_util.h"
#include "common/src/ptr_util.h"

const char* SECRET_PLACEHOLDER = "********";

// Test page: https://practicetestautomation.com/practice-test-login/

static void
refresh_selection(GtkSelectionModel* selection)
{
    orion::log_verbose("refresh_selection");
    GListModel* list = gtk_multi_selection_get_model(GTK_MULTI_SELECTION(selection));
    guint n = g_list_model_get_n_items(list);

    for (guint i = 0; i < n; i++) {
        gboolean is_selected = gtk_selection_model_is_selected(selection, i);
        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(g_list_model_get_item(list, i));
        defer([item]{ g_object_unref(item); });

        // password column
        {
            GtkLabel* label = passwords_columnview_item_get_password_label(item);
            const char* text = SECRET_PLACEHOLDER;
            if (is_selected) {
                text = passwords_columnview_item_get_password(item)->str;
            }
            gtk_label_set_label(label, text);
        }

        // verification code column
        {
            GtkWidget* box = passwords_columnview_item_get_verification_code_widget(item);
            GtkWidget* label = gtk_widget_get_first_child(box);
            gtk_widget_set_visible(GTK_WIDGET(label), is_selected);

            GtkWidget* placeholder = gtk_widget_get_next_sibling(label);
            GString* code = passwords_columnview_item_get_verification_code(item);
            gtk_label_set_text(GTK_LABEL(placeholder), code && code->len > 0 ? SECRET_PLACEHOLDER : "");
        }
    }
}

static void
on_row_left_clicked(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    if (n_press == 2) {
        // double click
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
        GtkWidget* bookmarks_dialog = gtk_widget_get_ancestor(widget, BOOKMARKS_DIALOG_TYPE);
        if (!bookmarks_dialog) {
            orion::log_error("on_row_left_clicked: could not find BookmarksDialog ancestor");
            return;
        }

        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(user_data);

        EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(edit_password_dialog_new());
        edit_password_dialog_set_id(dialog,
            passwords_columnview_item_get_id(item)
        );
        edit_password_dialog_set_url(dialog,
            passwords_columnview_item_get_url(item)->str
        );
        edit_password_dialog_set_username(dialog,
            passwords_columnview_item_get_username(item)->str
        );
        edit_password_dialog_set_password(dialog,
            passwords_columnview_item_get_password(item)->str
        );
        edit_password_dialog_set_verification_key(dialog,
            passwords_columnview_item_get_verification_key(item)->str
        );
        edit_password_dialog_set_verification_code(dialog,
            passwords_columnview_item_get_verification_code(item)->str
        );
        adw_dialog_present(ADW_DIALOG(dialog), bookmarks_dialog);
    }
}

static void
on_row_right_clicked(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
    BookmarksDialog* bookmarks_dialog = BOOKMARKS_DIALOG(gtk_widget_get_ancestor(widget, BOOKMARKS_DIALOG_TYPE));
    if (!bookmarks_dialog) {
        orion::log_error("on_row_right_clicked: could not find BookmarksDialog ancestor");
        return;
    }

    graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(widget, GTK_WIDGET(bookmarks_dialog), &zero, &pos)) {
		orion::log_warning("on_row_right_clicked: failed to compute point");
        return;
	}

	// Position of the widget + position of the click
	GdkRectangle rect{
		.x = int(pos.x + x),
		.y = int(pos.y + y),
		.width = 1,
		.height = 1,
	};

    orion::PasswordItemId id = passwords_columnview_item_get_id(item);
    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });

    orion::gtk::menu_append_action_with_string_target(
        menu, _("Copy Website"), "win.copy-website",
        passwords_columnview_item_get_url(item)->str
    );
    orion::gtk::menu_append_action_with_string_target(
        menu, _("Copy Username"), "win.copy-username",
        passwords_columnview_item_get_username(item)->str
    );
    orion::gtk::menu_append_action_with_string_target(
        menu, _("Copy Password"), "win.copy-password",
        passwords_columnview_item_get_password(item)->str
    );
    orion::gtk::menu_append_action_with_int64_target(
        menu, _("Edit"), "win.edit-password", id.value
    );
    
    GtkWidget* popover_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));

    bookmarks_dialog_set_passwords_popover_menu(bookmarks_dialog, GTK_POPOVER_MENU(popover_menu));
    
	gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu), &rect);
	gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
	gtk_popover_popup(GTK_POPOVER(popover_menu));

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
passwords_factory_setup_a(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(object);
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(list_item, label);
}

static void
passwords_factory_bind_a(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(object);
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(list_item));

	PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(gtk_list_item_get_item(list_item));
    std::string website = passwords_columnview_item_get_url(item)->str;

    orion::Uri uri = orion::Uri::parse_strict(website.c_str());
    if (!uri.authority().empty()) {
        website = uri.authority();
    }

    gtk_label_set_text(label, website.c_str());

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(label));
    orion::gtk::remove_click_controller(row_widget);
    
    {
        GtkGesture* gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_PRIMARY);
        g_signal_connect(gesture, "pressed", G_CALLBACK(on_row_left_clicked), item);
        gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(gesture));
    }
    {
        GtkGesture* gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_SECONDARY);
        g_signal_connect(gesture, "pressed", G_CALLBACK(on_row_right_clicked), item);
        gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(gesture));
    }
}

static void
passwords_factory_setup_b(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
passwords_factory_bind_b(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_label_set_text(label, (passwords_columnview_item_get_username(item))->str);
}

static void
passwords_factory_setup_c(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
passwords_factory_bind_c(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(object);
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(list_item));

    gpointer item_ptr = gtk_list_item_get_item(list_item);
    if (!PASSWORDS_COLUMNVIEW_IS_ITEM(item_ptr)) {
        orion::log_error("passwords_factory_bind_c: item is not a PasswordsColumnviewItem");
        return;
    }

    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(item_ptr);
    passwords_columnview_item_set_password_label(item, label);

    gtk_label_set_text(label, SECRET_PLACEHOLDER);
}

static void
passwords_factory_setup_d(GtkSignalListItemFactory* factory, GObject* listitem)
{
    // We need to use 2 separate labels because we want to bind verification code
    // property in bind_d using g_object_bind_property
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    GtkWidget* label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_widget_set_visible(label, FALSE);

    GtkWidget* placeholder = gtk_label_new(SECRET_PLACEHOLDER);
    
    gtk_box_append(GTK_BOX(box), label);
    gtk_box_append(GTK_BOX(box), placeholder);

    g_object_bind_property(
        label, "visible",
        placeholder, "visible",
        GBindingFlags(G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE)
    );

    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), box);
}

static void
passwords_factory_bind_d(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    GtkWidget* box = gtk_list_item_get_child(GTK_LIST_ITEM(listitem));
    GtkLabel* label = GTK_LABEL(gtk_widget_get_first_child(box));
    
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    passwords_columnview_item_set_verification_code_widget(item, box);

    GString* code = passwords_columnview_item_get_verification_code(item);
    if (code && code->len > 0) {
        gtk_label_set_text(label, SECRET_PLACEHOLDER);
    } else {
        gtk_label_set_text(label, "");
    }

    g_object_bind_property(
        item, "verification-code",
        label, "label",
        G_BINDING_SYNC_CREATE
    );
}

static int
passwords_columnview_compare_items(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    PasswordsColumnviewItem* actual_item_a = PASSWORDS_COLUMNVIEW_ITEM(const_cast<gpointer>(item_a));
    PasswordsColumnviewItem* actual_item_b = PASSWORDS_COLUMNVIEW_ITEM(const_cast<gpointer>(item_b));
    double seq_num_a = passwords_columnview_item_get_seq_num(actual_item_a);
    double seq_num_b = passwords_columnview_item_get_seq_num(actual_item_b);
    if (seq_num_a > seq_num_b) {
        return 1;
    } else if (seq_num_a < seq_num_b) {
        return -1;
    }
    return 0;
}

static void
passwords_column_view_item_created(const orion::PasswordItemDto& item_created, GListStore* store)
{
    gpointer item = passwords_columnview_item_new_from_dto(item_created);
    defer([item]{ g_object_unref(item); });
    g_list_store_insert_sorted(store, item, passwords_columnview_compare_items, NULL);
}

static void
passwords_column_view_item_deleted(orion::PasswordItemId item_deleted, GListStore* store)
{
    guint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for (guint i = 0; i < len; i++) {
        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if (passwords_columnview_item_get_id(item) == item_deleted) {
            g_list_store_remove(store, i);
            break;
        }
    }
}

static void
passwords_column_view_change(GtkMultiSelection* selection, const orion::PasswordItemsChange& change, BookmarksDialog* dialog)
{
    GListStore* store = G_LIST_STORE(gtk_multi_selection_get_model(selection));
    orion::gtk::WeakRefWrapper store_weak(store);

    for (const orion::PasswordItemDto& item_created : change.created) {
        passwords_column_view_item_created(item_created, store);
    }

    for (orion::PasswordItemId item_deleted : change.deleted) {
        passwords_column_view_item_deleted(item_deleted, store);
    }

    GtkBitset* selection_bitset = gtk_selection_model_get_selection(GTK_SELECTION_MODEL(selection));
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });

    for (const orion::PasswordItemDto& item_updated : change.updated) {
        passwords_column_view_item_deleted(item_updated.id, store);
        passwords_column_view_item_created(item_updated, store);
    }

    if (gtk_bitset_get_size(selection_bitset)) {
        gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection), gtk_bitset_get_nth(selection_bitset, 0), true);
    }

    bookmarks_dialog_on_passwords_columnview_changed(dialog);
    refresh_selection(GTK_SELECTION_MODEL(selection));
}

static gboolean
on_totp_timer(gpointer user_data)
{
    orion::log_verbose("passwords_columnview: on_totp_timer");

    GtkColumnView* columnview = GTK_COLUMN_VIEW(user_data);
    MainWindow* main_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(
        gtk_widget_get_ancestor(GTK_WIDGET(user_data), BOOKMARKS_DIALOG_TYPE)
    );

    orion::GetPasswordItemIdsQuery query{
        .profile_id = main_window_get_current_profile_id(main_win),
        .search_text = bookmarks_dialog_get_passwords_search_filter(dialog),
    };

    orion::gtk::WeakRefWrapper columnview_weak(columnview);

    global_core_api->get_password_item_ids(query, [columnview_weak](orion::GetPasswordItemIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_password_items(reply.ids, [columnview_weak](orion::GetPasswordItemsReply reply){
            VERIFY_LOG(reply.err);

            for (orion::PasswordItemDto pass_item : reply.items) {
                if (pass_item.verification_code.empty()) {
                    continue;
                }

                auto columnview_strong = columnview_weak.lock();
                if (!columnview_strong) return;
                GtkColumnView* columnview = GTK_COLUMN_VIEW(columnview_strong.get());

                GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(columnview));
                GListStore* store = G_LIST_STORE(gtk_multi_selection_get_model(selection));

                guint position{};
                if (!g_list_store_find_with_equal_func(
                    store,
                    passwords_columnview_item_new_from_dto(pass_item),
                    [](gconstpointer a, gconstpointer b) -> gboolean {
                        PasswordsColumnviewItem* item_a = PASSWORDS_COLUMNVIEW_ITEM(const_cast<gpointer>(a));
                        PasswordsColumnviewItem* item_b = PASSWORDS_COLUMNVIEW_ITEM(const_cast<gpointer>(b));
                        return passwords_columnview_item_get_id(item_a) == passwords_columnview_item_get_id(item_b);
                    },
                    &position
                )) {
                    orion::log_warning("on_totp_timer: failed to find passwword item in store");
                    return;
                }
                PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(
                    g_list_model_get_item(G_LIST_MODEL(store), position)
                );
                defer([item]{ g_object_unref(item); });

                passwords_columnview_item_set_verification_code(item, pass_item.verification_code.c_str());
            }
        });
    });
    return G_SOURCE_CONTINUE; // continue the timer
}

static void
on_selection_changed(GtkSelectionModel* selection, guint position, guint n_items, gpointer user_data)
{
    orion::log_verbose("on_selection_changed: position=%u, n_items=%u", position, n_items);
    refresh_selection(selection);
}

void
passwords_column_view_unmap(GtkWidget* self, gpointer user_data)
{
    orion::log_verbose("passwords_column_view_unmap");

    orion::ObserverId observer_id = static_cast<orion::ObserverId>(orion::void_ptr_to_int64(
        g_object_get_data(G_OBJECT(self), "password-items-change-observer-id")
    ));
    if (observer_id.value != 0) {
        global_core_api->password_items_change().clear_observer(&observer_id);
        g_object_set_data(G_OBJECT(self), "password-items-change-observer-id", orion::int64_to_void_ptr(0));
    }

    gulong handler_id = orion::void_ptr_to_gulong(
        g_object_get_data(G_OBJECT(self), "selection-changed-handler-id")
    );
    if (handler_id != 0) {
        GtkSelectionModel* selection = gtk_column_view_get_model(GTK_COLUMN_VIEW(self));
        g_signal_handler_disconnect(selection, handler_id);
        g_object_set_data(G_OBJECT(self), "selection-changed-handler-id", orion::int64_to_void_ptr(0));
    }

    guint timer_id = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(self), "password-items-change-totp-timer-id")
    );
    if (timer_id != 0) {
        g_source_remove(timer_id);
        g_object_set_data(G_OBJECT(self), "password-items-change-totp-timer-id", GUINT_TO_POINTER(0));
    }
    
    gtk_column_view_set_model(GTK_COLUMN_VIEW(self), NULL);
}

void
passwords_column_view_map(GtkWidget* self, gpointer user_data)
{
    orion::log_verbose("passwords_column_view_map");
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
    MainWindow* main_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_win);

    GListStore* root = g_list_store_new(G_TYPE_OBJECT);
	GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(root));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(self), GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });

    orion::gtk::WeakRefWrapper root_weak(root);
    orion::gtk::WeakRefWrapper selection_weak(selection);

    orion::GetPasswordItemIdsQuery query{
        .profile_id = profile_id,
        .search_text = bookmarks_dialog_get_passwords_search_filter(dialog),
    };

    global_core_api->get_password_item_ids(query, [root_weak,selection_weak](orion::GetPasswordItemIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_password_items(reply.ids, [root_weak,selection_weak](orion::GetPasswordItemsReply reply){
            VERIFY_LOG(reply.err);

            auto root_strong = root_weak.lock();
            if (!root_strong) return;
            GListStore* root = G_LIST_STORE(root_strong.get());

            auto selection_strong = selection_weak.lock();
            if (!selection_strong) return;
            GtkMultiSelection* selection = GTK_MULTI_SELECTION(selection_strong.get());
            
            g_list_store_remove_all(root);

            for (orion::PasswordItemDto pass_item : reply.items) {
                gpointer item = passwords_columnview_item_new_from_dto(pass_item);
                defer([item]{ g_object_unref(item); });
                g_list_store_insert_sorted(root, item, passwords_columnview_compare_items, NULL);
            }

            refresh_selection(GTK_SELECTION_MODEL(selection));
        });
    });

    // register signal handler only once
    gulong handler_id = orion::void_ptr_to_gulong(
        g_object_get_data(G_OBJECT(self), "selection-changed-handler-id")
    );

    if (handler_id == 0) {
        handler_id = g_signal_connect(selection, "selection-changed", G_CALLBACK(on_selection_changed), NULL);

        g_object_set_data(G_OBJECT(self), "selection-changed-handler-id",
            orion::int64_to_void_ptr(static_cast<int64_t>(handler_id))
        );
    }

    // register change observer only once
    orion::ObserverId observer_id = static_cast<orion::ObserverId>(orion::void_ptr_to_int64(
        g_object_get_data(G_OBJECT(self), "password-items-change-observer-id")
    ));

    if (observer_id.value == 0) {
        orion::gtk::WeakRefWrapper selection_weak(selection);
        orion::gtk::WeakRefWrapper dialog_weak(dialog);

        observer_id = global_core_api->password_items_change().add_observer(
            [selection_weak,dialog_weak](const orion::PasswordItemsChange& change){
                auto selection_strong = selection_weak.lock();
                if (!selection_strong) return;
                GtkMultiSelection* selection = GTK_MULTI_SELECTION(selection_strong.get());

                auto dialog_strong = dialog_weak.lock();
                if (!dialog_strong) return;
                BookmarksDialog* dialog = BOOKMARKS_DIALOG(dialog_strong.get());

                passwords_column_view_change(selection, change, dialog);
            });

        g_object_set_data(G_OBJECT(self), "password-items-change-observer-id",
            orion::int64_to_void_ptr(observer_id.value)
        );
    }

    // register TOTP timer only once
    guint timer_id = GPOINTER_TO_UINT(
        g_object_get_data(G_OBJECT(self), "password-items-change-totp-timer-id")
    );

    if (timer_id == 0) {
        timer_id = g_timeout_add_seconds(1, on_totp_timer, self);
        g_object_set_data(G_OBJECT(self), "password-items-change-totp-timer-id", GUINT_TO_POINTER(timer_id));

        // trigger the timer manually because the first timed call will trigger after 1 second
        on_totp_timer(self);
    }
}

void
passwords_column_view_realize(GtkWidget* self, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
	GListStore* root = g_list_store_new(G_TYPE_OBJECT);

	GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(root));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(self), GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });

    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(passwords_factory_setup_a), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(passwords_factory_bind_a), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Website"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(passwords_factory_setup_b), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(passwords_factory_bind_b), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Username"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(passwords_factory_setup_c), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(passwords_factory_bind_c), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Password"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(passwords_factory_setup_d), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(passwords_factory_bind_d), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Verification Code"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }
}