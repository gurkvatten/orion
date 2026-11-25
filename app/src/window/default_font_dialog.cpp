#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "default_font_dialog.h"

struct _DefaultFontDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(DefaultFontDialog, default_font_dialog, ADW_TYPE_DIALOG)

AdwDialog*
default_font_dialog_new()
{
	return ADW_DIALOG(g_object_new(DEFAULT_FONT_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_done_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_destroy(DefaultFontDialog* self)
{
}

static void
default_font_dialog_init(DefaultFontDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
default_font_dialog_class_init(DefaultFontDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/default_font_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
	gtk_widget_class_bind_template_callback(widget_class, on_done_clicked);
}
