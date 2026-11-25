#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <vector>
#include <string>
#include <iostream>
#include <functional>
#include <common/src/log.h>

#include "window/bookmarks_dialog.h"
#include "app.h"
#include "data_item/profiles_columnview_item.h"
#include "profiles_columnview.h"
#include "util/click_util.h"
#include "util/menu_util.h"
#include "util/text_util.h"
#include "window/main_window/main_window.h"

static void
on_column_view_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    ContextMenuData context_menu_data;
    context_menu_data.user_data = data->user_data;
    main_window_set_context_menu_data(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(data->child))), std::move(context_menu_data));
    orion::gtk::button_released_handler(data->parent, click, x, y, data->popover_menu, GTK_WIDGET(data->child));
}

// Unparents the popover menu on release by default
static void
setup_context_menu(BookmarksDialog* dialog, GtkWidget* child, GtkPopoverMenu* popover_menu, ProfilesColumnviewItem* item)
{
    orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
    data->parent = GTK_WIDGET(dialog);
    data->child = child;
    data->popover_menu = popover_menu;
    data->unparent_popover_menu_on_release = true;
    g_object_ref(item);
    data->user_data = item;
    auto closure_notify = [](gpointer ptr, GClosure* closure){
        orion::gtk::ReleasedCallbackData* data = static_cast<orion::gtk::ReleasedCallbackData*>(ptr);
        g_object_unref(G_OBJECT(data->user_data));
        if (data->unparent_popover_menu_on_release) {
            gtk_widget_unparent(GTK_WIDGET(data->popover_menu));
        }
        delete data;
    };

    orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(dialog), child, on_column_view_secondary_released, data, closure_notify);
}

static void
profiles_factory_setup_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
	GtkWidget* labelA = gtk_label_new(NULL);
    GtkWidget* labelB = gtk_label_new(NULL);
    gtk_label_set_ellipsize(GTK_LABEL(labelB), PANGO_ELLIPSIZE_END);
	gtk_widget_set_halign(labelA, GTK_ALIGN_CENTER);
    gtk_label_set_justify(GTK_LABEL(labelA), GTK_JUSTIFY_CENTER);
    gtk_widget_add_css_class(labelA, "profiles-icon-label-small");
    //TODO: non hard coded size request
    gtk_widget_set_size_request(labelA, 20, -1);
    gtk_label_set_use_markup(GTK_LABEL(labelA), TRUE);
    
	gtk_widget_set_halign(labelB, GTK_ALIGN_START);
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,10);
	gtk_box_append(GTK_BOX(box), labelA);
	gtk_box_append(GTK_BOX(box), labelB);

	gtk_list_item_set_child(GTK_LIST_ITEM(listitem), box);
}

gboolean
profiles_columnview_on_drop_target_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);
    if(G_VALUE_HOLDS(value, PROFILE_ARRAY_TYPE))
    {
        GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
        graphene_rect_t row_rectangle;
        if(gtk_widget_compute_bounds(row_widget, row_widget, &row_rectangle))
        {
            ProfilesColumnviewItem* this_item = PROFILES_COLUMNVIEW_ITEM(gtk_column_view_cell_get_item(cell));

            uint index = gtk_column_view_cell_get_position(cell);

            GtkColumnView* view = GTK_COLUMN_VIEW(gtk_widget_get_ancestor(row_widget, GTK_TYPE_COLUMN_VIEW));
            GListModel* store = G_LIST_MODEL(gtk_column_view_get_model(view));

            uint n_items = g_list_model_get_n_items(store);
            std::optional<double> seq_num_before, seq_num_after; //seq nums before/after the position to be inserted into
            if(y<(row_rectangle.size.height/2))
            {
                if(index == 0)
                {
                    seq_num_after = profiles_columnview_item_get_seq_num(this_item);
                }
                else
                {
                    ProfilesColumnviewItem* prev_item = PROFILES_COLUMNVIEW_ITEM(g_list_model_get_item(store, index-1));
                    defer([prev_item]{ g_object_unref(prev_item); });
                    seq_num_after = profiles_columnview_item_get_seq_num(this_item);
                    seq_num_before = profiles_columnview_item_get_seq_num(prev_item);
                }
            }
            else
            {
                if(index == n_items - 1)
                {
                    seq_num_before = profiles_columnview_item_get_seq_num(this_item);
                }
                else
                {
                    ProfilesColumnviewItem* next_item = PROFILES_COLUMNVIEW_ITEM(g_list_model_get_item(store, index+1));
                    defer([next_item]{ g_object_unref(next_item); });
                    seq_num_before = profiles_columnview_item_get_seq_num(this_item);
                    seq_num_after = profiles_columnview_item_get_seq_num(next_item);
                }
            }

            GArray* dropped_array = static_cast<ProfileArray*>(g_value_get_boxed(value))->array;
            const orion::ProfileId* dropped_items = (orion::ProfileId*)(dropped_array->data); 

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
                orion::ProfileId dropped_item = dropped_items[i];

                orion::UpdateProfileQuery query;
                query.id = dropped_item;
                query.seq_num = seq_num;
                query.update_seq_num = true;
                global_core_api->update_profile(query, [](orion::Error err){VERIFY_LOG(err);});
            }
        }
        return TRUE;
    }
    return FALSE;
}

