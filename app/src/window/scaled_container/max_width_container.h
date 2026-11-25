#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define MAX_WIDTH_CONTAINER_TYPE (max_width_container_get_type())

G_DECLARE_FINAL_TYPE(
    MaxWidthContainer,
    max_width_container,
    APP,
    MAX_WIDTH_CONTAINER,
    GtkWidget
)

GtkWidget* max_width_container_new(int max_width);

void max_width_container_set_max_width(
    MaxWidthContainer* self,
    int max_width);
void
max_width_container_set_child(
    MaxWidthContainer* self,
    GtkWidget* child);