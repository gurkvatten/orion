#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"

#include <core/core_api.h>

#define ADD_WEB_EXTENSION_DIALOG_TYPE (add_web_extension_dialog_get_type ())

G_DECLARE_FINAL_TYPE(AddWebExtensionDialog, add_web_extension_dialog, ADD_WEB_EXTENSION, DIALOG, AdwDialog)

AdwDialog* add_web_extension_dialog_new(
    const WebExtensions::ExtensionInfo& extension, std::shared_ptr<WebExtensions::WebExtensionInstallSource> source
);
WebExtensions::ExtensionInfo add_web_extension_dialog_extension(AddWebExtensionDialog* win);
std::shared_ptr<WebExtensions::WebExtensionInstallSource> add_web_extension_dialog_source(AddWebExtensionDialog* win);
int add_web_extension_dialog_response(AddWebExtensionDialog* win);
