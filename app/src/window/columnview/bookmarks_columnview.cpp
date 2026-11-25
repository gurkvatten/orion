#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <common/src/defer.h>

#include "window/bookmarks_dialog.h"
#include "app.h"
#include "data_item/bookmarks_columnview_item.h"
#include "bookmarks_columnview.h"
#include "util/click_util.h"
#include "util/favicon_util.h"
#include "window/editable_cell_widget/orion_editable_cell_entry.h"
#include "window/main_window/main_window.h"
#include "util/bookmarks_util.h"

//used in bookmarks_dialog.cpp

static void
bookmarks_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkWidget* entry = GTK_WIDGET(orion_editable_cell_entry_new(false)); //TODO: replace with custom widget


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

//BEWARE - hacky solution, passing a cell widget through these callbacks instead of the child widget

struct BookmarksReleasedCallbackData {
    GtkWidget* parent;
    GtkColumnViewCell* cell;
    GtkPopoverMenu* popover_menu;
    gpointer user_data;
};

void
bookmarks_released_callback_data_destroy(gpointer final_data, GClosure* closure)
{
    BookmarksReleasedCallbackData* data = static_cast<BookmarksReleasedCallbackData*>(final_data);
    //don't need to destroy popover_menu, that dies with the dialog,
    //also don't need to destroy the cell, that dies with the dialog as well
    //thus I can safely ignore this hopefully
    delete data;
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
    main_window_set_last_right_clicked_widget(win,(GtkWidget*)data->cell);
}

static void
on_column_view_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    orion::gtk::button_released_handler(data->parent, click, x, y, data->popover_menu, GTK_WIDGET(data->child));
}

static void
setup_column_view_context_menu(BookmarksDialog* dialog, GtkColumnView* column_view)
{
    GtkPopoverMenu* popover_menu = bookmarks_dialog_get_bookmarks_column_view_popover_menu(dialog);

    if (popover_menu) {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
        data->parent = GTK_WIDGET(dialog);
        //TODO: is this hack too horrible?
        data->child = GTK_WIDGET(column_view);
        data->popover_menu = popover_menu;
        data->user_data = NULL; //for now
        
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(dialog), GTK_WIDGET(column_view), on_column_view_secondary_released, data,
            [](gpointer data, GClosure* closure){delete static_cast<orion::gtk::ReleasedCallbackData*>(data);});
    }
}

static void
setup_cell_context_menu(BookmarksDialog* dialog, BookmarksColumnviewItemType type, GtkColumnViewCell* cell, GtkWidget* child, gpointer context_menu_data)
{
    // switch by item type (folder/item)
    GtkPopoverMenu* popover_menu = NULL;
    switch (type) {
        case BookmarksColumnviewItemType::folder_internal:
            popover_menu = bookmarks_dialog_get_bookmarks_internal_folder_popover_menu(dialog);
            break;
        case BookmarksColumnviewItemType::folder:
            popover_menu = bookmarks_dialog_get_bookmarks_folder_popover_menu(dialog);
            break;
        case BookmarksColumnviewItemType::bookmark:
            popover_menu = bookmarks_dialog_get_bookmarks_bookmark_popover_menu(dialog);
            break;
    }

    if (popover_menu) {
        BookmarksReleasedCallbackData* data = new BookmarksReleasedCallbackData;
        data->parent = GTK_WIDGET(dialog);
        //TODO: is this hack too horrible?
        data->cell = cell;
        data->popover_menu = popover_menu;
        data->user_data = context_menu_data;

        orion::gtk::register_right_click_with_user_data_and_closure(
            GTK_WIDGET(dialog), child, on_cell_row_secondary_released, data, bookmarks_released_callback_data_destroy
        );
    }
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

gboolean
bookmarks_columnview_on_drop_target_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("bookmarks_columnview_on_drop_target_drop");
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);
    if(G_VALUE_HOLDS(value, BOOKMARK_ARRAY_TYPE))
    {
        GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
        graphene_rect_t row_rectangle;
        if(gtk_widget_compute_bounds(row_widget, row_widget, &row_rectangle))
        {
            gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
            while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            BookmarksColumnviewItem* this_item = BOOKMARKS_COLUMNVIEW_ITEM(thing);

            uint index = 0;

            GListModel* store = G_LIST_MODEL(bookmarks_columnview_item_get_folder_list_store(this_item));
            uint n_items = g_list_model_get_n_items(store);
            //operating on the presumption that this will never fail cause it should not
            for(uint i = 0; i<n_items; i++)
            {
                gpointer tmp_item = g_list_model_get_item(store, i);
                defer([tmp_item]{ g_object_unref(tmp_item); });
                if(tmp_item==this_item)
                {
                    index = i;
                    break;
                }
            }

            std::optional<double> seq_num_before, seq_num_after; //seq nums before/after the position to be inserted into
            orion::BookmarkFolderId parent_folder_id;
            //TODO: handle dropping into root 
            if(bookmarks_columnview_item_get_item_type(this_item)==BookmarksColumnviewItemType::bookmark)
            {
                parent_folder_id = bookmarks_columnview_item_get_parent_folder_id(this_item);
                if(y<(row_rectangle.size.height/2))
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
                if(y<(row_rectangle.size.height/4))
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
                else if(y>(row_rectangle.size.height*3/4))
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
                    GListModel* subdirectory = G_LIST_MODEL(bookmarks_columnview_item_get_subdirectory_list_store(this_item));
                    uint n_items = g_list_model_get_n_items(subdirectory);
                    if(n_items)
                    {
                        BookmarksColumnviewItem* last_item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(subdirectory, n_items-1));
                        defer([last_item]{ g_object_unref(last_item); });
                        seq_num_before = bookmarks_columnview_item_get_sequence_number(last_item);
                    }
                    else
                    {
                        seq_num_before = 0;
                    }
                }
            }

            GArray* dropped_array = static_cast<BookmarkArray*>(g_value_get_boxed(value))->array;
            const IdAndType* dropped_items = (IdAndType*)(dropped_array->data); 

            double step_size = 1;
            if(seq_num_before.has_value() && seq_num_after.has_value())
            {
                step_size = (seq_num_after.value()-seq_num_before.value())/(dropped_array->len+1);
            }

            double start = 0;
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
                double seq_num = start+step_size*i;
                IdAndType dropped_item = dropped_items[i];
                if(dropped_item.type == BookmarksColumnviewItemType::bookmark)
                {
                    orion::UpdateBookmarkQuery query;
                    query.id = dropped_item.id.bookmark_id;
                    query.seq_num = seq_num;
                    query.update_seq_num = true;
                    query.parent_folder_id = parent_folder_id;
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark(query, [](orion::Error err){
                        if (err.code() == orion::ErrorCode::circular_reference) {
                            //failing silently
                            return;
                        }
                        VERIFY_LOG(err);
                    });
                }
                else
                {
                    orion::UpdateBookmarkFolderQuery query;
                    query.id = dropped_item.id.folder_id;
                    query.seq_num = seq_num;
                    query.update_seq_num = true;
                    query.parent_folder_id = parent_folder_id;
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark_folder(query, [](orion::Error err){
                        if (err.code() == orion::ErrorCode::circular_reference) {
                            //failing silently
                            return;
                        }
                        VERIFY_LOG(err);
                    });
                }
            }
        }
        return TRUE;
    }
    return FALSE;
}

