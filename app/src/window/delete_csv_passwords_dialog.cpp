#include <gtk/gtk.h>
#include <iostream>
#include <sstream>

#include <common/src/log.h>
#include <common/src/defer.h>
#include <util/text_util.h>

#include "app.h"
#include "delete_csv_passwords_dialog.h"

struct _DeleteCsvPasswordsDialog
{
	AdwDialog parent;

    uint n_imported;
    std::string csv_file_path;
    GtkLabel* title_label;
    GtkButton* delete_file_button;
};

G_DEFINE_TYPE(DeleteCsvPasswordsDialog, delete_csv_passwords_dialog, ADW_TYPE_DIALOG)

static void
set_title_label(DeleteCsvPasswordsDialog* self)
{
    // This translation approach will probably not work well for some languages and
    // may need to be revisited in the future
    std::string s = orion::text::ngettext_string(
        "%1$u password",
        "%1$u passwords",
        self->n_imported
    );

    gchar* filename = g_path_get_basename(self->csv_file_path.c_str());
    defer([filename]{ g_free(filename); });

    std::string title = orion::text::translatef(
        "Orion successfully imported %1$s. Do you want to delete ‘%2$s’?",
        s.c_str(), filename
    );
    gtk_label_set_text(self->title_label, title.c_str());
}

static void
set_delete_file_label(DeleteCsvPasswordsDialog* self)
{
    gchar* filename = g_path_get_basename(self->csv_file_path.c_str());
    defer([filename]{ g_free(filename); });

    std::string filename_str;

    if (filename) {
        filename_str = filename;
    }

    std::string label = "Delete ‘" + filename_str + "’";
    gtk_button_set_label(self->delete_file_button, label.c_str());
}

AdwDialog*
delete_csv_passwords_dialog_new()
{
	return ADW_DIALOG(g_object_new(DELETE_CSV_PASSWORDS_DIALOG_TYPE, NULL));
}

AdwDialog*
delete_csv_passwords_dialog_new_for_import_result(
    uint n_imported, const char* csv_file_path)
{
    DeleteCsvPasswordsDialog* self = APP_DELETE_CSV_PASSWORDS_DIALOG(delete_csv_passwords_dialog_new());
    
    self->n_imported = n_imported;
    self->csv_file_path = csv_file_path;

    set_title_label(self);
    set_delete_file_label(self);

    return ADW_DIALOG(self);
}

static void
on_not_now_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
delete_file(DeleteCsvPasswordsDialog* self)
{
    GError* error = NULL;
    GFile* file = g_file_new_for_path(self->csv_file_path.c_str());

    if (!g_file_delete(file, NULL, &error)) {
        defer([error]{ g_error_free(error); });
        orion::log_error("failed to delete file: %s", error->message);
        return;
    }
    defer([file]{ g_object_unref(file); });

    orion::log_info("file deleted: %s", self->csv_file_path.c_str());
}

static void
on_delete_file_clicked(GtkButton* button, gpointer user_data)
{
    DeleteCsvPasswordsDialog* self = APP_DELETE_CSV_PASSWORDS_DIALOG(user_data);
    delete_file(self);
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
delete_csv_passwords_dialog_finalize(GObject* object)
{
    DeleteCsvPasswordsDialog* self = APP_DELETE_CSV_PASSWORDS_DIALOG(object);

    std::destroy_at(&self->csv_file_path);
}

static void
delete_csv_passwords_dialog_init(DeleteCsvPasswordsDialog* self)
{
    std::construct_at(&self->csv_file_path);

	gtk_widget_init_template(GTK_WIDGET(self));
}

static void
delete_csv_passwords_dialog_class_init(DeleteCsvPasswordsDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = delete_csv_passwords_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/delete_csv_passwords_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, DeleteCsvPasswordsDialog, title_label);
    gtk_widget_class_bind_template_child(widget_class, DeleteCsvPasswordsDialog, delete_file_button);

	gtk_widget_class_bind_template_callback(widget_class, on_not_now_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_delete_file_clicked);
}
