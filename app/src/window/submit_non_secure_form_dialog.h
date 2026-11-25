#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define SUBMIT_NON_SECURE_FORM_DIALOG_TYPE (submit_non_secure_form_dialog_get_type ())

G_DECLARE_FINAL_TYPE(SubmitNonSecureFormDialog, submit_non_secure_form_dialog, SUBMIT_NON_SECURE_FORM, DIALOG, AdwDialog)

AdwDialog* submit_non_secure_form_dialog_new();