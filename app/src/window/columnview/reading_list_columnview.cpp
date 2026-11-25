#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <functional>
#include <utility>

#include <common/src/defer.h>

#include "window/bookmarks_dialog.h"
#include "app.h"
#include "data_item/reading_list_columnview_item.h"
#include "reading_list_columnview.h"
#include "window/main_window/main_window.h"
#include "util/favicon_util.h"

static void
reading_list_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* label = gtk_label_new(NULL);
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	GtkWidget* tree_expander = gtk_tree_expander_new();
	gtk_widget_set_hexpand(tree_expander, TRUE);
	gtk_tree_expander_set_child(GTK_TREE_EXPANDER(tree_expander), label);
	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), tree_expander);
}

static void
on_cell_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    //popover menu will be created here instead
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
setup_context_menu(BookmarksDialog* dialog, ReadingListColumnviewItemType type, GtkColumnViewCell* cell, GtkWidget* child, gpointer context_menu_data)
{
    // switch by item type (folder/item)
    GtkPopoverMenu* popover_menu = NULL;
    switch (type) {
        case ReadingListColumnviewItemType::folder:
            popover_menu = bookmarks_dialog_get_reading_list_folder_popover_menu(dialog);
            break;
        case ReadingListColumnviewItemType::address:
            popover_menu = bookmarks_dialog_get_reading_list_address_popover_menu(dialog);
            break;
    }

    if (popover_menu) {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
        data->parent = GTK_WIDGET(dialog);
        data->child = (GtkWidget*)cell;
        data->popover_menu = popover_menu;
        data->user_data = context_menu_data;
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(dialog), child, on_cell_row_secondary_released, data, 
            [](gpointer data, GClosure* closure){ delete static_cast<orion::gtk::ReleasedCallbackData*>(data); });
    }
}

static void
reading_list_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkTreeExpander* tree_expander = GTK_TREE_EXPANDER(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkLabel* label = GTK_LABEL(gtk_tree_expander_get_child(tree_expander));
	GtkTreeListRow* row = GTK_TREE_LIST_ROW(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_tree_expander_set_list_row(tree_expander, row);
	ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(gtk_tree_list_row_get_item(row));
    defer([item]{ g_object_unref(item); });
    ReadingListColumnviewItemType type = reading_list_columnview_item_get_item_type(item);
    if (type == ReadingListColumnviewItemType::folder)
    {
        // TODO: fix this
	    //gtk_label_set_text(label, g_date_time_format(reading_list_columnview_item_get_date(READING_LIST_COLUMNVIEW_ITEM(item)), "%d. %m. %Y"));
        gchar* date_string = g_date_time_format(reading_list_columnview_item_get_date(READING_LIST_COLUMNVIEW_ITEM(item)), "%d. %m. %Y");
        gtk_label_set_text(label, date_string);
        g_free(date_string);
    }
    else
    {
        gchar* date_string = g_date_time_format(reading_list_columnview_item_get_date(READING_LIST_COLUMNVIEW_ITEM(item)), "%T");
        gtk_label_set_text(label, date_string);
        g_free(date_string);
    }
    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(label));
    orion::gtk::remove_click_controller(row_widget);

    setup_context_menu(BOOKMARKS_DIALOG(user_data), type, GTK_COLUMN_VIEW_CELL(listitem), row_widget, gtk_column_view_get_model(bookmarks_dialog_get_reading_list_columnview(BOOKMARKS_DIALOG(user_data))));
}

