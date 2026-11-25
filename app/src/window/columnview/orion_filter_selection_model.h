#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

#define ORION_TYPE_FILTER_SELECTION_MODEL (orion_filter_selection_model_get_type())

G_DECLARE_FINAL_TYPE(OrionFilterSelectionModel, orion_filter_selection_model, ORION, FILTER_SELECTION_MODEL, GObject)

//takes a reference to incoming data (transfer none)
OrionFilterSelectionModel* orion_filter_selection_model_new(GtkSelectionModel* model);

//takes a reference to incoming data (transfer none)
void orion_filter_selection_model_set_filter(OrionFilterSelectionModel* selection_model, GtkFilter* filter);
//takes a reference to incoming data (transfer none)
void orion_filter_selection_model_set_model(OrionFilterSelectionModel* selection_model, GtkSelectionModel* model);

