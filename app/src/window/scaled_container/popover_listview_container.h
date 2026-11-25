#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define APP_TYPE_POPOVER_LISTVIEW_CONTAINER (popover_listview_container_get_type())

G_DECLARE_FINAL_TYPE(PopoverListviewContainer, popover_listview_container, APP, POPOVER_LISTVIEW_CONTAINER, GtkWidget)