static void
reading_list_factory_setup_b(GtkSignalListItemFactory* factory, GObject* item)
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
reading_list_factory_bind_b(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
    GtkBox* box = GTK_BOX(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkImage* icon = GTK_IMAGE(gtk_widget_get_first_child(GTK_WIDGET(box)));
	GtkLabel* label = GTK_LABEL(gtk_widget_get_last_child(GTK_WIDGET(box)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
    ReadingListColumnviewItemType type = reading_list_columnview_item_get_item_type(READING_LIST_COLUMNVIEW_ITEM(thing));
    if (type == ReadingListColumnviewItemType::address)
    {
	    gtk_label_set_text(label, (reading_list_columnview_item_get_title(item))->str);
        orion::Uri uri = orion::Uri::parse_user_input(reading_list_columnview_item_get_address(item)->str);
        orion::gtk::load_gtk_image_favicon_from_uri(ORION_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data))), icon, uri.uri().c_str());
    }
    else
    {
        gtk_label_set_label(label, "");
        gtk_image_clear(icon);
    }
}

static void
reading_list_factory_setup_c(GtkSignalListItemFactory* factory, gpointer item)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
items_changed(GListModel* self, guint position, guint removed, guint added, gpointer user_data)
{
    if(removed!=added)
    {
        GtkListItem* listitem = GTK_LIST_ITEM(user_data);
        GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
        GString* string = g_string_new("");
        uint n_items = g_list_model_get_n_items(self);
        if(n_items>0)
        {
            g_string_printf(string, ngettext("%u item", "%u items", n_items), n_items);
            gtk_label_set_text(GTK_LABEL(label), string->str);
            g_string_free(string, true);
        }
        else
        {
            GtkColumnView* reading_list_column_view = GTK_COLUMN_VIEW(gtk_widget_get_ancestor(gtk_list_item_get_child(listitem), GTK_TYPE_COLUMN_VIEW));
            
            GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(reading_list_column_view));
            GtkTreeListModel* model = GTK_TREE_LIST_MODEL(gtk_multi_selection_get_model(selection));
            GListModel* root = gtk_tree_list_model_get_model(model);
            G_LIST_STORE(root);
            gpointer thing = g_object_ref(gtk_list_item_get_item(listitem));
            while(!READING_LIST_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            defer([thing]{ g_object_unref(thing); });
            gpointer item = thing;

            //TODO: FIXME: figure out why this is failing
            uint len = g_list_model_get_n_items(root);
            for(uint i = 0; i<len; i++)
            {
                gpointer tmp_item = g_list_model_get_item(root, i);
                defer([tmp_item]{ g_object_unref(tmp_item); });
                if(tmp_item == item)
                {
                    g_list_store_remove(G_LIST_STORE(root), i);
                    break;
                }
            }
        }
    }
}

static void
reading_list_factory_bind_c(GtkSignalListItemFactory* factory, gpointer listitem, gpointer user_data)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(thing);
    if(reading_list_columnview_item_get_item_type(item) == ReadingListColumnviewItemType::address)
    {
	    gtk_label_set_text(label, reading_list_columnview_item_get_address(item)->str);
    }
    else
    {
        GListModel* store = G_LIST_MODEL(reading_list_columnview_item_get_store(item));
        GString* string = g_string_new("");
        uint n_items = g_list_model_get_n_items(store);
        g_string_printf(string, ngettext("%u item", "%u items", n_items), n_items);
        gtk_label_set_text(GTK_LABEL(label), string->str);
        g_string_free(string, true);
        g_signal_connect_object(store, "items-changed", G_CALLBACK(items_changed),
            G_OBJECT(listitem), G_CONNECT_DEFAULT);
    }
}

static void
reading_list_factory_setup_d(GtkSignalListItemFactory* factory, gpointer item)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
reading_list_factory_bind_d(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item)));
    
    gpointer thing = g_object_ref(gtk_list_item_get_item(GTK_LIST_ITEM(item)));
    while((GTK_IS_TREE_LIST_ROW(thing)))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    if((reading_list_columnview_item_get_referrer(READING_LIST_COLUMNVIEW_ITEM(thing)))!=NULL)
    {
        gtk_label_set_text(label, (reading_list_columnview_item_get_referrer(READING_LIST_COLUMNVIEW_ITEM(thing)))->str);
    }
    else
    {
        gtk_label_set_text(label, "");
    }
}

