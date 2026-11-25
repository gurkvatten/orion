#pragma once

#include <gtk/gtk.h>
#include "app.h"

#define ADD_BOOKMARK_FOLDER_TYPE (add_bookmark_folder_get_type())

typedef enum {
    ADD_BOOKMARK_FOLDER_NORMAL,
    ADD_BOOKMARK_FOLDER_BOOKMARKS,
    ADD_BOOKMARK_FOLDER_READING_LIST
} AddBookmarkFolder;

GType add_bookmark_folder_get_type (void);

G_DECLARE_FINAL_TYPE(Folder, folder, FOLDER, TYPE, GObject);

Folder* folder_new_from_bookmark_folder_dto(orion::BookmarkFolderDto dto);
Folder* folder_new_bookmarks();
Folder* folder_new_reading_list();
