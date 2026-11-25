#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define RSS_FEEDS_POPOVER_TYPE (rss_feeds_popover_get_type())

G_DECLARE_FINAL_TYPE(RssFeedsPopover, rss_feeds_popover, RSS_FEEDS, POPOVER, GtkPopover)

GtkPopover* rss_feeds_popover_new();