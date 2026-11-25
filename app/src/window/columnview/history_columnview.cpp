#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>

#include "window/bookmarks_dialog.h"
#include "app.h"
#include "data_item/history_columnview_item.h"
#include "history_columnview.h"
#include "window/main_window/main_window.h"
#include "util/favicon_util.h"
#include "util/ref_util.h"
#include "common/src/ptr_util.h"
#include "util/menu_util.h"

#include <common/src/defer.h>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

static void
history_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	GtkWidget* tree_expander = gtk_tree_expander_new();
	gtk_widget_set_hexpand(tree_expander, TRUE);
	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(tree_expander), label);
	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), tree_expander);
}

static GtkPopoverMenu*
create_history_address_context_menu(std::string uri, orion::HistoryEntryId id)
{
    GMenu* root = g_menu_new();
    defer([root]{ g_object_unref(root); });

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        /*
        orion::gtk::menu_append_action_with_string_target(
            section, _("Open in New Tab"), "win.open-history-address-in-new-tab", uri.c_str()
        );
        
        orion::gtk::menu_append_action_with_string_target(
            section, _("Open in New Window"), "win.open-history-address-in-new-window", uri.c_str()
        );
        */
        g_menu_append(section, _("Open in New Tab"), "win.open-history-address-in-new-tab");
        g_menu_append(section, _("Open in New Window"), "win.open-history-address-in-new-window");

        g_menu_append_section(root, NULL, G_MENU_MODEL(section));
    }
    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        /*
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Copy"), "win.copy-history-address", id.value
        );
        */
        
        g_menu_append(section, _("Copy"), "win.copy-history-address");
        
        //TODO:
        g_menu_append(section, _("Delete"), "win.history-columnview-delete");

        g_menu_append_section(root, NULL, G_MENU_MODEL(section));
    }

    GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(gtk_popover_menu_new_from_model(G_MENU_MODEL(root)));
    return popover_menu;
}

static void
on_popover_closed(GtkWidget* popover)
{
    g_idle_add_once([](gpointer data){ gtk_widget_unparent(GTK_WIDGET(data)); }, popover);
}

static void
on_address_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkWidget* row = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
    
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(row));
    if(!win) return;
	
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(GTK_COLUMN_VIEW_CELL(user_data)));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(thing);
    GtkPopover* popover = GTK_POPOVER(create_history_address_context_menu(history_columnview_item_get_address(item)->str, history_columnview_item_get_id(item)));
	// Position of the widget + position of the click

    BookmarksDialog* dialog = BOOKMARKS_DIALOG(gtk_widget_get_ancestor(row, BOOKMARKS_DIALOG_TYPE));
    graphene_point_t source{(float)x, (float)y};
    graphene_point_t target{};
	if (!gtk_widget_compute_point(row, GTK_WIDGET(dialog), &source, &target)) {
		orion::log_warning("button_released_handler: failed to compute point");
	}


	GdkRectangle rect{
		.x = (int)target.x,
		.y = (int)target.y,
		.width = 1,
		.height = 1,
	};
    gtk_widget_set_parent(GTK_WIDGET(popover), GTK_WIDGET(dialog));
	gtk_popover_set_pointing_to(popover, &rect);
	gtk_popover_set_has_arrow(popover, FALSE);


    
    ContextMenuData context_menu_data;
    context_menu_data.user_data = gtk_column_view_get_model(GTK_COLUMN_VIEW(gtk_widget_get_ancestor(row, GTK_TYPE_COLUMN_VIEW)));
    main_window_set_context_menu_data(win, std::move(context_menu_data));
    //TODO: this is a kinda hacky solution to pass through the columnview cell that I have right clicked, also row would be better but how do I access that?
    main_window_set_last_right_clicked_widget(win,(GtkWidget*)user_data);


    g_signal_connect_after(popover, "closed", G_CALLBACK(on_popover_closed), NULL);
    //gtk_popover_present(popover);
	gtk_popover_popup(popover);

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}


static void
on_folder_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    //TODO: data->child is the columnview cell instead of the row, messy but here we go
    GtkWidget* row = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(GTK_COLUMN_VIEW_CELL(data->child)));
	
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
    main_window_set_context_menu_data(win, std::move(context_menu_data));
    //TODO: this is a kinda hacky solution to pass through the columnview cell that I have right clicked, also row would be better but how do I access that?
    main_window_set_last_right_clicked_widget(win,(GtkWidget*)data->child);
}

