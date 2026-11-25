#include <gtk/gtk.h>

#include "app.h"
#include "rss_feeds_popover.h"
#include "util/action_util.h"

struct _RssFeedsPopover
{
	GtkPopover parent;
};

G_DEFINE_TYPE(RssFeedsPopover, rss_feeds_popover, GTK_TYPE_POPOVER)

GtkPopover*
rss_feeds_popover_new()
{
	return GTK_POPOVER(g_object_new(RSS_FEEDS_POPOVER_TYPE, NULL));
}

static void //TODO: make these work, maybe with less menu button hacks
rss_feeds_popover_init(RssFeedsPopover* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
rss_feeds_popover_class_init(RssFeedsPopoverClass* klass) //TODO: add emojichooser popover to allow for
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/rss_feeds_popover.ui");
}