GdkContentProvider*
bookmarks_columnview_on_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("bookmarks_columnview_on_drag_source_prepare");
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
    while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
    GtkColumnView* view = GTK_COLUMN_VIEW(gtk_widget_get_ancestor(gtk_column_view_cell_get_child(cell), GTK_TYPE_COLUMN_VIEW));

    GtkSelectionModel* selection = GTK_SELECTION_MODEL(gtk_column_view_get_model(view));
    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(bitset);

    BookmarkArray* bookmark_array = bookmark_array_new();

    if(bitset_size == 0)
    {
        IdAndType data = {.type = bookmarks_columnview_item_get_item_type(item), .id = bookmarks_columnview_item_get_id(item)};
        g_array_append_val(bookmark_array->array, data);
        GdkContentProvider* return_val = gdk_content_provider_new_typed(bookmark_array_get_type(), bookmark_array);
        bookmark_array_free(bookmark_array);
        gtk_bitset_unref(bitset);
        return return_val;
    }
    else
    {
        gboolean dragged_item_is_in_selection = false;
        //if current item is not in selection, ignore selection, only drag the current item
        std::vector<orion::gtk::GObjectPtr> selected_items;
        std::vector<orion::BookmarkFolderId> transferred_folders;
        for(uint64_t bitset_index = 0; bitset_index<bitset_size; bitset_index++)
        {
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(selection), gtk_bitset_get_nth(bitset, bitset_index));
            while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing);});
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            orion::gtk::GObjectPtr item_ptr(G_OBJECT(thing));
            BookmarksColumnviewItem* selected_item = BOOKMARKS_COLUMNVIEW_ITEM(item_ptr.get());
            if(selected_item == item)
            {
                dragged_item_is_in_selection = true;
            }
            selected_items.push_back(std::move(item_ptr));
            if(!(bookmarks_columnview_item_get_item_type(selected_item)==BookmarksColumnviewItemType::bookmark))
            {
                transferred_folders.push_back(bookmarks_columnview_item_get_id(selected_item).folder_id);
            }
        }

        gtk_bitset_unref(bitset);
        //removing items whose parent folders are already being transferred
        for(auto it = selected_items.begin(); it!=selected_items.end(); ++it)
        {
            orion::BookmarkFolderId id = bookmarks_columnview_item_get_parent_folder_id(BOOKMARKS_COLUMNVIEW_ITEM(it->get()));
            auto search = std::find_if(transferred_folders.cbegin(), transferred_folders.cend(), [id](orion::BookmarkFolderId a){return id == a;});
            if(search!=transferred_folders.cend())
            {
                //need to decrement once so that I avoid this skipping over items
                it = --selected_items.erase(it);
            }
        }

        if(dragged_item_is_in_selection)
        {
            for(orion::gtk::GObjectPtr& item_ptr : selected_items)
            {
                BookmarksColumnviewItem* selected_item = BOOKMARKS_COLUMNVIEW_ITEM(item_ptr.get());
                IdAndType data = {.type = bookmarks_columnview_item_get_item_type(selected_item), .id = bookmarks_columnview_item_get_id(selected_item)};
                g_array_append_val(bookmark_array->array, data);
            }
            GdkContentProvider* return_val = gdk_content_provider_new_typed(bookmark_array_get_type(), bookmark_array);
            bookmark_array_free(bookmark_array);
            return return_val;
        }
        else
        {
            IdAndType data = {.type = bookmarks_columnview_item_get_item_type(item), .id = bookmarks_columnview_item_get_id(item)};
            g_array_append_val(bookmark_array->array, data);
            GdkContentProvider* return_val = gdk_content_provider_new_typed(bookmark_array_get_type(), bookmark_array);
            bookmark_array_free(bookmark_array);
            return return_val;
        }
    }
}

// use anonymous namespace to avoid potential ODR violations
namespace {
    struct StringAndNullTerm {
        const char* string;
        const char* term = NULL;
    };
}

GdkDragAction
bookmarks_columnview_on_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("bookmarks_columnview_on_drop_target_motion");
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
    while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* this_item = BOOKMARKS_COLUMNVIEW_ITEM(thing);



    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
    graphene_rect_t row_rectangle;

    StringAndNullTerm css_class;

    if(gtk_widget_compute_bounds(row_widget, row_widget, &row_rectangle))
    {
        if(bookmarks_columnview_item_get_item_type(this_item)==BookmarksColumnviewItemType::bookmark)
        {
            if(y<(row_rectangle.size.height/2))
            {
                css_class.string = "dnd-highlight-top";
            }
            else
            {
                css_class.string = "dnd-highlight-bottom";
            }
        }
        else
        {
            if(y<(row_rectangle.size.height/4))
            {
                css_class.string = "dnd-highlight-top";
            }
            else if(y>(row_rectangle.size.height*3/4))
            {
                css_class.string = "dnd-highlight-bottom";
            }
            else
            {
                css_class.string = "dnd-highlight-whole";
            }
        }
    }

    //uses the fact that StringAndNullTerm* is equivalent to a 2 element array of const char*, last one being a NULL terminator
    gtk_widget_set_css_classes(row_widget, reinterpret_cast<const char**>(&css_class));

    return GDK_ACTION_MOVE;
}

