#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gdk/gdk.h>
#include <common/src/log.h>
#include <webkit/webkit.h>

#include "app.h"
#include "remember_password_dialog.h"
#include "util/text_util.h"
#include "window/main_window/main_window.h"

// Test page: https://practicetestautomation.com/practice-test-login/

struct _RememberPasswordDialog
{
	AdwDialog parent;
    gboolean request_handled = FALSE;
    RememberPasswordDialogArgs args;
};

G_DEFINE_TYPE(RememberPasswordDialog, remember_password_dialog, ADW_TYPE_DIALOG)

enum {
    SIGNAL_SAVE_PASSWORD,
    SIGNAL_NEVER_ALLOW,
    SIGNAL_NOT_NOW,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

AdwDialog*
remember_password_dialog_new(const RememberPasswordDialogArgs& args)
{
    RememberPasswordDialog* dialog = REMEMBER_PASSWORD_DIALOG(g_object_new(REMEMBER_PASSWORD_DIALOG_TYPE, NULL));
    dialog->args = args;
	return ADW_DIALOG(dialog);
}

static void
on_remember_password_dialog_close(AdwDialog* dialog, gpointer user_data)
{
    RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(dialog);

    if (!self->request_handled)
    {
        orion::log_debug("User closed dialog -> default behaviour");
    }
}

static void
on_save_password_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("on_save_password_button_clicked");
    RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(user_data);
    self->request_handled = TRUE;
    g_signal_emit(self, signals[SIGNAL_SAVE_PASSWORD], 0);
    orion::gtk::WeakRefWrapper self_weak(self);

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    orion::CreatePasswordItemQuery query{
        .profile_id = profile_id,
        .url = self->args.url,
        .username = self->args.username,
        .password = self->args.password,
    };

    global_core_api->create_password_item(query, [self_weak](orion::CreatePasswordItemReply reply){
        VERIFY_LOG(reply.err);
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(self_strong.get());
        adw_dialog_close(ADW_DIALOG(self));
    });
}

static void
on_never_allow_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("on_never_allow_button_clicked");
    RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(user_data);
    self->request_handled = TRUE;
    g_signal_emit(self, signals[SIGNAL_NEVER_ALLOW], 0);

    // Store this as a website record in password manager with a flag to never allow saving passwords for this website

    orion::gtk::WeakRefWrapper self_weak(self);

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    orion::CreateNeverSavePasswordItemQuery query{
        .profile_id = profile_id,
        .url = self->args.url,
    };

    global_core_api->create_never_save_password_item(query, [self_weak](orion::CreateNeverSavePasswordItemReply reply){
        VERIFY_LOG(reply.err);
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(self_strong.get());
        adw_dialog_close(ADW_DIALOG(self));
    });
}

static void
on_not_now_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("on_not_now_button_clicked");
    RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(user_data);
    self->request_handled = TRUE;
    g_signal_emit(self, signals[SIGNAL_NOT_NOW], 0);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
remember_password_finalize(GObject* object)
{
    RememberPasswordDialog* self = REMEMBER_PASSWORD_DIALOG(object);
    std::destroy_at(&self->args);

    G_OBJECT_CLASS(remember_password_dialog_parent_class)->finalize(object);
}

static void
remember_password_dialog_init(RememberPasswordDialog* self)
{
    std::construct_at(&self->args);

	gtk_widget_init_template(GTK_WIDGET(self));
    g_signal_connect(self, "closed", G_CALLBACK(on_remember_password_dialog_close), NULL);
}

static void 
remember_password_dialog_class_init(RememberPasswordDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = remember_password_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/remember_password_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_save_password_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_never_allow_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_not_now_button_clicked);

    signals[SIGNAL_SAVE_PASSWORD] = g_signal_new(
        "save-password",
        REMEMBER_PASSWORD_DIALOG_TYPE,
        G_SIGNAL_RUN_LAST,
        0, // no class handler slot
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[SIGNAL_NEVER_ALLOW] = g_signal_new(
        "never-allow",
        REMEMBER_PASSWORD_DIALOG_TYPE,
        G_SIGNAL_RUN_LAST,
        0, // no class handler slot
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[SIGNAL_NOT_NOW] = g_signal_new(
        "not-now",
        REMEMBER_PASSWORD_DIALOG_TYPE,
        G_SIGNAL_RUN_LAST,
        0, // no class handler slot
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

#endif
