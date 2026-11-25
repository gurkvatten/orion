#include "sidebar_reading_list_list_view.h"
#include "util/favicon_util.h"
#include "util/html_description_util.h"
#include "util/menu_util.h"
#include "sidebar.h"
#include "window/main_window/main_window.h"


static void
on_reading_list_list_view_unmap(GtkListView* view, gpointer user_data)
{
    g_signal_handlers_disconnect_by_data(view, user_data);
}


static void
mark_item_as_read(orion::ReadingListEntryId id)
{
    orion::UpdateReadingListEntryQuery query{};
    query.id = id;
    query.is_unread = false;
    query.update_is_unread = true;
    global_core_api->update_reading_list_entry(query, [](orion::Error err){VERIFY_LOG(err);});
}

void
reading_list_list_view_fetch_data(std::shared_ptr<orion::gtk::WeakRefWrapper> wrapped_store, std::string query_str, orion::ProfileId id, bool show_read)
{   
    orion::GetReadingListEntryIdsQuery query{};
    query.profile_id = id;
    if (!query_str.empty())
    {
        query.search_filter = std::move(query_str);
    }
    global_core_api->get_reading_list_entry_ids(query, [wrapped_store, show_read](orion::GetReadingListEntryIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_reading_list_entries(reply.ids, [wrapped_store, show_read](orion::GetReadingListEntriesReply reply){
            VERIFY_LOG(reply.err);
            //TODO: check if this works as intended
            GListStore* store = G_LIST_STORE(wrapped_store->get());
            if(store)
            {
                g_list_store_remove_all(store);
                orion::log_verbose("Global bool passed show_unread: %B", show_read);
                for(orion::ReadingListEntryDto entry : reply.entries)
                {
                    orion::log_verbose("Entry title: %s isUnRead: %B", entry.title.c_str(), entry.is_unread);
                    if (entry.is_unread || show_read) {
                        ReadingListColumnviewItem* item = reading_list_columnview_item_new_from_dto_with_desc(entry, store);
                        g_list_store_insert_sorted(store, item, reading_list_columnview_item_date_compare, NULL);
                        g_object_unref(item);
                    }
                    
                    
                }
                g_object_unref(store);
            }
        });
    });
}

void
sidebar_reading_list_list_view_handle_change(Sidebar* sidebar, std::shared_ptr<orion::gtk::WeakRefWrapper> wrapped_store)
{
    orion::ProfileId id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(sidebar))));
    reading_list_list_view_fetch_data(wrapped_store, get_sidebar_readinglist_query(sidebar), id, get_sidebar_readinglist_show_read(sidebar));
}

void
reading_list_list_view_refetch_data(GtkListView* view, std::string query, bool show_read){
    GtkSelectionModel* sel_model = gtk_list_view_get_model(view);
    if (!GTK_IS_SINGLE_SELECTION(sel_model)) return;

    orion::ProfileId id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))));

    GListModel* list_model = gtk_single_selection_get_model(GTK_SINGLE_SELECTION(sel_model));
    GListStore* store = G_LIST_STORE(list_model);

    g_object_set_data(G_OBJECT(store), "remember-show-read", GINT_TO_POINTER(show_read ? TRUE : FALSE));

    std::shared_ptr<orion::gtk::WeakRefWrapper> wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(store);
    reading_list_list_view_fetch_data(wrapped_ref, query, id, show_read);
    
}

static void
on_reading_list_selection_changed(GtkSingleSelection *selection, GParamSpec *pspec, gpointer user_data)
{
    ReadingListColumnviewItem *item = READING_LIST_COLUMNVIEW_ITEM(gtk_single_selection_get_selected_item(selection));
    
    if (!item) return;

    main_window_load_bookmark(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), reading_list_columnview_item_get_address(item)->str);
    mark_item_as_read(reading_list_columnview_item_get_id(item));
}

