#include <gdk/gdk.h>

#include "../app.h"
#include "edit_password_dialog.h"
#include "delete_verification_code_dialog.h"
#include "invalid_setup_key_dialog.h"
#include "util/text_util.h"

struct _EditPasswordDialog
{
	AdwDialog parent;

    orion::PasswordItemId id;
    GString* url;
    GString* username;
    GString* password;
    GString* verification_key;
    GString* verification_code;

    GtkLabel* url_label;
    GtkEntry* username_entry;
    GtkEntry* password_entry;
    GtkLabel* verification_key_label;
    GtkEntry* verification_key_entry;
    GtkLabel* verification_code_label;
    GtkLabel* verification_code_header;
    GtkLabel* setup_info_label;
    GtkBox* verification_code_box1;
    GtkBox* verification_code_box2;
    AdwToolbarView* toolbar_view;
    GtkBox* delete_box;

    GtkBox* verification_key_box_header;
    GtkBox* verification_key_box_content;
    GtkButton* delete_verification_code_button;
    GtkButton* verification_code_setup_button;
    GtkLabel* validity_label;
    GtkButton* save_password_button;

    guint timer_id;
};

G_DEFINE_TYPE(EditPasswordDialog, edit_password_dialog, ADW_TYPE_DIALOG)

AdwDialog*
edit_password_dialog_new()
{
	return ADW_DIALOG(g_object_new(EDIT_PASSWORD_DIALOG_TYPE, NULL));
}

static void
refresh_editables(EditPasswordDialog* self)
{
    gtk_editable_set_text(GTK_EDITABLE(self->username_entry), self->username->str);
    gtk_editable_set_text(GTK_EDITABLE(self->password_entry), self->password->str);
    gtk_editable_set_text(GTK_EDITABLE(self->verification_key_entry), self->verification_key->str);
}

static void
refresh_labels(EditPasswordDialog* self)
{
    if (self->verification_key->str && strlen(self->verification_key->str) > 0) {
        orion::GenerateTotpVerificationCodeQuery query{
            .verification_key = self->verification_key->str,
        };
        std::string code;
        orion::Error err = global_core_api->generate_totp_verification_code(query, &code);
        if (err) {
            LOG_ERROR(err);
        } else {
            edit_password_dialog_set_verification_code(self, code.c_str());
        }
    }

    gtk_label_set_text(self->url_label, self->url->str);
    gtk_label_set_text(self->verification_key_label, self->verification_key->str);
    gtk_label_set_text(self->verification_code_label, self->verification_code->str);

    uint64_t valid_seconds = 30 - (time(NULL) % 30);

    std::string text = orion::text::translatef(
        "This code is valid for the next %1$d seconds.\n"
        "A new code will be generated when it expires.",
        valid_seconds
    );

    gtk_label_set_text(self->validity_label, text.c_str());
}

static void
set_visible(GtkWidget* widget, gboolean visible)
{
    gtk_widget_set_child_visible(widget, visible);
    gtk_widget_set_visible(widget, visible);
}

static void
refresh_visibility(EditPasswordDialog* self)
{
    bool is_key_setup = (self->verification_key->len > 0);
    gtk_widget_set_margin_top(GTK_WIDGET(self->verification_key_box_header), is_key_setup ? 8 : 0);
    set_visible(GTK_WIDGET(self->verification_key_entry), !is_key_setup);
    set_visible(GTK_WIDGET(self->verification_key_box_content), is_key_setup);
    set_visible(GTK_WIDGET(self->verification_key_label), is_key_setup);
    set_visible(GTK_WIDGET(self->verification_code_setup_button), !is_key_setup);
    set_visible(GTK_WIDGET(self->verification_code_label), is_key_setup);
    set_visible(GTK_WIDGET(self->verification_code_header), is_key_setup);
    set_visible(GTK_WIDGET(self->validity_label), is_key_setup);
    set_visible(GTK_WIDGET(self->delete_verification_code_button), is_key_setup);
    set_visible(GTK_WIDGET(self->setup_info_label), !is_key_setup);
    set_visible(GTK_WIDGET(self->verification_code_box1), is_key_setup);
    set_visible(GTK_WIDGET(self->verification_code_box2), is_key_setup);
    set_visible(GTK_WIDGET(self->delete_box), is_key_setup);
    gtk_widget_queue_resize(GTK_WIDGET(self->toolbar_view));
    gtk_widget_queue_resize(GTK_WIDGET(self));
}

