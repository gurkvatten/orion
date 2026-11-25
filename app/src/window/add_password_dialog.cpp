#include <gtk/gtk.h>
#include <iostream>
#include <sstream>

#include <common/src/log.h>
#include <common/src/defer.h>

#include "app.h"
#include "add_password_dialog.h"
#include "window/main_window/main_window.h"
#include "util/text_util.h"

struct _AddPasswordDialog
{
	AdwDialog parent;

    GtkEntry* url_entry;
    GtkEntry* username_entry;
    GtkEntry* password_entry;
    GtkButton* save_password_button;
};

G_DEFINE_TYPE(AddPasswordDialog, add_password_dialog, ADW_TYPE_DIALOG)

AdwDialog*
add_password_dialog_new()
{
	return ADW_DIALOG(g_object_new(ADD_PASSWORD_DIALOG_TYPE, NULL));
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

struct AlreadyExistsData {
    AddPasswordDialog* dialog;
    orion::PasswordItemId password_item_id;
    std::string new_password;
};

static void
on_already_exists_response(AdwAlertDialog* self, gchar* response, gpointer user_data)
{
    AlreadyExistsData* data = static_cast<AlreadyExistsData*>(user_data);
    defer([data]{ delete data; });

    if (g_strcmp0(response, "yes") == 0) {
        orion::gtk::WeakRefWrapper dialog_weak(data->dialog);
        orion::UpdatePasswordItemQuery query{
            .id = data->password_item_id,
            .password = data->new_password,
            .update_password = true,
        };
        global_core_api->update_password_item(query, [dialog_weak](orion::Error err){
            VERIFY_LOG(err);

            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            AddPasswordDialog* dialog = ADD_PASSWORD_DIALOG(dialog_strong.get());
            
            // close AddPasswordDialog only on success
            adw_dialog_close(ADW_DIALOG(dialog));
        });
    }
}

static void
on_save_password_clicked(GtkButton* button, gpointer user_data)
{
    AddPasswordDialog* dialog = ADD_PASSWORD_DIALOG(user_data);
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);
    std::string url = gtk_editable_get_text(GTK_EDITABLE(ADD_PASSWORD_DIALOG(user_data)->url_entry));
    std::string username = gtk_editable_get_text(GTK_EDITABLE(ADD_PASSWORD_DIALOG(user_data)->username_entry));
    std::string password = gtk_editable_get_text(GTK_EDITABLE(ADD_PASSWORD_DIALOG(user_data)->password_entry));

    orion::Uri uri = orion::Uri::parse_strict(url.c_str());
    if (uri.scheme().empty()) {
        url = "https://" + url;
    }

    orion::CreatePasswordItemQuery query{
        .profile_id = profile_id,
        .url = url,
        .username = username,
        .password = password,
        .update_existing = false,
    };

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->create_password_item(query,
        [dialog_weak,url,username,password](orion::CreatePasswordItemReply reply){
            VERIFY_LOG(reply.err);

            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            AddPasswordDialog* dialog = ADD_PASSWORD_DIALOG(dialog_strong.get());

            if (!reply.already_exists) {
                // close the dialog only on success
                adw_dialog_close(ADW_DIALOG(dialog));
                return;
            }

            // password item already exists and nothing was changed
            std::string text = orion::text::translatef(
                "Do you want to update the password saved in your keychain for “%1$s” on “%2$s”?",
                username.c_str(), url.c_str()
            );

            AdwDialog* alert_dialog = adw_alert_dialog_new(
                _("Username for website already exists."),
                text.c_str()
            );

            adw_alert_dialog_add_response(ADW_ALERT_DIALOG(alert_dialog), "no", _("No"));
            adw_alert_dialog_add_response(ADW_ALERT_DIALOG(alert_dialog), "yes", _("Yes"));

            adw_alert_dialog_set_default_response(ADW_ALERT_DIALOG(alert_dialog), "yes");
            adw_alert_dialog_set_response_appearance(ADW_ALERT_DIALOG(alert_dialog), "yes", ADW_RESPONSE_SUGGESTED);
            adw_alert_dialog_set_close_response(ADW_ALERT_DIALOG(alert_dialog), "no");

            AlreadyExistsData* data = new AlreadyExistsData{
                .dialog = dialog,
                .password_item_id = reply.id,
                .new_password = password,
            };

            g_signal_connect(alert_dialog, "response", G_CALLBACK(on_already_exists_response), data);
            adw_dialog_present(alert_dialog, GTK_WIDGET(dialog));
        });
}

// enables Save Password button only if all fields are non-empty
static void
on_entry_changed(GtkEditable* editable, gpointer user_data)
{
    AddPasswordDialog* self = ADD_PASSWORD_DIALOG(user_data);

    const char* url_text = gtk_editable_get_text(GTK_EDITABLE(self->url_entry));
    const char* username_text = gtk_editable_get_text(GTK_EDITABLE(self->username_entry));
    const char* password_text = gtk_editable_get_text(GTK_EDITABLE(self->password_entry));

    bool enable_save = (url_text && url_text[0] != '\0') &&
                       (username_text && username_text[0] != '\0') &&
                       (password_text && password_text[0] != '\0');

    gtk_widget_set_sensitive(GTK_WIDGET(self->save_password_button), enable_save);
}

static void
add_password_dialog_init(AddPasswordDialog* self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

    g_signal_connect(self->url_entry, "changed", G_CALLBACK(on_entry_changed), self);
    g_signal_connect(self->username_entry, "changed", G_CALLBACK(on_entry_changed), self);
    g_signal_connect(self->password_entry, "changed", G_CALLBACK(on_entry_changed), self);

    gtk_widget_set_sensitive(GTK_WIDGET(self->save_password_button), FALSE);
}

static void
add_password_dialog_class_init(AddPasswordDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/add_password_dialog.ui");

	gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
	gtk_widget_class_bind_template_callback(widget_class, on_save_password_clicked);

    gtk_widget_class_bind_template_child(widget_class, AddPasswordDialog, url_entry);
    gtk_widget_class_bind_template_child(widget_class, AddPasswordDialog, username_entry);
    gtk_widget_class_bind_template_child(widget_class, AddPasswordDialog, password_entry);
    gtk_widget_class_bind_template_child(widget_class, AddPasswordDialog, save_password_button);
}
