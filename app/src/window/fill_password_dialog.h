#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>

#include "app.h"

#define FILL_PASSWORD_DIALOG_TYPE (fill_password_dialog_get_type())

G_DECLARE_FINAL_TYPE(FillPasswordDialog, fill_password_dialog, APP, FILL_PASSWORD_DIALOG, AdwDialog)

using FillPasswordDialogResultCb = bool(*)(std::string username, std::string password, gpointer user_data);

AdwDialog* fill_password_dialog_new(FillPasswordDialogResultCb cb, gpointer user_data);