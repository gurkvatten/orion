#include <gtk/gtk.h>
#include <iostream>
#include <sstream>

#include <common/src/log.h>
#include <util/text_util.h>

#include "app.h"
#include "import_csv_passwords_dialog.h"
#include "window/bookmarks_dialog.h"

struct _ImportCsvPasswordsDialog
{
	AdwDialog parent;

    uint n_imported;
    std::vector<orion::PasswordItemDto> not_imported_items;
    GtkLabel* title_label;
    GtkListBox* not_imported_passwords_list_box;
    GtkButton* open_passwords_button;
};

G_DEFINE_TYPE(ImportCsvPasswordsDialog, import_csv_passwords_dialog, ADW_TYPE_DIALOG)

static void
set_title_label(ImportCsvPasswordsDialog* self)
{
    // This translation approach will probably not work well for some languages and
    // may need to be revisited in the future
    std::string s1 = orion::text::ngettext_string(
        "%1$u password",
        "%1$u passwords",
        self->n_imported
    );

    std::string s2 = orion::text::ngettext_string(
        "%1$u password",
        "%1$u passwords",
        static_cast<uint>(self->not_imported_items.size())
    );

    std::string title = orion::text::translatef(
        "Orion successfully imported %1$s, but was unable to import"
        " %2$s because Orion already had passwords saved for those"
        " websites and user names.",
        s1.c_str(), s2.c_str()
    );
    gtk_label_set_text(self->title_label, title.c_str());
}

static void
populate_list_box(ImportCsvPasswordsDialog* self)
{
    for (const orion::PasswordItemDto& item : self->not_imported_items)
    {
        std::string row_text = item.url + " — " /* em dash */ + item.username;
        GtkWidget* row_label = gtk_label_new(row_text.c_str());
        gtk_widget_set_halign(row_label, GTK_ALIGN_START);
        GtkListBoxRow* row = GTK_LIST_BOX_ROW(gtk_list_box_row_new());
        gtk_list_box_row_set_child(row, row_label);
        gtk_list_box_append(self->not_imported_passwords_list_box, GTK_WIDGET(row));
    }
}

static void
set_open_passwords_button_visible(ImportCsvPasswordsDialog* self, bool visible)
{
    gtk_widget_set_visible(GTK_WIDGET(self->open_passwords_button), visible);
}

AdwDialog*
import_csv_passwords_dialog_new()
{
	return ADW_DIALOG(g_object_new(IMPORT_CSV_PASSWORDS_DIALOG_TYPE, NULL));
}

AdwDialog*
import_csv_passwords_dialog_new_for_import_result(ImportCsvPasswordsDialogOptions options)
{
    ImportCsvPasswordsDialog* self = APP_IMPORT_CSV_PASSWORDS_DIALOG(import_csv_passwords_dialog_new());
    
    self->n_imported = options.n_imported;
    self->not_imported_items = options.not_imported_items;

    set_title_label(self);
    populate_list_box(self);
    set_open_passwords_button_visible(self, options.show_open_passwords_button);

    return ADW_DIALOG(self);
}

static void
on_ok_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_open_password_preferences_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* self = ADW_DIALOG(user_data);
    GtkWidget* parent = gtk_widget_get_parent(GTK_WIDGET(self));
    adw_dialog_close(self);

    AdwDialog* bookmarks_dialog = bookmarks_dialog_new();
    bookmarks_dialog_sidebar_select(BOOKMARKS_DIALOG(bookmarks_dialog), 3); // Passwords page
    adw_dialog_present(bookmarks_dialog, parent);
}

static void
import_csv_passwords_dialog_finalize(GObject* object)
{
    ImportCsvPasswordsDialog* self = APP_IMPORT_CSV_PASSWORDS_DIALOG(object);

    std::destroy_at(&self->not_imported_items);
}

static void
import_csv_passwords_dialog_init(ImportCsvPasswordsDialog* self)
{
    std::construct_at(&self->not_imported_items);

	gtk_widget_init_template(GTK_WIDGET(self));
}

static void
import_csv_passwords_dialog_class_init(ImportCsvPasswordsDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = import_csv_passwords_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_csv_passwords_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, ImportCsvPasswordsDialog, title_label);
    gtk_widget_class_bind_template_child(widget_class, ImportCsvPasswordsDialog, not_imported_passwords_list_box);
    gtk_widget_class_bind_template_child(widget_class, ImportCsvPasswordsDialog, open_passwords_button);

	gtk_widget_class_bind_template_callback(widget_class, on_ok_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_open_password_preferences_clicked);
}
