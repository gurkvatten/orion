#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <common/src/log.h>

#include "app.h"
#include "overflow_menu_popover.h"

struct _OverflowMenuPopover
{
	GtkPopover parent;
};

G_DEFINE_TYPE(OverflowMenuPopover, overflow_menu_popover, GTK_TYPE_POPOVER)

GtkPopover*
overflow_menu_popover_new()
{
	return GTK_POPOVER(g_object_new(OVERFLOW_MENU_POPOVER_TYPE, NULL));
}

static void
overflow_menu_popover_init(OverflowMenuPopover* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
overflow_menu_popover_class_init(OverflowMenuPopoverClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/overflow_menu_popover.ui");
}