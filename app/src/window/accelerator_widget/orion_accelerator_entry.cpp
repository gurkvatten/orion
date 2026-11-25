#include <gtk/gtk.h>
#include <cstring>
#include <common/src/log.h>
#include <common/src/defer.h>

#include "app.h"
#include "orion_accelerator_entry.h"

struct _OrionAcceleratorEntryClass
{
  GtkEntryClass        parent_class;

  void (* finished)  (OrionAcceleratorEntry*, const char*);
};

struct _OrionAcceleratorEntry
{
	GtkEntry parent;

    guint keyval;
    GdkModifierType keystate;
};

enum {
    FINISHED,
    LAST_SIGNAL
};

static guint accelerator_entry_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(OrionAcceleratorEntry, orion_accelerator_entry, GTK_TYPE_ENTRY)

std::string
orion_accelerator_entry_get_accelerator_string(OrionAcceleratorEntry* entry)
{
    if (entry->keyval == 0) {
        return std::string();
    }
    char* accel_name = gtk_accelerator_name(entry->keyval, entry->keystate);
    if (!accel_name) {
        return std::string();
    }
    defer([accel_name]{ g_free(accel_name); });
    return std::string(gtk_accelerator_name(entry->keyval, entry->keystate));
}

void
orion_accelerator_entry_set_accelerator_from_string(OrionAcceleratorEntry* entry, const char* string)
{
    if (strlen(string)==0) {
        entry->keyval = 0;
        entry->keystate = GDK_NO_MODIFIER_MASK;
        gtk_editable_set_text(GTK_EDITABLE(entry), "");
    } else {
        guint keyval;
        GdkModifierType keystate;
        gtk_accelerator_parse(string, &keyval, &keystate);
        if(keyval == 0 && keystate == 0) {
            orion::log_error("OrionAcceleratorEntry failed to parse initialization string: \"%s\"", string);
        } else {
            char* friendly_string = gtk_accelerator_get_label(keyval, keystate);
            gtk_editable_set_text(GTK_EDITABLE(entry), friendly_string);
            g_free(friendly_string);
            entry->keystate = keystate;
            entry->keyval = keyval;
        }
    }
}

static gboolean
keyval_is_acceptable(guint keyval)
{
//TODO: go through https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gdk/gdkkeysyms.h and get a full list of needed chars
    if(gdk_keyval_is_upper(keyval)^gdk_keyval_is_lower(keyval)) return true; //these both return true if keyval cant have case, thus this only detects letters
    if((keyval>=GDK_KEY_F1)&&(keyval<=GDK_KEY_F12)) return true; //matches mostly F keys, also some GDK_KEY_L#
    if((keyval>=GDK_KEY_space)&&(keyval<=GDK_KEY_at)) return true; //matches some ascii special chars, all nums
    else return false;
}

static gboolean
on_event_controller_key_pressed(
    GtkEventControllerKey* self,
    guint keyval,
    guint keycode,
    GdkModifierType state,
    gpointer user_data)
{
    if(keyval_is_acceptable(keyval)) 
    {
        OrionAcceleratorEntry* entry = ORION_ACCELERATOR_ENTRY(user_data);
        entry->keyval = keyval;
        entry->keystate = state;

        //TODO: use keyval or keycode?
        gchar* string = gtk_accelerator_get_label(keyval, state);
        gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(entry)),string,strlen(string));
        g_free(string);
        gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(entry))),NULL); //not sure if this is safe or not
        return true;
    }
    else
    {
        return false;
    }
}

static void
on_self_state_flags_changed(OrionAcceleratorEntry* entry, GtkStateFlags flags, gpointer user_data)
{
    //TODO: triggers on getting focus but does not properly update the cursor at the correct time
    GtkStateFlags new_flags = gtk_widget_get_state_flags(GTK_WIDGET(entry));
    bool old_focused = (bool)(GTK_STATE_FLAG_FOCUS_WITHIN&flags);
    bool new_focused = (bool)(GTK_STATE_FLAG_FOCUS_WITHIN&new_flags);
    if(new_focused&&(!old_focused)) //trigger on getting focus
    {
        gtk_editable_set_position(GTK_EDITABLE(entry),-1); //sets cursor position to the end of the line
    }
}

static void
on_self_icon_press(OrionAcceleratorEntry* entry, GtkEntryIconPosition icon_pos, gpointer user_data)
{
    ORION_ACCELERATOR_ENTRY(entry)->keyval = 0;
    ORION_ACCELERATOR_ENTRY(entry)->keystate = GDK_NO_MODIFIER_MASK;
    gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(entry)),"",0);
}

OrionAcceleratorEntry*
orion_accelerator_entry_new()
{
	return ORION_ACCELERATOR_ENTRY(g_object_new(ORION_ACCELERATOR_ENTRY_TYPE, NULL));
}

static void
orion_accelerator_entry_init(OrionAcceleratorEntry* self)
{
    gtk_entry_set_input_purpose(GTK_ENTRY(self), GTK_INPUT_PURPOSE_TERMINAL);
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(self), GTK_ENTRY_ICON_SECONDARY, "cross-large-symbolic");

    GtkEventControllerKey* key_controller = GTK_EVENT_CONTROLLER_KEY(gtk_event_controller_key_new());
    gtk_widget_add_controller(GTK_WIDGET(self),GTK_EVENT_CONTROLLER(key_controller));
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(key_controller),GTK_PHASE_CAPTURE);
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(on_event_controller_key_pressed), self);

    g_signal_connect(self, "state-flags-changed", G_CALLBACK(on_self_state_flags_changed), NULL);
    g_signal_connect(self, "icon-press", G_CALLBACK(on_self_icon_press), NULL);
}

static void
orion_accelerator_entry_class_init(OrionAcceleratorEntryClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    
    klass->finished = NULL;
    //GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    accelerator_entry_signals[FINISHED] = g_signal_new("finished", G_OBJECT_CLASS_TYPE(object_class),
        GSignalFlags(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET(OrionAcceleratorEntryClass, finished), NULL, NULL, NULL,
        G_TYPE_NONE, 1, G_TYPE_STRING);
}