static void
setup_context_menu(
    BookmarksDialog* dialog, HistoryColumnviewItemType type, GtkColumnViewCell* cell,
    GtkWidget* row_widget, GtkSelectionModel* selection, HistoryColumnviewItem* item)
{
    // switch by item type (folder/item)

    if(type == HistoryColumnviewItemType::folder)
    {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData{};
        data->parent = GTK_WIDGET(dialog);
        data->child = cell;
        data->popover_menu = bookmarks_dialog_get_history_folder_popover_menu(dialog);
        data->user_data = selection;
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(dialog), GTK_WIDGET(row_widget), on_folder_row_secondary_released, data,
            [](gpointer data, GClosure* closure){
                delete static_cast<orion::gtk::ReleasedCallbackData*>(data);
            });
    }
    else
    {

        std::string uri = history_columnview_item_get_address(item)->str;
        orion::HistoryEntryId id = history_columnview_item_get_id(item);
        create_history_address_context_menu(uri, id);
        orion::gtk::register_right_click_with_user_data(GTK_WIDGET(row_widget), GTK_WIDGET(row_widget), on_address_row_secondary_released, cell);
    }
}

static int
history_item_compare(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    (void)user_data;
    GDateTime* date_a = history_columnview_item_get_date(HISTORY_COLUMNVIEW_ITEM(const_cast<gpointer>(item_a)));
    GDateTime* date_b = history_columnview_item_get_date(HISTORY_COLUMNVIEW_ITEM(const_cast<gpointer>(item_b)));
    return g_date_time_to_unix(date_b)-g_date_time_to_unix(date_a);
}

static void
history_column_view_day_created(GListStore* store, const orion::HistoryEntryDay& history_day_created)
{
    gpointer item = history_columnview_item_new_from_day(history_day_created);
    g_list_store_insert_sorted(store, item, history_item_compare, NULL); 
    g_object_unref(item);
}

static void
history_column_view_day_deleted(GListStore* store, orion::UnixTimeUtc64 history_day_deleted)
{
    GDateTime* day = g_date_time_new_from_unix_utc(history_day_deleted.value);
    uint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(uint i = 0; i<len; i++)
    {
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(g_date_time_equal(day, history_columnview_item_get_date(item)))
        {
            g_list_store_remove(store, i);
            break;
        }
    }
    g_date_time_unref(day);
}

static void
history_column_view_day_updated(GListStore* store, const orion::HistoryEntryDay& history_day_updated)
{
    GDateTime* day = g_date_time_new_from_unix_utc(history_day_updated.start_of_day.value);
    uint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(uint i = 0; i<len; i++)
    {
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(g_date_time_equal(day, history_columnview_item_get_date(item)))
        {
            history_columnview_item_set_items(item, history_day_updated.visit_count);
            break;
        }
    }
    g_date_time_unref(day);
}

static void
history_column_view_handle_day_change(GListStore* store, orion::HistoryEntryDaysChange change)
{
    for(orion::HistoryEntryDay history_day_created : change.created)
    {
        history_column_view_day_created(store, history_day_created);
    }
    for(orion::UnixTimeUtc64 history_day_deleted : change.deleted)
    {
        history_column_view_day_deleted(store, history_day_deleted);
    }
    for(orion::HistoryEntryDay history_day_updated : change.updated)
    {
        history_column_view_day_updated(store, history_day_updated);
    }
}

static void
history_column_view_entry_created(GListStore* store, const orion::HistoryEntryDto& entry_created)
{
    gpointer item = history_columnview_item_new_from_history_struct(entry_created, store);
    g_list_store_insert_sorted(store, item, history_item_compare, NULL);
    g_object_unref(item);
}

static void
history_column_view_entry_deleted(GListStore* store, orion::HistoryEntryId entry_deleted)
{
    guint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(guint i = 0; i<len; i++)
    {
        HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(history_columnview_item_get_id(item) == entry_deleted)
        {
            g_list_store_remove(store, i);
            break;
        }
    }
}

