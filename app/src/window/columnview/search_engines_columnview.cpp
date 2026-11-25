#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <common/src/log.h>
#include <common/src/defer.h>

#include "window/manage_search_engines_dialog.h"
#include "app.h"
#include "data_item/search_engines_columnview_item.h"
#include "search_engines_columnview.h"
#include "window/main_window/main_window.h"
#include "common/src/ptr_util.h"
#include "util/menu_util.h"
#include "util/dialog_util.h"

static void
on_row_left_clicked(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    if (n_press == 2) {
        // double click
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
        GtkWidget* dialog = gtk_widget_get_ancestor(widget, MANAGE_SEARCH_ENGINES_DIALOG_TYPE);
        if (!dialog) {
            orion::log_error("on_row_left_clicked: could not find ManageSearchEnginesDialog ancestor");
            return;
        }
        SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(user_data);
        orion::SearchEngineId id = search_engines_columnview_item_get_id(item);

        GActionMap* map = G_ACTION_MAP(orion::gtk::get_parent_window(widget));
        g_action_activate(g_action_map_lookup_action(map, "edit-search-engine"), g_variant_new_int64(id.value));
    }
}

static void
on_row_right_clicked(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));
    ManageSearchEnginesDialog* dialog = MANAGE_SEARCH_ENGINES_DIALOG(
        gtk_widget_get_ancestor(widget, MANAGE_SEARCH_ENGINES_DIALOG_TYPE));
    if (!dialog) {
        orion::log_error("on_row_right_clicked: could not find ManageSearchEnginesDialog ancestor");
        return;
    }

    graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(widget, GTK_WIDGET(dialog), &zero, &pos)) {
		orion::log_warning("on_row_right_clicked: failed to compute point");
        return;
	}

	// Position of the widget + position of the click
	GdkRectangle rect{
		.x = int(pos.x + x),
		.y = int(pos.y + y),
		.width = 1,
		.height = 1,
	};

    orion::SearchEngineId id = search_engines_columnview_item_get_id(item);
    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });

    orion::gtk::menu_append_action_with_int64_target(menu, _("Edit"), "win.edit-search-engine", id.value);
    
    GtkWidget* popover_menu = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));

    manage_search_engines_dialog_set_popover_menu(dialog, GTK_POPOVER_MENU(popover_menu));

	gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu), &rect);
	gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
	gtk_popover_popup(GTK_POPOVER(popover_menu));

	// Claiming is crucial, otherwise no more than a single right-click event will be triggered
	gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
search_engines_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* label = gtk_label_new("");
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
search_engines_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));

	SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    
    gtk_label_set_text(label, (search_engines_columnview_item_get_name(item))->str);

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(label));
    orion::gtk::remove_click_controller(row_widget);

    {
        GtkGesture* gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_PRIMARY);
        g_signal_connect(gesture, "pressed", G_CALLBACK(on_row_left_clicked), item);
        gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(gesture));
    }
    {
        GtkGesture* gesture = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_SECONDARY);
        g_signal_connect(gesture, "pressed", G_CALLBACK(on_row_right_clicked), item);
        gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(gesture));
    }
}

static void
search_engines_factory_setup_b(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkWidget* label = gtk_label_new("");
	gtk_widget_set_halign(label, GTK_ALIGN_START);

	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
search_engines_factory_bind_b(GtkSignalListItemFactory* factory, GObject* listitem)
{
	GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_label_set_text(label, (search_engines_columnview_item_get_search_url_template(item))->str);
}

static void
search_engines_factory_setup_c(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkWidget* label = gtk_label_new("");;
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
search_engines_factory_bind_c(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
	gtk_label_set_text(label, (search_engines_columnview_item_get_suggestions_url_template(item))->str);
}

static void
search_engines_factory_setup_d(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkWidget* label = gtk_label_new("");;
    gtk_widget_set_halign(label, GTK_ALIGN_START);

    gtk_list_item_set_child(GTK_LIST_ITEM(listitem), label);
}

static void
search_engines_factory_bind_d(GtkSignalListItemFactory* factory, GObject* listitem)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
    
    SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    gtk_label_set_text(label, (search_engines_columnview_item_get_keyword(item))->str);
}

static int
search_engines_column_view_compare_items(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    SearchEnginesColumnviewItem* actual_item_a = SEARCH_ENGINES_COLUMNVIEW_ITEM(const_cast<gpointer>(item_a));
    SearchEnginesColumnviewItem* actual_item_b = SEARCH_ENGINES_COLUMNVIEW_ITEM(const_cast<gpointer>(item_b));
    orion::SearchEngineId id_a = search_engines_columnview_item_get_id(actual_item_a);
    orion::SearchEngineId id_b = search_engines_columnview_item_get_id(actual_item_b);
    if (id_a.value > id_b.value) {
        return 1;
    } else if (id_a.value < id_b.value) {
        return -1;
    }
    return 0;
}

static void
search_engines_column_view_item_created(const orion::SearchEngineDto& item_created, GListStore* store)
{
    gpointer item = search_engines_columnview_item_new(item_created);
    defer([item]{ g_object_unref(item); });
    g_list_store_insert_sorted(store, item, search_engines_column_view_compare_items, NULL);
}

static void
search_engines_column_view_item_deleted(orion::SearchEngineId item_deleted, GListStore* store)
{
    guint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for (guint i = 0; i < len; i++) {
        SearchEnginesColumnviewItem* item = SEARCH_ENGINES_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if (search_engines_columnview_item_get_id(item) == item_deleted) {
            g_list_store_remove(store, i);
            break;
        }
    }
}

static void
search_engines_column_view_change(GtkMultiSelection* selection, const orion::SearchEnginesChange& change)
{
    GListStore* store = G_LIST_STORE(gtk_multi_selection_get_model(selection));
    orion::gtk::WeakRefWrapper store_weak(store);

    for (const orion::SearchEngineDto& item_created : change.created) {
        search_engines_column_view_item_created(item_created, store);
    }

    for (orion::SearchEngineId item_deleted : change.deleted) {
        search_engines_column_view_item_deleted(item_deleted, store);
    }

    GtkBitset* selection_bitset = gtk_selection_model_get_selection(GTK_SELECTION_MODEL(selection));
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });

    for (const orion::SearchEngineDto& item_updated : change.updated) {
        search_engines_column_view_item_deleted(item_updated.id, store);
        search_engines_column_view_item_created(item_updated, store);
    }

    if (gtk_bitset_get_size(selection_bitset)) {
        gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection), gtk_bitset_get_nth(selection_bitset, 0), true);
    }
}

