#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"
#include "window/main_window/main_window.h"

#define ADD_BOOKMARK_DIALOG_TYPE (add_bookmark_dialog_get_type())

G_DECLARE_FINAL_TYPE(AddBookmarkDialog, add_bookmark_dialog, ADD_BOOKMARK, DIALOG, AdwDialog)

AdwDialog* add_bookmark_dialog_new_init_from_current_page(MainWindow* win);
AdwDialog* add_bookmark_dialog_new_init_from_tab_dto(orion::TabDto tab);

void add_bookmark_dialog_set_bookmark_folder_id(AddBookmarkDialog* dialog, orion::BookmarkFolderId id);