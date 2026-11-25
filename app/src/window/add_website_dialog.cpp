#include <gdk/gdk.h>
#include <utility>

#include <common/src/uri.h>
#include "../app.h"
#include "add_website_dialog.h"

namespace {

struct AddWebsiteDialogContext {
    orion::ProfileId profile_id;
    std::string key_name;
    std::string type;
    std::vector<AddWebsiteDialogSettingOption> options;
};

}

struct _AddWebsiteDialog
{
	AdwDialog parent;

    GtkButton* cancel_button;
    GtkButton* add_button;
    GtkEntry* website_entry;
    GtkDropDown* value_drop_down;
    AddWebsiteDialogContext* context;
};

G_DEFINE_TYPE(AddWebsiteDialog, add_website_dialog, ADW_TYPE_DIALOG)

static void
add_website_dialog_update_options(AddWebsiteDialog* dialog)
{
    GtkStringList* model = gtk_string_list_new(nullptr);
    if (dialog->context) {
        for (const AddWebsiteDialogSettingOption& option : dialog->context->options) {
            gtk_string_list_append(model, option.label.c_str());
        }
    }

    gtk_drop_down_set_model(dialog->value_drop_down, G_LIST_MODEL(model));
    g_object_unref(model);
    gtk_drop_down_set_selected(dialog->value_drop_down, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(dialog->add_button), dialog->context && !dialog->context->options.empty());
}

AdwDialog*
add_website_dialog_new(
    orion::ProfileId profile_id,
    const std::string& key_name,
    const std::string& type,
    std::vector<AddWebsiteDialogSettingOption> options)
{
	AddWebsiteDialog* dialog = ADD_WEBSITE_DIALOG(g_object_new(ADD_WEBSITE_DIALOG_TYPE, NULL));
    delete dialog->context;
    dialog->context = new AddWebsiteDialogContext{
        .profile_id = profile_id,
        .key_name = key_name,
        .type = type,
        .options = std::move(options),
    };
    add_website_dialog_update_options(dialog);
	return ADW_DIALOG(dialog);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static std::string
add_website_dialog_normalized_website(GtkEntry* entry)
{
    char* stripped = g_strdup(gtk_editable_get_text(GTK_EDITABLE(entry)));
    g_strstrip(stripped);
    std::string input = stripped;
    g_free(stripped);

    if (input.empty()) return input;

    orion::Uri uri = orion::Uri::parse_user_input(input.c_str());
    if (!uri.authority().empty()) return uri.authority();
    return input;
}

static void
on_add_clicked(GtkButton* button, gpointer user_data)
{
    AddWebsiteDialog* dialog = ADD_WEBSITE_DIALOG(user_data);
    if (!dialog->context || dialog->context->options.empty()) return;

    std::string website = add_website_dialog_normalized_website(dialog->website_entry);
    if (website.empty()) return;

    guint selected = gtk_drop_down_get_selected(dialog->value_drop_down);
    if (selected >= dialog->context->options.size()) return;

    const AddWebsiteDialogSettingOption& option = dialog->context->options[selected];
    orion::SetSettingQuery query{
        .profile_id = dialog->context->profile_id,
        .key = global_core_api->get_website_setting_key(website, dialog->context->key_name),
        .realm = global_core_api->get_website_setting_realm(),
        .key_name = dialog->context->key_name,
        .type = dialog->context->type,
        .value = option.value,
    };

    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);
    });

    adw_dialog_close(ADW_DIALOG(dialog));
}

static void
add_website_dialog_init(AddWebsiteDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    add_website_dialog_update_options(win);
    
    g_signal_connect(win->cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), win);
    g_signal_connect(win->add_button, "clicked", G_CALLBACK(on_add_clicked), win);
}

static void
add_website_dialog_finalize(GObject* object)
{
    AddWebsiteDialog* dialog = ADD_WEBSITE_DIALOG(object);
    delete dialog->context;
    dialog->context = nullptr;

    G_OBJECT_CLASS(add_website_dialog_parent_class)->finalize(object);
}

static void
add_website_dialog_class_init(AddWebsiteDialogClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = add_website_dialog_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/add_website_dialog.ui");
    
	gtk_widget_class_bind_template_child(widget_class, AddWebsiteDialog, cancel_button);
	gtk_widget_class_bind_template_child(widget_class, AddWebsiteDialog, add_button);
	gtk_widget_class_bind_template_child(widget_class, AddWebsiteDialog, website_entry);
	gtk_widget_class_bind_template_child(widget_class, AddWebsiteDialog, value_drop_down);
}
