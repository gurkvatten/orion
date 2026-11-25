#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define ADD_WEBSITE_PLAY_DIALOG_TYPE (add_website_play_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AddWebsitePlayDialog, add_website_play_dialog, ADD_WEBSITE_PLAY, DIALOG, AdwDialog)

AdwDialog* add_website_play_dialog_new();