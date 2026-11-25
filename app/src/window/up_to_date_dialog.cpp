#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "up_to_date_dialog.h"

struct _UpToDateDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(UpToDateDialog, up_to_date_dialog, ADW_TYPE_DIALOG)

AdwDialog*
up_to_date_dialog_new()
{
	return ADW_DIALOG(g_object_new(UP_TO_DATE_DIALOG_TYPE, NULL));
}

static void
on_destroy(UpToDateDialog* win)
{
	orion::log_info("up_to_date: on_destroy()");
}

static void
up_to_date_dialog_init(UpToDateDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));

	g_signal_connect(win, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_info("up_to_date: on_cancel_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
up_to_date_dialog_class_init(UpToDateDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/up_to_date_dialog.ui");

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_clicked);
}
