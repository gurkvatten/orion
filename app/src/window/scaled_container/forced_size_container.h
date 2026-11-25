#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_TYPE_FORCED_SIZE_CONTAINER (forced_size_container_get_type())

G_DECLARE_FINAL_TYPE(ForcedSizeContainer, forced_size_container, APP, FORCED_SIZE_CONTAINER, GtkWidget)