GdkDragAction
bookmarks_columnview_on_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("bookmarks_columnview_on_drop_target_leave");
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
    
    const char* empty_string_array[1] = {NULL};

    //uses the fact that StringAndNullTerm* is equivalent to a 2 element array of const char*, last one being a NULL terminator
    gtk_widget_set_css_classes(row_widget, empty_string_array);

    return GDK_ACTION_MOVE;
}

gboolean
bookmarks_columnview_on_drop_target_accept(GtkDropTarget* self, GdkDrop* drop, gpointer user_data)
{
    orion::log_verbose("bookmarks_columnview_on_drop_target_accept");
    return TRUE;
}

static void
bookmarks_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkBox* box = GTK_BOX(gtk_tree_expander_get_child(tree_expander));
	GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_tree_expander_set_list_row(tree_expander, row);
	BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(gtk_tree_list_row_get_item(row));
    defer([item]{ g_object_unref(item); });

    gtk_widget_unparent(gtk_widget_get_last_child(GTK_WIDGET(box)));
    const char* text = bookmarks_columnview_item_get_leftside_text(BOOKMARKS_COLUMNVIEW_ITEM(item))->str;
    OrionEditableCellEntry* entry = orion_editable_cell_entry_new(false);
    gtk_box_append(box, GTK_WIDGET(entry));
	orion_editable_cell_entry_set_text(entry,text);
    g_signal_connect(entry, "finished", G_CALLBACK(on_editable_cell_entry_finished), item);

    GtkImage* icon = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
	
    BookmarksColumnviewItemType type = bookmarks_columnview_item_get_item_type(item);
    //TODO: potentially needs reffing of item but the right click menu should never be accessible after closing
    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(box));


    orion::gtk::remove_click_controller(row_widget);
    orion::gtk::remove_dnd_controllers(row_widget);

    setup_cell_context_menu(BOOKMARKS_DIALOG(user_data), type, GTK_COLUMN_VIEW_CELL(listitem), row_widget, gtk_column_view_get_model(bookmarks_dialog_get_bookmarks_columnview(BOOKMARKS_DIALOG(user_data))));
    
    GtkDragSource* drag_source = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);

    //TODO: better data sending and just all around actually usable drag and drop handling, this is just a mockup
    g_signal_connect(drag_source, "prepare", G_CALLBACK(bookmarks_columnview_on_drag_source_prepare), listitem);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drag_source));

    GtkDropTarget* drop_target = gtk_drop_target_new(BOOKMARK_ARRAY_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(bookmarks_columnview_on_drop_target_drop), listitem);
    g_signal_connect(drop_target, "motion", G_CALLBACK(bookmarks_columnview_on_drop_target_motion), listitem);
    g_signal_connect(drop_target, "leave", G_CALLBACK(bookmarks_columnview_on_drop_target_leave), listitem);
    g_signal_connect(drop_target, "accept", G_CALLBACK(bookmarks_columnview_on_drop_target_accept), listitem);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drop_target));
    
    if(type==BookmarksColumnviewItemType::bookmark)
    {
        orion::Uri uri = orion::Uri::parse_user_input(bookmarks_columnview_item_get_rightside_text(item)->str);
        orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), icon, uri.uri().c_str());
    }
    else
    {
        gtk_image_set_from_icon_name(icon, bookmarks_columnview_item_get_icon_name(item));
    }
}

static void
bookmarks_factory_setup_b(GtkSignalListItemFactory* factory, GObject* item)
{
	GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_NONE);
    gtk_label_set_wrap(GTK_LABEL(label), FALSE);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
items_changed(GListModel* self, guint position, guint removed, guint added, gpointer user_data)
{
    if(removed!=added)
    {
        GtkLabel* label = GTK_LABEL(user_data);
        GString* string = g_string_new("");
        uint n_items = g_list_model_get_n_items(self);
        g_string_printf(string, ngettext("%u item", "%u items", n_items), n_items);
        gtk_label_set_text(GTK_LABEL(label), string->str);
        g_string_free(string, true);
    }
}

static void
bookmarks_factory_bind_b(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
	GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
    if(bookmarks_columnview_item_get_item_type(item) == BookmarksColumnviewItemType::bookmark)
    {
	    gtk_label_set_text(label, bookmarks_columnview_item_get_rightside_text(item)->str);
    }
    else
    {
        GListModel* store = G_LIST_MODEL(bookmarks_columnview_item_get_subdirectory_list_store(item));
        GString* string = g_string_new("");
        uint n_items = g_list_model_get_n_items(store);
        g_string_printf(string, ngettext("%u item", "%u items", n_items), n_items);
        gtk_label_set_text(GTK_LABEL(label), string->str);
        g_string_free(string, true);
        g_signal_connect_object(store, "items-changed", G_CALLBACK(items_changed),
            G_OBJECT(label), G_CONNECT_DEFAULT);
    }
}

void
bookmarks_columnview_model_user_data_destroy(gpointer data)
{
}

GListModel*
bookmarks_columnview_model_create_submodel (gpointer item, gpointer data)
{
    gpointer model = bookmarks_columnview_item_get_subdirectory_list_store(BOOKMARKS_COLUMNVIEW_ITEM(item));
    //NOTE: since we want the datamodels to be handled by the data items, not the tree list model
    //we have to increment ref count here as tree list model frees otherwise
    if(model) g_object_ref(model);
    return G_LIST_MODEL(model);
}

