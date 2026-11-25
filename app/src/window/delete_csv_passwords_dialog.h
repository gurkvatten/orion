#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define DELETE_CSV_PASSWORDS_DIALOG_TYPE (delete_csv_passwords_dialog_get_type())

G_DECLARE_FINAL_TYPE(DeleteCsvPasswordsDialog, delete_csv_passwords_dialog, APP, DELETE_CSV_PASSWORDS_DIALOG, AdwDialog)

AdwDialog* delete_csv_passwords_dialog_new();
AdwDialog* delete_csv_passwords_dialog_new_for_import_result(uint n_imported, const char* csv_file_name);