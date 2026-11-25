#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "app.h"

#define ADD_PASSWORD_DIALOG_TYPE (add_password_dialog_get_type())

G_DECLARE_FINAL_TYPE(AddPasswordDialog, add_password_dialog, ADD_PASSWORD, DIALOG, AdwDialog)

AdwDialog* add_password_dialog_new();