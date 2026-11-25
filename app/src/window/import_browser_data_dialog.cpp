#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "import_browser_data_dialog.h"
#include "import_from_browser_dialog.h"
#include "import_from_extension_dialog.h"

struct _ImportBrowserDataDialog
{
	AdwDialog parent;
    GtkListBox* list_box;
};

G_DEFINE_TYPE(ImportBrowserDataDialog, import_browser_data_dialog, ADW_TYPE_DIALOG)

AdwDialog*
import_browser_data_dialog_new()
{
	return ADW_DIALOG(g_object_new(IMPORT_BROWSER_DATA_DIALOG_TYPE, NULL));
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

    ImportBrowserDataDialog* self = IMPORT_BROWSER_DATA_DIALOG(user_data);
    GtkListBoxRow* row = gtk_list_box_get_selected_row(self->list_box);
    const char* name = gtk_widget_get_name(GTK_WIDGET(row));
    orion::log_debug("on_import_clicked: name:%s", name);

    if (strcmp(name, "browser") == 0) {
        adw_dialog_present(import_from_browser_dialog_new(), GTK_WIDGET(user_data));
    } else if (strcmp(name, "extension") == 0) {
        adw_dialog_present(import_from_extension_dialog_new(), GTK_WIDGET(user_data));
    }
}

static void
on_destroy(ImportBrowserDataDialog* self)
{
}

static void
import_browser_data_dialog_init(ImportBrowserDataDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
import_browser_data_dialog_class_init(ImportBrowserDataDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_browser_data_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, ImportBrowserDataDialog, list_box);
	gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
	gtk_widget_class_bind_template_callback(widget_class, on_import_clicked);
}