static void
history_column_view_handle_entry_change(HistoryColumnviewItem* item, const orion::HistoryEntriesChange& change)
{
    GListStore* store = history_columnview_item_get_list_store(item);
    GDateTime* before_time = history_columnview_item_get_date(item);
    GDateTime* after_time = g_date_time_add_days(before_time, 1);
    int64_t before = g_date_time_to_unix(before_time);
    int64_t after = g_date_time_to_unix(after_time);
    g_date_time_unref(after_time);
    for(orion::HistoryEntryDto history_entry_created: change.created)
    {
        if((history_entry_created.visited_at.value > before) && (history_entry_created.visited_at.value < after))
        {
            history_column_view_entry_created(store, history_entry_created);
        }
    }
    for(orion::HistoryEntryId history_entry_deleted: change.deleted)
    {
        history_column_view_entry_deleted(store, history_entry_deleted);
    }
    for(orion::HistoryEntryDto history_entry_updated: change.updated)
    {
        if((history_entry_updated.visited_at.value > before) && (history_entry_updated.visited_at.value < after))
        {
            history_column_view_entry_deleted(store, history_entry_updated.id);
            history_column_view_entry_created(store, history_entry_updated);
        }
    }
}

static void
history_column_view_row_expanded_changed(GtkTreeListRow* row, GParamSpec* pspec, gpointer data)
{
    orion::log_verbose("history_column_view_row_expanded_changed");
    gpointer thing = gtk_tree_list_row_get_item(row);
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    orion::gtk::GObjectPtr strong_ptr(G_OBJECT(thing));

    if (!BOOKMARKS_IS_DIALOG(data)) {
        // can happen when closing Bookmarks dialog
        orion::log_warning("history_column_view_row_expanded_changed: not bookmarks dialog: %s", G_OBJECT_TYPE_NAME(data));
        return;
    }

    BookmarksDialog* dialog = BOOKMARKS_DIALOG(data);
    if (!dialog) return;

    std::string search_filter = bookmarks_dialog_get_history_search_filter(dialog);

    HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(strong_ptr.get());
    if(gtk_tree_list_row_get_expanded(row))
    {
        //time for folder should always be start of day
        GDateTime* time = history_columnview_item_get_date(item);
        GListStore* store = history_columnview_item_get_list_store(item);

        orion::GetHistoryEntryIdsQuery query;
        query.visited_after = orion::UnixTimeUtc64(g_date_time_to_unix(time));
        GDateTime* time_later = g_date_time_add_days(time, 1);
        query.visited_before = orion::UnixTimeUtc64(g_date_time_to_unix(time_later));
        query.profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(data))));
        query.search_filter = search_filter;
        g_date_time_unref(time_later);

        auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(store);
        global_core_api->get_history_entry_ids(query, [wrapped_ref](orion::GetHistoryEntryIdsReply reply)
        {
            VERIFY_LOG(reply.err);
            global_core_api->get_history_entries(reply.ids, [wrapped_ref](orion::GetHistoryEntriesReply reply)
            {
                VERIFY_LOG(reply.err);
                GListStore* store = G_LIST_STORE(wrapped_ref->get());
                if(store)
                {
                    g_list_store_remove_all(store);
                    for(orion::HistoryEntryDto history_entry : reply.entries)
                    {
                        gpointer item = history_columnview_item_new_from_history_struct(history_entry, store);
                        g_list_store_insert_sorted(store, item, history_item_compare, NULL);
                        g_object_unref(item);
                    }
                    g_object_unref(store);
                }
            });
        });
        
        // We can register the observer and store its id in the item itself and then rely on the finalize
        // method of the item to remove the observer. The issue with this approach is that finalize method
        // of the item is not guaranteed to be called when the dialog is closed (the reason for this is
        // that someone is still holding a strong ref to them), so if the user does not collapse all
        // folders before closing the dialog, at least one observer may remain registered. So we register
        // all these observers in Bookmarks dialog too and remove all of them that remained when the dialog
        // is closed.
        orion::ObserverId id = global_core_api->history_entries_change().add_observer([item](const orion::HistoryEntriesChange& change){
            history_column_view_handle_entry_change(item, change);
        });
        history_columnview_item_set_observer_id(item, id);
        bookmarks_dialog_add_history_entries_observer_id(dialog, id);
    }
    else
    {
        // The user collapsed the folder, so we remove the observer. Note that the user do not have
        // to collapse all the folders before closing the dialog, so some observers will remain
        // registered. These will be removed when the dialog is closed.
        std::optional<orion::ObserverId> id = history_columnview_item_get_observer_id(item);
        if(id.has_value())
        {
            global_core_api->history_entries_change().remove_observer(id.value());
            history_columnview_item_set_observer_id(item, std::nullopt);
            bookmarks_dialog_remove_history_entries_observer_id(dialog, id.value());
        }
    }
}

