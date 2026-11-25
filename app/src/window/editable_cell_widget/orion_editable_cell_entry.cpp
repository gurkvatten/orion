#include <gtk/gtk.h>
#include <cstring>
#include <common/src/log.h>
#include <cstring>

#include "app.h"
#include "orion_editable_cell_entry.h"
#include "util/text_util.h"

struct _OrionEditableCellEntryClass
{
    GtkEntryClass parent_class;

    //TODO: rename this
    void (*finished) (OrionEditableCellEntry* entry, const char* text, gboolean cancelled);
};

struct _OrionEditableCellEntry
{
	GtkEntry parent;

    GString* string;

    gboolean editable;

    GtkEventControllerKey* key_controller;
};

enum {
    FINISHED,
    LAST_SIGNAL
};

static guint editable_cell_entry_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(OrionEditableCellEntry, orion_editable_cell_entry, GTK_TYPE_ENTRY)

void
orion_editable_cell_entry_set_text(OrionEditableCellEntry* entry, const char* text)
{
    g_string_assign(entry->string, text);
    gtk_entry_buffer_set_text(GTK_ENTRY_BUFFER(gtk_entry_get_buffer(GTK_ENTRY(entry))), text, strlen(text));
    gtk_widget_queue_resize(GTK_WIDGET(entry));
}

void
orion_editable_cell_entry_set_editable(OrionEditableCellEntry* entry, gboolean value)
{
    ORION_EDITABLE_CELL_ENTRY(entry)->editable = value;
    gtk_widget_set_can_focus(GTK_WIDGET(entry), value);
    gtk_widget_set_can_target(GTK_WIDGET(entry), value);
}

gboolean
orion_editable_cell_entry_get_editable(OrionEditableCellEntry* entry)
{
    return ORION_EDITABLE_CELL_ENTRY(entry)->editable;
}

gboolean
on_editable_entry_event_controller_key_pressed (
  GtkEventControllerKey* self,
  guint keyval,
  guint keycode,
  GdkModifierType state,
  gpointer user_data
)
{
    gtk_widget_queue_resize(GTK_WIDGET(user_data));
    //TODO: go through https://gitlab.gnome.org/GNOME/gtk/-/blob/main/gdk/gdkkeysyms.h and get a full list of needed chars
    if(keyval==GDK_KEY_Escape) 
    {
        const char* text = ORION_EDITABLE_CELL_ENTRY(user_data)->string->str;
        gtk_entry_buffer_set_text(gtk_entry_get_buffer(GTK_ENTRY(user_data)), text, strlen(text));
        gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))),NULL);
        g_signal_emit(ORION_EDITABLE_CELL_ENTRY(user_data), editable_cell_entry_signals[FINISHED], 0, text, TRUE);
        return true;
    }
    else
    {
        return false;
    }
}

static void
on_self_activate(OrionEditableCellEntry* entry, gpointer user_data)
{
    const char* text = gtk_entry_buffer_get_text(GTK_ENTRY_BUFFER(gtk_entry_get_buffer(GTK_ENTRY(entry))));
    g_string_assign(entry->string, text);
    gtk_window_set_focus(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(entry))),NULL);
    if (g_signal_has_handler_pending (entry, editable_cell_entry_signals[FINISHED], 0, TRUE))
    {
        gboolean cancelled = FALSE;
        g_signal_emit(entry, editable_cell_entry_signals[FINISHED], 0, text, cancelled);
    }
}

static void
on_self_state_flags_changed(OrionEditableCellEntry* entry, GtkStateFlags flags, gpointer user_data)
{
    //TODO: triggers on getting focus but does not properly update the cursor at the correct time
    GtkStateFlags new_flags = gtk_widget_get_state_flags(GTK_WIDGET(entry));
    bool old_focused = (bool)(GTK_STATE_FLAG_FOCUS_WITHIN&flags);
    bool new_focused = (bool)(GTK_STATE_FLAG_FOCUS_WITHIN&new_flags);
    if(new_focused&&(!old_focused)) //trigger on getting focus
    {
        //TODO: select whole text on interacting with entry?
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
    }
    if(old_focused&&(!new_focused))
    {
        //TODO: relatively dangerous approach, relies on this callback happening after activate one
        const char* text = entry->string->str;
        gtk_entry_buffer_set_text(GTK_ENTRY_BUFFER(gtk_entry_get_buffer(GTK_ENTRY(entry))), text, strlen(text));
        orion_editable_cell_entry_set_editable(ORION_EDITABLE_CELL_ENTRY(entry), false);
    }
}

OrionEditableCellEntry*
orion_editable_cell_entry_new(gboolean editable)
{
    OrionEditableCellEntry* entry = ORION_EDITABLE_CELL_ENTRY(g_object_new(ORION_EDITABLE_CELL_ENTRY_TYPE, NULL));
    orion_editable_cell_entry_set_editable(entry, editable);
	return entry;
}

static void
orion_editable_cell_entry_init(OrionEditableCellEntry* self)
{
    self->string = g_string_new(NULL);

    gtk_widget_set_name(GTK_WIDGET(self),"editable-cell-entry");

    self->key_controller = GTK_EVENT_CONTROLLER_KEY(gtk_event_controller_key_new());
    gtk_widget_add_controller(GTK_WIDGET(self),GTK_EVENT_CONTROLLER(self->key_controller));
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(self->key_controller),GTK_PHASE_CAPTURE);
    g_signal_connect(self->key_controller, "key-pressed", G_CALLBACK(on_editable_entry_event_controller_key_pressed), self);

    g_signal_connect(self, "state-flags-changed", G_CALLBACK(on_self_state_flags_changed), NULL);
    g_signal_connect(self, "activate", G_CALLBACK(on_self_activate), NULL);
}

static void
orion_editable_cell_entry_finalize(GObject* object)
{
    OrionEditableCellEntry* entry = ORION_EDITABLE_CELL_ENTRY(object);
    g_string_free(entry->string, TRUE);
    G_OBJECT_CLASS(orion_editable_cell_entry_parent_class)->finalize(object);
}

static void
orion_editable_cell_entry_measure(GtkWidget* self, GtkOrientation orientation, int for_size,
    int* minimum, int* natural,
    int* minimum_baseline, int* natural_baseline)
{
    GTK_WIDGET_CLASS(orion_editable_cell_entry_parent_class)->measure(self, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);

    if(orientation == GTK_ORIENTATION_HORIZONTAL)
    {
        if(ORION_EDITABLE_CELL_ENTRY(self)->string!=NULL)
        {
            PangoContext* context = gtk_widget_get_pango_context(self);
            int old_width = orion::text::get_text_pixel_width(ORION_EDITABLE_CELL_ENTRY(self)->string->str, context);
            int current_width = orion::text::get_text_pixel_width(gtk_editable_get_text(GTK_EDITABLE(self)), context);
            int width = std::max(old_width, current_width);
            *minimum = width;
            *natural = width;
        }
        else
        {
            *minimum = 0;
            *natural = 0;
        }
    }
}

static void
orion_editable_cell_entry_class_init(OrionEditableCellEntryClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = orion_editable_cell_entry_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    widget_class->measure = orion_editable_cell_entry_measure;
    
    guint n_params = 2;

    editable_cell_entry_signals[FINISHED] = g_signal_new("finished", G_OBJECT_CLASS_TYPE(object_class),
        GSignalFlags(G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION), G_STRUCT_OFFSET(OrionEditableCellEntryClass, finished), NULL, NULL, NULL,
        G_TYPE_NONE, n_params, G_TYPE_STRING, G_TYPE_BOOLEAN);

}