struct StringAndNullTerm {
    const char* string;
    const char* term = NULL;
};

GdkDragAction
profiles_columnview_on_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);


    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
    graphene_rect_t row_rectangle;

    StringAndNullTerm css_class;

    if(gtk_widget_compute_bounds(row_widget, row_widget, &row_rectangle))
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

    //uses the fact that StringAndNullTerm* is equivalent to a 2 element array of const char*, last one being a NULL terminator
    gtk_widget_set_css_classes(row_widget, reinterpret_cast<const char**>(&css_class));

    return GDK_ACTION_MOVE;
}

GdkDragAction
profiles_columnview_on_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(gtk_column_view_cell_get_child(cell));
    const char* empty_array[1] = {NULL};
    gtk_widget_set_css_classes(row_widget, empty_array);
    return GDK_ACTION_MOVE;
}

static GdkContentProvider*
profiles_columnview_on_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkColumnViewCell* cell = GTK_COLUMN_VIEW_CELL(user_data);
    gpointer thing = g_object_ref(gtk_column_view_cell_get_item(cell));
    while(!PROFILES_COLUMNVIEW_IS_ITEM(thing))
    {
        defer([thing]{ g_object_unref(thing); });
        thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
    }
    defer([thing]{ g_object_unref(thing); });
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(thing);
    GtkColumnView* view = GTK_COLUMN_VIEW(gtk_widget_get_ancestor(gtk_column_view_cell_get_child(cell), GTK_TYPE_COLUMN_VIEW));

    GtkSelectionModel* selection = GTK_SELECTION_MODEL(gtk_column_view_get_model(view));
    GtkBitset* bitset = gtk_selection_model_get_selection(selection);
    uint64_t bitset_size = gtk_bitset_get_size(bitset);

    ProfileArray* profile_array = profile_array_new();

    if(bitset_size == 0)
    {
        auto data = profiles_columnview_item_get_id(item);
        g_array_append_val(profile_array->array, data);
        gtk_bitset_unref(bitset);
        GdkContentProvider* return_val = gdk_content_provider_new_typed(profile_array_get_type(), profile_array);
        profile_array_free(profile_array);
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
            while(!PROFILES_COLUMNVIEW_IS_ITEM(thing))
            {
                defer([thing]{ g_object_unref(thing); });
                thing = gtk_tree_list_row_get_item(GTK_TREE_LIST_ROW(thing));
            }
            orion::gtk::GObjectPtr item_ptr(G_OBJECT(thing));
            ProfilesColumnviewItem* selected_item = PROFILES_COLUMNVIEW_ITEM(item_ptr.get());
            if(selected_item == item)
            {
                dragged_item_is_in_selection = true;
            }
            selected_items.push_back(std::move(item_ptr));
        }

        gtk_bitset_unref(bitset);
        //removing items whose parent folders are already being transferred

        if(dragged_item_is_in_selection)
        {
            for(orion::gtk::GObjectPtr& item_ptr : selected_items)
            {
                ProfilesColumnviewItem* selected_item = PROFILES_COLUMNVIEW_ITEM(item_ptr.get());
                auto data = profiles_columnview_item_get_id(selected_item);
                g_array_append_val(profile_array->array, data);
            }
            GdkContentProvider* return_val = gdk_content_provider_new_typed(profile_array_get_type(), profile_array);
            profile_array_free(profile_array);
            return return_val;
        }
        else
        {
            auto data = profiles_columnview_item_get_id(item);
            g_array_append_val(profile_array->array, data);
            GdkContentProvider* return_val = gdk_content_provider_new_typed(profile_array_get_type(), profile_array);
            profile_array_free(profile_array);
            return return_val;
        }
    }
}

