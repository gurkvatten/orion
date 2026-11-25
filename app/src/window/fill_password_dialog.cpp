#include <gdk/gdk.h>

#include "app.h"
#include "fill_password_dialog.h"
#include "window/main_window/main_window.h"
#include "data_item/passwords_columnview_item.h"
#include "common/src/ptr_util.h"

#include <common/src/defer.h>

struct _FillPasswordDialog
{
    AdwDialog parent;

    GtkEntry* search_entry;
    GtkColumnView* column_view;
    GtkButton* fill_button;
    GtkSingleSelection* selection;
    FillPasswordDialogResultCb result_cb;
    gpointer cb_user_data;
};

G_DEFINE_TYPE(FillPasswordDialog, fill_password_dialog, ADW_TYPE_DIALOG)

AdwDialog*
fill_password_dialog_new(FillPasswordDialogResultCb cb, gpointer user_data)
{
    FillPasswordDialog* self = APP_FILL_PASSWORD_DIALOG(g_object_new(FILL_PASSWORD_DIALOG_TYPE, NULL));
    self->result_cb = cb;
    self->cb_user_data = user_data;
    return ADW_DIALOG(self);
}

static void
on_fill_button_clicked(GtkButton* button, gpointer user_data)
{
    FillPasswordDialog* dialog = APP_FILL_PASSWORD_DIALOG(user_data);
    if (dialog->result_cb) {
        PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(gtk_single_selection_get_selected_item(dialog->selection));
        if (!item) {
            orion::log_warning("on_fill_button_clicked: no item selected");
            return;
        }

        const char* username_str = passwords_columnview_item_get_username(item)->str;
        const char* password_str = passwords_columnview_item_get_password(item)->str;

        std::string username = username_str ? username_str : "";
        std::string password = password_str ? password_str : "";

        if (!dialog->result_cb(username, password, dialog->cb_user_data)) {
            // failed to fill, do not close the dialog
            return;
        }
    }
    adw_dialog_close(ADW_DIALOG(dialog));
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
factory_setup_a(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(object);
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(list_item, label);
}

static void
factory_bind_a(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
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
}


static void
factory_setup_b(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(GTK_LIST_ITEM(object), label);
}

static void
factory_bind_b(GtkSignalListItemFactory* factory, GObject* object, gpointer user_data)
{
	GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(object)));
    
    PasswordsColumnviewItem* item = PASSWORDS_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(object)));
	gtk_label_set_text(label, (passwords_columnview_item_get_username(item))->str);
}

static int
column_view_compare_items(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
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

static gboolean
transform_selected_to_sensitive(GBinding* binding, const GValue* from_value, GValue* to_value, gpointer user_data)
{
    guint position = g_value_get_uint(from_value);
    g_value_set_boolean(to_value, position != GTK_INVALID_LIST_POSITION);
    return TRUE;
}

static void
reload_items(FillPasswordDialog* dialog)
{
    MainWindow* main_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_win);

    const char* search_str = gtk_editable_get_text(GTK_EDITABLE(dialog->search_entry));

    GListStore* list = G_LIST_STORE(gtk_single_selection_get_model(dialog->selection));
    orion::gtk::WeakRefWrapper list_weak(list);

    orion::GetPasswordItemIdsQuery query{
        .profile_id = profile_id,
        .search_text = search_str ? search_str : "",
    };

    global_core_api->get_password_item_ids(query, [list_weak](orion::GetPasswordItemIdsReply reply){
        VERIFY_LOG(reply.err);

        global_core_api->get_password_items(reply.ids, [list_weak](orion::GetPasswordItemsReply reply){
            VERIFY_LOG(reply.err);

            auto list_strong = list_weak.lock();
            if (!list_strong) return;
            GListStore* list = G_LIST_STORE(list_strong.get());

            g_list_store_remove_all(list);

            for (orion::PasswordItemDto pass_item : reply.items) {
                gpointer item = passwords_columnview_item_new_from_dto(pass_item);
                defer([item]{ g_object_unref(item); });
                g_list_store_insert_sorted(list, item, column_view_compare_items, NULL);
            }
        });
    });
}

static void
on_search_entry_changed(GtkEditable* editable, gpointer user_data)
{
    FillPasswordDialog* dialog = APP_FILL_PASSWORD_DIALOG(user_data);

    const char* text = gtk_editable_get_text(editable);
    orion::log_verbose("on_search_entry_changed: text: %s", text);
    reload_items(dialog);
}

static void
column_view_unrealize(GtkWidget* self, gpointer user_data)
{
    FillPasswordDialog* dialog = APP_FILL_PASSWORD_DIALOG(user_data);
    g_object_unref(dialog->selection);
    dialog->selection = NULL;
}

static void
column_view_realize(GtkWidget* self, gpointer user_data)
{
    FillPasswordDialog* dialog = APP_FILL_PASSWORD_DIALOG(user_data);
    GListStore* list = g_list_store_new(G_TYPE_OBJECT);

    // will be unreffed in unrealize
    dialog->selection = gtk_single_selection_new(G_LIST_MODEL(list));

    gtk_single_selection_set_autoselect(GTK_SINGLE_SELECTION(dialog->selection), FALSE);

    gtk_column_view_set_model(GTK_COLUMN_VIEW(self), GTK_SELECTION_MODEL(dialog->selection));

    reload_items(dialog);

    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(factory_setup_a), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(factory_bind_a), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Website"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(factory_setup_b), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(factory_bind_b), dialog);
        GtkColumnViewColumn* column = gtk_column_view_column_new(_("Username"), factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(GTK_COLUMN_VIEW(self), column);
    }

    g_object_bind_property_full(
        dialog->selection, "selected",
        dialog->fill_button, "sensitive",
        G_BINDING_SYNC_CREATE,
        transform_selected_to_sensitive,
        NULL, NULL, NULL
    );
}

static void
fill_password_dialog_init(FillPasswordDialog* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(self->column_view, "realize", G_CALLBACK(column_view_realize), self);
    g_signal_connect(self->column_view, "unrealize", G_CALLBACK(column_view_unrealize), self);
    g_signal_connect(self->search_entry, "changed", G_CALLBACK(on_search_entry_changed), self);
}

static void
fill_password_dialog_class_init(FillPasswordDialogClass* klass)
{
    gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/fill_password_dialog.ui");

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FillPasswordDialog, column_view);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FillPasswordDialog, search_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), FillPasswordDialog, fill_button);

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_fill_button_clicked);
}