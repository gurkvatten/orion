#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define CLEAR_HISTORY_DIALOG_TYPE (clear_history_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ClearHistoryDialog, clear_history_dialog, CLEAR_HISTORY, DIALOG, AdwDialog)

AdwDialog* clear_history_dialog_new();