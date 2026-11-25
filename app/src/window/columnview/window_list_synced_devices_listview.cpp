#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include <common/src/defer.h>

#include "app.h"
#include "window_list_synced_devices_listview.h"
#include "data_item/g_string_and_icon_name_item.h"

//used in search_completion_popover.cpp

static void
synced_devices_factory_setup(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label),PANGO_ELLIPSIZE_END);

    GtkWidget* image = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(image),20);

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_append(GTK_BOX(box),image);
    gtk_box_append(GTK_BOX(box),label);

	GtkWidget* tree_expander = gtk_tree_expander_new();
	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(tree_expander), box);
	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), tree_expander);
}

static void
synced_devices_factory_bind(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));

	GtkBox* box = GTK_BOX(gtk_widget_get_first_child(GTK_WIDGET(tree_expander)));

    GtkImage* image = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
    GtkLabel* label = GTK_LABEL(gtk_widget_get_next_sibling(GTK_WIDGET(image)));

    gpointer item = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_tree_expander_set_list_row(tree_expander, GTK_TREE_LIST_ROW(item));
    while(GTK_IS_TREE_LIST_ROW(item))
    {
        defer([item]{ g_object_unref(item); });
	    item = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(item));
    }
    defer([item]{ g_object_unref(item); });

    GStringAndIconNameItem* data_item = G_STRING_AND_ICON_NAME_ITEM(item);
    
    gtk_image_set_from_icon_name(image,g_string_and_icon_name_item_get_icon_name(data_item));
    GString* string = g_string_and_icon_name_item_get_string(data_item);
    
    if(string!=NULL)gtk_label_set_text(label, string->str);
}

static void
user_data_destroy(void* data)
{
	//TODO: figure out whether I need to do anything proper here
}

GListModel*
synced_devices_tree_list_model_create(gpointer item, gpointer data)
{
	if (g_string_and_icon_name_item_get_item_type(G_STRING_AND_ICON_NAME_ITEM(item)) == GStringAndIconNameType::folder) //TODO: proper data loading
	{
	    GListStore* store = g_list_store_new(G_TYPE_OBJECT);
        GString* string;
        for(int i = 1; i<4; i++)
        {
            string = g_string_new("test");
            g_string_printf(string,"Window %d",i);
            gpointer item = g_string_and_icon_name_item_new(string, "window-flat-symbolic", GStringAndIconNameType::entry);
            g_list_store_append(store, item);
            g_object_unref(item);
        }
        return G_LIST_MODEL(store);
	}
	else
	{
		return NULL;
	}
}

void
synced_devices_list_view_realize(GtkListView* synced_devices_list_view, gpointer user_data) //TODO: init the model for this properly
{
	GListStore* root = g_list_store_new(G_TYPE_OBJECT);
    gpointer item = g_string_and_icon_name_item_new(
            g_string_new("My iPhone"),
            "smartphone2-symbolic",
            GStringAndIconNameType::entry
        );
	g_list_store_append(root, item);
    g_object_unref(item);
    item = g_string_and_icon_name_item_new(
            g_string_new("Kids iPad"),
            "tablet-symbolic",
            GStringAndIconNameType::entry
        );
	g_list_store_append(root, item);
    g_object_unref(item);
    item = g_string_and_icon_name_item_new(
            g_string_new("Mac Mini"),
            "computer-symbolic",
            GStringAndIconNameType::folder
        );
	g_list_store_append(root, item);
    g_object_unref(item);
    item = g_string_and_icon_name_item_new(
            g_string_new("MacBook Air"),
            "phonelink-symbolic",
            GStringAndIconNameType::folder
        );
	g_list_store_append(root, item);
    g_object_unref(item);

	GtkTreeListModel* model = gtk_tree_list_model_new(G_LIST_MODEL(root), FALSE, TRUE, synced_devices_tree_list_model_create, NULL, user_data_destroy);

	GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(model));
	gtk_single_selection_set_autoselect(selection, TRUE);
	GtkListItemFactory* factory;
	gtk_list_view_set_model(synced_devices_list_view, GTK_SELECTION_MODEL(selection));

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(synced_devices_factory_setup), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(synced_devices_factory_bind), NULL);
    gtk_list_view_set_factory(GTK_LIST_VIEW(synced_devices_list_view),factory);
}