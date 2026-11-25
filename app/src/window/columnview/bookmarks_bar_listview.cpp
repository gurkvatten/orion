#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include <common/src/defer.h>
#include <common/src/log.h>

#include "app.h"
#include "bookmarks_bar_listview.h"
#include "data_item/bookmarks_columnview_item.h"
#include "window/main_window/header/bookmarks_bar.h"
#include "util/click_util.h"
#include "util/text_util.h"
#include "util/ref_util.h"
#include "util/favicon_util.h"
#include "util/menu_util.h"
#include "window/main_window/header/bookmarks_bar.h"
#include "window/main_window/main_window.h"
#include "window/editable_cell_widget/orion_editable_cell_entry.h"

//used in bookmarks_bar.cpp

struct BookmarksFoldersIdStruct
{
    BookmarksBar* bookmarks_bar;
    orion::BookmarkFolderId id;
};

gboolean
bookmarks_bar_listview_on_drop_target_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    if(G_VALUE_HOLDS(value, BOOKMARK_ARRAY_TYPE))
    {
        GtkWidget* button = gtk_list_item_get_child(listitem);
        graphene_rect_t row_rectangle;
        if(gtk_widget_compute_bounds(button, button, &row_rectangle))
        {
            BookmarksColumnviewItem* this_item = BOOKMARKS_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));

            uint index = 0;

            GtkListView* view = GTK_LIST_VIEW(gtk_widget_get_ancestor(button, GTK_TYPE_LIST_VIEW));
            GListModel* store = G_LIST_MODEL(gtk_no_selection_get_model(GTK_NO_SELECTION(gtk_list_view_get_model(view))));

            uint n_items = g_list_model_get_n_items(store);
            uint i = 0;
            for(; i<n_items; i++)
            {
                orion::gtk::GObjectPtr ref(G_OBJECT(g_list_model_get_item(store, i)));
                if((gpointer)ref.get() == this_item)
                {
                    index = i;
                    break;
                }
            }
            if(i == n_items)
            {
                //only reachable if this_item is somehow not part of the glistmodel, should never happen
                orion::log_debug("bookmarks_bar_listview_on_drop_target_drop(): target item not found in list model");
                return false;
            }

            orion::BookmarkFolderId parent_folder_id;
            std::optional<double> seq_num_before, seq_num_after; //seq nums before/after the position to be inserted into
            if(bookmarks_columnview_item_get_item_type(this_item)==BookmarksColumnviewItemType::bookmark)
            {
                parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(this_item);
                if(x<(row_rectangle.size.width/2))
                {
                    if(index == 0)
                    {
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(this_item);
                    }
                    else
                    {
                        BookmarksColumnviewItem* prev_item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(store, index-1));
                        defer([prev_item]{ g_object_unref(prev_item); });
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(this_item);
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(prev_item);
                    }
                }
                else
                {
                    if(index == n_items - 1)
                    {
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(this_item);
                    }
                    else
                    {
                        BookmarksColumnviewItem* next_item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(store, index+1));
                        defer([next_item]{ g_object_unref(next_item); });
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(this_item);
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(next_item);
                    }
                }
            }
            else
            {
                if(x<(row_rectangle.size.width/4))
                {
                    parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(this_item);
                    if(index == 0)
                    {
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(this_item);
                    }
                    else
                    {
                        BookmarksColumnviewItem* prev_item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(store, index-1));
                        defer([prev_item]{ g_object_unref(prev_item); });
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(this_item);
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(prev_item);
                    }
                }
                else if(x>(row_rectangle.size.width*3/4))
                {
                    parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(this_item);
                    if(index == n_items - 1)
                    {
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(this_item);
                    }
                    else
                    {
                        BookmarksColumnviewItem* next_item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(store, index+1));
                        defer([next_item]{ g_object_unref(next_item); });
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(this_item);
                        seq_num_after = bookmarks_columnview_item_get_sequence_number(next_item);
                    }
                }
                else
                {
                    parent_folder_id = bookmarks_columnview_item_get_id(this_item).folder_id;
                    //leaving this blank, know what to drop into and do not need to set seq num here
                }
            }

            GArray* dropped_array = static_cast<BookmarkArray*>(g_value_get_boxed(value))->array;
            const IdAndType* dropped_items = (IdAndType*)(dropped_array->data); 

            double step_size = 1;
            if(seq_num_before.has_value() && seq_num_after.has_value())
            {
                step_size = (seq_num_after.value()-seq_num_before.value())/(dropped_array->len+1);
            }

            std::optional<double> start = 0;
            if(seq_num_before.has_value())
            {
                start = seq_num_before.value()+step_size;
            }
            else if(seq_num_after.has_value()) // technically unnecessary but I will use this anyway
            {
                start = seq_num_after.value()-step_size*dropped_array->len;
            }


            for(guint i = 0; i<dropped_array->len; i++)
            {
                IdAndType dropped_item = dropped_items[i];
                if(dropped_item.type==BookmarksColumnviewItemType::bookmark)
                {
                    orion::UpdateBookmarkQuery query{};
                    query.id = dropped_item.id.bookmark_id;
                    if(start.has_value()) 
                    {
                        query.seq_num = start.value()+step_size*i;
                        query.update_seq_num = true;
                    }
                    query.parent_folder_id = parent_folder_id;
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark(query, [](orion::Error err){VERIFY_LOG(err);});
                }
                else
                {
                    orion::UpdateBookmarkFolderQuery query{};
                    query.id = dropped_item.id.folder_id;
                    if(start.has_value()) 
                    {
                        query.seq_num = start.value()+step_size*i;
                        query.update_seq_num = true;
                    }
                    query.parent_folder_id = parent_folder_id;
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark_folder(query, [](orion::Error err){VERIFY_LOG(err);});
                }
            }
        }
        return TRUE;
    }
    return FALSE;
}