void
edit_password_dialog_set_id(EditPasswordDialog* dialog, orion::PasswordItemId id)
{
    dialog->id = id;
}

void
edit_password_dialog_set_url(EditPasswordDialog* dialog, const char* url)
{
    g_string_assign(dialog->url, url);
}

void edit_password_dialog_set_username(EditPasswordDialog* dialog, const char* username)
{
    g_string_assign(dialog->username, username);
}

void edit_password_dialog_set_password(EditPasswordDialog* dialog, const char* password)
{
    g_string_assign(dialog->password, password);
}

void edit_password_dialog_set_verification_key(EditPasswordDialog* dialog, const char* verification_key)
{
    g_string_assign(dialog->verification_key, verification_key);
}

void edit_password_dialog_set_verification_code(EditPasswordDialog* dialog, const char* verification_code)
{
    g_string_assign(dialog->verification_code, verification_code);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_save_password_clicked(GtkButton* button, gpointer user_data)
{
    EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(user_data);

    std::string username = std::string(gtk_editable_get_text(GTK_EDITABLE(dialog->username_entry)));
    std::string password = std::string(gtk_editable_get_text(GTK_EDITABLE(dialog->password_entry)));
    std::string verification_key = std::string(gtk_editable_get_text(GTK_EDITABLE(dialog->verification_key_entry)));

    orion::UpdatePasswordItemQuery query{
        .id = dialog->id,
        .username = username,
        .password = password,
        .verification_key = verification_key,
        .update_username = true,
        .update_password = true,
        .update_verification_key = true,
    };

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->update_password_item(query, [dialog_weak](orion::Error err){
        VERIFY_LOG(err);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(dialog_strong.get());

        adw_dialog_close(ADW_DIALOG(dialog));
    });
}

static void
on_delete_verification_code_clicked(GtkButton* button, gpointer user_data)
{
    EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(user_data);
    AdwDialog* dialog = delete_verification_code_dialog_new();

    delete_verification_code_dialog_set_delete_cb(
        DELETE_VERIFICATION_CODE_DIALOG(dialog),
        [](gpointer user_data){
            EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(user_data);
            orion::gtk::WeakRefWrapper self_weak(self);

            // delete the verification key and code
            global_core_api->update_password_item(orion::UpdatePasswordItemQuery{
                .id = self->id,
                .verification_key = "",
                .verification_code = "",
                .update_verification_key = true,
                .update_verification_code = true,
            }, [self_weak](orion::Error err){
                VERIFY_LOG(err);

                auto self_strong = self_weak.lock();
                if (!self_strong) return;
                EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(self_strong.get());

                edit_password_dialog_set_verification_key(self, "");
                edit_password_dialog_set_verification_code(self, "");

                refresh_labels(self);
                refresh_editables(self);
                refresh_visibility(self);
            });
        },
        self
    );
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

static void
on_setup_clicked(GtkButton* button, gpointer user_data)
{
    EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(user_data);
    const char* key = gtk_editable_get_text(GTK_EDITABLE(dialog->verification_key_entry));
    orion::GenerateTotpVerificationCodeQuery query{
        .verification_key = key,
    };
    std::string code;
    orion::Error err = global_core_api->generate_totp_verification_code(query, &code);
    if (err) {
        adw_dialog_present(invalid_setup_key_dialog_new(), GTK_WIDGET(user_data));
        VERIFY_LOG(err);
    }

    orion::gtk::WeakRefWrapper dialog_weak(dialog);
    std::string key_str = key;

    // store the key
    global_core_api->update_password_item(orion::UpdatePasswordItemQuery{
        .id = APP_EDIT_PASSWORD_DIALOG(user_data)->id,
        .verification_key = key,
        .update_verification_key = true,
    }, [dialog_weak,key_str](orion::Error err){
        VERIFY_LOG(err);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        EditPasswordDialog* dialog = APP_EDIT_PASSWORD_DIALOG(dialog_strong.get());

        edit_password_dialog_set_verification_key(dialog, key_str.c_str());

        refresh_labels(dialog);
        refresh_editables(dialog);
        refresh_visibility(dialog);
    });
}

// enables Save Password button only if at least one field was changed and all fields are non-empty
static void
on_entry_changed(GtkEditable* editable, gpointer user_data)
{
    EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(user_data);

    const char* username_text = gtk_editable_get_text(GTK_EDITABLE(self->username_entry));
    const char* password_text = gtk_editable_get_text(GTK_EDITABLE(self->password_entry));

    bool enable_save = (username_text && username_text[0] != '\0') &&
                       (password_text && password_text[0] != '\0') &&
                       (strcmp(username_text, self->username->str) != 0 ||
                       (strcmp(password_text, self->password->str) != 0));

    gtk_widget_set_sensitive(GTK_WIDGET(self->save_password_button), enable_save);

    const char* key = gtk_editable_get_text(GTK_EDITABLE(self->verification_key_entry));

    bool enable_setup = key && key[0] != '\0';

    gtk_widget_set_sensitive(GTK_WIDGET(self->verification_code_setup_button), enable_setup);
}

static gboolean
on_timer(gpointer user_data)
{
    EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(user_data);

    bool is_key_setup = (self->verification_key->len > 0);
    uint64_t valid_seconds = 30 - (time(NULL) % 30);

    orion::log_debug("edit_password_dialog: timer fired to refresh TOTP code: is_key_setup=%d valid_seconds=%d",
        is_key_setup, valid_seconds
    );

    if (!is_key_setup) {
        return G_SOURCE_CONTINUE; // continue the timer
    }

    // do not refresh editables here, it would mess with for example text selection in GtkEntry
    refresh_labels(self);

    return G_SOURCE_CONTINUE; // continue the timer
}

static void
edit_password_dialog_realize(EditPasswordDialog* self, gpointer user_data)
{
    refresh_labels(self);
    refresh_editables(self);
    refresh_visibility(self);
}

static void
edit_password_dialog_finalize(GObject* object)
{
    EditPasswordDialog* self = APP_EDIT_PASSWORD_DIALOG(object);

    g_string_free(self->url, TRUE);
    g_string_free(self->username, TRUE);
    g_string_free(self->password, TRUE);
    g_string_free(self->verification_key, TRUE);
    g_string_free(self->verification_code, TRUE);

    if (self->timer_id) {
        g_source_remove(self->timer_id);
        self->timer_id = 0;
    }

    std::destroy_at(&self->id);

    G_OBJECT_CLASS(edit_password_dialog_parent_class)->finalize(object);
}

static void
edit_password_dialog_init(EditPasswordDialog* self)
{
    gtk_widget_init_template(GTK_WIDGET(self));

    std::construct_at(&self->id);

    g_signal_connect(self, "realize", G_CALLBACK(edit_password_dialog_realize), NULL);
    g_signal_connect(self->username_entry, "changed", G_CALLBACK(on_entry_changed), self);
    g_signal_connect(self->password_entry, "changed", G_CALLBACK(on_entry_changed), self);
    g_signal_connect(self->verification_key_entry, "changed", G_CALLBACK(on_entry_changed), self);

    self->url = g_string_new("");
    self->username = g_string_new("");
    self->password = g_string_new("");
    self->verification_key = g_string_new("");
    self->verification_code = g_string_new("");

    self->timer_id = g_timeout_add_seconds(1, on_timer, self);

    // trigger the timer manually because the first timed call will trigger after 1 second
    on_timer(self);
}

static void
edit_password_dialog_class_init(EditPasswordDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = edit_password_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/edit_password_dialog.ui");

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_save_password_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_delete_verification_code_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_setup_clicked);

    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, url_label);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, username_entry);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, password_entry);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_key_label);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_key_entry);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_key_box_header);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_key_box_content);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, delete_verification_code_button);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_code_setup_button);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, validity_label);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, save_password_button);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_code_label);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_code_header);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, setup_info_label);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_code_box1);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, verification_code_box2);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, toolbar_view);
    gtk_widget_class_bind_template_child(widget_class, EditPasswordDialog, delete_box);
}
