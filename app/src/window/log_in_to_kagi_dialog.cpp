#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "log_in_to_kagi_dialog.h"

struct _LogInToKagiDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(LogInToKagiDialog, log_in_to_kagi_dialog, ADW_TYPE_DIALOG)

AdwDialog*
log_in_to_kagi_dialog_new()
{
	return ADW_DIALOG(g_object_new(LOG_IN_TO_KAGI_DIALOG_TYPE, NULL));
}

static void
on_close_button_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_destroy(LogInToKagiDialog* self)
{
}

static void
log_in_to_kagi_dialog_init(LogInToKagiDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), nullptr);
}

static void
log_in_to_kagi_dialog_class_init(LogInToKagiDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/log_in_to_kagi_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_close_button_clicked);
}
