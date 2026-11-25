#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gdk/gdk.h>
#include <common/src/log.h>
#include <webkit/webkit.h>
#include "../app.h"
#include "permissions_request_binary_dialog.h"
#include "../util/text_util.h"
#include <common/src/log.h>
#include "orion_webview_tabview_widgets/permission_handler.h"

namespace //force internal linkage to avoid potential ODR violations
{
    struct PermissionReqestBinaryDialogCppWrapper
    {
        std::string website;
        std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr;
    };
}

struct _PermissionsRequestBinaryDialog
{
	AdwDialog parent;
    
    GtkLabel* perm_label;
    GtkLabel* change_label;
    gboolean request_handeled = FALSE;
    PermissionHandler* handler;
    PermissionReqestBinaryDialogCppWrapper cpp_wrapper;
};

G_DEFINE_TYPE(PermissionsRequestBinaryDialog, permissions_request_binary_dialog, ADW_TYPE_DIALOG)

AdwDialog*
permissions_request_binary_dialog_new(PermissionHandler* handler, const std::string& website, std::shared_ptr<orion::gtk::GObjectPtr> request_ultra_ptr)
{
    orion::log_debug("Display permissions request dialog.");
    PermissionsRequestBinaryDialog* dialog = PERMISSIONS_REQUEST_BINARY_DIALOG(g_object_new(PERMISSIONS_REQUEST_BINARYY_DIALOG_TYPE, NULL));
    
    dialog->cpp_wrapper.request_ultra_ptr = request_ultra_ptr;

    dialog->handler = handler;
    dialog->cpp_wrapper.website = website;

    gtk_label_set_text(dialog->perm_label, handler->primary_description(dialog->cpp_wrapper.website).c_str());
    gtk_label_set_text(dialog->change_label, handler->secondary_description().c_str());

	return ADW_DIALOG(dialog);
}

static void 
resolve_request(gpointer user_data , gboolean allow, gboolean rememberChoice){
    PermissionsRequestBinaryDialog* dialog = PERMISSIONS_REQUEST_BINARY_DIALOG(user_data);
    WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(dialog->cpp_wrapper.request_ultra_ptr.get()->get());

    allow ? webkit_permission_request_allow(request) : webkit_permission_request_deny(request);

    if (allow) {
        dialog->handler->allow_persistently(dialog->cpp_wrapper.website);
    } else {
        dialog->handler->deny_persistently(dialog->cpp_wrapper.website);
    }

    orion::log_debug("Users choice: permission is granted: %s. Choice is remembered: %s", allow ? "TRUE" : "FALSE", rememberChoice ?  "TRUE" : "FALSE");

    dialog->request_handeled = TRUE;

    adw_dialog_close(ADW_DIALOG(user_data));
}


static void
permissions_request_binary_dialog_init(PermissionsRequestBinaryDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    std::construct_at(&win->cpp_wrapper);
}

static void
on_allow_once_button_clicked(GtkButton* button, gpointer user_data)
{
    resolve_request(user_data, TRUE, FALSE);
}

static void
on_dont_allow_button_clicked(GtkButton* button, gpointer user_data)
{
    resolve_request(user_data, FALSE, FALSE);
}

static void
permissions_request_binary_dialog_finalize(GObject* object){
    orion::log_debug("permissions_request_binary_dialog_finalize()");
    PermissionsRequestBinaryDialog* dialog = PERMISSIONS_REQUEST_BINARY_DIALOG(object);

    if(!dialog->request_handeled){
        orion::log_debug("permissions_request_dialog: dialog closed -> cancelling request");
        WebKitPermissionRequest* request = WEBKIT_PERMISSION_REQUEST(dialog->cpp_wrapper.request_ultra_ptr.get()->get());
        webkit_permission_request_deny(request);
    }

    std::destroy_at(&dialog->cpp_wrapper);
    G_OBJECT_CLASS(permissions_request_binary_dialog_parent_class)->finalize(object);
}

static void 
permissions_request_binary_dialog_class_init(PermissionsRequestBinaryDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = permissions_request_binary_dialog_finalize;
    
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/permissions_request_binary_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_allow_once_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_dont_allow_button_clicked);

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), PermissionsRequestBinaryDialog, perm_label);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), PermissionsRequestBinaryDialog, change_label);
}
#endif