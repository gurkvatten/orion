#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define DOWNLOADS_DIALOG_TYPE (downloads_dialog_get_type())

G_DECLARE_FINAL_TYPE(DownloadsDialog, downloads_dialog, DOWNLOADS, DIALOG, AdwDialog)

AdwDialog* downloads_dialog_new();