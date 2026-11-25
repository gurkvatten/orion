
#pragma once

#include <gtk/gtk.h>
#include <core/dto/toolbar_item_dto.h>

void create_toolbar_button_context_menu(GMenu* menu, orion::ToolbarItemType item_type, orion::ToolbarItemId item_id);
void create_empty_space_context_menu(GMenu* menu, orion::ToolbarItemType item_type, orion::ToolbarItemId item_id);