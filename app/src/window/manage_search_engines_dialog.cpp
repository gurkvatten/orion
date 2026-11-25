#include <gdk/gdk.h>
#include <common/src/log.h>

#include "../app.h"
#include "manage_search_engines_dialog.h"
#include "columnview/search_engines_columnview.h"
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include "app.h"
#include "util/action_util.h"
#include "window/add_search_engine_dialog.h"
#include "util/dialog_util.h"

struct _ManageSearchEnginesDialog
{
	AdwDialog parent;
    GtkColumnView* search_engines_column_view;
    GtkPopoverMenu* passwords_popover_menu;
};

G_DEFINE_TYPE(ManageSearchEnginesDialog, manage_search_engines_dialog, ADW_TYPE_DIALOG)

AdwDialog*
manage_search_engines_dialog_new()
{
	return ADW_DIALOG(g_object_new(MANAGE_SEARCH_ENGINES_DIALOG_TYPE, NULL));
}

void
manage_search_engines_dialog_set_popover_menu(ManageSearchEnginesDialog* self, GtkPopoverMenu* popover_menu)
{
    if (self->passwords_popover_menu) {
        gtk_widget_unparent(GTK_WIDGET(self->passwords_popover_menu));
    }
    self->passwords_popover_menu = popover_menu;
    gtk_widget_set_parent(GTK_WIDGET(popover_menu), GTK_WIDGET(self));
}

static void
manage_search_engines_dialog_dispose(GObject* gobject)
{
    ManageSearchEnginesDialog* self = MANAGE_SEARCH_ENGINES_DIALOG(gobject);
    
    if (self->passwords_popover_menu) {
        gtk_widget_unparent(GTK_WIDGET(self->passwords_popover_menu));
        self->passwords_popover_menu = NULL;
    }

    G_OBJECT_CLASS(manage_search_engines_dialog_parent_class)->dispose(gobject);
}

static void
manage_search_engines_dialog_init(ManageSearchEnginesDialog* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));
    g_signal_connect(win->search_engines_column_view, "realize", G_CALLBACK(search_engines_column_view_realize), win);    
    g_signal_connect(win->search_engines_column_view, "map", G_CALLBACK(search_engines_column_view_map), win);
    g_signal_connect(win->search_engines_column_view, "unmap", G_CALLBACK(search_engines_column_view_unmap), win);
}

static void
on_done_button_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_close(ADW_DIALOG(user_data));
}

static void
on_plus_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = add_search_engine_dialog_new(orion::SearchEngineDto{});
    adw_dialog_present(dialog, GTK_WIDGET(button));
}

static void
on_minus_button_clicked(GtkButton* button, gpointer user_data)
{
    ManageSearchEnginesDialog* dialog = MANAGE_SEARCH_ENGINES_DIALOG(user_data);
    
    std::vector<orion::SearchEngineId> selected_ids;
    search_engines_column_view_get_selected_search_engine_ids(dialog->search_engines_column_view, &selected_ids);

    orion::gtk::WeakRefWrapper dialog_weak(dialog);

    global_core_api->delete_search_engines(selected_ids, [dialog_weak](orion::Error err){
        VERIFY_LOG(err);
    });
}

static void 
manage_search_engines_dialog_class_init(ManageSearchEnginesDialogClass* klass)
{
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/manage_search_engines_dialog.ui");

    G_OBJECT_CLASS(klass)->dispose = manage_search_engines_dialog_dispose;

    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_done_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_plus_button_clicked);
    gtk_widget_class_bind_template_callback(GTK_WIDGET_CLASS(klass), on_minus_button_clicked);

    gtk_widget_class_bind_template_child(GTK_WIDGET_CLASS(klass), ManageSearchEnginesDialog, search_engines_column_view);
}
