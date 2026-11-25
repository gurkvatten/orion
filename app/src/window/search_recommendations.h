#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define SEARCH_RECOMMENDATIONS_TYPE (search_recommendations_get_type ())

G_DECLARE_FINAL_TYPE(SearchRecommendations, search_recommendations, SEARCH, RECOMMENDATIONS, GtkBox)

SearchRecommendations* search_recommendations_new();
void search_recommendations_hide_overlay(SearchRecommendations *self);