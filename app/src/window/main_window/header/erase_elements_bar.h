#pragma once

#include <gtk/gtk.h>

#define APP_TYPE_ERASE_ELEMENTS_BAR (erase_elements_bar_get_type ())
G_DECLARE_FINAL_TYPE(EraseElementsBar, erase_elements_bar, APP, ERASE_ELEMENTS_BAR, GtkBox);

void erase_elements_bar_start(EraseElementsBar* self);
bool erase_elements_bar_get_is_running(EraseElementsBar* self);
void erase_elements_bar_reset_selectors(EraseElementsBar* self);