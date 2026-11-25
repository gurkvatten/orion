#pragma once

#include "app.h"

#include <gtk/gtk.h>
#include <adwaita.h>

#include <string>

#define APP_TYPE_SEARCH_COMPLETION_MODEL (search_completion_model_get_type())

G_DECLARE_FINAL_TYPE(SearchCompletionModel, search_completion_model, APP, SEARCH_COMPLETION_MODEL, GObject)