static GdkContentProvider*
bookmarks_bar_listview_on_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);
    BookmarkArray* bookmark_array = bookmark_array_new();
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));

    BookmarkOrFolderId id = bookmarks_columnview_item_get_id(item);
    IdAndType data = {.type = bookmarks_columnview_item_get_item_type(item), .id = id};
    g_array_append_val(bookmark_array->array, data);
    GdkContentProvider* return_val = gdk_content_provider_new_typed(bookmark_array_get_type(), bookmark_array);
    bookmark_array_free(bookmark_array);
    return return_val;
}

static void
on_cell_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    GtkWidget* child = gtk_list_item_get_child(GTK_LIST_ITEM(data->child));
    
    graphene_point_t zero{};
    graphene_point_t pos{};
	if (!gtk_widget_compute_point(child, data->parent, &zero, &pos)) {
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
    context_menu_data.user_data = gtk_list_view_get_model(GTK_LIST_VIEW(data->user_data));
    main_window_set_context_menu_data(win, std::move(context_menu_data));
    //TODO: this is a kinda hacky solution to pass through the listitem that I have right clicked, also row would be better but how do I access that?
    main_window_set_last_right_clicked_widget(win, (GtkWidget*)data->child);
}

static void
factory_setup(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    orion::log_verbose("factory_setup");
    //sadly can't do anything here as the only widget needed is based on the item type
}

static uint
insert_seq_num_into_vec_return_pos(std::vector<double>& vec, double seq_num)
{
    auto it = std::find_if(vec.begin(), vec.end(), [seq_num](double a){return a>seq_num;});
    uint res = std::distance(vec.begin(), it);
    vec.insert(it, seq_num);
    return res;
}

