#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define ALLOW_DOWNLOADS_DIALOG_TYPE (allow_downloads_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AllowDownloadsDialog, allow_downloads_dialog, ALLOW_DOWNLOADS, DIALOG, AdwDialog)

AdwDialog* allow_downloads_dialog_new();