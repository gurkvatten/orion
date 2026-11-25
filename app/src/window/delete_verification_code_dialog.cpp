#include <gdk/gdk.h>

#include "../app.h"
#include "delete_verification_code_dialog.h"

struct _DeleteVerificationCodeDialog
{
	AdwDialog parent;
    std::function<void(gpointer)> delete_cb;
    gpointer delete_cb_user_data;
};

G_DEFINE_TYPE(DeleteVerificationCodeDialog, delete_verification_code_dialog, ADW_TYPE_DIALOG)

AdwDialog*
delete_verification_code_dialog_new()
{
	return ADW_DIALOG(g_object_new(DELETE_VERIFICATION_CODE_DIALOG_TYPE, NULL));
}

static void
on_delete_clicked(GtkButton* button, gpointer user_data)
{
    DeleteVerificationCodeDialog* self = DELETE_VERIFICATION_CODE_DIALOG(user_data);
    if (self->delete_cb) {
        self->delete_cb(self->delete_cb_user_data);
    }
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

void
delete_verification_code_dialog_set_delete_cb(
    DeleteVerificationCodeDialog* self, std::function<void(gpointer)> cb, gpointer user_data
)
{
    self->delete_cb = cb;
    self->delete_cb_user_data = user_data;
}

static void
delete_verification_code_dialog_finalize(GObject* object)
{
    DeleteVerificationCodeDialog* self = DELETE_VERIFICATION_CODE_DIALOG(object);

    std::destroy_at(&self->delete_cb);

    G_OBJECT_CLASS(delete_verification_code_dialog_parent_class)->finalize(object);
}

static void
delete_verification_code_dialog_init(DeleteVerificationCodeDialog* win)
{
    std::construct_at(&win->delete_cb);

	gtk_widget_init_template(GTK_WIDGET(win));
}

static void
delete_verification_code_dialog_class_init(DeleteVerificationCodeDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = delete_verification_code_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/delete_verification_code_dialog.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_delete_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
}
