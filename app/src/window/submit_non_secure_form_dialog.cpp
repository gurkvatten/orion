#include <gdk/gdk.h>
#include <common/src/log.h>

#include "../app.h"
#include "submit_non_secure_form_dialog.h"

struct _SubmitNonSecureFormDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(SubmitNonSecureFormDialog, submit_non_secure_form_dialog, ADW_TYPE_DIALOG)

AdwDialog*
submit_non_secure_form_dialog_new()
{
	return ADW_DIALOG(g_object_new(SUBMIT_NON_SECURE_FORM_DIALOG_TYPE, NULL));
}

static void
submit_non_secure_form_dialog_init(SubmitNonSecureFormDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
on_close_button_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_debug("submit_non_secure_form: on_close_button_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));
}

static void 
submit_non_secure_form_dialog_class_init(SubmitNonSecureFormDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/submit_non_secure_form_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_close_button_clicked);
}
