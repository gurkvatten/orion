#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gdk/gdk.h>
#include <common/src/log.h>
#include <webkit/webkit.h>

#include "../app.h"
#include "auth_dialog.h"
#include "../util/text_util.h"

struct _AuthDialog
{
	AdwDialog parent;
    WebKitAuthenticationRequest* request; //How to pass in this request
    GtkEntry* username_entry;
    GtkPasswordEntry* password_entry;
    GtkCheckButton* remember_checkbox;
    gboolean auth_handeled = false;
    GtkLabel* log_in_host_label;
    GtkLabel* security_label;
};

G_DEFINE_TYPE(AuthDialog, auth_dialog, ADW_TYPE_DIALOG)

AdwDialog*
auth_dialog_new(WebKitAuthenticationRequest* request, gboolean is_secure)
{   
    AuthDialog* dialog = AUTH_DIALOG(g_object_new(AUTH_DIALOG_TYPE, NULL));
    dialog->request = request; 

    //TODO Implement that proposed_credential will acctually exist
    /*
    WebKitCredential* credential = webkit_credential_new("admin", "secret", WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_set_proposed_credential(request, credential);
    */

    WebKitSecurityOrigin* origin =  webkit_authentication_request_get_security_origin(request);

    gtk_label_set_text(dialog->log_in_host_label, orion::text::translatef("Log in to %1$s:%2$d", webkit_security_origin_get_host(origin), webkit_security_origin_get_port(origin)).c_str());

    //TODO check the security
    const char* security_message;
    if (is_secure) {
        security_message = _("Your login information will be sent securely.");
    } else {
        security_message = _("Your login information will be sent unencrypted.");
    }
    gtk_label_set_text(dialog->security_label, security_message);

    WebKitCredential* proposed_credential = webkit_authentication_request_get_proposed_credential(request);
    if (proposed_credential != NULL)
    {
        const gchar* proposed_username = webkit_credential_get_username(proposed_credential);
        const gchar* proposed_password = webkit_credential_get_password(proposed_credential);

        gtk_editable_set_text(GTK_EDITABLE(dialog->username_entry), proposed_username);
        gtk_editable_set_text(GTK_EDITABLE(dialog->password_entry), proposed_password);
        webkit_credential_free(proposed_credential);
    }


    webkit_security_origin_unref(origin);
    return ADW_DIALOG(dialog);
}

static void
on_auth_dialog_close(AdwDialog* dialog, gpointer user_data)
{
    AuthDialog* self = AUTH_DIALOG(dialog);

    if (self->request && !self->auth_handeled) {
        orion::log_debug("auth_dialog: dialog closed without login or canceling -> cancelling request");
        webkit_authentication_request_cancel(self->request);
    }
}

static void
auth_dialog_init(AuthDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    g_signal_connect(win, "closed", G_CALLBACK(on_auth_dialog_close), NULL);
}

static void
on_cancel_button_clicked(GtkButton* button, gpointer user_data)
{
    AuthDialog* self = AUTH_DIALOG(user_data);
	orion::log_debug("auth_dialog: on_close_button_clicked()");
    const gchar* realm = webkit_authentication_request_get_realm(self->request);
    const gchar* host = webkit_authentication_request_get_host(self->request);
    orion::log_debug("auth_dialog: Canceling authentification challenge - for realm: \"%s\", host: \"%s\"", realm, host);
    webkit_authentication_request_cancel(self->request);

    self->auth_handeled = TRUE;
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_log_in_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("auth_dialog: on_log_in_button_clicked()");
    AuthDialog* self = AUTH_DIALOG(user_data);

    const gchar* username = gtk_editable_get_text(GTK_EDITABLE(self->username_entry));
    const gchar* password = gtk_editable_get_text(GTK_EDITABLE(self->password_entry));

    //orion::log_debug("auth_dialog: username: \"%s\", passwd: \"%s\"", username, password);

    gboolean remember_me = gtk_check_button_get_active(self->remember_checkbox);
    if (remember_me)
    {
        /*TODO remember credentials */
    }

    WebKitCredential* credential = webkit_credential_new(username, password, WEBKIT_CREDENTIAL_PERSISTENCE_NONE);
    webkit_authentication_request_authenticate(self->request, credential);
    webkit_credential_free(credential); //TODO Potential use after free in signals?

    self->auth_handeled = TRUE;
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void 
auth_dialog_class_init(AuthDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/auth_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_log_in_button_clicked);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AuthDialog, username_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AuthDialog, password_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AuthDialog, remember_checkbox);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AuthDialog, log_in_host_label);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), AuthDialog, security_label);
}
#endif