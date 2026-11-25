#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#define INSTALL_SITE_AS_APP_DIALOG_TYPE (install_site_as_app_dialog_get_type ())

G_DECLARE_FINAL_TYPE(InstallSiteAsAppDialog, install_site_as_app_dialog, INSTALL_SITE_AS_APP, DIALOG, AdwDialog)

AdwDialog* install_site_as_app_dialog_new();