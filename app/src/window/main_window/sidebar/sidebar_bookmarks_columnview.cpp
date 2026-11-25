#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "sidebar.h"
#include "sidebar_bookmarks_columnview.h"
#include "data_item/bookmarks_columnview_item.h"
#include "util/click_util.h"
#include "util/favicon_util.h"
#include "util/menu_util.h"
#include "util/click_util.h"
#include "window/editable_cell_widget/orion_editable_cell_entry.h"
#include "window/main_window/main_window.h"
#include "window/columnview/bookmarks_columnview.h"

#include <common/src/defer.h>

//BEWARE - hacky solution, passing a cell widget through these callbacks instead of the child widget

struct BookmarksReleasedCallbackData {
    GtkWidget* parent;
    GtkColumnViewCell* cell;
    GtkPopoverMenu* popover_menu;
    gpointer user_data;
};

static void
on_column_view_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    orion::gtk::button_released_handler(data->parent, click, x, y, data->popover_menu, GTK_WIDGET(data->child));
}

static void
setup_column_view_context_menu(Sidebar* sidebar, GtkColumnView* column_view)
{
    GtkPopoverMenu* popover_menu = sidebar_get_bookmarks_column_view_popover_menu(sidebar);

    if (popover_menu) {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
        data->parent = GTK_WIDGET(sidebar);
        //TODO: is this hack too horrible?
        data->child = GTK_WIDGET(column_view);
        data->popover_menu = popover_menu;
        data->user_data = NULL; //for now
        
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(sidebar), GTK_WIDGET(column_view), on_column_view_secondary_released, data,
            [](gpointer data, GClosure* closure){delete static_cast<orion::gtk::ReleasedCallbackData*>(data);});
    }
}

static void
sidebar_bookmarks_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* entry = GTK_WIDGET(orion_editable_cell_entry_new(false));

	GtkWidget* icon = gtk_image_new();

	GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
	gtk_widget_set_hexpand(box, TRUE);
	gtk_box_append(GTK_BOX(box), icon);
	gtk_box_append(GTK_BOX(box), entry);

	GtkWidget* tree_expander = gtk_tree_expander_new();
	gtk_widget_set_hexpand(tree_expander, TRUE);
	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(tree_expander), box);
	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), tree_expander);
}

static void
on_cell_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<BookmarksReleasedCallbackData*>(user_data);
    //TODO: data->child is the columnview cell instead of the row, messy but here we go
    GtkWidget* row = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(data->cell));
	
    graphene_point_t zero{};
    graphene_point_t pos{};
	if (!gtk_widget_compute_point(row, data->parent, &zero, &pos)) {
		orion::log_warning("button_released_handler: failed to compute point");
	}

	// Position of the widget + position of the click
	GdkRectangle rect{
		.x = int(pos.x + x),
		.y = int(pos.y + y),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(GTK_POPOVER(data->popover_menu), &rect);
	gtk_popover_set_has_arrow(GTK_POPOVER(data->popover_menu), FALSE);
	gtk_popover_popup(GTK_POPOVER(data->popover_menu));

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(data->parent));
    ContextMenuData context_menu_data;
    context_menu_data.user_data = data->user_data;

    //store the selected item indexes in context_menu_data immediately, this avoids issues when
    //the selection index detection in action.cpp can give wrong indexes
    GtkSelectionModel* selection = GTK_SELECTION_MODEL(data->user_data);
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(selection);
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });
    uint64_t bitset_size = gtk_bitset_get_size(selection_bitset);
    for(uint64_t i = 0; i<bitset_size; i++) {
        uint64_t item_index = gtk_bitset_get_nth(selection_bitset, i);
        orion::gtk::GObjectPtr thing(G_OBJECT(g_list_model_get_item(G_LIST_MODEL(selection), item_index)));
        context_menu_data.selected_items.push_back(std::move(thing));
        orion::log_verbose("sidebar_bookmarks_columnview: on_cell_row_secondary_released: item_index=%llu", item_index);
    }

    main_window_set_context_menu_data(win, std::move(context_menu_data));

    //TODO: this is a kinda hacky solution to pass through the columnview cell that I have right clicked, also row would be better but how do I access that?
    main_window_set_last_right_clicked_widget(win,(GtkWidget*)data->cell);
}

static void
on_editable_cell_entry_finished(OrionEditableCellEntry* entry, const char* text, gboolean cancelled, gpointer user_data)
{
    if(cancelled)
    {
        //user cancelled rename (eg., by pressing Escape)
        return;
    }
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(user_data);
    if(bookmarks_columnview_item_get_item_type(item)==BookmarksColumnviewItemType::bookmark)
    {
        orion::BookmarkId id = bookmarks_columnview_item_get_id(item).bookmark_id;
        orion::UpdateBookmarkQuery query;
        query.id = id;
        query.update_name = true;
        query.name = text;
        global_core_api->update_bookmark(query, [](orion::Error err){VERIFY_LOG(err);});
    }
    else
    {
        orion::BookmarkFolderId id = bookmarks_columnview_item_get_id(item).folder_id;
        orion::UpdateBookmarkFolderQuery query;
        query.id = id;
        query.update_name = true;
        query.name = text;
        global_core_api->update_bookmark_folder(query, [](orion::Error err){VERIFY_LOG(err);});
    }
}