void
search_engines_column_view_unmap(GtkWidget* self, gpointer user_data)
{
    orion::log_verbose("search_engines_column_view_unmap");
    gtk_column_view_set_model(GTK_COLUMN_VIEW(self), NULL);
}

void
search_engines_column_view_map(GtkWidget* self, gpointer user_data)
{
    orion::log_verbose("search_engines_column_view_map");
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    GListStore* root = g_list_store_new(G_TYPE_OBJECT);
	GtkMultiSelection* selection = gtk_multi_selection_new(G_LIST_MODEL(root));
	gtk_column_view_set_model(GTK_COLUMN_VIEW(self), GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });

    // register change observer only once
    orion::ObserverId observer_id = static_cast<orion::ObserverId>(orion::void_ptr_to_int64(
        g_object_get_data(G_OBJECT(self), "search-engines-change-observer-id")));

    if (observer_id.value == 0) {
        orion::gtk::WeakRefWrapper selection_weak(selection);

        observer_id = global_core_api->search_engines_change().add_observer(
            [selection_weak](const orion::SearchEnginesChange& change){
                auto selection_strong = selection_weak.lock();
                if (!selection_strong) return;
                GtkMultiSelection* selection = GTK_MULTI_SELECTION(selection_strong.get());

                search_engines_column_view_change(selection, change);
            });

        g_object_set_data(G_OBJECT(self), "search-engines-change-observer-id",
            orion::int64_to_void_ptr(observer_id.value));
    }

    orion::gtk::WeakRefWrapper root_weak(root);

    global_core_api->get_search_engines_by_query({profile_id},
        [root_weak](orion::GetSearchEnginesReply reply) {
            VERIFY_LOG(reply.err);

            auto root_strong = root_weak.lock();
            if (!root_strong) return;
            GListStore* root = G_LIST_STORE(root_strong.get());

            for (const auto& engine : reply.search_engines) {
                g_list_store_append(root, search_engines_columnview_item_new(engine));
            }
         }
    );
}

void
search_engines_column_view_realize(GtkWidget* self, gpointer user_data)
{
    GtkColumnView* column_view = GTK_COLUMN_VIEW(self);
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(search_engines_factory_setup_a), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(search_engines_factory_bind_a), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new("Name", factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(column_view, column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(search_engines_factory_setup_b), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(search_engines_factory_bind_b), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new("Search URL template", factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
	    gtk_column_view_append_column(column_view, column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(search_engines_factory_setup_c), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(search_engines_factory_bind_c), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new("Suggestions URL template", factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(column_view, column);
    }
    {
        GtkListItemFactory* factory = gtk_signal_list_item_factory_new();
        g_signal_connect(factory, "setup", G_CALLBACK(search_engines_factory_setup_d), NULL);
        g_signal_connect(factory, "bind", G_CALLBACK(search_engines_factory_bind_d), NULL);
        GtkColumnViewColumn* column = gtk_column_view_column_new("Keyword", factory);
        defer([column]{ g_object_unref(column); });
        gtk_column_view_column_set_expand(column, TRUE);
        gtk_column_view_append_column(column_view, column);
    }
}

void
search_engines_column_view_get_selected_search_engine_ids(GtkColumnView* column_view,
    std::vector<orion::SearchEngineId>* out_selected_ids)
{
    GtkMultiSelection* selection = GTK_MULTI_SELECTION(gtk_column_view_get_model(column_view));

    GtkBitset* selection_bitset = gtk_selection_model_get_selection(GTK_SELECTION_MODEL(selection));
    defer([selection_bitset]{ gtk_bitset_unref(selection_bitset); });

    GtkBitsetIter iter{};
    guint position = 0;

    if (gtk_bitset_iter_init_first(&iter, selection_bitset, &position)) {
        do {
            gpointer item = g_list_model_get_item(G_LIST_MODEL(selection), position);
            defer([item]{ g_object_unref(item); });
            SearchEnginesColumnviewItem* search_item = SEARCH_ENGINES_COLUMNVIEW_ITEM(item);
            out_selected_ids->push_back(search_engines_columnview_item_get_id(search_item));
        } while (gtk_bitset_iter_next(&iter, &position));
    }
}