#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define APP_TAB_SWITCHER_TYPE (tab_switcher_get_type ())

G_DECLARE_FINAL_TYPE(TabSwitcher, tab_switcher, APP, TAB_SWITCHER, AdwBin)

void tab_switcher_set_tab_view(TabSwitcher* self, AdwTabView* tab_view);

void tab_switcher_increment_ctrl_count(TabSwitcher* self, bool is_left_control);
void tab_switcher_decrement_ctrl_count(TabSwitcher* self, bool is_left_control);
void tab_switcher_reset_ctrl_count(TabSwitcher* self);
void tab_switcher_select_next_tab(TabSwitcher* self);
void tab_switcher_select_prev_tab(TabSwitcher* self);
