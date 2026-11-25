#define ENABLE_VERBOSE_LOGS 0
#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "get_orion_plus_dialog.h"

struct _GetOrionPlusDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(GetOrionPlusDialog, get_orion_plus_dialog, ADW_TYPE_DIALOG)

AdwDialog*
get_orion_plus_dialog_new()
{
	return ADW_DIALOG(g_object_new(GET_ORION_PLUS_DIALOG_TYPE, NULL));
}

static void
on_destroy(GetOrionPlusDialog* win)
{
	orion::log_verbose("get_orion_plus: on_destroy()");
}

static void
get_orion_plus_dialog_init(GetOrionPlusDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));

	g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
on_close_button_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_info("get_orion_plus: on_close_button_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
get_orion_plus_dialog_class_init(GetOrionPlusDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/get_orion_plus_dialog.ui");

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_close_button_clicked);
}