//NOTE: this is a recursive algorithm with pretty bad time complexity, I should delete from these vectors to make it better
//
static GListStore*
bookmarks_column_view_create_folder_list_store(orion::BookmarkFolderId folder_id,
                                       std::vector<orion::BookmarkDto>& bookmark_structs,
                                       std::vector<orion::BookmarkFolderDto>& bookmark_folder_structs)
{
    GListStore* this_folder = g_list_store_new(G_TYPE_OBJECT);
    for(auto& folder : bookmark_folder_structs)
    {
        if(folder.parent_folder_id == folder_id)
        {
            GListStore* subdirectory = bookmarks_column_view_create_folder_list_store(folder.id, bookmark_structs, bookmark_folder_structs);
            gpointer item = bookmarks_columnview_item_new_from_folder_struct(folder, this_folder, subdirectory);
            //TODO: fix naming scheme for what's a folder and what's a subdirectory
            g_list_store_append(this_folder, item);
            g_object_unref(item);
        }
    }
    for(auto& bookmark : bookmark_structs)
    {
        if(bookmark.parent_folder_id == folder_id)
        {
            gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, this_folder);
            g_list_store_insert_sorted(this_folder, item, bookmarks_items_sequence_number_compare, NULL);
            g_object_unref(item);
        }
    }
    return this_folder;
}

static void
on_selection_items_changed(GListModel* model, guint position, guint removed, guint added, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
    std::optional<orion::BookmarkFolderId> selected_folder_id = bookmarks_dialog_get_selected_bookmark_folder(dialog);

    if(selected_folder_id.has_value())
    {
        for(uint i = 0; i<added; i++)
        {
            gpointer thing = g_list_model_get_item(model, position+i);
            while(GTK_IS_TREE_LIST_ROW(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
            {
                if(bookmarks_columnview_item_get_id(item).folder_id == selected_folder_id)
                {
                    g_signal_handlers_disconnect_by_func(model, (void*)on_selection_items_changed, user_data);
                    bookmarks_dialog_set_selected_bookmark_folder(dialog, std::nullopt);
                    gtk_selection_model_select_item(GTK_SELECTION_MODEL(model), position+i, true);
                    GtkTreeListRow* row = GTK_TREE_LIST_ROW(g_list_model_get_item(model, position+i));
                    defer([row]{ g_object_unref(row); });
                    gtk_tree_list_row_set_expanded(row, true);
                }
            }
        }
    }
}

static void
bookmarks_column_view_handle_reply(GtkColumnView* view,
                                       std::vector<orion::BookmarkDto>& bookmark_structs,
                                       std::vector<orion::BookmarkFolderDto>& bookmark_folder_structs)
{

    GListStore* root_store = g_list_store_new(G_TYPE_OBJECT);
    for (auto& folder : bookmark_folder_structs)
    {
        if (folder.parent_folder_id.value == 0)
        {
            GListStore* subdirectory = bookmarks_column_view_create_folder_list_store(folder.id, bookmark_structs, bookmark_folder_structs);
            gpointer item = bookmarks_columnview_item_new_from_folder_struct(folder, root_store, subdirectory);
            g_list_store_insert_sorted(root_store, item, bookmarks_items_sequence_number_compare, NULL);
            g_object_unref(item);
        }
    }
    for (auto& bookmark : bookmark_structs)
    {
        if (bookmark.parent_folder_id.value == 0)
        {
            gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, root_store);
            g_list_store_insert_sorted(root_store, item, bookmarks_items_sequence_number_compare, NULL);
            g_object_unref(item);
        }
    }
    GtkTreeListModel* tree_list_model = gtk_tree_list_model_new(G_LIST_MODEL(root_store), FALSE, TRUE, bookmarks_columnview_model_create_submodel, NULL, bookmarks_columnview_model_user_data_destroy);
    GtkMultiSelection* selection = gtk_multi_selection_new(NULL);
    BookmarksDialog* dialog = reinterpret_cast<BookmarksDialog*>(gtk_widget_get_ancestor(GTK_WIDGET(view), BOOKMARKS_DIALOG_TYPE));
    if(dialog) g_signal_connect_object(selection, "items-changed", G_CALLBACK(on_selection_items_changed), dialog, G_CONNECT_DEFAULT);
    gtk_multi_selection_set_model(selection, G_LIST_MODEL(tree_list_model));
    gtk_column_view_set_model(view, GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });
}

static void
bookmarks_column_view_handle_reply_flat(GtkColumnView* view, std::vector<orion::BookmarkDto> bookmark_structs)
{
    GListStore* root_store = g_list_store_new(G_TYPE_OBJECT);

    for (auto& bookmark : bookmark_structs)
    {
        gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, root_store);
        g_list_store_insert_sorted(root_store, item, bookmarks_items_sequence_number_compare, NULL);
        g_object_unref(item);
    }
    // I know that the TreeListModel is greatly redundant. Look at it as a way of future-proofing ;P
    GtkTreeListModel* tree_list_model = gtk_tree_list_model_new(G_LIST_MODEL(root_store), FALSE, TRUE, bookmarks_columnview_model_create_submodel, NULL, bookmarks_columnview_model_user_data_destroy);
    GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(tree_list_model));
    gtk_column_view_set_model(view, GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });
}

//disconnects itself causing destruction of user_data, destroys column view model
//this should always ensure that model gets destroyed before observers disconnect,
//meaning data vectors get destroyed after the model
static void
on_bookmarks_column_view_unmap(GtkColumnView* bookmarks_column_view, gpointer user_data)
{
    g_object_set_data(G_OBJECT(bookmarks_column_view), "has-observers", GINT_TO_POINTER(FALSE));
    gtk_column_view_set_model(bookmarks_column_view, NULL);
    g_signal_handlers_disconnect_by_data(bookmarks_column_view, user_data);
}

static void
bookmarks_column_view_change_handlers_destroy(gpointer data, GClosure* closure)
{
    std::vector<orion::ObserverId>* vec = static_cast<std::vector<orion::ObserverId>*>(data);
    global_core_api->bookmark_folders_change().remove_observer(vec->at(0));
    global_core_api->bookmarks_change().remove_observer(vec->at(1));
    delete vec;
}

