#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define MANAGE_WEBSITE_DATA_DIALOG_TYPE (manage_website_data_dialog_get_type ())

G_DECLARE_FINAL_TYPE(ManageWebsiteDataDialog, manage_website_data_dialog, MANAGE_WEBSITE_DATA, DIALOG, AdwDialog)

AdwDialog* manage_website_data_dialog_new();