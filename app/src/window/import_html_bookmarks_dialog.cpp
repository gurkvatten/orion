#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <format>
#include <common/src/log.h>
#include <util/text_util.h>

#include "app.h"
#include "import_html_bookmarks_dialog.h"

// TODO: Add Cancel button and cancellation support to core API and importer if needed

struct _ImportHtmlBookmarksDialog
{
	AdwDialog parent;
    GtkLabel* status_label;
    GtkLabel* result_label;
    GtkButton* close_button;
};

G_DEFINE_TYPE(ImportHtmlBookmarksDialog, import_html_bookmarks_dialog, ADW_TYPE_DIALOG)

AdwDialog*
import_html_bookmarks_dialog_new()
{
	return ADW_DIALOG(g_object_new(IMPORT_HTML_BOOKMARKS_DIALOG_TYPE, NULL));
}

void
import_html_bookmarks_dialog_set_result(ImportHtmlBookmarksDialog* self, int num_imported, orion::Error err)
{
    if (err) {
        gtk_label_set_label(self->status_label, _("Failed to import bookmarks"));
    } else {
        gtk_label_set_label(self->status_label, _("Import complete:"));
        std::string s{};
        if (num_imported == 1) {
            s = orion::text::translatef("%1$d item imported", num_imported);
        } else {
            s = orion::text::translatef("%1$d items imported", num_imported);
        }
        std::string text = std::format("– {} ({})", _("Bookmarks"), s);
        gtk_label_set_label(self->result_label, text.c_str());
    }

    // Unblock Close button
    gtk_widget_set_sensitive(GTK_WIDGET(self->close_button), TRUE);

    // Enable closing the dialog
    adw_dialog_set_can_close(ADW_DIALOG(self), TRUE);
}

static void
on_close_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_destroy(ImportHtmlBookmarksDialog* self)
{
}

static void
import_html_bookmarks_dialog_init(ImportHtmlBookmarksDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
import_html_bookmarks_dialog_class_init(ImportHtmlBookmarksDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_html_bookmarks_dialog.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_close_clicked);

    gtk_widget_class_bind_template_child(widget_class, ImportHtmlBookmarksDialog, status_label);
    gtk_widget_class_bind_template_child(widget_class, ImportHtmlBookmarksDialog, result_label);
    gtk_widget_class_bind_template_child(widget_class, ImportHtmlBookmarksDialog, close_button);
}