//TODO: maybe figure out a non recursive algorithm for handling this?
GMenuModel*
create_folder_menu(orion::BookmarkFolderId id, BookmarksBar* bookmarks_bar)
{
    GMenu* menu = g_menu_new();

    auto folders = bookmarks_bar_get_folders(bookmarks_bar);
    auto bookmarks = bookmarks_bar_get_bookmarks(bookmarks_bar);

    std::vector<double> seq_nums;
    for(orion::BookmarkFolderDto folder : *folders)
    {
        if(folder.parent_folder_id == id)
        {
            uint pos = insert_seq_num_into_vec_return_pos(seq_nums, folder.seq_num);
            GMenuModel* submenu = create_folder_menu(folder.id, bookmarks_bar);
            g_menu_insert_submenu(menu, pos, folder.name.c_str(), submenu);
            g_object_unref(submenu);
        }
    }
    for(orion::BookmarkDto bookmark : *bookmarks)
    {
        if(bookmark.parent_folder_id == id)
        {
            uint pos = insert_seq_num_into_vec_return_pos(seq_nums, bookmark.seq_num);
            GMenuItem* item = g_menu_item_new(bookmark.name.c_str(), NULL);
            g_menu_item_set_action_and_target(item, "win.open-uri-in-new-tab", "s", bookmark.address.c_str());
            g_menu_insert_item(menu, pos, item);
            g_object_unref(item);
        }
    }
    if(seq_nums.size()==0)
    {
        GMenuItem* empty_item = g_menu_item_new(_("No Bookmarks in Folder"), NULL);
        //NOTE: action is set to an invalid string to make this insensitive
        g_menu_item_set_action_and_target(empty_item, "NOT_AN_ACTION", NULL);
        g_menu_append_item(menu, empty_item);
        g_object_unref(empty_item);
    }
    else
    {
        //GMenuItem* open_in_new_tabs_item = g_menu_item_new(_("Open in New Tabs"), NULL);
        //g_menu_item_set_action_and_target(open_in_new_tabs_item, "win.bookmarks-bar-open-in-new-tabs", "x", id, NULL);
        GMenu* section = g_menu_new();
        //g_menu_append_item(section, open_in_new_tabs_item);
        orion::gtk::menu_append_action_with_int64_target(section, _("Open in New Tabs"), "win.bookmarks-bar-open-in-new-tabs", id.value);
        //g_object_unref(open_in_new_tabs_item);

        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        g_object_unref(section);
    }

    return G_MENU_MODEL(menu);
}

static void
menu_button_create_popup_func(GtkMenuButton* button, gpointer user_data)
{
    BookmarksFoldersIdStruct* data = static_cast<BookmarksFoldersIdStruct*>(user_data);

    //NOTE: has to be a GTK_POPOVER_MENU_NESTED to avoid GTK_POPOVER_MENU_SLIDING's inherent GtkStack
    //this stack has pages for each submenu with the name coresponding to the submenu label,
    //thus not allowing duplicate names 

    GMenuModel* model = create_folder_menu(data->id, data->bookmarks_bar);
    GtkWidget* popover = gtk_popover_menu_new_from_model_full(model, GTK_POPOVER_MENU_NESTED);
    gtk_menu_button_set_popover(button, popover);
    g_object_unref(model);
    //g_object_unref(popover);
}

static void
on_bookmark_button_clicked(GtkButton* button, gpointer user_data)
{
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    std::string address = bookmarks_columnview_item_get_rightside_text(item)->str;
    main_window_load_bookmark(win, address);
}

static void
factory_unbind(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    orion::log_verbose("factory_unbind");
    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), NULL);
}

static void
on_rename_finished(OrionEditableCellEntry* entry, const char* text, gboolean cancelled, gpointer user_data)
{
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(user_data);
    if(cancelled)
    {
        //user cancelled rename (eg., by pressing Escape)
        GtkListView* view = bookmarks_columnview_item_get_parent_list_view(item);
        //refresh list view to hide entry when icon_only bookmarks bar style is used
        bookmarks_bar_list_view_refresh(view);
        return;
    }

    if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
    {
        orion::UpdateBookmarkQuery query{};
        query.id = bookmarks_columnview_item_get_id(item).bookmark_id;
        query.name = text;
        query.update_name = true;
        global_core_api->update_bookmark(query, [](orion::Error err){VERIFY_LOG(err);});
    }
    else
    {
        orion::UpdateBookmarkFolderQuery query{};
        query.id = bookmarks_columnview_item_get_id(item).folder_id;
        query.name = text;
        query.update_name = true;
        global_core_api->update_bookmark_folder(query, [](orion::Error err){VERIFY_LOG(err);});
    }
}

struct BookmarksBarAndData
{
    BookmarksBar* bookmarks_bar;
    GtkListView* view;
    gpointer user_data;
};
//only holds "weak references", no need to carefully destroy