static void
reading_list_columnview_user_data_destroy(gpointer user_data)
{
    //does not need to memory manage user_data as that is memory managed by change observer
}


GListModel*
reading_list_columnview_create_submodel(gpointer item, gpointer data)
{
	if (reading_list_columnview_item_get_item_type(READING_LIST_COLUMNVIEW_ITEM(item)) == ReadingListColumnviewItemType::folder)
	{
        GDateTime* start_time = reading_list_columnview_item_get_date(READING_LIST_COLUMNVIEW_ITEM(item));
        int64_t start_time_unix_utc = g_date_time_to_unix(start_time);
        GDateTime* end_time = g_date_time_add_days(start_time, 1);
        int64_t end_time_unix_utc = g_date_time_to_unix(end_time);
        g_date_time_unref(end_time);

        std::vector<orion::ReadingListEntryDto>* entries = static_cast<std::vector<orion::ReadingListEntryDto>*>(data);
	    GListStore* store = g_list_store_new(G_TYPE_OBJECT);
        for(orion::ReadingListEntryDto entry : (*entries))
        {
            orion::UnixTimeUtc64 dto_value = entry.added_on;
            int64_t added_on = static_cast<int64_t>(dto_value);

            if(std::cmp_greater(added_on,start_time_unix_utc) && std::cmp_less_equal(added_on,end_time_unix_utc))
            {
                gpointer item = reading_list_columnview_item_new_from_dto(entry, store);
                g_list_store_insert_sorted(store, item, reading_list_columnview_item_date_compare, NULL);
                g_object_unref(item);
            }
        }
        reading_list_columnview_item_folder_set_store(READING_LIST_COLUMNVIEW_ITEM(item), store);

        //reffing because the item itself should be responsible for the management of store
        g_object_ref(store);
        return G_LIST_MODEL(store);
	}
	else
	{
		return NULL;
	}
}

//returns 0:00 of the day for entry in local timezone
static GDateTime*
reading_list_entry_dto_get_start_of_day(orion::ReadingListEntryDto entry)
{
    orion::UnixTimeUtc64 added_on = entry.added_on;
    int64_t unix_time = int64_t(added_on);

    GDateTime* utc_time = g_date_time_new_from_unix_utc(unix_time);
    GTimeZone* local_tz = g_time_zone_new_local();

    GDateTime* local_time = g_date_time_to_timezone(utc_time, local_tz);
    g_date_time_unref(utc_time);
    
    int d, m, y;
    g_date_time_get_ymd(local_time, &y, &m, &d);
    g_date_time_unref(local_time);

    GDateTime* result = g_date_time_new(local_tz, y, m, d, 0, 0, 0);
    g_time_zone_unref(local_tz);
    return result;
}

gboolean
g_date_time_compare_lower_bound(GDateTime* a, GDateTime* b)
{
    //returns true if a>b
    return g_date_time_compare(a, b)>0;
}

//caller is responsible for freeing the return value
static std::list<GDateTime*>*
reading_list_entry_vector_get_dates(const std::vector<orion::ReadingListEntryDto> &entries)
{
    std::list<GDateTime*> *dates = new std::list<GDateTime*>;
    for(orion::ReadingListEntryDto entry : entries)
    {
        GDateTime* date = reading_list_entry_dto_get_start_of_day(entry);

        //finds first date which is smaller than ours
        auto lower_it = std::lower_bound(dates->cbegin(), dates->cend(), date, g_date_time_compare_lower_bound);

        //if we reach the end, append
        if(lower_it == dates->cend())
        {
            dates->push_back(date);
        }
        //lower_it is either lower or equal, oonly insert if not equal
        else if(!g_date_time_equal(*lower_it, date))
        {
            dates->insert(lower_it, date);
        }
        else
        {
            g_date_time_unref(date);
        }
    }
    return dates;
}

static void
reading_list_column_view_unmap(GtkColumnView* reading_list_column_view, gpointer user_data)
{
    gtk_column_view_set_model(reading_list_column_view, NULL);
    g_signal_handlers_disconnect_by_data(reading_list_column_view, user_data);
}

