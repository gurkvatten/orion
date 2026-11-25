#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "app.h"
#include "data_item/downloads_item.h"

#define APP_TYPE_DOWNLOADS_ITEM_WIDGET (downloads_item_widget_get_type())

G_DECLARE_FINAL_TYPE(DownloadsItemWidget, downloads_item_widget, APP, DOWNLOADS_ITEM_WIDGET, GtkBox);
