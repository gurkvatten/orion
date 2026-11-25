#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"
#include <core/dto/search_engine_dto.h>

#define ADD_SEARCH_ENGINE_DIALOG_TYPE (add_search_engine_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AddSearchEngineDialog, add_search_engine_dialog, ADD_SEARCH_ENGINE, DIALOG, AdwDialog)

AdwDialog* add_search_engine_dialog_new(const orion::SearchEngineDto& dto);