static void
profiles_columnview_on_drag_source_drag_begin(GtkDragSource* self, GdkDrag* drag, gpointer user_data)
{
    ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(user_data);
    GtkWidget* label = gtk_label_new(profiles_columnview_item_get_icon(item)->str);
    gtk_drag_source_set_icon(self, gtk_widget_paintable_new(label), 50, 50);
}

static void
unbind(GtkListItem* item, GParamSpec* pspec, GtkWidget* row_widget)
{
    orion::gtk::remove_click_controller(row_widget);
    orion::gtk::remove_dnd_controllers(row_widget);
    g_signal_handlers_disconnect_by_data(item, row_widget);
}

static void
profiles_factory_bind_a(GtkSignalListItemFactory* factory, GObject* listitem, gpointer user_data)
{
    BookmarksDialog* dialog = BOOKMARKS_DIALOG(user_data);
	GtkWidget* box = GTK_WIDGET(gtk_list_item_get_child(GTK_LIST_ITEM(listitem)));
	GtkLabel* icon = GTK_LABEL(gtk_widget_get_first_child(box));
	GtkLabel* profile = GTK_LABEL(gtk_widget_get_last_child(box));
	ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(listitem)));
    gtk_label_set_label(profile, profiles_columnview_item_get_profile(item)->str);
    gtk_label_set_label(icon, profiles_columnview_item_get_icon(item)->str);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(dialog)));
    orion::ProfileId current_profile_id = main_window_get_current_profile_id(win);
    orion::ProfileId item_profile_id = profiles_columnview_item_get_id(item);

    GtkWidget* row_widget = orion::gtk::widget_get_column_view_row_widget(GTK_WIDGET(box));
    
    GtkPopoverMenu* popover_menu = NULL;
    if(profiles_columnview_item_get_is_internal(item))
    {
        popover_menu = bookmarks_dialog_get_internal_profile_popover_menu(dialog);
    }
    else
    {
        popover_menu = bookmarks_dialog_get_profiles_popover_menu(dialog);
    }

    if(popover_menu)
    {
        if (item_profile_id != current_profile_id) {
            GMenuModel* menu = gtk_popover_menu_get_menu_model(popover_menu);
            orion::gtk::menu_prepend_item(G_MENU(menu), _("Switch to Profile"), "win.profiles-columnview-switch");
        }
        setup_context_menu(BOOKMARKS_DIALOG(user_data), row_widget, popover_menu, item);
    }

    GtkDragSource* drag_source = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
    g_signal_connect(drag_source, "prepare", G_CALLBACK(profiles_columnview_on_drag_source_prepare), listitem);
    g_signal_connect(drag_source, "drag-begin", G_CALLBACK(profiles_columnview_on_drag_source_drag_begin), item);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drag_source));

    GtkDropTarget* drop_target = gtk_drop_target_new(PROFILE_ARRAY_TYPE, GDK_ACTION_MOVE);
    g_signal_connect(drop_target, "drop", G_CALLBACK(profiles_columnview_on_drop_target_drop), listitem);
    g_signal_connect(drop_target, "motion", G_CALLBACK(profiles_columnview_on_drop_target_motion), listitem);
    g_signal_connect(drop_target, "leave", G_CALLBACK(profiles_columnview_on_drop_target_leave), listitem);
    gtk_widget_add_controller(row_widget, GTK_EVENT_CONTROLLER(drop_target));

    g_signal_connect_object(listitem, "notify::item", G_CALLBACK(unbind), row_widget, G_CONNECT_DEFAULT);
}

static void
profiles_factory_setup_b(GtkSignalListItemFactory* factory, GObject* item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
profiles_factory_bind_b(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    ProfilesColumnviewItem* dataitem = PROFILES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(item)));
    
    int64_t windows = profiles_columnview_item_get_windows(dataitem);
    int64_t tabs = profiles_columnview_item_get_tabs(dataitem);

    std::string caption = _("No windows");
    if (windows > 0) {
        caption = orion::text::ngettext_string(
            "%" PRId64 " window",
            "%" PRId64 " windows",
            windows
        ) + ", " + orion::text::ngettext_string(
            "%" PRId64 " tab",
            "%" PRId64 " tabs",
            tabs
        );
    }

    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item)));
    gtk_label_set_label(label, caption.c_str());
}

static void
profiles_factory_setup_c(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
profiles_factory_bind_c(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item)));
    
    ProfilesColumnviewItem* dataitem = PROFILES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(item)));
    gtk_label_set_text(label, profiles_columnview_item_get_default(dataitem)?"Yes":"");
}

