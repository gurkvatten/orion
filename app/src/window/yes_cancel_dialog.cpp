#include <gdk/gdk.h>

#include "../app.h"
#include "yes_cancel_dialog.h"

struct _YesCancelDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(YesCancelDialog, yes_cancel_dialog, ADW_TYPE_DIALOG)

AdwDialog*
yes_cancel_dialog_new()
{
	return ADW_DIALOG(g_object_new(YES_CANCEL_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_yes_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
yes_cancel_dialog_init(YesCancelDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
yes_cancel_dialog_class_init(YesCancelDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/yes_cancel_dialog.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_yes_clicked);
}
