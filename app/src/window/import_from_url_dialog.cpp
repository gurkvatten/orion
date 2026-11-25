#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <common/src/log.h>

#include "app.h"
#include "import_from_url_dialog.h"

struct _ImportFromUrlDialog
{
    AdwDialog parent;

    GtkEntry* entry;
};

G_DEFINE_TYPE(ImportFromUrlDialog, import_from_url_dialog, ADW_TYPE_DIALOG)

enum {
    SIG_FINISHED,
    N_SIGNALS,
};

static guint signals[N_SIGNALS] = {};

AdwDialog*
import_from_url_dialog_new()
{
    return ADW_DIALOG(g_object_new(APP_TYPE_IMPORT_FROM_URL_DIALOG, NULL));
}

static void
import_from_url_dialog_init(ImportFromUrlDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    ImportFromUrlDialog* dialog = APP_IMPORT_FROM_URL_DIALOG(user_data);
    g_signal_emit(user_data, signals[SIG_FINISHED], 0, NULL);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_import_button_clicked(GtkButton* button, gpointer user_data)
{
    ImportFromUrlDialog* dialog = APP_IMPORT_FROM_URL_DIALOG(user_data);
    g_signal_emit(user_data, signals[SIG_FINISHED], 0, gtk_editable_get_text(GTK_EDITABLE(dialog->entry)));
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
import_from_url_dialog_class_init(ImportFromUrlDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/import_from_url_dialog.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_import_button_clicked);

    gtk_widget_class_bind_template_child(widget_class, ImportFromUrlDialog, entry);

    signals[SIG_FINISHED] = g_signal_new("finished", APP_TYPE_IMPORT_FROM_URL_DIALOG, G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
        g_cclosure_marshal_VOID__STRING, G_TYPE_NONE, 1, G_TYPE_STRING, NULL);
}