static void
profiles_factory_setup_d(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    GtkWidget* label = gtk_label_new(NULL);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_list_item_set_child(GTK_LIST_ITEM(item), label);
}

static void
profiles_factory_bind_d(GtkSignalListItemFactory* factory, gpointer item, gpointer user_data)
{
    GtkLabel* label = GTK_LABEL(gtk_list_item_get_child(GTK_LIST_ITEM(item)));
    
    ProfilesColumnviewItem* dataitem = PROFILES_COLUMNVIEW_ITEM(gtk_list_item_get_item(GTK_LIST_ITEM(item)));
    gtk_label_set_text(label, profiles_columnview_item_get_synced(dataitem)?"Yes":"");
}

static int
profiles_columnview_compare_items(gconstpointer item_a, gconstpointer item_b, gpointer user_data)
{
    ProfilesColumnviewItem* actual_item_a = PROFILES_COLUMNVIEW_ITEM(const_cast<gpointer>(item_a));
    ProfilesColumnviewItem* actual_item_b = PROFILES_COLUMNVIEW_ITEM(const_cast<gpointer>(item_b));
    double seq_num_a = profiles_columnview_item_get_seq_num(actual_item_a);
    double seq_num_b = profiles_columnview_item_get_seq_num(actual_item_b);
    if(seq_num_a>seq_num_b) return 1;
    else if(seq_num_a<seq_num_b) return -1;
    else return 0;
}

static void
profiles_column_view_profile_created(const orion::ProfileDto& profile_created, GListStore* store, uint num_windows, uint num_tabs)
{
    gpointer item = profiles_columnview_item_new_from_dto(profile_created, num_windows, num_tabs);
    g_list_store_insert_sorted(store, item, profiles_columnview_compare_items, NULL);
    g_object_unref(item);
}

static void
profiles_column_view_profile_deleted(orion::ProfileId profile_deleted, GListStore* store)
{
    uint len = g_list_model_get_n_items(G_LIST_MODEL(store));
    for(uint i = 0; i<len; i++)
    {
        ProfilesColumnviewItem* item = PROFILES_COLUMNVIEW_ITEM(g_list_model_get_item(G_LIST_MODEL(store), i));
        defer([item]{ g_object_unref(item); });
        if(profiles_columnview_item_get_id(item) == profile_deleted)
        {
            g_list_store_remove(store, i);
            break;
        }
    }
}

static void
fetch_profile_window_and_tab_counts(
    orion::ProfileId profile_id, std::function<void(uint num_windows, uint num_tabs)> callback
)
{
    global_core_api->get_window_ids({.profile_id=profile_id}, [cb=std::move(callback)](orion::GetWindowIdsReply reply){
        VERIFY_LOG(reply.err);
        uint num_windows = reply.ids.size();

        if (num_windows == 0) {
            cb(0, 0);
            return;
        }

        auto num_tabs = std::make_shared<uint>(0);
        auto done_windows = std::make_shared<uint>(0);

        for (orion::WindowId window_id : reply.ids) {
            orion::GetTabIdsQuery query{
                .window_id = window_id,
            };

            // note: do not try to std::move cb here, we need it potentially for each window_id
            global_core_api->get_tab_ids(query,
                [cb,num_windows,done_windows,num_tabs]
                (orion::GetTabIdsReply reply){
                    VERIFY_LOG(reply.err);
                    *num_tabs += reply.ids.size();
                    (*done_windows)++;
                    if (*done_windows == num_windows) {
                        cb(num_windows, *num_tabs);
                    }
                });
        }
    });
}

static void
profiles_column_view_change(GtkSingleSelection* selection, const orion::ProfilesChange& change)
{
    GListStore* store = G_LIST_STORE(gtk_single_selection_get_model(selection));
    orion::gtk::WeakRefWrapper store_weak(store);

    for(const orion::ProfileDto& profile_created : change.created)
    {
        fetch_profile_window_and_tab_counts(profile_created.id,
            [profile_created,store_weak](uint num_windows, uint num_tabs){
                auto store_strong = store_weak.lock();
                if (!store_strong) return;
                GListStore* store = G_LIST_STORE(store_strong.get());

                profiles_column_view_profile_created(profile_created, store, num_windows, num_tabs);
            });
    }
    for(orion::ProfileId profile_deleted : change.deleted)
    {
        profiles_column_view_profile_deleted(profile_deleted, store);
    }
    GtkBitset* selection_bitset = gtk_selection_model_get_selection(GTK_SELECTION_MODEL(selection));
    for(const orion::ProfileDto& profile_updated : change.updated)
    {
        profiles_column_view_profile_deleted(profile_updated.id, store);

        fetch_profile_window_and_tab_counts(profile_updated.id,
            [profile_updated,store_weak](uint num_windows, uint num_tabs){
                auto store_strong = store_weak.lock();
                if (!store_strong) return;
                GListStore* store = G_LIST_STORE(store_strong.get());

                profiles_column_view_profile_created(profile_updated, store, num_windows, num_tabs);
            });
    }
    if(gtk_bitset_get_size(selection_bitset)) gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection), gtk_bitset_get_nth(selection_bitset, 0), true);
    gtk_bitset_unref(selection_bitset);
}