void
on_reading_list_list_view_map(GtkListView* view, gpointer user_data)
{
    orion::ProfileId id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))));
    GListStore* store = g_list_store_new(G_TYPE_OBJECT);
    
    std::shared_ptr<orion::gtk::WeakRefWrapper> wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(store);

    bool show_read = get_sidebar_readinglist_show_read(APP_SIDEBAR(user_data));
    reading_list_list_view_fetch_data(wrapped_ref, get_sidebar_readinglist_query(APP_SIDEBAR(user_data)), id, show_read);

    orion::ObserverId* observer_id = new orion::ObserverId;
    
    g_object_set_data(G_OBJECT(store), "remember-show-read", GINT_TO_POINTER(show_read ? TRUE : FALSE));

    orion::gtk::WeakRefWrapper weak_sidebar(APP_SIDEBAR(user_data));
    *observer_id = global_core_api->reading_list_entries_change().add_observer([wrapped_ref, weak_sidebar](const orion::ReadingListEntriesChange& change){

        auto ptr = weak_sidebar.lock();
        Sidebar* sidebar = APP_SIDEBAR(ptr.get());

        sidebar_reading_list_list_view_handle_change(sidebar, wrapped_ref);

        
    });
    g_signal_connect_data(view, "unmap", G_CALLBACK(on_reading_list_list_view_unmap), observer_id,
    [](gpointer data, GClosure* closure){
        orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
        global_core_api->reading_list_entries_change().remove_observer(*id);
        delete id;
    }, G_CONNECT_DEFAULT);

    GtkSingleSelection *selection = GTK_SINGLE_SELECTION(gtk_single_selection_new(G_LIST_MODEL(store)));
    gtk_single_selection_set_autoselect(selection, FALSE);
    gtk_list_view_set_model(view, GTK_SELECTION_MODEL(selection));

    g_signal_connect(selection, "notify::selected-item", G_CALLBACK(on_reading_list_selection_changed), view);

}

void
on_reading_list_list_view_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GMenu* menu = g_menu_new();
    GMenu* section = g_menu_new();
    g_menu_append(section, _("Edit Reading List"), "win.open-reading-list-bookmarks");
    g_menu_append(section, _("Clear All Items…"), "win.reading-list-clear-all-items");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    GtkWidget* popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    g_object_unref(menu);
    gtk_widget_set_parent(popover, gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));

    GdkRectangle rec = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rec);
    gtk_popover_popup(GTK_POPOVER(popover));

}

void
on_reading_list_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    char* uri = reading_list_columnview_item_get_address(item)->str;
    int64_t id = reading_list_columnview_item_get_id(item).value;


    GMenu* menu = g_menu_new();

    GMenu* section = g_menu_new();
    orion::gtk::menu_append_action_with_int64_target(section, _("Open in New Tab"), "win.reading-list-open-entry-new-tab-mark-read", id);
    orion::gtk::menu_append_action_with_int64_target(section, _("Open in New Window"), "win.reading-list-open-entry-new-window-mark-read", id);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    //TODO: implement 
    section = g_menu_new();
    orion::gtk::menu_append_action_with_string_target(section, _("Copy Link"), "win.clipboard-copy", uri);
    //TODO: check whether is already read

    if (reading_list_columnview_item_get_is_unred(item))
    {
        orion::gtk::menu_append_action_with_int64_target(section, _("Mark as Read"), "win.reading-list-entry-mark-as-read", id);
    }else
    {
        orion::gtk::menu_append_action_with_int64_target(section, _("Mark as Unread"), "win.reading-list-entry-mark-as-unread", id);
    }
    
    //TODO: stateful and or better name
    g_menu_append(section, _("Save Offline"), NULL);
    orion::gtk::menu_append_action_with_int64_target(section, _("Remove Item"), "win.reading-list-entry-delete", id);
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    section = g_menu_new();
    g_menu_append(section, _("Edit Reading List"), "win.open-reading-list-bookmarks");
    g_menu_append(section, _("Clear All Items…"), "win.reading-list-clear-all-items");
    g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
    g_object_unref(section);

    GtkWidget* popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    g_object_unref(menu);
    gtk_widget_set_parent(popover, gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture)));

    GdkRectangle rec = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rec);
    gtk_popover_popup(GTK_POPOVER(popover));
}

void
load_image_from_reading_list_item(GtkImage* image, gpointer user_data)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(image))), image, reading_list_columnview_item_get_address(item)->str);
}

void
load_description_label_from_reading_list_item(GtkLabel* label, gpointer user_data)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    orion::gtk::load_gtk_label_from_uri_description(label, reading_list_columnview_item_get_address(item)->str);
}


char*
get_reading_list_title_from_item(GtkListItem* listitem)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        return g_strdup(reading_list_columnview_item_get_title(item)->str);
    }
    else
    {
        return NULL;
    }
}

char*
get_reading_list_address_from_item(GtkListItem* listitem)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        return g_strdup(reading_list_columnview_item_get_address(item)->str);
    }
    else
    {
        return NULL;
    }
}

char*
get_reading_list_description_from_item(GtkListItem* listitem)
{
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_list_item_get_item(listitem));
    if(item)
    {
        return g_strdup(reading_list_columnview_item_get_desc(item)->str);
    }
    else
    {
        return NULL;
    }
}