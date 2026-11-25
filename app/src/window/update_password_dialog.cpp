#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <gdk/gdk.h>
#include <common/src/log.h>
#include <webkit/webkit.h>

#include "app.h"
#include "update_password_dialog.h"
#include "util/text_util.h"
#include "window/main_window/main_window.h"

struct _UpdatePasswordDialog
{
	AdwDialog parent;
    gboolean request_handled = FALSE;
    UpdatePasswordDialogArgs args;
};

G_DEFINE_TYPE(UpdatePasswordDialog, update_password_dialog, ADW_TYPE_DIALOG)

enum {
    SIGNAL_UPDATE_PASSWORD,
    SIGNAL_DO_NOT_UPDATE,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

AdwDialog*
update_password_dialog_new(const UpdatePasswordDialogArgs& args)
{
    UpdatePasswordDialog* dialog = UPDATE_PASSWORD_DIALOG(g_object_new(UPDATE_PASSWORD_DIALOG_TYPE, NULL));
    dialog->args = args;
	return ADW_DIALOG(dialog);
}

static void
on_update_password_dialog_close(AdwDialog* dialog, gpointer user_data)
{
    UpdatePasswordDialog* self = UPDATE_PASSWORD_DIALOG(dialog);

    if (!self->request_handled)
    {
        orion::log_debug("User closed dialog -> default behaviour");
    }
}

static void
on_update_password_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("on_update_password_button_clicked");
    UpdatePasswordDialog* self = UPDATE_PASSWORD_DIALOG(user_data);
    self->request_handled = TRUE;
    g_signal_emit(self, signals[SIGNAL_UPDATE_PASSWORD], 0);
    orion::gtk::WeakRefWrapper self_weak(self);

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    // Update the password item in the backend
    orion::UpdatePasswordItemQuery query{
        .id = self->args.password_item_id,
        .password = self->args.new_password,
        .update_password = true,
    };

    global_core_api->update_password_item(query, [self_weak](orion::Error err){
        VERIFY_LOG(err);
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        UpdatePasswordDialog* self = UPDATE_PASSWORD_DIALOG(self_strong.get());
        adw_dialog_close(ADW_DIALOG(self));
    });
}

static void
on_do_not_update_button_clicked(GtkButton* button, gpointer user_data)
{
    orion::log_debug("on_do_not_update_button_clicked");
    UpdatePasswordDialog* self = UPDATE_PASSWORD_DIALOG(user_data);
    self->request_handled = TRUE;
    g_signal_emit(self, signals[SIGNAL_DO_NOT_UPDATE], 0);
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
update_password_finalize(GObject* object)
{
    UpdatePasswordDialog* self = UPDATE_PASSWORD_DIALOG(object);
    std::destroy_at(&self->args);

    G_OBJECT_CLASS(update_password_dialog_parent_class)->finalize(object);
}

static void
update_password_dialog_init(UpdatePasswordDialog* self)
{
    std::construct_at(&self->args);

	gtk_widget_init_template(GTK_WIDGET(self));
    g_signal_connect(self, "closed", G_CALLBACK(on_update_password_dialog_close), NULL);
}

static void 
update_password_dialog_class_init(UpdatePasswordDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = update_password_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX
        "/src/window/update_password_dialog.ui");
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_update_password_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_do_not_update_button_clicked);

    signals[SIGNAL_UPDATE_PASSWORD] = g_signal_new(
        "update-password",
        UPDATE_PASSWORD_DIALOG_TYPE,
        G_SIGNAL_RUN_LAST,
        0, // no class handler slot
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    signals[SIGNAL_DO_NOT_UPDATE] = g_signal_new(
        "do-not-update",
        UPDATE_PASSWORD_DIALOG_TYPE,
        G_SIGNAL_RUN_LAST,
        0, // no class handler slot
        NULL, NULL,
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);
}

#endif
