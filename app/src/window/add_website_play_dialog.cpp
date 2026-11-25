#include <gdk/gdk.h>

#include "../app.h"
#include "add_website_play_dialog.h"

struct _AddWebsitePlayDialog
{
	AdwDialog parent;
};

G_DEFINE_TYPE(AddWebsitePlayDialog, add_website_play_dialog, ADW_TYPE_DIALOG)

AdwDialog*
add_website_play_dialog_new()
{
	return ADW_DIALOG(g_object_new(ADD_WEBSITE_PLAY_DIALOG_TYPE, NULL));
}

static void
add_website_play_dialog_init(AddWebsitePlayDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
add_website_play_dialog_class_init(AddWebsitePlayDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/add_website_play_dialog.ui");
}
