#include <gtk/gtk.h>

#include "app.h"
#include "profiles_icon_selection_box.h"
#include "start_page_settings_popover.h"
#include "util/text_util.h"

struct _ProfilesIconSelectionBox
{
	GtkBox parent;

    AdwToggleGroup* toggle_group;
    GtkLabel* emoji_label;
    GtkEmojiChooser* emoji_chooser;
    GtkMenuButton* emoji_chooser_button;
    gulong selection_handler_id;
    gulong emoji_chooser_handler_id;
    orion::ProfileId id;
};

G_DEFINE_TYPE(ProfilesIconSelectionBox, profiles_icon_selection_box, GTK_TYPE_BOX)

//returns UINT_MAX if fail
static uint
profiles_icon_selection_match_circle_icon(ProfilesIconSelectionBox* self, const char* icon)
{
    for(int i = 0; i<8; i++)
    {
        //needs to disable use markup to get back the full source string
        GtkLabel* label = GTK_LABEL(adw_toggle_get_child(adw_toggle_group_get_toggle(self->toggle_group, i)));
        const char* label_text = gtk_label_get_label(label);
        if(strcmp(icon, label_text) == 0)
        {
            return i;
        }
    }
    return UINT_MAX;
}

void
profiles_icon_selection_set_selection(ProfilesIconSelectionBox* self, const char* icon)
{
    g_return_if_fail(PROFILES_IS_ICON_SELECTION_BOX(self));
    //detects if emoji, if so, set last toggle, else match with previous toggles, error if none match
    if(orion::text::text_is_emoji(icon, pango_cairo_font_map_get_default()))
    {
        gtk_label_set_text(self->emoji_label, icon);
        adw_toggle_group_set_active(self->toggle_group, 8);
    }
    else
    {
        uint index = profiles_icon_selection_match_circle_icon(self, icon);
        if(index != UINT_MAX)
        {
            adw_toggle_group_set_active(self->toggle_group, index);
        }
        else
        {
            orion::log_error("profiles_icon_selection_set_selection: couldn't match icon to any possible selection,\n"
                "received icon name: %s", icon);
        }
    }
}

const char*
profiles_icon_selection_get_current_selection(ProfilesIconSelectionBox* self)
{
    guint active_index = adw_toggle_group_get_active(self->toggle_group);
    if(active_index == GTK_INVALID_LIST_POSITION)
    {
        return NULL;
    }
    else
    {
        GtkLabel* label = GTK_LABEL(adw_toggle_get_child(adw_toggle_group_get_toggle(self->toggle_group, active_index)));
        return gtk_label_get_label(label);
    }
}

static void
update_profile_from_id(ProfilesIconSelectionBox* self)
{
    orion::UpdateProfileQuery query;
    query.id = self->id;
    query.update_icon_name = true;
    query.icon_name = profiles_icon_selection_get_current_selection(self);
    global_core_api->update_profile(query, [](orion::Error err){VERIFY_LOG(err);});
}

static void
on_toggle_group_notify_active(AdwToggleGroup* toggle_group, GParamSpec* pspec, gpointer user_data)
{
    ProfilesIconSelectionBox* self = PROFILES_ICON_SELECTION_BOX(user_data);
    update_profile_from_id(self);
}

static void
on_emoji_chooser_emoji_picked_bound(AdwToggleGroup* toggle_group, GParamSpec* pspec, gpointer user_data)
{
    ProfilesIconSelectionBox* self = PROFILES_ICON_SELECTION_BOX(user_data);
    update_profile_from_id(self);
}

void
profiles_icon_selection_bind_to_profile_id(ProfilesIconSelectionBox* self, orion::ProfileId id)
{
    self->id = id;

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);
    global_core_api->get_profiles({id}, [wrapped_ref](orion::GetProfilesReply reply){
        VERIFY_LOG(reply.err);
        ProfilesIconSelectionBox* self = PROFILES_ICON_SELECTION_BOX(wrapped_ref->get());
        if(self)
        {
            if(reply.profiles.size() == 1)
            {
                profiles_icon_selection_set_selection(self, reply.profiles.at(0).icon_name.c_str());
            }
            else
            {
                orion::log_debug("ProfilesIconSelectionBox profiles_icon_selection_bind_to_profile_id:");
                orion::log_error("Tried to load profile icon but got none from backend");
            }
            g_signal_connect(self->emoji_chooser, "emoji-picked", G_CALLBACK(on_emoji_chooser_emoji_picked_bound), self);
            g_signal_connect(self->toggle_group, "notify::active", G_CALLBACK(on_toggle_group_notify_active), self);
            g_object_unref(self);
        }
    });
}

static void
profiles_icon_selection_box_dispose(GObject* gobject)
{
    gtk_widget_dispose_template(GTK_WIDGET(gobject), PROFILES_ICON_SELECTION_BOX_TYPE);
    G_OBJECT_CLASS(profiles_icon_selection_box_parent_class)->dispose(gobject);
}

ProfilesIconSelectionBox*
profiles_icon_selection_box_new()
{
	return PROFILES_ICON_SELECTION_BOX(g_object_new(PROFILES_ICON_SELECTION_BOX_TYPE, NULL));
}

static void
on_emoji_chooser_emoji_picked(GtkEmojiChooser* self, gchar* text, gpointer user_data)
{
    gtk_label_set_text(GTK_LABEL(user_data), text);
}

static void
profiles_icon_selection_box_finalize(GObject* gobject)
{
    ProfilesIconSelectionBox* self = PROFILES_ICON_SELECTION_BOX(gobject);
    std::destroy_at(&self->id);
    G_OBJECT_CLASS(profiles_icon_selection_box_parent_class)->finalize(gobject);
}

static void
profiles_icon_selection_box_init(ProfilesIconSelectionBox* win)
{
    std::construct_at(&win->id);

	gtk_widget_init_template(GTK_WIDGET(win));

    g_signal_connect(win->emoji_chooser, "emoji-picked", G_CALLBACK(on_emoji_chooser_emoji_picked), win->emoji_label);
    
}

static void
profiles_icon_selection_box_class_init(ProfilesIconSelectionBoxClass* klass)
{
    G_OBJECT_CLASS(klass)->dispose = profiles_icon_selection_box_dispose;
    G_OBJECT_CLASS(klass)->finalize = profiles_icon_selection_box_finalize;
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/profiles_icon_selection_box.ui");
    
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ProfilesIconSelectionBox, toggle_group);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ProfilesIconSelectionBox, emoji_label);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ProfilesIconSelectionBox, emoji_chooser);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ProfilesIconSelectionBox, emoji_chooser_button);
}