static void
reading_list_model_entry_created(GtkTreeListModel* model, const orion::ReadingListEntryDto& reading_list_entry_created, std::vector<orion::ReadingListEntryDto> tree_list_user_data)
{
    tree_list_user_data.push_back(reading_list_entry_created);
    
    GListModel* root = gtk_tree_list_model_get_model(model);
    GDateTime* start_of_day = reading_list_entry_dto_get_start_of_day(reading_list_entry_created);
    
    uint len = g_list_model_get_n_items(root);
    for(uint i = 0; i<len; i++)
    {
        ReadingListColumnviewItem* item = READING_LIST_COLUMNVIEW_ITEM(g_list_model_get_item(root, i));
        defer([item]{ g_object_unref(item); });
        if(g_date_time_equal(start_of_day, reading_list_columnview_item_get_date(item)))
        {
            GListStore* store = reading_list_columnview_item_get_store(item);
            gpointer item = reading_list_columnview_item_new_from_dto(reading_list_entry_created, store);
            g_list_store_insert_sorted(store, item, 
                reading_list_columnview_item_date_compare, NULL);
            g_object_unref(item);
            g_date_time_unref(start_of_day);
            return;
        }
    }
    gpointer item = reading_list_columnview_item_new_day(start_of_day, 1);
    g_list_store_insert_sorted(G_LIST_STORE(root), item, reading_list_columnview_item_date_compare, NULL);
    g_object_unref(item);
    //if I append into the vector, 
}

static void
reading_list_model_entry_deleted(GtkTreeListModel* model, orion::ReadingListEntryId reading_list_entry_deleted, std::vector<orion::ReadingListEntryDto> tree_list_user_data)
{
    auto it = std::find_if(tree_list_user_data.begin(), tree_list_user_data.end(),
        [reading_list_entry_deleted](orion::ReadingListEntryDto entry){return entry.id == reading_list_entry_deleted;});
    tree_list_user_data.erase(it);

    uint len = g_list_model_get_n_items(G_LIST_MODEL(model));
    for(uint i = 0; i<len; i++)
    {
        gpointer thing = g_list_model_get_item(G_LIST_MODEL(model), i);
        while(!READING_LIST_COLUMNVIEW_IS_ITEM(thing))
        {
            defer([thing]{ g_object_unref(thing); });
            thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
        }
        defer([thing]{ g_object_unref(thing); });
        ReadingListColumnviewItem* tree_list_item = READING_LIST_COLUMNVIEW_ITEM(thing);

        GListStore* store = reading_list_columnview_item_get_store(tree_list_item);
        uint store_len = g_list_model_get_n_items(G_LIST_MODEL(store));

        for(uint a = 0; a<store_len; a++)
        {
            ReadingListColumnviewItem* store_item = READING_LIST_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), a));
            defer([store_item]{ g_object_unref(store_item); });
            if(reading_list_columnview_item_get_id(store_item) == reading_list_entry_deleted)
            {
                g_list_store_remove(store, a);
                return;
            }
        }
    }
}

static void
reading_list_columnview_handle_change(const orion::ReadingListEntriesChange& change, GtkColumnView* reading_list_column_view, std::vector<orion::ReadingListEntryDto> tree_list_user_data)
{
    GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(reading_list_column_view));
    if(!selection) return;
    GtkTreeListModel* model = GTK_TREE_LIST_MODEL(gtk_multi_selection_get_model(selection));
    for(const orion::ReadingListEntryDto& reading_list_entry_created : change.created)
    {
        reading_list_model_entry_created(model, reading_list_entry_created, tree_list_user_data);
    }
    for(orion::ReadingListEntryId reading_list_entry_deleted : change.deleted)
    {
        reading_list_model_entry_deleted(model, reading_list_entry_deleted, tree_list_user_data);
    }
    for(const orion::ReadingListEntryDto& reading_list_entry_updated : change.updated)
    {
        reading_list_model_entry_deleted(model, reading_list_entry_updated.id, tree_list_user_data);
        reading_list_model_entry_created(model, reading_list_entry_updated, tree_list_user_data);
    }
}

