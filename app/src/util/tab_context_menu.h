#pragma once

#include <gtk/gtk.h>
#include <core/dto/window_dto.h>

#include "window/main_window/main_window.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "action/main_window/action.h"

namespace orion::gtk {

enum class CreateTabContextMenuType
{
    main_tabbar,
    sidebar,
};

void fill_tab_context_menu(
    GMenu* menu, MainWindow* win, AdwTabView* tab_view, AdwTabPage* page,
    orion::GetOtherTabIdsReply other_tab_ids_reply, CreateTabContextMenuType type, Cb<Error> done_cb = nullptr
);

void append_move_tab_submenu(MainWindow* win, TabId tab_id, GMenu* menu, TabType tab_type);

} // namespace orion::gtk