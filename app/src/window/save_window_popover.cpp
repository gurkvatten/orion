#include <gtk/gtk.h>
#include <adwaita.h>
#include <iostream>
#include <common/src/log.h>
#include <core/dto/window_dto.h>
#include <ranges>

#include "app.h"
#include "save_window_popover.h"
#include "util/action_util.h"
#include "window/main_window/main_window.h"
#include "window/accelerator_widget/orion_accelerator_entry.h"
#include "util/text_util.h"

struct _SaveWindowPopover
{
	GtkPopover parent;

	GtkMenuButton* emoji_chooser_button;
#ifdef ENABLE_ADW_17_API
	AdwToggleGroup* icon_toggle_group;
    AdwToggle* emoji_toggle;
#endif
    
	GtkEntry* name_entry;
    OrionAcceleratorEntry* shortcut_entry;
    orion::WindowId window_id;
    bool delete_window_on_cancel;
};

G_DEFINE_TYPE(SaveWindowPopover, save_window_popover, GTK_TYPE_POPOVER)

const guint global_emoji_icon_toggle_index = 8; // index in icon toggle group

GtkPopover*
save_window_popover_new()
{
	return GTK_POPOVER(g_object_new(SAVE_WINDOW_POPOVER_TYPE, NULL));
}

void
save_window_popover_set_name(SaveWindowPopover* self, const char* name)
{
	gtk_editable_set_text(GTK_EDITABLE(self->name_entry), name);
}

void
save_window_popover_select_name(SaveWindowPopover* self)
{
	gtk_editable_select_region(GTK_EDITABLE(self->name_entry), 0, -1); //select all text in entry
}

void
save_window_popover_set_keyboard_shortcut(SaveWindowPopover* self, const char* shortcut)
{
    orion_accelerator_entry_set_accelerator_from_string(self->shortcut_entry, shortcut);
}

void
save_window_popover_set_window_id(SaveWindowPopover* self, orion::WindowId window_id)
{
    self->window_id = window_id;
}

void
save_window_popover_set_delete_window_on_cancel(SaveWindowPopover* self, bool delete_window_on_cancel)
{
    self->delete_window_on_cancel = delete_window_on_cancel;
}

//returns UINT_MAX if fail
static uint
save_window_popover_find_symbolic_icon_index(SaveWindowPopover* self, const char* icon_name)
{
    for(guint i = 0; i<global_emoji_icon_toggle_index; i++)
    {
        //needs to disable use markup to get back the full source string
        AdwToggle* toggle = adw_toggle_group_get_toggle(self->icon_toggle_group, i);
        const char* toggle_icon_name = adw_toggle_get_icon_name(toggle);
        if(strcmp(icon_name, toggle_icon_name) == 0) return i;
    }
    return UINT_MAX;
}


#ifdef ENABLE_ADW_17_API

static gboolean
get_emoji_chooser_button_sensitivity(SaveWindowPopover* popover, guint index)
{
    return index == global_emoji_icon_toggle_index;
}

static void
on_emoji_picked(AdwToggle* toggle, gchar* emoji, GtkEmojiChooser* emoji_chooser)
{
    adw_toggle_set_label(toggle, emoji); //TODO: more functionality here
}

void
save_window_popover_set_icon_name(SaveWindowPopover* self, const char* icon_name)
{
    if (orion::text::text_is_emoji(icon_name, pango_cairo_font_map_get_default()))
    {
        adw_toggle_set_label(self->emoji_toggle, icon_name);
        adw_toggle_group_set_active(self->icon_toggle_group, global_emoji_icon_toggle_index);
    }
    else
    {
        uint index = save_window_popover_find_symbolic_icon_index(self, icon_name);
        if (index != UINT_MAX) {
            adw_toggle_group_set_active(self->icon_toggle_group, index);
        } else {
            orion::log_error("save_window_popover_set_icon_name: could not match icon to any possible selection: icon_name='%s'", icon_name);
        }
    }
}

