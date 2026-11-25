#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "import_from_extension_dialog.h"
#include "util/dialog_util.h"

struct _ImportFromExtensionDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(ImportFromExtensionDialog, import_from_extension_dialog, ADW_TYPE_DIALOG)

AdwDialog*
import_from_extension_dialog_new()
{
	return ADW_DIALOG(g_object_new(IMPORT_FROM_EXTENSION_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_import_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_import_csv_file_clicked(GtkButton* button, gpointer user_data)
{
    auto file_dialog = gtk_file_dialog_new();
	gtk_file_dialog_open(file_dialog, orion::gtk::get_parent_window(GTK_WIDGET(user_data)), NULL, NULL, NULL);
	g_object_unref(file_dialog);
}

static void
on_destroy(ImportFromExtensionDialog* self)
{
}

static void
import_from_extension_dialog_init(ImportFromExtensionDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
import_from_extension_dialog_class_init(ImportFromExtensionDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_from_extension_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
	gtk_widget_class_bind_template_callback(widget_class, on_import_clicked);
	gtk_widget_class_bind_template_callback(widget_class, on_import_csv_file_clicked);
}