static void
factory_bind(GtkSignalListItemFactory* factory, GObject* listitem, gpointer userdata)
{
    orion::log_verbose("factory_bind");
    //gpointer user_data = static_cast<BookmarksBarAndData*>(userdata)->user_data;
    BookmarksBar* bookmarks_bar = static_cast<BookmarksBarAndData*>(userdata)->bookmarks_bar;
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(bookmarks_bar)));
	GtkWidget* button;
    BookmarksColumnviewItem* data_item = BOOKMARKS_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    GtkPopoverMenu* context_menu;
    BookmarksColumnviewItemType type = bookmarks_columnview_item_get_item_type(data_item);
    BookmarkOrFolderId id = bookmarks_columnview_item_get_id(data_item);
    OrionEditableCellEntry* entry = orion_editable_cell_entry_new(false);
    const char* text = bookmarks_columnview_item_get_leftside_text(data_item)->str;
    orion_editable_cell_entry_set_text(entry, text);
    bookmarks_columnview_item_set_parent_list_view(data_item, static_cast<BookmarksBarAndData*>(userdata)->view);
    g_signal_connect_object(entry, "finished", G_CALLBACK(on_rename_finished), data_item, G_CONNECT_DEFAULT);

    BookmarksFoldersIdStruct* popover_data;
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    GtkWidget* icon{};

    if(type == BookmarksColumnviewItemType::bookmark) {
        icon = gtk_image_new();
        const char* text_right = bookmarks_columnview_item_get_rightside_text(data_item)->str;
        orion::Uri uri = orion::Uri::parse_user_input(text_right);
        orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(win), GTK_IMAGE(icon), uri.uri().c_str());
    } else {
        icon = gtk_image_new_from_icon_name("folder-visiting-symbolic");
    }

    //note: bookmarks_bar_rename_bookmark relies on the structure of this box
    gtk_box_append(GTK_BOX(box), icon);
    gtk_box_append(GTK_BOX(box), GTK_WIDGET(entry));

    orion::settings::BookmarksBarStyle style = bookmarks_bar_get_style(bookmarks_bar);
    switch(style)
    {
        case orion::settings::BookmarksBarStyle::icon_and_text:
            break;
        case orion::settings::BookmarksBarStyle::icon_only:
            gtk_widget_set_visible(GTK_WIDGET(entry), false);
            break;
        case orion::settings::BookmarksBarStyle::text_only: // fallthrough to default
        default:
            gtk_widget_set_visible(icon, false);
            break;
    }

    switch (type) {
        case BookmarksColumnviewItemType::folder_internal:
        case BookmarksColumnviewItemType::folder:
            context_menu = bookmarks_bar_get_folder_popover_menu(bookmarks_bar);
            button = gtk_menu_button_new();
            if(style == orion::settings::BookmarksBarStyle::text_only) {
                gtk_menu_button_set_always_show_arrow(GTK_MENU_BUTTON(button), true);
            }
            //gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(button), create_folder_menu(std::get<orion::BookmarkFolderId>(id), static_cast<BookmarksFoldersStruct*>(user_data)));
            popover_data = new BookmarksFoldersIdStruct;
            //popover_data->data = static_cast<BookmarksFoldersStruct*>(user_data);
            popover_data->bookmarks_bar = bookmarks_bar;
            popover_data->id = id.folder_id;
            gtk_menu_button_set_create_popup_func(GTK_MENU_BUTTON(button), menu_button_create_popup_func, popover_data, [](gpointer data){delete static_cast<BookmarksFoldersIdStruct*>(data);});
            //gtk_menu_button_set_child(GTK_MENU_BUTTON(button), GTK_WIDGET(entry));
            gtk_menu_button_set_child(GTK_MENU_BUTTON(button), box);
            //TODO: get rid of this in favor of a properly generated menu using a treelistmodel
            break;

        case BookmarksColumnviewItemType::bookmark:
            context_menu = bookmarks_bar_get_bookmark_popover_menu(bookmarks_bar);
            button = gtk_button_new();
            //gtk_button_set_child(GTK_BUTTON(button), GTK_WIDGET(entry));
            gtk_button_set_child(GTK_BUTTON(button), box);
            g_signal_connect(button, "clicked", G_CALLBACK(on_bookmark_button_clicked), data_item);
            break;

        default:
            return; //unknown item type, nothing to do
    }

    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), button);


    //do not need to remove these as the button is always initialized fresh
    GtkDragSource* drag_source = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
    g_signal_connect(drag_source, "prepare", G_CALLBACK(bookmarks_bar_listview_on_drag_source_prepare), listitem);
    gtk_widget_add_controller(GTK_WIDGET(button), GTK_EVENT_CONTROLLER(drag_source));

    GtkDropTarget* drop_target = gtk_drop_target_new(BOOKMARK_ARRAY_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(bookmarks_bar_listview_on_drop_target_drop), listitem);
    gtk_widget_add_controller(GTK_WIDGET(button), GTK_EVENT_CONTROLLER(drop_target));



    if(context_menu)
    {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
        //TODO: figure this out
        data->parent = GTK_WIDGET(bookmarks_bar);
        data->child = (GtkWidget*)listitem;
        data->popover_menu = context_menu;
        data->user_data = static_cast<BookmarksBarAndData*>(userdata)->view;
        orion::gtk::register_right_click_with_user_data_and_closure(NULL, GTK_WIDGET(button), on_cell_row_secondary_released, data, 
            [](gpointer data, GClosure* closure){
                delete static_cast<orion::gtk::ReleasedCallbackData*>(data);
            });
    }
}

