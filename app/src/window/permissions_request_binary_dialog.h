#pragma once

#include <gtk/gtk.h>
#include <adwaita.h>
#include "../app.h"
#include <common/src/log.h>
#include "orion_webview_tabview_widgets/permission_handler.h"

#define PERMISSIONS_REQUEST_BINARYY_DIALOG_TYPE (permissions_request_binary_dialog_get_type ())

G_DECLARE_FINAL_TYPE(PermissionsRequestBinaryDialog, permissions_request_binary_dialog, PERMISSIONS_REQUEST_BINARY, DIALOG, AdwDialog)

AdwDialog* permissions_request_binary_dialog_new(PermissionHandler* handler, const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr);