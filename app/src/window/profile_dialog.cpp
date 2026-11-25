#include <gdk/gdk.h>

#include "../app.h"
#include "profile_dialog.h"
#include "profiles_icon_selection_box.h"
#include "window/accelerator_widget/orion_accelerator_entry.h"

struct _ProfileDialog
{
	AdwDialog parent;

    GtkEntry* profile_name_entry;
    ProfilesIconSelectionBox* profiles_icon_selection_box;
    GtkWidget* setting_profile_template;
    GtkDropDown* setting_profile_template_dropdown;
    OrionAcceleratorEntry* accelerator_entry;
    GtkButton* finish_button;

    orion::ProfileId id;
};


G_DEFINE_TYPE(ProfileDialog, profile_dialog, ADW_TYPE_DIALOG);

static void on_create_clicked(GtkButton* button, gpointer user_data);
static void on_edit_clicked(GtkButton* button, gpointer user_data);

static AdwDialog*
profile_dialog_new_create_internal(
    ProfileDialogCreateDoneCb done_cb,
    const char* title,
    const char* finish_button_label,
    bool show_profile_template_selection
)
{
    ProfileDialog* dialog = PROFILE_DIALOG(g_object_new(PROFILE_DIALOG_TYPE, NULL));

    if (done_cb) {
        auto* done_cb_ptr = new ProfileDialogCreateDoneCb(std::move(done_cb));
        g_object_set_data_full(
            G_OBJECT(dialog),
            "profile-dialog-create-done-cb",
            done_cb_ptr,
            [](gpointer data){ delete static_cast<ProfileDialogCreateDoneCb*>(data); }
        );
    }

    adw_dialog_set_title(ADW_DIALOG(dialog), title);
    gtk_button_set_label(dialog->finish_button, finish_button_label);

    gtk_widget_set_visible(GTK_WIDGET(dialog->setting_profile_template), show_profile_template_selection);
    gtk_widget_set_visible(GTK_WIDGET(dialog->setting_profile_template_dropdown), show_profile_template_selection);
    gtk_drop_down_set_selected(dialog->setting_profile_template_dropdown, 0); // default to "Clean Profile"

    g_signal_connect(dialog->finish_button, "clicked", G_CALLBACK(on_create_clicked), dialog);

	return ADW_DIALOG(dialog);
}

AdwDialog*
profile_dialog_new_create(ProfileDialogCreateDoneCb done_cb)
{
    return profile_dialog_new_create_internal(
        std::move(done_cb),
        _("New Profile"),
        _("Create"),
        true
    );
}

AdwDialog*
profile_dialog_new_import(ProfileDialogCreateDoneCb done_cb)
{
    return profile_dialog_new_create_internal(
        std::move(done_cb),
        _("Import Profile"),
        _("Import"),
        false
    );
}

AdwDialog*
profile_dialog_new_edit(orion::ProfileId id)
{
    ProfileDialog* dialog = PROFILE_DIALOG(g_object_new(PROFILE_DIALOG_TYPE, NULL));

    adw_dialog_set_title(ADW_DIALOG(dialog), _("Edit Profile"));
    gtk_button_set_label(dialog->finish_button, _("Edit"));
    g_signal_connect(dialog->finish_button, "clicked", G_CALLBACK(on_edit_clicked), dialog);
    dialog->id = id;

    gtk_widget_set_sensitive(GTK_WIDGET(dialog), FALSE);

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(dialog);
    global_core_api->get_profiles({id}, [wrapped_ref](orion::GetProfilesReply reply){
        VERIFY_LOG(reply.err);
        ProfileDialog* dialog = PROFILE_DIALOG(wrapped_ref->get());
        if(dialog)
        {
            if(reply.profiles.size() != 1)
            {
                orion::log_warning("couldn't get profile for editing, does it exist?");
                adw_dialog_close(ADW_DIALOG(dialog)); 
            }
            else
            {
                orion::ProfileDto dto = reply.profiles.at(0);
                gtk_editable_set_text(GTK_EDITABLE(dialog->profile_name_entry), dto.name.c_str());
                profiles_icon_selection_set_selection(dialog->profiles_icon_selection_box, dto.icon_name.c_str());
                orion_accelerator_entry_set_accelerator_from_string(dialog->accelerator_entry, dto.keyboard_shortcut.c_str());
                gtk_widget_set_sensitive(GTK_WIDGET(dialog), TRUE);
            }
            g_object_unref(dialog);
        }
    });

	return ADW_DIALOG(dialog);
}

static void
on_cancel_clicked(GtkButton* button, gpointer user_data)
{
    adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_create_clicked(GtkButton* button, gpointer user_data)
{
    ProfileDialog* dialog = PROFILE_DIALOG(user_data);
    const char* icon_name = profiles_icon_selection_get_current_selection(dialog->profiles_icon_selection_box);
    const char* name = gtk_editable_get_text(GTK_EDITABLE(dialog->profile_name_entry));
    if(icon_name != NULL && strlen(name)>0)
    {
        orion::CreateProfileQuery query;
        query.name = name;
        query.icon_name = icon_name;
        query.keyboard_shortcut = orion_accelerator_entry_get_accelerator_string(dialog->accelerator_entry);

        ProfileDialogCreateDoneCb done_cb;
        auto* done_cb_ptr = static_cast<ProfileDialogCreateDoneCb*>(
            g_object_get_data(G_OBJECT(dialog), "profile-dialog-create-done-cb")
        );
        if (done_cb_ptr) {
            done_cb = *done_cb_ptr;
        }

        orion::gtk::WeakRefWrapper dialog_weak(dialog);
        global_core_api->create_profile(query, [dialog_weak, done_cb = std::move(done_cb)](orion::CreateProfileReply reply){
            VERIFY_LOG(reply.err);

            if (done_cb) {
                done_cb(reply);
            }

            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;
            ProfileDialog* dialog = PROFILE_DIALOG(dialog_strong.get());
            adw_dialog_close(ADW_DIALOG(dialog));
        });
    }
}

static void
on_edit_clicked(GtkButton* button, gpointer user_data)
{
    ProfileDialog* dialog = PROFILE_DIALOG(user_data);

    const char* icon_name = profiles_icon_selection_get_current_selection(dialog->profiles_icon_selection_box);
    const char* name = gtk_editable_get_text(GTK_EDITABLE(dialog->profile_name_entry));
    if(icon_name != NULL && strlen(name)>0)
    {
        orion::UpdateProfileQuery query;
        query.id = dialog->id;
        query.update_name = true;
        query.name = name;
        query.update_icon_name = true;
        query.icon_name = icon_name;
        query.keyboard_shortcut = orion_accelerator_entry_get_accelerator_string(dialog->accelerator_entry);
        //TODO: this
        global_core_api->update_profile(query, [](orion::Error err){VERIFY_LOG(err);});
        adw_dialog_close(ADW_DIALOG(user_data));
    }
}

static void
on_profile_name_entry_changed(GtkEntry* entry, gpointer user_data)
{
    gtk_widget_set_sensitive(GTK_WIDGET(user_data), gtk_entry_get_text_length(entry)>0);
}

static void
profile_dialog_finalize(GObject* object)
{
    ProfileDialog* self = PROFILE_DIALOG(object);
    std::destroy_at(&self->id);
    G_OBJECT_CLASS(profile_dialog_parent_class)->finalize(object);
}

static void
profile_dialog_init(ProfileDialog* win)
{
    std::construct_at(&win->id);
    
    g_type_ensure(PROFILES_ICON_SELECTION_BOX_TYPE);

	gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect(win->profile_name_entry, "changed", G_CALLBACK(on_profile_name_entry_changed), win->finish_button);
}

static void
profile_dialog_class_init(ProfileDialogClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = profile_dialog_finalize;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/profile_dialog.ui");

    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, profile_name_entry);
    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, profiles_icon_selection_box);
    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, setting_profile_template);
    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, setting_profile_template_dropdown);
    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, accelerator_entry);
    gtk_widget_class_bind_template_child(widget_class, ProfileDialog, finish_button);

    gtk_widget_class_bind_template_callback(widget_class, on_cancel_clicked);
}