static void
bookmarks_columnview_model_insert_bookmark(GtkTreeListModel* tree_list_model, orion::BookmarkDto bookmark)
{
    GListStore* root = G_LIST_STORE(gtk_tree_list_model_get_model(tree_list_model));
    if(bookmark.parent_folder_id.value == 0)
    {
        gpointer item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, root);
        g_list_store_insert_sorted(root,
            item,
            bookmarks_items_sequence_number_compare, NULL);
        g_object_unref(item);
    }
    else
    {
        //goes through what the tree list model exposes (so only the renderable part of the tree)
        //finds folder with same id as our parent_folder_id, inserts into its subdirectory
        guint n = g_list_model_get_n_items(G_LIST_MODEL(tree_list_model));
        for(guint i = 0; i<n; i++)
        {
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(tree_list_model), i);
            while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
            {
                auto folder_id = bookmarks_columnview_item_get_id(item).folder_id;
                if(folder_id==bookmark.parent_folder_id)
                {
                    GListStore* store = bookmarks_columnview_item_get_subdirectory_list_store(item);
                    BookmarksColumnviewItem* item_insert = bookmarks_columnview_item_new_from_bookmark_struct(bookmark, store);
                    g_list_store_insert_sorted(store,
                        item_insert,
                        bookmarks_items_sequence_number_compare, NULL);
                    g_object_unref(item_insert);
                    break;
                }
            }
        }
    }
}

static void
bookmarks_columnview_model_insert_folder(GtkTreeListModel* tree_list_model, orion::BookmarkFolderDto folder)
{
    GListStore* root = G_LIST_STORE(gtk_tree_list_model_get_model(tree_list_model));
    if(folder.parent_folder_id.value == 0)
    {
        gpointer item = bookmarks_columnview_item_new_from_folder_struct(folder, root, g_list_store_new(G_TYPE_OBJECT));
        g_list_store_insert_sorted(root,
            item,
            bookmarks_items_sequence_number_compare, NULL);
        g_object_unref(item);
    }
    else
    {
        //goes through what the tree list model exposes (so only the renderable part of the tree)
        //finds folder with same id as our parent_folder_id, inserts into its subdirectory
        guint n = g_list_model_get_n_items(G_LIST_MODEL(tree_list_model));
        for(guint i = 0; i<n; i++)
        {
            gpointer thing = g_list_model_get_item(G_LIST_MODEL(tree_list_model), i);
            while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
            if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
            {
                auto folder_id = bookmarks_columnview_item_get_id(item).folder_id;
                if(folder_id==folder.parent_folder_id)
                {
                    GListStore* store = bookmarks_columnview_item_get_subdirectory_list_store(item);
                    BookmarksColumnviewItem* item_insert = bookmarks_columnview_item_new_from_folder_struct(folder, store, g_list_store_new(G_TYPE_OBJECT));
                    g_list_store_insert_sorted(store,
                        item_insert,
                        bookmarks_items_sequence_number_compare, NULL);
                    g_object_unref(item_insert);
                    break;
                }
            }
        }
    }
}

//sadly, merging this and bookmark remove gets messy with the std::variants
static void
bookmarks_columnview_model_folder_remove(GtkTreeListModel* tree_list_model, orion::BookmarkFolderId id)
{
    //goes through all of the items tree list model exposes, looks for same id
    //when found, grabs the parent directory list store, finds the item in that
    //when found, removes the item
    guint tree_list_model_n_items = g_list_model_get_n_items(G_LIST_MODEL(tree_list_model));
    gboolean not_yet_deleted = true;
    for(guint tree_list_model_index = 0; tree_list_model_index<tree_list_model_n_items && not_yet_deleted; tree_list_model_index++)
    {
        gpointer thing = g_list_model_get_item(G_LIST_MODEL(tree_list_model), tree_list_model_index);
        while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* tree_list_model_item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
        if(bookmarks_columnview_item_get_item_type(tree_list_model_item)!=BookmarksColumnviewItemType::bookmark)
        if(bookmarks_columnview_item_get_id(tree_list_model_item).folder_id==id)
        {
            GListStore* store = bookmarks_columnview_item_get_folder_list_store(tree_list_model_item);
            guint folder_list_store_n_items = g_list_model_get_n_items(G_LIST_MODEL(store));
            for(guint folder_list_store_index = 0; (folder_list_store_index<folder_list_store_n_items) && not_yet_deleted; folder_list_store_index++)
            {
                BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), folder_list_store_index));
                defer([item]{ g_object_unref(item); });
                if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
                if(bookmarks_columnview_item_get_id(item).folder_id==id)
                {
                    g_list_store_remove(store, folder_list_store_index);
                    not_yet_deleted = false;
                }
            }
        }
    }
}

