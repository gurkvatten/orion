#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define MOVE_DOWNLOADS_TO_TRASH_DIALOG_TYPE (move_downloads_to_trash_dialog_get_type())

G_DECLARE_FINAL_TYPE(MoveDownloadsToTrashDialog, move_downloads_to_trash, MOVE_DOWNLOADS_TO_TRASH, DIALOG, AdwDialog)

enum class MoveDownloadsToTrashDialogResult{
    confirm,
    cancel,
};

AdwDialog* move_downloads_to_trash_dialog_new();