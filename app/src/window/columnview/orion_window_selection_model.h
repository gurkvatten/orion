#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define ORION_TYPE_WINDOW_SELECTION_MODEL (orion_window_selection_model_get_type())

G_DECLARE_FINAL_TYPE(OrionWindowSelectionModel, orion_window_selection_model, ORION, WINDOW_SELECTION_MODEL, GObject)

gboolean orion_window_selection_model_actual_select_item(OrionWindowSelectionModel* model, guint position);
OrionWindowSelectionModel* orion_window_selection_model_new(GListModel* model);