static void
bookmarks_columnview_model_folder_update(GtkTreeListModel* tree_list_model, orion::BookmarkFolderDto folder)
{
    //goes through all of the items tree list model exposes, looks for same id
    //when found, grabs the parent directory list store, finds the item in that
    //when found, removes the item
    guint tree_list_model_n_items = g_list_model_get_n_items(G_LIST_MODEL(tree_list_model));
    gboolean not_yet_extracted = true;
    BookmarksColumnviewItem* extracted_item = NULL;
    for(guint tree_list_model_index = 0; tree_list_model_index<tree_list_model_n_items && not_yet_extracted; tree_list_model_index++)
    {
        gpointer thing = g_list_model_get_item(G_LIST_MODEL(tree_list_model), tree_list_model_index);
        while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* tree_list_model_item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
        if(bookmarks_columnview_item_get_item_type(tree_list_model_item)!=BookmarksColumnviewItemType::bookmark)
        if(bookmarks_columnview_item_get_id(tree_list_model_item).folder_id==folder.id)
        {
            GListStore* store = bookmarks_columnview_item_get_folder_list_store(tree_list_model_item);
            guint folder_list_store_n_items = g_list_model_get_n_items(G_LIST_MODEL(store));
            for(guint folder_list_store_index = 0; (folder_list_store_index<folder_list_store_n_items) && not_yet_extracted; folder_list_store_index++)
            {
                BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), folder_list_store_index));
                defer([item]{ g_object_unref(item); });
                if(bookmarks_columnview_item_get_item_type(item)!=BookmarksColumnviewItemType::bookmark)
                if(bookmarks_columnview_item_get_id(item).folder_id==folder.id)
                {
                    extracted_item = g_object_ref(tree_list_model_item);
                    g_list_store_remove(store, folder_list_store_index);
                    g_signal_handlers_disconnect_matched(bookmarks_columnview_item_get_subdirectory_list_store(item),
                        G_SIGNAL_MATCH_FUNC, 0, 0, NULL, gpointer(items_changed), NULL);
                    not_yet_extracted = false;
                }
            }
        }
    }
    if(extracted_item)
    {
        bookmarks_columnview_item_update_from_folder_dto(extracted_item, folder);
        
        if(folder.parent_folder_id.value)
        {
            std::vector<std::pair<GListStore*, uint>> store_stack;
            store_stack.push_back({G_LIST_STORE(gtk_tree_list_model_get_model(tree_list_model)), 0});

            //goes through the tree of folders held by BookmarksColumnviewItems,
            //using store_stack as a stack to keep track of parent list stores and explored indexes
            //looks for folder with id same as updated parent folder id
            while(store_stack.size())
            {
                GListStore* current_store = store_stack.back().first;
                uint n_items = g_list_model_get_n_items(G_LIST_MODEL(current_store));
                for(; store_stack.back().second<n_items; store_stack.back().second++)
                {
                    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(current_store), store_stack.back().second));
                    defer([item]{ g_object_unref(item); });
                    if(bookmarks_columnview_item_get_item_type(item) != BookmarksColumnviewItemType::bookmark)
                    {
                        if(bookmarks_columnview_item_get_id(item).folder_id == folder.parent_folder_id)
                        {
                            GListStore* store = bookmarks_columnview_item_get_subdirectory_list_store(item);
                            g_list_store_insert_sorted(store, extracted_item, bookmarks_items_sequence_number_compare, NULL);
                            bookmarks_columnview_item_set_parent_folder_list_store(extracted_item, store);
                            g_object_unref(extracted_item);
                            //item inserted back, can return
                            return;
                        }
                        else
                        {
                            store_stack.back().second++;
                            store_stack.push_back({bookmarks_columnview_item_get_subdirectory_list_store(item), 0});
                            break;
                        }
                    }
                }
                if(store_stack.back().second == n_items)
                {
                    store_stack.pop_back();
                }
            }
        }
        else
        {
            GListStore* store = G_LIST_STORE(gtk_tree_list_model_get_model(tree_list_model));
            g_list_store_insert_sorted(store, extracted_item, bookmarks_items_sequence_number_compare, NULL);
            bookmarks_columnview_item_set_parent_folder_list_store(extracted_item, store);
            g_object_unref(extracted_item);
            return;
        }


        //NOTE: if we reach this, it means that the data item has been unsuccessfully inserted into columnview, meaning something is wrong
        orion::log_debug("failed to insert updated item into bookmarks column view's data model");
    }
}

static void
bookmarks_columnview_model_bookmark_remove(GtkTreeListModel* tree_list_model, orion::BookmarkId id)
{
    //goes through all of the items tree list model exposes, looks for same id
    //when found, grabs the parent directory list store, finds the item in that
    //when found, removes the item
    guint tree_list_model_n_items = g_list_model_get_n_items(G_LIST_MODEL(tree_list_model));
    gboolean not_yet_deleted = true;
    for(guint tree_list_model_index = 0; tree_list_model_index<tree_list_model_n_items && not_yet_deleted; tree_list_model_index++)
    {
        gpointer thing = g_list_model_get_item(G_LIST_MODEL(tree_list_model), tree_list_model_index);
        while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        BookmarksColumnviewItem* tree_list_model_item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
        if(bookmarks_columnview_item_get_item_type(tree_list_model_item)==BookmarksColumnviewItemType::bookmark)
        if(bookmarks_columnview_item_get_id(tree_list_model_item).bookmark_id==id)
        {
            GListStore* store = bookmarks_columnview_item_get_folder_list_store(tree_list_model_item);
            guint folder_list_store_n_items = g_list_model_get_n_items(G_LIST_MODEL(store));
            for(guint folder_list_store_index = 0; (folder_list_store_index<folder_list_store_n_items) && not_yet_deleted; folder_list_store_index++)
            {
                BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), folder_list_store_index));
                defer([item]{ g_object_unref(item); });
                if(bookmarks_columnview_item_get_item_type(item)==BookmarksColumnviewItemType::bookmark)
                if(bookmarks_columnview_item_get_id(item).bookmark_id==id)
                {
                    g_list_store_remove(store, folder_list_store_index);
                    not_yet_deleted = false;
                }
            }
        }
    }
}

void
on_bookmarks_folders_change(GtkColumnView* column_view, const orion::BookmarkFoldersChange& change)
{
    GtkSelectionModel* selection = gtk_column_view_get_model(column_view);
    if(!selection) return;
    GtkTreeListModel* tree_list_model;
    if(GTK_IS_MULTI_SELECTION(selection)) {
        tree_list_model = GTK_TREE_LIST_MODEL(gtk_multi_selection_get_model(GTK_MULTI_SELECTION(selection)));
    } else if(GTK_IS_NO_SELECTION(selection)) {
        tree_list_model = GTK_TREE_LIST_MODEL(gtk_no_selection_get_model(GTK_NO_SELECTION(selection)));
    } else {
        return; //no model, nothing to do
    }

    //all of these first update the vector the columnview is based on,
    //then call functions to update the visible model
    for(orion::BookmarkFolderDto folder_created : change.created)
    {
        BookmarksColumnviewItem* item = bookmarks_columnview_item_new_from_folder_struct(folder_created, NULL, g_list_store_new(G_TYPE_OBJECT));
        bookmarks_columnview_model_insert_folder(tree_list_model, folder_created);
        g_object_unref(item);
    }

    for(orion::BookmarkFolderDto folder_updated : change.updated)
    {
        orion::BookmarkFolderId id = folder_updated.id;

        bookmarks_columnview_model_folder_update(tree_list_model, folder_updated);
    }

    for(orion::BookmarkFolderId folder_deleted : change.deleted)
    {

        bookmarks_columnview_model_folder_remove(tree_list_model, folder_deleted);
    }
}

