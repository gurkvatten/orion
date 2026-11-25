#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#define REMEMBER_PASSWORD_DIALOG_TYPE (remember_password_dialog_get_type ())

G_DECLARE_FINAL_TYPE(RememberPasswordDialog, remember_password_dialog, REMEMBER_PASSWORD, DIALOG, AdwDialog)

struct RememberPasswordDialogArgs
{
    std::string url;
    std::string username;
    std::string password;
};

AdwDialog* remember_password_dialog_new(const RememberPasswordDialogArgs& args);