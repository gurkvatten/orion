#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"

#define APP_TAB_BAR_TYPE (tab_bar_get_type ())

G_DECLARE_FINAL_TYPE(TabBar, tab_bar, APP, TAB_BAR, GtkBox)

void tab_bar_set_tab_view(TabBar* self, OrionWebviewTabView* tab_view);