static void
sidebar_bookmarks_factory_unbind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));

    orion::gtk::remove_click_controller(gtk_tree_expander_get_child(tree_expander));
}

static void
sidebar_bookmarks_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkBox* box = GTK_BOX(gtk_tree_expander_get_child(tree_expander));
	GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_tree_expander_set_list_row(tree_expander, row);
	BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(gtk_tree_list_row_get_item(row));

    gtk_widget_unparent(gtk_widget_get_last_child(GTK_WIDGET(box)));
    const char* text = bookmarks_columnview_item_get_leftside_text(BOOKMARKS_COLUMNVIEW_ITEM(item))->str;
    OrionEditableCellEntry* entry = orion_editable_cell_entry_new(false);
    gtk_box_append(box, GTK_WIDGET(entry));
	orion_editable_cell_entry_set_text(entry,text);
    g_signal_connect(entry, "finished", G_CALLBACK(on_editable_cell_entry_finished), item);

    GtkImage* icon = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));

    Sidebar* sidebar = APP_SIDEBAR(user_data);

    // switch by item type (folder/item)
    BookmarksColumnviewItemType type = bookmarks_columnview_item_get_item_type(item);
    GtkPopoverMenu* popover_menu = NULL;
    switch (type) {
        //TODO: check whether these are different
        case BookmarksColumnviewItemType::folder_internal:
            gtk_image_set_from_icon_name(icon, bookmarks_columnview_item_get_icon_name(item));
            popover_menu = sidebar_get_bookmarks_internal_folder_popover_menu(sidebar);
            break;
        case BookmarksColumnviewItemType::folder:
            gtk_image_set_from_icon_name(icon, bookmarks_columnview_item_get_icon_name(item));
            popover_menu = sidebar_get_bookmarks_folder_popover_menu(sidebar);
            break;
        case BookmarksColumnviewItemType::bookmark:
            popover_menu = sidebar_get_bookmarks_bookmark_popover_menu(sidebar);
            orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), icon, bookmarks_columnview_item_get_rightside_text(item)->str);
            break;
    }


    if (popover_menu) {
        BookmarksReleasedCallbackData* data = new BookmarksReleasedCallbackData;
        data->parent = GTK_WIDGET(user_data);
        data->cell = GTK_COLUMN_VIEW_CELL(listitem);
        data->popover_menu = popover_menu;
        data->user_data = gtk_column_view_get_model(sidebar_get_bookmarks_column_view(sidebar));
        GtkTreeExpander* tree_expander =  GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
        GtkBox* box = GTK_BOX(gtk_tree_expander_get_child(tree_expander));
        GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(box));
 
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(user_data), row_widget, on_cell_row_secondary_released, data, 
            [](gpointer data, GClosure* closure){
                delete static_cast<BookmarksReleasedCallbackData*>(data);
            });
    }

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(box));
    orion::gtk::remove_dnd_controllers(row_widget);
    GtkDragSource* drag_source = gtk_drag_source_new();
    g_signal_connect(drag_source, "prepare", G_CALLBACK(bookmarks_columnview_on_drag_source_prepare), listitem);
    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drag_source));
    
    GtkDropTarget* drop_target = gtk_drop_target_new(BOOKMARK_ARRAY_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(bookmarks_columnview_on_drop_target_drop), listitem);
    g_signal_connect(drop_target, "motion", G_CALLBACK(bookmarks_columnview_on_drop_target_motion), listitem);
    g_signal_connect(drop_target, "leave", G_CALLBACK(bookmarks_columnview_on_drop_target_leave), listitem);
    g_signal_connect(drop_target, "accept", G_CALLBACK(bookmarks_columnview_on_drop_target_accept), listitem);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drop_target));
}

void
sidebar_bookmarks_column_view_map(GtkColumnView* bookmarks_columnview, gpointer user_data)
{
    bookmarks_column_view_map(bookmarks_columnview, NULL);
}

void
sidebar_bookmarks_column_view_map_flat_query(GtkColumnView* bookmarks_columnview, gpointer user_data, std::string query)
{
    bookmarks_column_view_map_flat_query(bookmarks_columnview, NULL, std::move(query));
}

void
sidebar_bookmarks_column_view_realize(GtkColumnView* bookmarks_columnview, gpointer user_data)
{
	GtkListItemFactory* factory;
	GtkColumnViewColumn* column;

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(sidebar_bookmarks_factory_setup_a), NULL);
	g_signal_connect(factory, "unbind", G_CALLBACK(sidebar_bookmarks_factory_unbind_a), user_data);
	g_signal_connect(factory, "bind", G_CALLBACK(sidebar_bookmarks_factory_bind_a), user_data);
	column = gtk_column_view_column_new(NULL, factory);
    setup_column_view_context_menu(APP_SIDEBAR(user_data), bookmarks_columnview);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(bookmarks_columnview, column);
	g_object_unref(column);
    // hide the header
    //NOTE: partial hack because the columnview has no proper way to hide the header
    GtkWidget* header_widget = gtk_widget_get_first_child(GTK_WIDGET(bookmarks_columnview));
    gtk_widget_set_visible(header_widget, false);
}

void
on_sidebar_bookmarks_column_view_activate(GtkColumnView* view, guint pos, gpointer user_data){
    on_bookmarks_column_view_activate(view, pos, user_data);
}

