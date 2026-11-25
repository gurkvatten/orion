#include <gdk/gdk.h>

#include "../app.h"
#include "error_url_dialog.h"

struct _ErrorUrlDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(ErrorUrlDialog, error_url_dialog, ADW_TYPE_DIALOG)

AdwDialog*
error_url_dialog_new()
{
	return ADW_DIALOG(g_object_new(ERROR_URL_DIALOG_TYPE, NULL));
}

static void
error_url_dialog_init(ErrorUrlDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
error_url_dialog_class_init(ErrorUrlDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/error_url_dialog.ui");
}
