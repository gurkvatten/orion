#include <gtk/gtk.h>
#include <iostream>
#include <sstream>
#include <common/src/log.h>

#include "app.h"
#include "extensions_keyboard_shortcuts_dialog.h"
#include "window/accelerator_widget/orion_accelerator_entry.h"

struct _ExtensionsKeyboardShortcutsDialog
{
	AdwPreferencesDialog parent;
    orion::ExtensionId id;

    AdwPreferencesPage* pref_page;
    OrionAcceleratorEntry* activate_accelerator_entry;
    OrionAcceleratorEntry* lock_accelerator_entry;
};

G_DEFINE_TYPE(ExtensionsKeyboardShortcutsDialog, extensions_keyboard_shortcuts_dialog, ADW_TYPE_PREFERENCES_DIALOG)


static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_save_clicked(GtkButton* button, gpointer user_data)
{
    ExtensionsKeyboardShortcutsDialog* dialog = EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG(user_data);
    orion::UpdateExtensionQuery query{};
    query.id = dialog->id;
    query.update_activate_shortcut = true;
    query.activate_shortcut = orion_accelerator_entry_get_accelerator_string(dialog->activate_accelerator_entry);
    query.update_lock_shortcut = true;
    query.lock_shortcut = orion_accelerator_entry_get_accelerator_string(dialog->lock_accelerator_entry);
    global_core_api->update_extension(query, [](orion::UpdateExtensionReply reply){VERIFY_LOG(reply.err);});

    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_reset_clicked(GtkButton* button, gpointer user_data)
{
    ExtensionsKeyboardShortcutsDialog* dialog = EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG(user_data);
    orion::GetInstalledExtensionsQuery query{};
    query.ids = {dialog->id};
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(dialog);
    global_core_api->get_installed_extensions(query, [wrapped_ref](orion::GetInstalledExtensionsReply reply){
        VERIFY_LOG(reply.err);
        ExtensionsKeyboardShortcutsDialog* dialog = EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG(wrapped_ref->get());
        if(dialog)
        {
            if(reply.extensions.size()==1)
            {
                orion::ExtensionDto& extension = reply.extensions.at(0);
                //TODO: set pref page description to correct count
                //adw_preferences_page_set_description(dialog->pref_page, TODO);
                orion_accelerator_entry_set_accelerator_from_string(dialog->activate_accelerator_entry, extension.activate_shortcut.c_str());
                orion_accelerator_entry_set_accelerator_from_string(dialog->lock_accelerator_entry, extension.lock_shortcut.c_str());
            }
            g_object_unref(dialog);
        }
    });
}

static void
extensions_keyboard_shortcuts_dialog_init(ExtensionsKeyboardShortcutsDialog* win)
{
    std::construct_at(&win->id);
	gtk_widget_init_template(GTK_WIDGET(win));
}

AdwDialog*
extensions_keyboard_shortcuts_dialog_new(orion::ExtensionId id)
{
    ExtensionsKeyboardShortcutsDialog* dialog = (ExtensionsKeyboardShortcutsDialog*)g_object_new(EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG_TYPE, NULL);
    dialog->id = id;

    orion::GetInstalledExtensionsQuery query{};
    query.ids = {id};
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(dialog);
    global_core_api->get_installed_extensions(query, [wrapped_ref](orion::GetInstalledExtensionsReply reply){
        VERIFY_LOG(reply.err);
        ExtensionsKeyboardShortcutsDialog* dialog = EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG(wrapped_ref->get());
        if(dialog)
        {
            if(reply.extensions.size()==1)
            {
                orion::ExtensionDto& extension = reply.extensions.at(0);
                //TODO: set pref page description to correct count
                //adw_preferences_page_set_description(dialog->pref_page, TODO);
                orion_accelerator_entry_set_accelerator_from_string(dialog->activate_accelerator_entry, extension.activate_shortcut.c_str());
                orion_accelerator_entry_set_accelerator_from_string(dialog->lock_accelerator_entry, extension.lock_shortcut.c_str());
            }
            g_object_unref(dialog);
        }
    });



	return ADW_DIALOG(dialog);
}

static void
extensions_keyboard_shortcuts_dialog_finalize(GObject* win)
{
    ExtensionsKeyboardShortcutsDialog* dialog = EXTENSIONS_KEYBOARD_SHORTCUTS_DIALOG(win);
    std::destroy_at(&dialog->id);
    G_OBJECT_CLASS(extensions_keyboard_shortcuts_dialog_parent_class)->finalize(win);
}

static void
extensions_keyboard_shortcuts_dialog_class_init(ExtensionsKeyboardShortcutsDialogClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/extensions_keyboard_shortcuts_dialog.ui");
    gtk_widget_class_bind_template_child(widget_class, ExtensionsKeyboardShortcutsDialog, pref_page);
    gtk_widget_class_bind_template_child(widget_class, ExtensionsKeyboardShortcutsDialog, activate_accelerator_entry);
    gtk_widget_class_bind_template_child(widget_class, ExtensionsKeyboardShortcutsDialog, lock_accelerator_entry);
    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_save_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reset_clicked);

    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = extensions_keyboard_shortcuts_dialog_finalize;
}