static void
history_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
	GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkLabel* label = GTK_LABEL(gtk_tree_expander_get_child(tree_expander));
	GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_tree_expander_set_list_row(tree_expander, row);
    gpointer thing = gtk_tree_list_row_get_item(row);
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    orion::gtk::GObjectPtr strong_ptr(G_OBJECT(thing));
	HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(strong_ptr.get());
    HistoryColumnviewItemType type = history_columnview_item_get_item_type(item);


    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(tree_expander));

    orion::gtk::remove_click_controller(row_widget);

    if (type == HistoryColumnviewItemType::folder)
    {
        // TODO: fix this
	    //gtk_label_set_text(label, g_date_time_format(history_columnview_item_get_date(HISTORY_COLUMNVIEW_ITEM(item)), "%d. %m. %Y"));
        g_signal_connect(row, "notify::expanded", G_CALLBACK(history_column_view_row_expanded_changed), dialog);
        GDateTime* date = history_columnview_item_get_date(item);
        gchar* date_string = g_date_time_format(date, "%d. %m. %y");
        gtk_label_set_text(label, date_string);
        g_free(date_string);
    }
    else
    {
        gtk_label_set_text(label, g_date_time_format(history_columnview_item_get_date(HISTORY_COLUMNVIEW_ITEM(item)), "%T"));
    }

    GtkSelectionModel* selection = gtk_column_view_get_model(bookmarks_dialog_get_history_columnview(dialog));
    
    setup_context_menu(dialog, type, GTK_COLUMN_VIEW_CELL(listitem), row_widget, selection, item);
}

/*
static void
history_factory_unbind_a(GtkSignalListItemFactory* factory, GObject* listitem)
{

}
*/

static void
history_factory_setup_b(GtkSignalListItemFactory* factory, GObject* item)
{
	GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);

    GtkWidget* icon = gtk_image_new();

    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 15);
	gtk_widget_set_hexpand(box, TRUE);
	gtk_box_append(GTK_BOX(box), GTK_WIDGET(icon));
	gtk_box_append(GTK_BOX(box), GTK_WIDGET(label));

	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_list_item_set_child(GTK_LIST_ITEM(item), box);
}