void
on_bookmarks_change(GtkColumnView* column_view, const orion::BookmarksChange& change)
{
    GtkSelectionModel* selection = gtk_column_view_get_model(column_view);
    if(!selection) return;
    GtkTreeListModel* tree_list_model;
    if(GTK_IS_MULTI_SELECTION(selection)) {
        tree_list_model = GTK_TREE_LIST_MODEL(gtk_multi_selection_get_model(GTK_MULTI_SELECTION(selection)));
    } else if(GTK_IS_NO_SELECTION(selection)) {
        tree_list_model = GTK_TREE_LIST_MODEL(gtk_no_selection_get_model(GTK_NO_SELECTION(selection)));
    } else {
        return; //no model, nothing to do
    }

    //all of these first update the vector the columnview is based on,
    //then call functions to update the visible model
    for(orion::BookmarkDto bookmark_created : change.created)
    {
        BookmarksColumnviewItem* item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark_created, NULL);
        bookmarks_columnview_model_insert_bookmark(tree_list_model, bookmark_created);
        g_object_unref(item);
    }

    for(orion::BookmarkId bookmark_deleted : change.deleted)
    {
        bookmarks_columnview_model_bookmark_remove(tree_list_model, bookmark_deleted);
    }

    for(orion::BookmarkDto bookmark_updated : change.updated)
    {
        orion::BookmarkId id = bookmark_updated.id;

        bookmarks_columnview_model_bookmark_remove(tree_list_model, bookmark_updated.id);
        BookmarksColumnviewItem* item = bookmarks_columnview_item_new_from_bookmark_struct(bookmark_updated, NULL);
        bookmarks_columnview_model_insert_bookmark(tree_list_model, bookmark_updated);
        g_object_unref(item);
    }
}

void
bookmarks_column_view_prepare_model_with_query(GtkColumnView* bookmarks_column_view, std::string query_str)
{
    g_object_ref(bookmarks_column_view);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(bookmarks_column_view)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::gtk::WeakRefWrapper ref(bookmarks_column_view);

    fetch_bookmarks_data(profile_id, std::move(query_str), [ref](BookmarksAllReply reply){
        VERIFY_LOG(reply.error);
        auto ptr = ref.lock();
        if (!ptr) return;

        GtkColumnView* bookmarks_column_view = GTK_COLUMN_VIEW(ptr.get());
        bookmarks_column_view_handle_reply_flat(bookmarks_column_view, std::move(reply.bookmarks));
    }); 
        
    gboolean flag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(bookmarks_column_view), "has-observers"));
    if(!flag){
        orion::ObserverId folder_observer_id = global_core_api->bookmark_folders_change().add_observer([bookmarks_column_view](const orion::BookmarkFoldersChange& change){
            on_bookmarks_folders_change(bookmarks_column_view, change); 
        });
        orion::ObserverId bookmark_observer_id = global_core_api->bookmarks_change().add_observer([bookmarks_column_view](const orion::BookmarksChange& change){
            on_bookmarks_change(bookmarks_column_view, change);
        });

        g_object_set_data(G_OBJECT(bookmarks_column_view), "has-observers", GINT_TO_POINTER(TRUE));

            //unmap signal disconnects itself
        std::vector<orion::ObserverId>* observer_id_vec = new std::vector<orion::ObserverId>;
        observer_id_vec->push_back(folder_observer_id);
        observer_id_vec->push_back(bookmark_observer_id);
        g_signal_connect_data(bookmarks_column_view, "unmap", G_CALLBACK(on_bookmarks_column_view_unmap), observer_id_vec, bookmarks_column_view_change_handlers_destroy, G_CONNECT_DEFAULT);

    }
}




void
bookmarks_column_view_prepare_model(GtkColumnView* bookmarks_column_view)
{
    g_object_ref(bookmarks_column_view);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(bookmarks_column_view)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(bookmarks_column_view);
    global_core_api->get_bookmark_folder_ids(profile_id, [wrapped_ref, profile_id](orion::GetBookmarkFolderIdsReply reply)
    {
        VERIFY_LOG(reply.err);
        orion::log_verbose("bookmarks_column_view_map(): got %zu bookmark folder ids", reply.ids.size()); 
        
        global_core_api->get_bookmark_folders(reply.ids, [wrapped_ref, profile_id](orion::GetBookmarkFoldersReply reply)
        {
            VERIFY_LOG(reply.err);
            orion::log_verbose("bookmarks_column_view_map(): got %zu bookmark folders", reply.folders.size());
            //these vectors die with the columnview so I need to remove the observer before the columnview dies
            auto folders_copy = std::make_shared<std::vector<orion::BookmarkFolderDto>>(reply.folders);
            //only stays alive when column view is mapped so all data should still be alive

            orion::GetBookmarkIdsQuery query{};
            query.profile_id = profile_id;
            global_core_api->get_bookmark_ids(query, [wrapped_ref, folders_copy](orion::GetBookmarkIdsReply reply)
            {
                VERIFY_LOG(reply.err);
                orion::log_verbose("bookmarks_column_view_map(): got %zu bookmark ids", reply.ids.size());
                
                orion::GetBookmarksQuery query{
                    .ids = reply.ids,
                };
                global_core_api->get_bookmarks(query, [wrapped_ref, folders_copy](orion::GetBookmarksReply reply)
                {
                    VERIFY_LOG(reply.err);
                    GtkColumnView* bookmarks_column_view = GTK_COLUMN_VIEW(wrapped_ref->get());
                    if(bookmarks_column_view)
                    {
                        orion::log_verbose("bookmarks_column_view_map(): got %zu bookmarks", reply.bookmarks.size());
                        
                        bookmarks_column_view_handle_reply(bookmarks_column_view, reply.bookmarks, *folders_copy);
                        g_object_unref(bookmarks_column_view);
                    }
                });
            });
        });
    });

    gboolean flag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(bookmarks_column_view), "has-observers"));
    if(!flag){
        orion::ObserverId folder_observer_id = global_core_api->bookmark_folders_change().add_observer([bookmarks_column_view](const orion::BookmarkFoldersChange& change){
            on_bookmarks_folders_change(bookmarks_column_view, change); 
        });
        orion::ObserverId bookmark_observer_id = global_core_api->bookmarks_change().add_observer([bookmarks_column_view](const orion::BookmarksChange& change){
            on_bookmarks_change(bookmarks_column_view, change);
        });

        g_object_set_data(G_OBJECT(bookmarks_column_view), "has-observers", GINT_TO_POINTER(TRUE));

            //unmap signal disconnects itself
        std::vector<orion::ObserverId>* observer_id_vec = new std::vector<orion::ObserverId>;
        observer_id_vec->push_back(folder_observer_id);
        observer_id_vec->push_back(bookmark_observer_id);
        g_signal_connect_data(bookmarks_column_view, "unmap", G_CALLBACK(on_bookmarks_column_view_unmap), observer_id_vec, bookmarks_column_view_change_handlers_destroy, G_CONNECT_DEFAULT);

    }}

