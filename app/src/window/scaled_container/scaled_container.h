#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_SCALED_CONTAINER_TYPE (scaled_container_get_type())

G_DECLARE_FINAL_TYPE(ScaledContainer, scaled_container, APP, SCALED_CONTAINER, GtkWidget)