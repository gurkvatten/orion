#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define EDIT_PASSWORD_DIALOG_TYPE (edit_password_dialog_get_type())

G_DECLARE_FINAL_TYPE(EditPasswordDialog, edit_password_dialog, APP, EDIT_PASSWORD_DIALOG, AdwDialog)

AdwDialog* edit_password_dialog_new();
void edit_password_dialog_set_id(EditPasswordDialog* dialog, orion::PasswordItemId id);
void edit_password_dialog_set_url(EditPasswordDialog* dialog, const char* url);
void edit_password_dialog_set_username(EditPasswordDialog* dialog, const char* username);
void edit_password_dialog_set_password(EditPasswordDialog* dialog, const char* password);
void edit_password_dialog_set_verification_key(EditPasswordDialog* dialog, const char* verification_key);
void edit_password_dialog_set_verification_code(EditPasswordDialog* dialog, const char* verification_code);