#include <gtk/gtk.h>

#include "search_completion_popover.h"

#include <common/src/log.h>
#include <common/src/defer.h>

#include "app.h"

#include "data_item/wrapping_item.h"
#include "data_item/search_completion_columnview_item_full.h"

struct _SearchCompletionPopover
{
	GtkPopover parent;

    GtkColumnView* columnview;
};

G_DEFINE_TYPE(SearchCompletionPopover, search_completion_popover, GTK_TYPE_POPOVER)

GtkPopover*
search_completion_popover_new()
{
	return GTK_POPOVER(g_object_new(SEARCH_COMPLETION_POPOVER_TYPE, NULL));
}

//TODO: implement proper hiding behaviour tied to the address entry in our main window
/*
static void
on_search_completion_columnview_activate(GtkColumnView* columnview, guint position, gpointer user_data)
{
    gtk_popover_popdown(GTK_POPOVER(user_data));
}
*/

//TODO: possibly use these signals to transfer focus?
/*
gboolean
on_list_view_keynav_failed(GtkWidget* self, GtkDirectionType direction, gpointer user_data)
{
    orion::log_debug("keynav failed %i", direction);
    //when returning false, keynav should be handled by parent widget
    switch(direction)
    {
        case GTK_DIR_TAB_FORWARD:
            return false;
        case GTK_DIR_DOWN:
            orion::log_debug("test");
            return false;
        default:
            return true;
    }
}

static void
on_list_view_move_focus(GtkWidget* self, GtkDirectionType direction, gpointer user_data)
{
    orion::log_debug("move focus %i", direction);
}
*/

static void
search_completion_popover_init(SearchCompletionPopover* win)
{
	gtk_widget_init_template(GTK_WIDGET(win));

    //g_signal_connect(win->search_completion_columnview, "realize", G_CALLBACK(search_completion_column_view_realize), NULL);
    //GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
    //TODO: completely remove these
    // g_signal_connect(win->top_hits_list_view, "realize", G_CALLBACK(top_hits_list_view_realize), size_group);
    // g_signal_connect(win->suggestions_list_view, "realize", G_CALLBACK(suggestions_list_view_realize), size_group);
    // g_signal_connect(win->bookmarks_list_view, "realize", G_CALLBACK(bookmarks_list_view_realize), size_group);
    // g_signal_connect_data(win->history_list_view, "realize", G_CALLBACK(history_list_view_realize), size_group,
    //[](gpointer data, GClosure* closure){g_object_unref(data);}, G_CONNECT_DEFAULT);
    

    
    //TODO: implement proper hiding behaviour tied to the address entry in our main window
    //g_signal_connect(win->search_completion_columnview, "activate", G_CALLBACK(on_search_completion_columnview_activate), win);
}

// static void
// header_factory_setup(GtkSignalListItemFactory* factory, GtkListHeader* header)
// {

// }

// static void
// header_factory_bind(GtkSignalListItemFactory* factory, GtkListHeader* header, SearchCompletionPopover* self)
// {
// }

static void
header_setup(GtkSignalListItemFactory* factory, GtkListItem* list_item)
{
    GtkWidget* label_widget = gtk_label_new(NULL);
    GtkLabel* label = GTK_LABEL(label_widget);
    gtk_label_set_ellipsize(label, PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(label,0.95);
    gtk_widget_set_size_request(label_widget,160,-1);
    gtk_widget_add_css_class(label_widget, "bold-text");
    gtk_widget_add_css_class(label_widget, "dimmed-text");
    gtk_list_item_set_child(list_item, label_widget);
}

static void
header_bind(GtkSignalListItemFactory* factory, GtkListItem* list_item)
{
    WrappingItem* wrapping_item = APP_WRAPPING_ITEM(gtk_list_item_get_item(list_item));
    GtkStringObject* string_object = GTK_STRING_OBJECT(wrapping_item_get_wrapper_data(wrapping_item));

    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(list_item));
    if(string_object)
    {
        gtk_label_set_label(label, gtk_string_object_get_string(string_object));
    }
    else
    {
        gtk_label_set_label(label, NULL);
    }
}

static void
item_setup(GtkSignalListItemFactory* factory, GtkListItem* list_item)
{
    GtkWidget* labelA = gtk_label_new(NULL);
    GtkWidget* labelB = gtk_label_new(NULL);
    gtk_widget_add_css_class(labelA, "bold-text");
    gtk_widget_add_css_class(labelB, "dimmed-text");
    
    GtkWidget* image = gtk_image_new();
    gtk_image_set_pixel_size(GTK_IMAGE(image),25);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_widget_set_size_request(box,-1,30);
    gtk_box_append(GTK_BOX(box),image);
    gtk_box_append(GTK_BOX(box),labelA);
    gtk_box_append(GTK_BOX(box),labelB);

}

static void
item_bind(GtkSignalListItemFactory* factory, GtkListItem* list_item)
{
    WrappingItem* wrapping_item = APP_WRAPPING_ITEM(gtk_list_item_get_item(list_item));
    SearchCompletionColumnviewItemFull* data_item = SEARCH_COMPLETION_COLUMNVIEW_ITEM_FULL(wrapping_item_get_wrapper_data(wrapping_item));


	GtkBox* box = GTK_BOX(gtk_list_item_get_child(list_item));

    GtkImage* image = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
    GtkLabel* labelA = GTK_LABEL(gtk_widget_get_next_sibling(GTK_WIDGET(image)));
    GtkLabel* labelB = GTK_LABEL(gtk_widget_get_next_sibling(GTK_WIDGET(labelA)));

    gtk_image_set_from_icon_name(image,search_completion_columnview_item_full_get_icon_name(data_item));

    GString* string = search_completion_columnview_item_full_get_website(data_item);
    if(string!=NULL)gtk_label_set_text(labelA, string->str);
    
    string = search_completion_columnview_item_full_get_address(data_item);
    if(string!=NULL)
    {
        string = g_string_new(string->str);
        g_string_prepend(string, "— ");
        if(string!=NULL) gtk_label_set_text(labelB, string->str);
        g_string_free(string,true);
    }
}

static void
on_items_changed(GListModel* model, guint pos, guint added, guint removed, gpointer self)
{
    if(added != removed)
    {
        gtk_popover_present(GTK_POPOVER(self));
    }
}

static void
search_completion_popover_root(GtkWidget* widget)
{
    SearchCompletionPopover* self = SEARCH_COMPLETION_POPOVER(widget);
    GTK_WIDGET_CLASS(search_completion_popover_parent_class)->root(widget);

    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(header_setup), NULL);
        g_signal_connect(factory, "setup", G_CALLBACK(header_bind), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new(NULL, factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_append_column(self->columnview, column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(item_setup), NULL);
        g_signal_connect(factory, "setup", G_CALLBACK(item_bind), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new(NULL, factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_append_column(self->columnview, column);
    }

    (void)on_items_changed;
    //TODO: setup our model -- will need to do dynamically, not sure how exactly
    //g_signal_connect(model, "items-changed", G_CALLBACK(on_items_changed), self);
}

static void
search_completion_popover_class_init(SearchCompletionPopoverClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = search_completion_popover_root;

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/search_completion_popover.ui");

    gtk_widget_class_bind_template_child(widget_class, SearchCompletionPopover, columnview);
    // gtk_widget_class_bind_template_callback(widget_class, header_factory_setup);
    // gtk_widget_class_bind_template_callback(widget_class, header_factory_setup);
}
