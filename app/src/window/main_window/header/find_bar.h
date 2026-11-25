#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <string>

#define APP_FIND_BAR_TYPE (find_bar_get_type ())

G_DECLARE_FINAL_TYPE(FindBar, find_bar, APP, FIND_BAR, AdwBin)

void find_bar_focus_entry(FindBar* self);
void find_bar_refresh_search(FindBar* self);
void find_bar_set_text_and_search(FindBar* self, const std::string& text);
