#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define IMPORT_CSV_PASSWORDS_DIALOG_TYPE (import_csv_passwords_dialog_get_type())

G_DECLARE_FINAL_TYPE(ImportCsvPasswordsDialog, import_csv_passwords_dialog, APP, IMPORT_CSV_PASSWORDS_DIALOG, AdwDialog)

struct ImportCsvPasswordsDialogOptions {
    uint n_imported;
    std::vector<orion::PasswordItemDto> not_imported_items;
    bool show_open_passwords_button;
};

AdwDialog* import_csv_passwords_dialog_new();
AdwDialog* import_csv_passwords_dialog_new_for_import_result(ImportCsvPasswordsDialogOptions options);