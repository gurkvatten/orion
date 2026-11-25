#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define EDIT_BOOKMARK_DIALOG_TYPE (edit_bookmark_dialog_get_type ())

G_DECLARE_FINAL_TYPE(EditBookmarkDialog, edit_bookmark_dialog, EDIT_BOOKMARK, DIALOG, AdwDialog)

AdwDialog* edit_bookmark_dialog_new();
AdwDialog* edit_bookmark_dialog_new_for_id(orion::BookmarkId id);