void
bookmarks_column_view_map(GtkColumnView* bookmarks_column_view, BookmarksDialog* win)
{
    bookmarks_column_view_prepare_model(bookmarks_column_view);
}

void
bookmarks_column_view_map_flat_query(GtkColumnView* bookmarks_column_view, BookmarksDialog* win, std::string query)
{
    bookmarks_column_view_prepare_model_with_query(bookmarks_column_view, std::move(query));
}


gboolean
column_view_on_drop_target_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("columnview_on_drop_target_drop");
    GtkColumnView* view = GTK_COLUMN_VIEW(user_data);
    if(G_VALUE_HOLDS(value, BOOKMARK_ARRAY_TYPE))
    {
        GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(view));
        if(selection)
        {
            double seq_num = 0;
            GtkTreeListModel* model = GTK_TREE_LIST_MODEL(gtk_multi_selection_get_model(selection));
            GListModel* root = G_LIST_MODEL(gtk_tree_list_model_get_model(model));
            
            uint n_items = g_list_model_get_n_items(root);
            if(n_items)
            {
                BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(g_list_model_get_item(root, n_items-1));
                defer([item]{ g_object_unref(item); });
                seq_num = bookmarks_columnview_item_get_sequence_number(item)+1;
            }


            
            GArray* dropped_array = static_cast<BookmarkArray*>(g_value_get_boxed(value))->array;
            const IdAndType* dropped_items = (IdAndType*)(dropped_array->data); 


            for(guint i = 0; i<dropped_array->len; i++)
            {
                IdAndType dropped_item = dropped_items[i];
                if(dropped_item.type == BookmarksColumnviewItemType::bookmark)
                {
                    orion::UpdateBookmarkQuery query;
                    query.id = dropped_item.id.bookmark_id;
                    query.seq_num = seq_num;
                    query.update_seq_num = true;
                    query.parent_folder_id = orion::BookmarkFolderId(0); // root folder
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark(query, [](orion::Error err){VERIFY_LOG(err);});
                }
                else
                {
                    orion::UpdateBookmarkFolderQuery query;
                    query.id = dropped_item.id.folder_id;
                    query.seq_num = seq_num;
                    query.update_seq_num = true;
                    query.parent_folder_id = orion::BookmarkFolderId(0); // root folder
                    query.update_parent_folder_id = true;
                    global_core_api->update_bookmark_folder(query, [](orion::Error err){VERIFY_LOG(err);});
                }
                seq_num++;
            }

            return TRUE;
        }
        else
        {
            orion::log_error("attempted dropping into unitialized bookmarks column view, ignoring");
            return FALSE;
        }
    }
    return FALSE;
}

void
bookmarks_column_view_init(GtkColumnView* bookmarks_column_view, BookmarksDialog* win)
{
    g_type_ensure(BOOKMARK_ARRAY_TYPE);
    setup_column_view_context_menu(win, bookmarks_column_view);
    
	GtkListItemFactory* factory;
	GtkColumnViewColumn* column;

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(bookmarks_factory_setup_a), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bookmarks_factory_bind_a), win);
	column = gtk_column_view_column_new("Website", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(bookmarks_column_view, column);
	g_object_unref(column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(bookmarks_factory_setup_b), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(bookmarks_factory_bind_b), win);
	column = gtk_column_view_column_new("Address", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(bookmarks_column_view, column);
	g_object_unref(column);

    GtkDropTarget* drop_target = gtk_drop_target_new(BOOKMARK_ARRAY_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(column_view_on_drop_target_drop), bookmarks_column_view);
    gtk_widget_add_controller(GTK_WIDGET(bookmarks_column_view), GTK_EVENT_CONTROLLER(drop_target));
}

void
on_bookmarks_column_view_activate(GtkColumnView* view, guint pos, gpointer user_data)
{
    GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(view));
    GListModel* model = G_LIST_MODEL(gtk_multi_selection_get_model(selection));
    gpointer thing = g_list_model_get_item(model, pos);
    while(!BOOKMARKS_COLUMNVIEW_IS_ITEM(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    BookmarksColumnviewItem* item = BOOKMARKS_COLUMNVIEW_ITEM(thing);
    if(bookmarks_columnview_item_get_item_type(item)==BookmarksColumnviewItemType::bookmark)
    {
        main_window_load_bookmark(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))), bookmarks_columnview_item_get_rightside_text(item)->str);
    }
}