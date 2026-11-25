#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define EXPORT_PROFILE_DATA_DIALOG_TYPE (export_profile_data_dialog_get_type())

G_DECLARE_FINAL_TYPE(ExportProfileDataDialog, export_profile_data_dialog, APP, EXPORT_PROFILE_DATA_DIALOG, AdwDialog)

AdwDialog* export_profile_data_dialog_new();
AdwDialog* export_profile_data_dialog_new_for_file_name(const char* file_name, orion::ProfileId profile_id);
