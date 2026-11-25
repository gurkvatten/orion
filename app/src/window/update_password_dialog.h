#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define UPDATE_PASSWORD_DIALOG_TYPE (update_password_dialog_get_type ())

G_DECLARE_FINAL_TYPE(UpdatePasswordDialog, update_password_dialog, UPDATE_PASSWORD, DIALOG, AdwDialog)

struct UpdatePasswordDialogArgs
{
    orion::PasswordItemId password_item_id;
    std::string url;
    std::string username;
    std::string new_password;
};

AdwDialog* update_password_dialog_new(const UpdatePasswordDialogArgs& args);