void
reading_list_columnview_init_model(GtkColumnView* reading_list_column_view, std::vector<orion::ReadingListEntryDto>& entries)
{
    std::list<GDateTime*> *dates = reading_list_entry_vector_get_dates(entries);

	GListStore* root = g_list_store_new(G_TYPE_OBJECT);

    for(GDateTime* date : *dates)
    {
        gpointer item = reading_list_columnview_item_new_day(date, 0);
        g_list_store_append(root, item);
        g_object_unref(item);
    }

    delete dates;

	GtkTreeListModel* model = gtk_tree_list_model_new(G_LIST_MODEL(root), FALSE, TRUE, reading_list_columnview_create_submodel, &entries, reading_list_columnview_user_data_destroy);
    G_LIST_MODEL(gtk_tree_list_model_get_model(model));

	GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(model));
	gtk_column_view_set_model(reading_list_column_view, GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });
}

void
reading_list_column_view_map(GtkColumnView* reading_list_column_view, BookmarksDialog* win)
{
    //TODO: change this to use profiles correctly, currently just a hack to work on the rest
    orion::ProfileId id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(win))));
    orion::GetReadingListEntryIdsQuery query;
    query.added_after = orion::UnixTimeUtc64(0);
    query.added_before = orion::UnixTimeUtc64Max;
    query.profile_id = id;
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(reading_list_column_view);
    auto entries = std::make_shared<std::vector<orion::ReadingListEntryDto>>();
    global_core_api->get_reading_list_entry_ids(query, [wrapped_ref, entries](orion::GetReadingListEntryIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_reading_list_entries(reply.ids, [wrapped_ref, entries](orion::GetReadingListEntriesReply reply){
            VERIFY_LOG(reply.err);
            GtkColumnView* reading_list_column_view = GTK_COLUMN_VIEW(wrapped_ref->get());
            if(reading_list_column_view)
            {
                entries->assign(reply.entries.cbegin(), reply.entries.cend());
                reading_list_columnview_init_model(reading_list_column_view, *entries);
                g_object_unref(reading_list_column_view);
            }
        });
    });
        
    orion::ObserverId* observer_id = new orion::ObserverId;
    *observer_id = global_core_api->reading_list_entries_change().add_observer(
        [reading_list_column_view, entries](const orion::ReadingListEntriesChange& change){
            reading_list_columnview_handle_change(change, reading_list_column_view, *entries);
        });

    g_signal_connect_data(reading_list_column_view, "unmap", G_CALLBACK(reading_list_column_view_unmap), observer_id,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->reading_list_entries_change().remove_observer(*id);
            delete id;
        },
        G_CONNECT_DEFAULT);
}

void
reading_list_column_view_realize(GtkColumnView* reading_list_column_view, BookmarksDialog* win) //TODO: proper values
{

	GtkListItemFactory* factory;
	GtkColumnViewColumn* column;

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(reading_list_factory_setup_a), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(reading_list_factory_bind_a), win);
	column = gtk_column_view_column_new("Added On", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(reading_list_column_view, column);
	g_object_unref(column);

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(reading_list_factory_setup_b), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(reading_list_factory_bind_b), win);
	column = gtk_column_view_column_new("Title", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(reading_list_column_view, column);
	g_object_unref(column);

    factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(reading_list_factory_setup_c), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(reading_list_factory_bind_c), win);
	column = gtk_column_view_column_new("Address", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(reading_list_column_view, column);
	g_object_unref(column);

    factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(reading_list_factory_setup_d), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(reading_list_factory_bind_d), win);
	column = gtk_column_view_column_new("Referrer", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(reading_list_column_view, column);
	g_object_unref(column);
}