static void
bookmarks_bar_handle_reply(GtkListView* view, BookmarksBar* bookmarks_bar, orion::BookmarkFolderId bookmarks_bar_folder_id)
{
    GListStore* root = g_list_store_new(G_TYPE_OBJECT);

    auto folders = bookmarks_bar_get_folders(bookmarks_bar);
    auto bookmarks = bookmarks_bar_get_bookmarks(bookmarks_bar);

    for(orion::BookmarkFolderDto folder : *folders)
    {
        if(folder.parent_folder_id == bookmarks_bar_folder_id)
        {
            gpointer item = bookmarks_columnview_item_new_from_folder_struct(folder, NULL, NULL);
            g_list_store_insert_sorted(root, item, bookmarks_items_sequence_number_compare, NULL);
            g_object_unref(item);
        }
    }
    for(orion::BookmarkDto bookmark : *bookmarks)
    {
        if(bookmark.parent_folder_id == bookmarks_bar_folder_id)
        {
            gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, NULL);
            g_list_store_insert_sorted(root, item, bookmarks_items_sequence_number_compare, NULL);
            g_object_unref(item);
        }
    }


    GtkNoSelection* selection = gtk_no_selection_new(G_LIST_MODEL(root));
    gtk_list_view_set_model(view, GTK_SELECTION_MODEL(selection));
    //do not unref root here, it needs to stay alive
}

static void
handle_bookmark_created(orion::BookmarkDto bookmark_created, GListStore* store, orion::BookmarkFolderId bookmarks_bar_folder_id)
{
    if(bookmark_created.parent_folder_id==bookmarks_bar_folder_id)
    {
        gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark_created, NULL);
        g_list_store_insert_sorted(store, item, bookmarks_items_sequence_number_compare, NULL);
        g_object_unref(item);
    }
}

static void
handle_bookmark_deleted(orion::BookmarkId bookmark_deleted, GListStore* store)
{
    uint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(uint i = 0; i<len; i++)
    {
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(bookmarks_columnview_item_get_item_type(item)==BookmarksColumnviewItemType::bookmark)
        {
            if(bookmarks_columnview_item_get_id(item).bookmark_id==bookmark_deleted)
            {
                g_list_store_remove(store, i);
                break;
            }
        }
    }
}


