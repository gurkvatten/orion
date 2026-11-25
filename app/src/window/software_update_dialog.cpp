#include <gtk/gtk.h>
#include <common/src/log.h>

#include "app.h"
#include "software_update_dialog.h"
#include "updating_orion_dialog.h"
#include "util/dialog_util.h"

struct _SoftwareUpdateDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(SoftwareUpdateDialog, software_update_dialog, ADW_TYPE_DIALOG)

AdwDialog*
software_update_dialog_new()
{
	return ADW_DIALOG(g_object_new(SOFTWARE_UPDATE_DIALOG_TYPE, NULL));
}

static void
software_update_dialog_init(SoftwareUpdateDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
on_install_update_clicked(GtkButton* button, gpointer user_data)
{
	orion::log_info("software_update: on_install_update_clicked()");

	adw_dialog_close(ADW_DIALOG(user_data));

	adw_dialog_present(updating_orion_dialog_new(), GTK_WIDGET(orion::gtk::get_parent_window(GTK_WIDGET(user_data))));
}

static void
software_update_dialog_class_init(SoftwareUpdateDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/software_update_dialog.ui");

	gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_install_update_clicked);
}
