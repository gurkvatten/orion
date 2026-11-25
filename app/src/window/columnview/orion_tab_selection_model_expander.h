#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#define ORION_TYPE_TAB_SELECTION_MODEL_EXPANDER (orion_tab_selection_model_expander_get_type())

G_DECLARE_FINAL_TYPE(OrionTabSelectionModelExpander, orion_tab_selection_model_expander, ORION, TAB_SELECTION_MODEL_EXPANDER, GtkWidget)