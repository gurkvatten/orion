#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <common/src/log.h>

#include "app.h"
#include "window_list_popover.h"
#include "util/action_util.h"
#include "window/columnview/window_list_synced_devices_listview.h"

// TODO: rename this to WindowListDialog
struct _WindowListPopover
{
	AdwDialog parent;
    GtkListView* synced_devices_list_view;
};

G_DEFINE_TYPE(WindowListPopover, window_list_popover, ADW_TYPE_DIALOG)

// TODO: fix this hack
// This is a workaround - this stores the last window list popover needed to handle the "response" signal
static WindowListPopover* window_list_popover;

AdwDialog*
window_list_popover_new()
{
	auto popover = ADW_DIALOG(g_object_new(WINDOW_LIST_POPOVER_TYPE, NULL));
    window_list_popover = WINDOW_LIST_POPOVER(popover);
	return popover;
}

static void
on_view_stack_notify_visible_child(AdwViewStack* view_stack, GParamSpec* pspec, gpointer user_data)
{
    //gtk_popover_popdown(GTK_POPOVER(user_data));
    //gtk_popover_popup(GTK_POPOVER(user_data));
    gtk_widget_queue_resize(GTK_WIDGET(user_data));
    gtk_widget_queue_allocate(GTK_WIDGET(user_data));
}

static void
window_list_popover_init(WindowListPopover* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    
    g_signal_connect(win->synced_devices_list_view,"realize",G_CALLBACK(synced_devices_list_view_realize),NULL);
}

static void
window_list_popover_class_init(WindowListPopoverClass* klass)
{
    auto widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/window_list_popover.ui");

    gtk_widget_class_bind_template_child(widget_class, WindowListPopover, synced_devices_list_view);
    gtk_widget_class_bind_template_callback(widget_class, on_view_stack_notify_visible_child);
}