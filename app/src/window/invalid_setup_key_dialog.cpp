#include <gdk/gdk.h>

#include "../app.h"
#include "invalid_setup_key_dialog.h"

struct _InvalidSetupKeyDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(InvalidSetupKeyDialog, invalid_setup_key_dialog, ADW_TYPE_DIALOG)

AdwDialog*
invalid_setup_key_dialog_new()
{
	return ADW_DIALOG(g_object_new(INVALID_SETUP_KEY_DIALOG_TYPE, NULL));
}

static void
on_ok_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
invalid_setup_key_dialog_init(InvalidSetupKeyDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
invalid_setup_key_dialog_class_init(InvalidSetupKeyDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/invalid_setup_key_dialog.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_ok_clicked);
}
