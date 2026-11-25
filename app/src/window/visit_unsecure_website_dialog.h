#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define VISIT_UNSECURE_WEBSITE_DIALOG_TYPE (visit_unsecure_website_dialog_get_type ())

G_DECLARE_FINAL_TYPE(VisitUnsecureWebsiteDialog, visit_unsecure_website_dialog, VISIT_UNSECURE_WEBSITE, DIALOG, AdwDialog)

AdwDialog* visit_unsecure_website_dialog_new();