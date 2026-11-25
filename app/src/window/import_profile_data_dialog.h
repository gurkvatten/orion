#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_PROFILE_DATA_DIALOG_TYPE (import_profile_data_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportProfileDataDialog, import_profile_data_dialog, APP, IMPORT_PROFILE_DATA_DIALOG, AdwDialog)

AdwDialog* import_profile_data_dialog_new();
AdwDialog* import_profile_data_dialog_new_for_file_name(const char* file_name, orion::ProfileId profile_id);

