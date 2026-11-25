#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include <functional>
#include "../app.h"

#define DELETE_VERIFICATION_CODE_DIALOG_TYPE (delete_verification_code_dialog_get_type ())

G_DECLARE_FINAL_TYPE(DeleteVerificationCodeDialog, delete_verification_code_dialog, DELETE_VERIFICATION_CODE, DIALOG, AdwDialog)

AdwDialog* delete_verification_code_dialog_new();
void delete_verification_code_dialog_set_delete_cb(
    DeleteVerificationCodeDialog* self, std::function<void(gpointer)> cb, gpointer user_data
);