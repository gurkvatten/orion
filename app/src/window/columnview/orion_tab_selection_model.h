#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include <string>

#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

#define ORION_TYPE_TAB_SELECTION_MODEL (orion_tab_selection_model_get_type())

G_DECLARE_FINAL_TYPE(OrionTabSelectionModel, orion_tab_selection_model, ORION, TAB_SELECTION_MODEL, GObject)

OrionTabSelectionModel* orion_tab_selection_model_new(GListModel* model);

void orion_tab_selection_model_set_show_nested(OrionTabSelectionModel* selection_model, gboolean show_nested);

void orion_tab_selection_model_set_item_expanded(OrionTabSelectionModel* model, uint pos, bool item_expanded);
uint orion_tab_selection_model_get_item_indent(OrionTabSelectionModel* model, uint pos);

