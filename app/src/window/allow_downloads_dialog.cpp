#include <gdk/gdk.h>

#include "../app.h"
#include "allow_downloads_dialog.h"

struct _AllowDownloadsDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(AllowDownloadsDialog, allow_downloads_dialog, ADW_TYPE_DIALOG)

AdwDialog*
allow_downloads_dialog_new()
{
	return ADW_DIALOG(g_object_new(ALLOW_DOWNLOADS_DIALOG_TYPE, NULL));
}

static void
allow_downloads_dialog_init(AllowDownloadsDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
allow_downloads_dialog_class_init(AllowDownloadsDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/allow_downloads_dialog.ui");
}
