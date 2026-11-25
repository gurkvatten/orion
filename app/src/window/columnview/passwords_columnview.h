#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

void passwords_column_view_map(GtkWidget* self, gpointer user_data);
void passwords_column_view_unmap(GtkWidget* self, gpointer user_data);
void passwords_column_view_realize(GtkWidget* self, gpointer user_data);