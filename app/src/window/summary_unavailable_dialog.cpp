#include <gdk/gdk.h>

#include "../app.h"
#include "summary_unavailable_dialog.h"
#include "settings_dialog.h"
#include "window/main_window/main_window.h"

struct _SummaryUnavailableDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(SummaryUnavailableDialog, summary_unavailable_dialog, ADW_TYPE_DIALOG)

AdwDialog*
summary_unavailable_dialog_new()
{
	return ADW_DIALOG(g_object_new(SUMMARY_UNAVAILABLE_DIALOG_TYPE, NULL));
}

static void
on_open_settings_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    adw_dialog_close(ADW_DIALOG(user_data));
    settings_dialog_present(win, 7);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
summary_unavailable_dialog_init(SummaryUnavailableDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
summary_unavailable_dialog_class_init(SummaryUnavailableDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/summary_unavailable_dialog.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_open_settings_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
}