static void
on_bookmarks_change(std::vector<orion::BookmarkDto>* bookmarks, GtkListView* view, const orion::BookmarksChange& change, orion::BookmarkFolderId bookmarks_bar_folder_id)
{
    GtkNoSelection* selection = GTK_NO_SELECTION(gtk_list_view_get_model(view));
    GListStore* store = G_LIST_STORE(gtk_no_selection_get_model(selection));

    for(orion::BookmarkDto bookmark_created : change.created)
    {
        bookmarks->push_back(bookmark_created);
        handle_bookmark_created(bookmark_created, store, bookmarks_bar_folder_id);
    }
    for(orion::BookmarkId bookmark_deleted : change.deleted)
    {
        for(auto it = bookmarks->begin(); it != bookmarks->end(); ++it)
        {
            if(it->id == bookmark_deleted)
            {
                bookmarks->erase(it);
                break;
            }
        }
        handle_bookmark_deleted(bookmark_deleted, store);
    }
    for(orion::BookmarkDto bookmark_updated : change.updated)
    {
        for(auto it = bookmarks->begin(); it != bookmarks->end(); ++it)
        {
            if(it->id == bookmark_updated.id)
            {
                *it = bookmark_updated;
                break;
            }
        }
        handle_bookmark_deleted(bookmark_updated.id, store);
        handle_bookmark_created(bookmark_updated, store, bookmarks_bar_folder_id);
    }
}

static void
handle_folder_created(orion::BookmarkFolderDto folder_created, GListStore* store, orion::BookmarkFolderId bookmarks_bar_folder_id)
{
    if(folder_created.parent_folder_id == bookmarks_bar_folder_id)
    {
        gpointer item = bookmarks_columnview_item_new_from_folder_struct(folder_created, NULL, NULL);
        g_list_store_insert_sorted(store, item, bookmarks_items_sequence_number_compare, NULL);
        g_object_unref(item);
    }
}

static void
handle_folder_deleted(orion::BookmarkFolderId folder_deleted, GListStore* store)
{
    uint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(uint i = 0; i<len; i++)
    {
        BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
        {
            if(bookmarks_columnview_item_get_id(item).folder_id==folder_deleted)
            {
                g_list_store_remove(store, i);
                break;
            }
        }
    }
}

static void
on_bookmarks_folders_change(std::vector<orion::BookmarkFolderDto>* folders, GtkListView* view, const orion::BookmarkFoldersChange& change, orion::BookmarkFolderId bookmarks_bar_folder_id)
{
    GtkNoSelection* selection = GTK_NO_SELECTION(gtk_list_view_get_model(view));
    GListModel* model = gtk_no_selection_get_model(selection);
    GListStore* store = G_LIST_STORE(model);

    for(orion::BookmarkFolderDto folder_created : change.created)
    {
        folders->push_back(folder_created);
        handle_folder_created(folder_created, store, bookmarks_bar_folder_id);
    }
    for(orion::BookmarkFolderId folder_deleted : change.deleted)
    {
        for(auto it = folders->begin(); it != folders->end(); ++it)
        {
            if(it->id == folder_deleted)
            {
                folders->erase(it);
                break;
            }
        }
        handle_folder_deleted(folder_deleted, store);
    }
    for(orion::BookmarkFolderDto folder_updated : change.updated)
    {
        for(auto it = folders->begin(); it != folders->end(); ++it)
        {
            if(it->id == folder_updated.id)
            {
                *it = folder_updated;
                break;
            }
        }
        handle_folder_deleted(folder_updated.id, store);
        handle_folder_created(folder_updated, store, bookmarks_bar_folder_id);
    }
}