const char*
save_window_popover_get_icon_name(SaveWindowPopover* self)
{
    guint active_index = adw_toggle_group_get_active(self->icon_toggle_group);
    if (active_index == GTK_INVALID_LIST_POSITION) return NULL;

    AdwToggle* toggle = adw_toggle_group_get_toggle(self->icon_toggle_group, active_index);
    
    if (active_index == adw_toggle_group_get_n_toggles(self->icon_toggle_group)-1) {
        // emoji
        return adw_toggle_get_label(toggle);
    }
    else
    {
        return adw_toggle_get_icon_name(toggle);
    }
}
#endif // ENABLE_ADW_17_API

static void
on_cancel_clicked(GtkButton* button, SaveWindowPopover* popover)
{
    if (popover->delete_window_on_cancel) {
        orion::log_debug("delete_window_on_cancel is true, deleting window with id %lld", popover->window_id.value);
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(popover)));
        if (!win) {
            orion::log_warning("on_cancel_clicked: could not get main window");
            return;
        }
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);
        orion::WindowId window_id = popover->window_id;
        orion::gtk::WeakRefWrapper win_weak(win);

        global_core_api->get_window_ids({.profile_id=profile_id}, [win_weak,window_id](orion::GetWindowIdsReply reply){
            VERIFY_LOG(reply.err);

            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* main_window = MAIN_WINDOW(win_strong.get());

            // Try to find another window to switch to
            orion::WindowId new_window_id{};
            for (const auto& id : std::views::reverse(reply.ids)) {
                if (id != window_id) {
                    new_window_id = id;
                    break;
                }
            }

            if (new_window_id.value) {
                orion::log_debug(
                    "on_cancel_clicked: switching from window id %lld to %lld", 
                    window_id.value, new_window_id.value
                );
                main_window_switch_to_window_id_in_place(main_window, new_window_id);
            }

            global_core_api->delete_windows({window_id}, [](orion::Error err){
                VERIFY_LOG(err);
            });
        });
    }

    gtk_popover_popdown(GTK_POPOVER(popover));
}

static void
on_save_clicked(GtkButton* button, SaveWindowPopover* win)
{
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(win)));
    if (!main_window) {
        orion::log_warning("on_save_clicked: could not get main window");
        return;
    }

    if (win->window_id == orion::WindowId(0)) {
        orion::log_warning("on_save_clicked: window_id is not set");
        return;
    }

    const char* name = gtk_editable_get_text(GTK_EDITABLE(win->name_entry));
    if(name==NULL || strlen(name)==0) {
        orion::log_warning("on_save_clicked: name is empty");
        return;
    }
    gtk_popover_popdown(GTK_POPOVER(win));

    orion::UpdateWindowQuery query{};
    query.id = win->window_id;

    query.is_tab_group = false; // saving a window always makes it a normal window
    query.name = std::string(name);
    query.icon_name = save_window_popover_get_icon_name(win);
    query.keyboard_shortcut = orion_accelerator_entry_get_accelerator_string(win->shortcut_entry);

    query.update_is_tab_group = true;
    query.update_name = true;
    query.update_icon_name = true;
    query.update_keyboard_shortcut = true;

    global_core_api->update_window(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
}

static void
save_window_popover_finalize(GObject* object)
{
    SaveWindowPopover* self = SAVE_WINDOW_POPOVER(object);
    std::destroy_at(&self->window_id);
    G_OBJECT_CLASS(save_window_popover_parent_class)->finalize(object);
}

static void
save_window_popover_init(SaveWindowPopover* self)
{
    std::construct_at(&self->window_id);
	gtk_widget_init_template(GTK_WIDGET(self));
}

static void
save_window_popover_class_init(SaveWindowPopoverClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = save_window_popover_finalize;

	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/save_window_popover.ui");
	#ifdef ENABLE_ADW_17_API
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SaveWindowPopover, icon_toggle_group);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SaveWindowPopover, emoji_chooser_button);
	gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SaveWindowPopover, emoji_toggle);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_emoji_picked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), get_emoji_chooser_button_sensitivity);
	#endif
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SaveWindowPopover, name_entry);
    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), SaveWindowPopover, shortcut_entry);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_save_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_cancel_clicked);
}
