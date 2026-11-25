#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_TYPE_JS_SIDEBAR (js_sidebar_get_type())

G_DECLARE_FINAL_TYPE(JsSidebar, js_sidebar, APP, JS_SIDEBAR, GtkBox)

//vulnerable to brute forcing epoch but this is to only be triggered from user provided scripts
//epoch also always starts counting up from 0, no randomisation is applied
//treat this epoch as a synchronisation tool, not a security token
int32_t js_sidebar_start_operation(JsSidebar* self, const char* title);
void js_sidebar_clear(JsSidebar* self, int32_t epoch);
void js_sidebar_set_content(JsSidebar* self, int32_t epoch, std::string_view content);
void js_sidebar_add_content(JsSidebar* self, int32_t epoch, std::string_view content);

char* js_sidebar_get_content(JsSidebar* self);


bool js_sidebar_match_epoch(JsSidebar* self, int32_t epoch);