void
bookmarks_bar_list_view_fetch_backend_data(GtkListView* view, gpointer user_data, orion::ProfileId profile_id)
{
    orion::log_verbose("bookmarks_bar_list_view_fetch_backend_data");

    //apparently, memory managing a weak ref is a lot more difficult than g_object_refing the view and unrefing at the end of the callbacks...
    auto weak_view_ref = orion::gtk::WeakRefWrapper(view);
    auto weak_bar_ref = orion::gtk::WeakRefWrapper(APP_BOOKMARKS_BAR(user_data));

    global_core_api->get_bookmark_folder_ids(profile_id, [weak_view_ref,profile_id,weak_bar_ref](orion::GetBookmarkFolderIdsReply reply)
    {
        VERIFY_LOG(reply.err);
        
        global_core_api->get_bookmark_folders(reply.ids, [weak_view_ref,profile_id,weak_bar_ref](orion::GetBookmarkFoldersReply reply)
        {
            VERIFY_LOG(reply.err);
            auto folders_copy = std::make_shared<std::vector<orion::BookmarkFolderDto>>(reply.folders);
            //only stays alive when observers are alive

            orion::GetBookmarkIdsQuery query{};
            query.profile_id = profile_id;
            global_core_api->get_bookmark_ids(query, [weak_view_ref,folders_copy,weak_bar_ref](orion::GetBookmarkIdsReply reply)
            {
                VERIFY_LOG(reply.err);
                
                orion::GetBookmarksQuery query{
                    .ids = reply.ids,
                };
                global_core_api->get_bookmarks(query, [weak_view_ref,folders_copy,weak_bar_ref](orion::GetBookmarksReply reply)
                {
                    VERIFY_LOG(reply.err);
                    auto view_ref = weak_view_ref.lock();
                    if(!view_ref) return;
                    auto bar_ref = weak_bar_ref.lock();
                    if(!bar_ref) return;

                    GtkListView* view = GTK_LIST_VIEW(view_ref.get());
                    BookmarksBar* bookmarks_bar = APP_BOOKMARKS_BAR(bar_ref.get());
                    auto bookmarks_copy = std::make_shared<std::vector<orion::BookmarkDto>>(reply.bookmarks);

                    orion::BookmarkFolderId bookmarks_bar_folder_id{}; //zero initialized (and 0 means invalid folder id)
                    for(orion::BookmarkFolderDto folder : *folders_copy)
                    {
                        if(folder.type == orion::BookmarkFolderType::bookmarks_bar)
                        {
                            bookmarks_bar_folder_id = folder.id;
                        }
                    }
                    
                    if(!bookmarks_bar_folder_id.value)
                    {
                        orion::log_error("could not find bookmarks bar folder");
                        return;
                    }

                    bookmarks_bar_set_bookmars_bar_folder_id(bookmarks_bar, bookmarks_bar_folder_id);
                    
                    orion::BookmarkFolderId* persistent_id = new orion::BookmarkFolderId(bookmarks_bar_folder_id.value);
                    g_object_set_data_full(G_OBJECT(view), "bookmarks-bar-folder-id", persistent_id,
                        [](gpointer data){
                            delete static_cast<orion::BookmarkFolderId*>(data);
                        });
                        
                    bookmarks_bar_set_bookmarks(bookmarks_bar, *bookmarks_copy.get());
                    bookmarks_bar_set_folders(bookmarks_bar, *folders_copy.get());

                    bookmarks_bar_handle_reply(view, bookmarks_bar, bookmarks_bar_folder_id);
                });
            });
        });
    });
}

//causes "setup" and "bind" calls on each item in the list view
void
bookmarks_bar_list_view_refresh(GtkListView* view)
{
    GtkSelectionModel* selection_model = gtk_list_view_get_model(view);
    if(!selection_model)
    {
        //view not prepared yet
        return;
    }
    
    //force a refresh of all items by temporarily removing the model and setting it back
    gtk_list_view_set_model(view, NULL);
    gtk_list_view_set_model(view, selection_model);
}

static void
bookmarks_bar_list_view_add_change_observers(GtkListView* self, BookmarksBar* bookmarks_bar)
{
    auto weak_bar_ref = orion::gtk::WeakRefWrapper(bookmarks_bar);
    auto weak_view_ref = orion::gtk::WeakRefWrapper(self);

    orion::ObserverId folder_observer_id = global_core_api->bookmark_folders_change().add_observer(
        [weak_bar_ref,weak_view_ref](const orion::BookmarkFoldersChange& change){
            orion::log_verbose("bookmarks_bar_list_view: folders change observed");
            auto bar_ref = weak_bar_ref.lock();
            if(!bar_ref) return;
            auto view_ref = weak_view_ref.lock();
            if(!view_ref) return;
            BookmarksBar* bar = APP_BOOKMARKS_BAR(bar_ref.get());
            GtkListView* view = GTK_LIST_VIEW(view_ref.get());
            orion::BookmarkFolderId bookmarks_bar_folder_id = bookmarks_bar_get_bookmars_bar_folder_id(bar);
            if(!bookmarks_bar_folder_id.value)
            {
                //bookmarks bar list view is not prepared yet
                return;
            }
            auto folders = bookmarks_bar_get_folders(bar);
            on_bookmarks_folders_change(folders, view, change, bookmarks_bar_folder_id); 
        });
    bookmarks_bar_set_folders_change_observer_id(bookmarks_bar, folder_observer_id);

    orion::ObserverId bookmark_observer_id = global_core_api->bookmarks_change().add_observer(
        [weak_bar_ref,weak_view_ref](const orion::BookmarksChange& change){
            orion::log_verbose("bookmarks_bar_list_view: bookmarks change observed");
            auto bar_ref = weak_bar_ref.lock();
            if(!bar_ref) return;
            auto view_ref = weak_view_ref.lock();
            if(!view_ref) return;
            BookmarksBar* bar = APP_BOOKMARKS_BAR(bar_ref.get());
            GtkListView* view = GTK_LIST_VIEW(view_ref.get());
            orion::BookmarkFolderId bookmarks_bar_folder_id = bookmarks_bar_get_bookmars_bar_folder_id(bar);
            if(!bookmarks_bar_folder_id.value)
            {
                //bookmarks bar list view is not prepared yet
                return;
            }
            auto bookmarks = bookmarks_bar_get_bookmarks(bar);
            on_bookmarks_change(bookmarks, view, change, bookmarks_bar_folder_id); 
        });
    bookmarks_bar_set_bookmarks_change_observer_id(bookmarks_bar, bookmark_observer_id);
}

