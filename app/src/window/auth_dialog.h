#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define AUTH_DIALOG_TYPE (auth_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AuthDialog, auth_dialog, AUTH, DIALOG, AdwDialog)

AdwDialog* auth_dialog_new(WebKitAuthenticationRequest* request, gboolean is_secure);