static void
profiles_column_view_unmap(GtkColumnView* profiles_column_view, gpointer user_data)
{
    gtk_column_view_set_model(profiles_column_view, NULL);
    g_signal_handlers_disconnect_by_func(profiles_column_view, (gpointer)profiles_column_view_unmap, user_data);
}

void
profiles_column_view_map(GtkColumnView* profiles_column_view, BookmarksDialog* win)
{
	GListStore* root = g_list_store_new(G_TYPE_OBJECT);
    orion::gtk::WeakRefWrapper root_weak(root);

    global_core_api->get_profile_ids([root_weak](orion::GetProfileIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_profiles(reply.ids, [root_weak](orion::GetProfilesReply reply){
            VERIFY_LOG(reply.err);
            auto root_strong = root_weak.lock();
            if (!root_strong) return;
            GListStore* root = G_LIST_STORE(root_strong.get());
            
            g_list_store_remove_all(root);
            for(orion::ProfileDto profile : std::vector<orion::ProfileDto>(reply.profiles))
            {
                fetch_profile_window_and_tab_counts(profile.id,
                    [profile,root_weak](uint num_windows, uint num_tabs){
                        auto root_strong = root_weak.lock();
                        if (!root_strong) return;
                        GListStore* root = G_LIST_STORE(root_strong.get());

                        gpointer item = profiles_columnview_item_new_from_dto(profile, num_windows, num_tabs);
                        defer([item]{ g_object_unref(item); });
                        g_list_store_insert_sorted(root, item, profiles_columnview_compare_items, NULL);
                    });
            }
        });
    });




	GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(root));
	gtk_single_selection_set_autoselect(selection, TRUE);
	gtk_column_view_set_model(profiles_column_view, GTK_SELECTION_MODEL(selection));
    defer([selection]{ g_object_unref(selection); });

    orion::gtk::WeakRefWrapper selection_weak(selection);
    orion::ObserverId* id = new orion::ObserverId;
    *id = global_core_api->profiles_change().add_observer(
        [selection_weak](const orion::ProfilesChange& change){
            auto selection_strong = selection_weak.lock();
            if (!selection_strong) return;
            GtkSingleSelection* selection = GTK_SINGLE_SELECTION(selection_strong.get());
            profiles_column_view_change(selection, change);
        });

    g_signal_connect_data(profiles_column_view, "unmap", G_CALLBACK(profiles_column_view_unmap), id, 
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(data);
            global_core_api->profiles_change().remove_observer(*id);
            delete id;
        }, G_CONNECT_DEFAULT);
}

void
profiles_column_view_realize(GtkColumnView* profiles_column_view, BookmarksDialog* win)
{
	GtkListItemFactory* factory;
	GtkColumnViewColumn* column;

	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(profiles_factory_setup_a), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(profiles_factory_bind_a), win);
	column = gtk_column_view_column_new("Profile", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(profiles_column_view, column);
	g_object_unref(column);
    
	factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(profiles_factory_setup_b), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(profiles_factory_bind_b), win);
	column = gtk_column_view_column_new("Open Windows", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(profiles_column_view, column);
	g_object_unref(column);

    factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(profiles_factory_setup_c), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(profiles_factory_bind_c), win);
	column = gtk_column_view_column_new("Is Default", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(profiles_column_view, column);
	g_object_unref(column);

    factory = gtk_signal_list_item_factory_new();
	g_signal_connect(factory, "setup", G_CALLBACK(profiles_factory_setup_d), NULL);
	g_signal_connect(factory, "bind", G_CALLBACK(profiles_factory_bind_d), win);
	column = gtk_column_view_column_new("Synced", factory);
	gtk_column_view_column_set_expand(column, TRUE);
	gtk_column_view_append_column(profiles_column_view, column);
	g_object_unref(column);
}