static void
bookmarks_bar_list_view_remove_change_observers(BookmarksBar* bookmarks_bar)
{
    orion::ObserverId folder_observer_id = bookmarks_bar_get_folders_change_observer_id(bookmarks_bar);
    orion::ObserverId bookmark_observer_id = bookmarks_bar_get_bookmarks_change_observer_id(bookmarks_bar);
    global_core_api->bookmark_folders_change().remove_observer(folder_observer_id);
    global_core_api->bookmarks_change().remove_observer(bookmark_observer_id);
}

void
bookmarks_bar_list_view_map(GtkListView* view, gpointer user_data)
{
    orion::log_verbose("bookmarks_bar_list_view_map");

    GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
    defer([factory]{ g_object_unref(factory); }); //unref factory at end of this scope

    BookmarksBarAndData* factory_data = new BookmarksBarAndData;
    factory_data->bookmarks_bar = APP_BOOKMARKS_BAR(user_data);
    factory_data->user_data = nullptr;
    factory_data->view = view;

    g_signal_connect(factory, "setup", G_CALLBACK(factory_setup), NULL);
    g_signal_connect(factory, "unbind", G_CALLBACK(factory_unbind), NULL);
    g_signal_connect_data(factory, "bind", G_CALLBACK(factory_bind), factory_data,
        [](gpointer data, GClosure* closure){delete static_cast<BookmarksBarAndData*>(data);}, G_CONNECT_DEFAULT);

    //when factory is destroyed, its signals should be disconnected automatically
    gtk_list_view_set_factory(view, factory);
}

void
bookmarks_bar_list_view_unmap(GtkListView* view, gpointer user_data)
{
    orion::log_verbose("bookmarks_bar_list_view_unmap");
}

void
bookmarks_bar_list_view_dispose(GtkListView* view, BookmarksBar* bookmarks_bar)
{
    orion::log_verbose("bookmarks_bar_list_view_dispose");
    
    gtk_list_view_set_model(view, NULL);
    gtk_list_view_set_factory(view, NULL);
    g_signal_handlers_disconnect_by_data(view, bookmarks_bar);
}

void
bookmarks_bar_list_view_finalize(GtkListView* view, BookmarksBar* bookmarks_bar)
{
    orion::log_verbose("bookmarks_bar_list_view_finalize");

    //stop observing changes
    bookmarks_bar_list_view_remove_change_observers(bookmarks_bar);
}

void
bookmarks_bar_list_view_constructed(GtkListView* view, BookmarksBar* bookmarks_bar)
{
}

void
bookmarks_bar_list_view_init(GtkListView* view, BookmarksBar* bookmarks_bar)
{
    orion::log_verbose("bookmarks_bar_list_view_init");

    //start observing changes here (and stop in finalize) - we do not want to miss changes
    //even while the list view is not visible because we would have to reload everything
    //when it becomes visible again
    bookmarks_bar_list_view_add_change_observers(view, bookmarks_bar);
}