static void
history_factory_bind_b(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
    GtkBox* box = GTK_BOX(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    GtkImage* icon = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
    GtkLabel* label = GTK_LABEL(gtk_widget_get_last_child(GTK_WIDGET(box)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    orion::gtk::GObjectPtr strong_ptr(G_OBJECT(thing));
	HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(strong_ptr.get());

    HistoryColumnviewItemType type = history_columnview_item_get_item_type(item);
    if (type == HistoryColumnviewItemType::address)
    {
        gtk_label_set_text(label, (history_columnview_item_get_website(HISTORY_COLUMNVIEW_ITEM(thing)))->str);

        orion::Uri uri = orion::Uri::parse_user_input(history_columnview_item_get_address(item)->str);
        orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), icon, uri.uri().c_str());
    }
}

static void
history_factory_unbind_b(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    GtkBox* box = GTK_BOX(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    GtkImage* icon = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
    GtkLabel* label = GTK_LABEL(gtk_widget_get_last_child(GTK_WIDGET(box)));
    gtk_image_clear(icon);
    gtk_label_set_label(label, "");
}

static void
history_factory_setup_c(GtkSignalListItemFactory* factory, gpointer item)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
history_factory_bind_c(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    orion::gtk::GObjectPtr strong_ptr(G_OBJECT(thing));
	HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(strong_ptr.get());
    if(history_columnview_item_get_item_type(item)==HistoryColumnviewItemType::address)
    {
        gtk_label_set_text(label, (history_columnview_item_get_address(item))->str);
    }
    else
    {
        history_columnview_item_set_items_label(item, label);
    }
}

static void
history_factory_unbind_c(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while(GTK_IS_TREE_LIST_ROW(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    orion::gtk::GObjectPtr strong_ptr(G_OBJECT(thing));
	HistoryColumnviewItem* item = HISTORY_COLUMNVIEW_ITEM(strong_ptr.get());
    history_columnview_item_set_items_label(item, NULL);
}

GListModel*
history_tree_list_model_create(gpointer item, gpointer data)
{
	if (history_columnview_item_get_item_type(HISTORY_COLUMNVIEW_ITEM(item)) == HistoryColumnviewItemType::folder) //TODO: proper data loading
	{
	    GListStore* store = history_columnview_item_get_list_store(HISTORY_COLUMNVIEW_ITEM(item));
        g_object_ref(store);
        return G_LIST_MODEL(store);
	}
	else
	{
		return NULL;
	}
}

void
history_column_view_unmap(GtkColumnView* history_column_view, gpointer user_data)
{
    orion::log_verbose("history_column_view_unmap");
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);

    orion::ObserverId observer_id = static_cast<orion::ObserverId>(orion::void_ptr_to_int64(
        g_object_get_data(G_OBJECT(history_column_view), "history-entry-days-observer-id")
    ));
    if (observer_id.value != 0) {
        global_core_api->history_entry_days_change().clear_observer(&observer_id);
        g_object_set_data(G_OBJECT(history_column_view), "history-entry-days-observer-id", orion::int64_to_void_ptr(0));
    }

    for (orion::ObserverId id : bookmarks_dialog_get_history_entries_observer_ids(dialog)) {
        global_core_api->history_entries_change().remove_observer(id);
    }
    bookmarks_dialog_clear_history_entries_observer_ids(dialog);

    gtk_column_view_set_model(history_column_view, NULL);
}

void
history_column_view_map(GtkColumnView* history_column_view, BookmarksDialog* dialog)
{
    orion::log_verbose("history_column_view_map");
	GListStore* root = g_list_store_new(G_TYPE_OBJECT);

	GtkTreeListModel* model = gtk_tree_list_model_new(G_LIST_MODEL(root), FALSE, FALSE, history_tree_list_model_create, NULL, NULL);

    orion::gtk::WeakRefWrapper root_weak(root);
    orion::gtk::WeakRefWrapper model_weak(model);

    MainWindow* main_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(history_column_view)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_win);

    orion::GetNonemptyHistoryDaysQuery query{
        .profile_id = profile_id,
        .search_filter = bookmarks_dialog_get_history_search_filter(dialog),
    };

    global_core_api->get_nonempty_history_days(query, [root_weak,model_weak](orion::GetNonemptyHistoryDaysReply reply){
        VERIFY_LOG(reply.err);
        
        auto root_strong = root_weak.lock();
        if (!root_strong) return;
        GListStore* root = G_LIST_STORE(root_strong.get());

        auto model_strong = model_weak.lock();
        if (!model_strong) return;
        GtkTreeListModel* model = GTK_TREE_LIST_MODEL(model_strong.get());
        
        g_list_store_remove_all(root);
        for(orion::HistoryEntryDay day : std::vector<orion::HistoryEntryDay>(reply.days))
        {
            gpointer item = history_columnview_item_new_from_day(day);
            g_list_store_insert_sorted(root, item, history_item_compare, NULL);
            g_object_unref(item);
        }

        GtkTreeListRow* first_row = gtk_tree_list_model_get_row(model, 0);
        if (first_row) {
            gtk_tree_list_row_set_expanded(first_row, true);
            g_object_unref(first_row);
        }
    });

	GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(model));
	gtk_column_view_set_model(history_column_view, GTK_SELECTION_MODEL(selection));
    g_object_unref(selection);

    orion::ObserverId observer_id = static_cast<orion::ObserverId>(orion::void_ptr_to_int64(
        g_object_get_data(G_OBJECT(history_column_view), "history-entry-days-observer-id")
    ));

    if (observer_id.value == 0) {
        observer_id = global_core_api->history_entry_days_change().add_observer([root_weak](const orion::HistoryEntryDaysChange& change){
            auto root_strong = root_weak.lock();
            if (!root_strong) return;
            GListStore* root = G_LIST_STORE(root_strong.get());
            history_column_view_handle_day_change(root, change);
        });

        g_object_set_data(G_OBJECT(history_column_view), "history-entry-days-observer-id",
            orion::int64_to_void_ptr(observer_id.value)
        );
    }
}

void
history_column_view_realize(GtkColumnView* historyColumnView, BookmarksDialog* win) //TODO: init 
{
	GtkListItemFactory* factory;
	GtkColumnViewColumn* column;

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(history_factory_setup_a), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(history_factory_bind_a), win);
	column = gtk_column_view_column_new("Visited", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(historyColumnView, column);
	g_object_unref(column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(history_factory_setup_b), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(history_factory_bind_b), win);
	g_signal_connect(factory, "unbind", G_CALLBACK(history_factory_unbind_b), win);
	column = gtk_column_view_column_new("Website", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(historyColumnView, column);
	g_object_unref(column);

    factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(history_factory_setup_c), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(history_factory_bind_c), win);
    g_signal_connect(factory, "unbind", G_CALLBACK(history_factory_unbind_c), win);
	column = gtk_column_view_column_new("Address", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(historyColumnView, column);
	g_object_unref(column);
}
