#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <common/src/defer.h>
#include <core/settings.h>
#include <common/src/log.h>

#include "app.h"
#include "window_list_box.h"
#include "util/click_util.h"
#include "util/menu_util.h"
#include "util/text_util.h"
#include "util/dialog_util.h"
#include "window/main_window/main_window.h"
#include "data_item/window_item.h"

struct _WindowListBox
{
	GtkBox parent;
	GtkListView* list_view;

    GtkPopoverMenu* window_menu;
    GtkPopoverMenu* tab_menu;
    
    int n_selections; // number of active (allocated) selections (useful for debugging)
    gulong on_notify_window_id_handler_id;
    GListStore* sections;
    guint last_selected_window_item_index = GTK_INVALID_LIST_POSITION;
    orion::ObserverId switch_saved_windows_in_place_setting_observer_id;
    guint num_tab_groups = 0;
    guint num_saved_windows = 0;
    GtkLabel* header_label;
    GtkBox* bottom_header;
    GtkLabel* drag_label;
};

G_DEFINE_TYPE(WindowListBox, window_list_box, GTK_TYPE_BOX) //has to be box, list box is "final"

static void
on_window_menu_button_map(GtkMenuButton* button, gpointer user_data);

static std::string
get_header_label(guint n_items)
{
    return std::string(_("Tab Groups")) + " (" + std::to_string(n_items) + ")";
}

static char*
get_middle_header_label(GObject* obj, guint n_items)
{
    return g_strdup(get_header_label(n_items).c_str());
}

static void
refresh_headers(WindowListBox* window_list_box)
{
    gtk_label_set_text(window_list_box->header_label, get_header_label(window_list_box->num_saved_windows).c_str());
    gtk_widget_set_visible(GTK_WIDGET(window_list_box->header_label), window_list_box->num_tab_groups == 0);

    gtk_widget_set_visible(GTK_WIDGET(window_list_box->bottom_header), window_list_box->num_saved_windows == 0);
}

// probably not used anymore - remove this
static void
on_tabs_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    WindowListBox* window_list_box = WINDOW_LIST_BOX(data->parent);
    orion::gtk::button_released_handler(data->parent, click, x, y, window_list_box->tab_menu, GTK_WIDGET(data->child));
}

static void
on_windows_row_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    auto data = static_cast<orion::gtk::ReleasedCallbackData*>(user_data);
    WindowListBox* window_list_box = WINDOW_LIST_BOX(data->parent);
    orion::gtk::button_released_handler(data->parent, click, x, y, window_list_box->window_menu, GTK_WIDGET(data->child));
}

static void
window_list_box_register_list_box_right_clicks(WindowListBox* self, GtkListBox* list_box)
{
    // traverse all list box rows
    GtkWidget* row = gtk_widget_get_first_child(GTK_WIDGET(list_box));

    orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
    data->parent = GTK_WIDGET(self);
    data->child = row;
    orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(self), row, on_tabs_row_secondary_released, data,
            [](gpointer data, GClosure* closure){ delete static_cast<orion::gtk::ReleasedCallbackData*>(data); });

    if(row) row = gtk_widget_get_next_sibling(row); //jump over an inactive 
    if(row) row = gtk_widget_get_next_sibling(row);

    while (row) {
        orion::gtk::ReleasedCallbackData* data = new orion::gtk::ReleasedCallbackData;
        data->parent = GTK_WIDGET(self);
        data->child = row;
        orion::gtk::register_right_click_with_user_data_and_closure(GTK_WIDGET(self), row, on_windows_row_secondary_released, data,
            [](gpointer data, GClosure* closure){ delete static_cast<orion::gtk::ReleasedCallbackData*>(data); });
        row = gtk_widget_get_next_sibling(row);
    }
}

static void
setup_context_menus(WindowListBox* self)
{
	GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_window/sidebar/context_menus.ui");

	orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->window_menu, "window_row_context_menu");
	orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->tab_menu, "window_tabs_row_context_menu");

	g_object_unref(builder);
}

static void
select_item_by_window_id(GtkSingleSelection* selection, orion::WindowId window_id, WindowListBox* window_list_box)
{
    uint len = g_list_model_get_n_items(G_LIST_MODEL(selection));
    for(uint i = 0; i<len; i++)
    {
        WindowItem* item = WINDOW_ITEM(g_list_model_get_item(G_LIST_MODEL(selection), i));
        defer( [item]{g_object_unref(item); });
        if(window_item_get_id(item) == window_id)
        {
            gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection), i, true);
            window_list_box->last_selected_window_item_index = i;
            break;
        }
    }
}

static void
handle_created(
    orion::WindowDto window_created, orion::ProfileId profile_id, GListStore* tab_groups,
    GListStore* saved_windows, WindowListBox* window_list_box)
{
    if(window_created.profile_id != profile_id) return;
    
    WindowItem* item = window_item_new(window_created);
    defer([item]{g_object_unref(item);});
        
    if(window_created.is_tab_group)
    {
        g_list_store_insert_sorted(tab_groups, item, window_item_seq_num_compare, NULL);
        window_list_box->num_tab_groups++;
    }
    else
    {
        g_list_store_insert_sorted(saved_windows, item, window_item_seq_num_compare, NULL);
        window_list_box->num_saved_windows++;
    }
    orion::log_verbose("window_list_box: handle_created: created window: id=%lld name='%s' seq_num=%f is_tab_group=%d",
        window_created.id.value, window_created.name.c_str(), window_created.seq_num, window_created.is_tab_group
    );
}

static void
handle_deleted(
    orion::WindowId window_deleted, orion::ProfileId profile_id, GListStore* tab_groups,
    GListStore* saved_windows, WindowListBox* window_list_box)
{
    auto log_item = [](WindowItem* existing_item, bool is_tab_group){
        orion::log_verbose("window_list_box: handle_deleted: deleted window: id=%lld name='%s' seq_num=%f is_tab_group=%d",
            window_item_get_id(existing_item).value,
            window_item_get_title(existing_item),
            window_item_get_seq_num(existing_item),
            is_tab_group
        );
    };

    uint len = g_list_model_get_n_items(G_LIST_MODEL(tab_groups));
    uint i = 0;
    for(i = 0; i<len; i++)
    {
        WindowItem* existing_item = WINDOW_ITEM(g_list_model_get_item(G_LIST_MODEL(tab_groups), i));
        defer( [existing_item]{g_object_unref(existing_item); });
        if(window_item_get_id(existing_item)==window_deleted)
        {
            g_list_store_remove(tab_groups, i);
            log_item(existing_item, true);
            window_list_box->num_tab_groups--;
            break;
        }
    }
    //will only occur if loop did not get broken, so item has not been found yet
    if(i==len)
    {
        len = g_list_model_get_n_items(G_LIST_MODEL(saved_windows));
        i = 0;
        for(i = 0; i<len; i++)
        {
            WindowItem* existing_item = WINDOW_ITEM(g_list_model_get_item(G_LIST_MODEL(saved_windows), i));
            defer( [existing_item]{g_object_unref(existing_item); });
            if(window_item_get_id(existing_item)==window_deleted)
            {
                g_list_store_remove(saved_windows, i);
                log_item(existing_item, false);
                window_list_box->num_saved_windows--;
                break;
            }
        }
    }
}

static void
handle_change(const orion::WindowsChange& change, orion::ProfileId profile_id, GtkFlattenListModel* list, WindowListBox* window_list_box)
{
    GListModel* sections = G_LIST_MODEL(gtk_flatten_list_model_get_model(list));
    GListStore* tab_groups = G_LIST_STORE(g_list_model_get_item(sections, 0));
    defer( [tab_groups]{g_object_unref(tab_groups); });
    GListStore* saved_windows = G_LIST_STORE(g_list_model_get_item(sections, 1));
    defer( [saved_windows]{g_object_unref(saved_windows); });
    
    for(auto window_created : change.created)
    {
        handle_created(window_created, profile_id, tab_groups, saved_windows, window_list_box);
    }
    for(auto window_deleted : change.deleted)
    {
        handle_deleted(window_deleted.id, profile_id, tab_groups, saved_windows, window_list_box);
    }
    for(auto window_updated : change.updated)
    {
        handle_deleted(window_updated.id, profile_id, tab_groups, saved_windows, window_list_box);
        handle_created(window_updated, profile_id, tab_groups, saved_windows, window_list_box);
    }

    refresh_headers(window_list_box);
}

// should be called on user change only, not when programmatically changing selection
// or reacting to backend changes
static void
on_selected_item_changed_by_user(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    GtkSingleSelection* selection = GTK_SINGLE_SELECTION(object);
    WindowItem* item = WINDOW_ITEM(gtk_single_selection_get_selected_item(selection));
    if (!item) return;
    orion::log_verbose("window_list_box: on_selected_item_changed_by_user: selected window id %lld", window_item_get_id(item).value);

    WindowListBox* window_list_box = WINDOW_LIST_BOX(user_data);
    if (!window_list_box) {
        orion::log_error("window_list_box: on_selected_item_changed_by_user: could not get WindowListBox from user_data");
        return;
    }

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(window_list_box)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::gtk::WeakRefWrapper weak_win(win);
    orion::gtk::WeakRefWrapper weak_item(item);
    orion::gtk::WeakRefWrapper weak_window_list_box(window_list_box);
    orion::gtk::WeakRefWrapper weak_selection(selection);

    global_core_api->get_setting(profile_id, orion::settings::windows::show_recently_opened_on_top,
        [weak_win,weak_item,weak_window_list_box,weak_selection,profile_id]
        (orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            bool show_recently_opened_on_top = reply.value;
            
            global_core_api->get_setting(profile_id, orion::settings::windows::switch_saved_windows_in_place,
                [weak_win,weak_item,weak_window_list_box,weak_selection,show_recently_opened_on_top,profile_id]
                (orion::GetSettingReply<bool> reply){
                    VERIFY_LOG(reply.err);

                    bool switch_in_place = reply.value;

                    auto win_ptr = weak_win.lock();
                    if (!win_ptr) return;
                    MainWindow* win = MAIN_WINDOW(win_ptr.get());

                    auto item_ptr = weak_item.lock();
                    if (!item_ptr) return;
                    WindowItem* item = WINDOW_ITEM(item_ptr.get());

                    auto window_list_box_ptr = weak_window_list_box.lock();
                    if (!window_list_box_ptr) return;
                    WindowListBox* window_list_box = WINDOW_LIST_BOX(window_list_box_ptr.get());

                    auto selection_ptr = weak_selection.lock();
                    if (!selection_ptr) return;
                    GtkSingleSelection* selection = GTK_SINGLE_SELECTION(selection_ptr.get());

                    orion::WindowId window_id = window_item_get_id(item);

                    // check if the window is already open in a UI window
                    std::vector<orion::UiWindowId> ui_ids = global_core_api->get_ui_windows_by_window_id(window_id);
                    if (!ui_ids.empty()) {
                        // the window with window_id is already open in a UI window, just focus that window
                        // even if switch_in_place is true

                        // revert the selection back
                        g_signal_handlers_block_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
                        gtk_single_selection_set_selected(selection, window_list_box->last_selected_window_item_index);
                        g_signal_handlers_unblock_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);

                        MainWindow* win = MAIN_WINDOW(static_cast<void*>(ui_ids.front())); // take the first one
                        gtk_window_present(GTK_WINDOW(win)); // focus the corresponding main window
                        return; // do not reorder the list even if show_recently_opened_on_top is true
                    }

                    if (switch_in_place) {
                        // switch in-place
                        main_window_switch_to_window_id_in_place(win, window_id);
                    } else {
                        // open in new window (or switch to existing one if already open)
                        if (window_list_box->last_selected_window_item_index == GTK_INVALID_LIST_POSITION) {
                            // this should never happen
                            orion::log_warning("window_list_box: on_selected_item_changed_by_user: last_selected_window_item_index is invalid");
                        } else {
                            // revert the selection back
                            g_signal_handlers_block_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
                            gtk_single_selection_set_selected(selection, window_list_box->last_selected_window_item_index);
                            g_signal_handlers_unblock_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
                        }

                        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "open-window-in-separate-window-or-switch-to-existing");
                        g_action_activate(action, g_variant_new_int64(window_item_get_id(item).value));
                    }

                    if (show_recently_opened_on_top) {
                        // move the window to the top of the list
                        global_core_api->get_window_ids({.profile_id=profile_id},
                            [profile_id,window_id](orion::GetWindowIdsReply reply){

                            VERIFY_LOG(reply.err);

                            global_core_api->get_windows(reply.ids, [profile_id,window_id](orion::GetWindowsReply reply){
                                VERIFY_LOG(reply.err);

                                if (reply.windows.empty()) {
                                    return; // nothing to do
                                }

                                orion::WindowDto first_window = reply.windows[0];

                                orion::UpdateWindowQuery query{
                                    .id = window_id,
                                    .seq_num = first_window.seq_num - 1.0, // insert before the first window
                                    .update_seq_num = true,
                                };
                                global_core_api->update_window(query, [](orion::Error err){
                                    VERIFY_LOG(err);
                                });
                            });
                        });
                    }
                });
        });
}

struct OnNotifyWindowIdData
{
    GListModel* selection;
    WindowListBox* window_list_box;
};

static void
rebuild_all_menu_button_menus(WindowListBox* window_list_box)
{
    GtkSelectionModel* model = gtk_list_view_get_model(window_list_box->list_view);
    // force refresh of all menu buttons by temporarily unsetting the model and setting
    // it back which will cause map signals to be emitted again
    gtk_list_view_set_model(window_list_box->list_view, NULL);
    gtk_list_view_set_model(window_list_box->list_view, model);
}

// called when the "window-id" property of MainWindow changes
static void
on_notify_window_id(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    OnNotifyWindowIdData* notify_data = static_cast<OnNotifyWindowIdData*>(user_data);
    GListModel* selection = G_LIST_MODEL(notify_data->selection);
    orion::WindowId id = main_window_get_window_id(win);
    orion::log_verbose("window_list_box: on_notify_window_id: window_id=%lld", id.value);
    WindowListBox* window_list_box = notify_data->window_list_box;

    g_signal_handlers_block_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
    
    select_item_by_window_id(GTK_SINGLE_SELECTION(selection), id, window_list_box);
    rebuild_all_menu_button_menus(window_list_box);

    g_signal_handlers_unblock_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
}

static void
disconnect_notify_window_id_handler(WindowListBox* window_list_box, MainWindow* win)
{
    if (window_list_box->on_notify_window_id_handler_id) {
        g_signal_handler_disconnect(win, window_list_box->on_notify_window_id_handler_id);
        window_list_box->on_notify_window_id_handler_id = 0;
    }
}

static GdkContentProvider*
on_window_item_drag_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    WindowItem* item = WINDOW_ITEM(gtk_list_item_get_item(list_item));
    return gdk_content_provider_new_typed(WINDOW_ITEM_TYPE, item);
}

static gboolean
on_window_item_drop(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* target_item = GTK_LIST_ITEM(user_data);
    WindowItem* target = WINDOW_ITEM(gtk_list_item_get_item(target_item));
    WindowItem* source = WINDOW_ITEM(g_value_get_object(value));

    if (!WINDOW_IS_ITEM(source) || !WINDOW_IS_ITEM(target)) {
        orion::log_warning("window_list_box: on_drop: source or target is not a WindowItem");
        return FALSE;
    }

    if (source == target) {
        // dropped on itself, nothing to do
        return FALSE;
    }

    GtkWidget* row = gtk_list_item_get_child(target_item);
    graphene_rect_t row_rect{};
    if (!gtk_widget_compute_bounds(row, row, &row_rect)) {
        orion::log_warning("window_list_box: on_drop: could not compute bounds of target row");
        return FALSE;
    }

    bool is_before_target = (y < row_rect.size.height / 2);

    GtkListView* list_view = GTK_LIST_VIEW(gtk_widget_get_ancestor(gtk_list_item_get_child(target_item), GTK_TYPE_LIST_VIEW));
    GtkSelectionModel* selection = (gtk_list_view_get_model(list_view));
    GtkFlattenListModel* model = GTK_FLATTEN_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));

    GListModel* sections = G_LIST_MODEL(gtk_flatten_list_model_get_model(model));
    GListStore* tab_groups_store = G_LIST_STORE(g_list_model_get_item(sections, 0));
    defer( [tab_groups_store]{g_object_unref(tab_groups_store); });
    GListStore* saved_windows_store = G_LIST_STORE(g_list_model_get_item(sections, 1));
    defer( [saved_windows_store]{g_object_unref(saved_windows_store); });

    if (g_list_store_find(saved_windows_store, source, NULL) && g_list_store_find(tab_groups_store, target, NULL)) {
        // source is in saved windows, target is in tab groups
        orion::log_warning("window_list_box: on_window_item_drop: cannot move saved window into tab groups");
        return FALSE;
    }

    bool is_converting_tab_group_to_saved_window =
        g_list_store_find(tab_groups_store, source, NULL) && g_list_store_find(saved_windows_store, target, NULL);

    orion::WindowId source_window_id = window_item_get_id(source);

    orion::UpdateWindowQuery query{
        .id = window_item_get_id(source),
        .seq_num = window_item_get_seq_num(target),
        .seq_num_collision_mode = is_before_target ? orion::SeqNumCollisionMode::before : orion::SeqNumCollisionMode::after,
        .update_seq_num = true,
    };

    if (is_converting_tab_group_to_saved_window) {
        query.name = window_item_get_title(source);
        query.is_tab_group = false;
        query.update_name = true;
        query.update_is_tab_group = true;
    }

    orion::gtk::WeakRefWrapper row_weak(row);

    global_core_api->update_window(query, [is_converting_tab_group_to_saved_window,source_window_id,row_weak](orion::Error err){
        VERIFY_LOG(err);

        if (is_converting_tab_group_to_saved_window) {
            auto row_ptr = row_weak.lock();
            if (!row_ptr) return;
            GtkWidget* row = GTK_WIDGET(row_ptr.get());

            GActionMap* map = G_ACTION_MAP(gtk_widget_get_root(row));
            g_action_activate(g_action_map_lookup_action(map, "edit-window-group"), g_variant_new_int64(source_window_id.value));
        }
    });
    return TRUE;
}

static GdkDragAction
on_window_item_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* target_item = GTK_LIST_ITEM(user_data);
    WindowItem* target = WINDOW_ITEM(gtk_list_item_get_item(target_item));
    if (!WINDOW_IS_ITEM(target)) {
        orion::log_warning("window_list_box: on_window_item_motion: target is not a WindowItem");
        return GDK_ACTION_MOVE;
    }
    GtkWidget* row = gtk_list_item_get_child(target_item);
    graphene_rect_t row_rect{};
    if (!gtk_widget_compute_bounds(row, row, &row_rect)) {
        orion::log_warning("window_list_box: on_window_item_motion: could not compute bounds of target row");
        return GDK_ACTION_MOVE;
    }

    std::string css_class;

    if (y < row_rect.size.height / 2) {
        css_class = "dnd-highlight-top";
    } else {
        css_class = "dnd-highlight-bottom";
    }

    // convert to null-terminated array of const char*
    std::vector<const char*> vec;
    vec.push_back(css_class.c_str());
    vec.push_back(NULL);
    gtk_widget_set_css_classes(row, vec.data());
    return GDK_ACTION_MOVE;
}

static GdkDragAction
on_window_item_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* target_item = GTK_LIST_ITEM(user_data);
    WindowItem* target = WINDOW_ITEM(gtk_list_item_get_item(target_item));
    if (!WINDOW_IS_ITEM(target)) {
        orion::log_warning("window_list_box: on_window_item_leave: target is not a WindowItem");
        return GDK_ACTION_MOVE;
    }
    GtkWidget* row = gtk_list_item_get_child(target_item);
    // empty null-terminated array of const char*
    std::vector<const char*> vec;
    vec.push_back(NULL);
    gtk_widget_set_css_classes(row, vec.data());
    return GDK_ACTION_MOVE;
}

static GdkDragAction
on_window_item_motion_label(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    return GDK_ACTION_MOVE;
}

static gboolean
on_window_item_drop_label(GtkDropTarget* self, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    WindowItem* source = WINDOW_ITEM(g_value_get_object(value));
    if (!WINDOW_IS_ITEM(source)) {
        orion::log_warning("window_list_box: on_window_item_drop (label): source is not a WindowItem");
        return FALSE;
    }

    GtkWidget* label = GTK_WIDGET(user_data);
    WindowListBox* window_list_box = WINDOW_LIST_BOX(gtk_widget_get_ancestor(label, WINDOW_LIST_BOX_TYPE));
    GtkSelectionModel* selection = (gtk_list_view_get_model(window_list_box->list_view));
    GtkFlattenListModel* model = GTK_FLATTEN_LIST_MODEL(gtk_single_selection_get_model(GTK_SINGLE_SELECTION(selection)));
    GListModel* sections = G_LIST_MODEL(gtk_flatten_list_model_get_model(model));
    GListStore* tab_groups_store = G_LIST_STORE(g_list_model_get_item(sections, 0));
    defer( [tab_groups_store]{g_object_unref(tab_groups_store); });

    guint n_items = g_list_model_get_n_items(G_LIST_MODEL(tab_groups_store));
    if (n_items == 0) {
        orion::log_warning("window_list_box: on_window_item_drop (label): no tab group exists");
        return FALSE;
    }

    WindowItem* target = WINDOW_ITEM(g_list_model_get_item(G_LIST_MODEL(tab_groups_store), n_items - 1));
    defer([target]{ g_object_unref(target); });
    orion::WindowId source_window_id = window_item_get_id(source);

    orion::UpdateWindowQuery query{
        .id = window_item_get_id(source),
        .name = window_item_get_title(source),
        .is_tab_group = false,
        .seq_num = window_item_get_seq_num(target),
        .seq_num_collision_mode = orion::SeqNumCollisionMode::after,
        .update_name = true,
        .update_is_tab_group = true,
        .update_seq_num = true,
    };

    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(window_list_box)));
    orion::gtk::WeakRefWrapper main_window_weak(main_window);

    global_core_api->update_window(query, [source_window_id,main_window_weak](orion::Error err){
        VERIFY_LOG(err);

        auto main_window_ptr = main_window_weak.lock();
        if (!main_window_ptr) return;
        MainWindow* main_window = MAIN_WINDOW(main_window_ptr.get());

        GActionMap* map = G_ACTION_MAP(main_window);
        g_action_activate(g_action_map_lookup_action(map, "edit-window-group"), g_variant_new_int64(source_window_id.value));
    });
    return TRUE;
}

static void
item_factory_setup(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data)
{
    orion::log_verbose("window_list_box: item_factory_setup");
    GtkWidget* row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_list_item_set_child(list_item, row);

    {
        // image icon
        GtkWidget* image = gtk_image_new();
        gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(image, GTK_ALIGN_CENTER);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
        gtk_widget_set_margin_end(image, 8);
        gtk_box_append(GTK_BOX(row), image);
    }
    {
        // label icon (emoji)
        GtkWidget* label = gtk_label_new(NULL);
        gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
        gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_CENTER);
        gtk_widget_set_margin_end(label, 4);
        gtk_box_append(GTK_BOX(row), label);
    }
    {
        // title
        GtkWidget* label = gtk_label_new(NULL);
        gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_label_set_single_line_mode(GTK_LABEL(label), true);
        gtk_box_append(GTK_BOX(row), label);
    }
    {
        // menu button
        GtkWidget* menu_button = gtk_menu_button_new();
        gtk_widget_set_margin_top(menu_button, 6);
        gtk_widget_set_margin_bottom(menu_button, 6);
        gtk_widget_set_margin_start(menu_button, 6);
        gtk_widget_set_halign(menu_button, GTK_ALIGN_END);
        gtk_widget_set_hexpand(menu_button, TRUE);
        gtk_box_append(GTK_BOX(row), menu_button);

        GtkWidget* image = gtk_image_new_from_icon_name("more-small-symbolic");
        gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
        gtk_menu_button_set_child(GTK_MENU_BUTTON(menu_button), image);

        g_signal_connect(menu_button, "map", G_CALLBACK(on_window_menu_button_map), list_item);
    }
    {
        // gesture click for right-click context menu
        GtkGesture* click = gtk_gesture_click_new();
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(click), GDK_BUTTON_SECONDARY);
        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(click));
        g_signal_connect(click, "released", G_CALLBACK(on_windows_list_view_row_secondary_released), list_item);
    }
    {
        // drag source
        GtkDragSource* drag_source = gtk_drag_source_new();
        gtk_drag_source_set_actions(drag_source, GDK_ACTION_MOVE);
        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(drag_source));
        g_signal_connect(drag_source, "prepare", G_CALLBACK(on_window_item_drag_prepare), list_item);
    }
    {
        // drop target
        GtkDropTarget* drop_target = gtk_drop_target_new(WINDOW_ITEM_TYPE, GDK_ACTION_MOVE);
        gtk_widget_add_controller(row, GTK_EVENT_CONTROLLER(drop_target));
        g_signal_connect(drop_target, "drop", G_CALLBACK(on_window_item_drop), list_item);
        g_signal_connect(drop_target, "motion", G_CALLBACK(on_window_item_motion), list_item);
        g_signal_connect(drop_target, "leave", G_CALLBACK(on_window_item_leave), list_item);
    }
    
    {
        // popover menu
        // must be last child of the GtkBox root (referenced in code using gtk_widget_get_last_child)
        GtkWidget* popover_menu = gtk_popover_menu_new_from_model(NULL);
        gtk_box_append(GTK_BOX(row), popover_menu);
    }
}

static void
item_factory_unbind(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data)
{
    orion::log_verbose("window_list_box: item_factory_unbind");
    GtkWidget* row = gtk_list_item_get_child(list_item);
    for(GtkWidget* child = gtk_widget_get_first_child(row); child; child = gtk_widget_get_next_sibling(child))
    {
        gtk_widget_remove_css_class(child, "dim-icon");
    }
}

static void
item_factory_bind(GtkSignalListItemFactory* factory, GtkListItem* list_item, gpointer user_data)
{
    orion::log_verbose("window_list_box: item_factory_bind");
    //ItemFactoryBindData* data = static_cast<ItemFactoryBindData*>(user_data);

    GtkWidget* row = gtk_list_item_get_child(list_item);
    WindowItem* window_item = WINDOW_ITEM(gtk_list_item_get_item(list_item));

    bool is_image_icon = window_item_get_is_image_icon(window_item);

    GtkWidget* image_icon = gtk_widget_get_first_child(row);
    gtk_image_set_from_icon_name(GTK_IMAGE(image_icon), window_item_get_icon_name(window_item));
    gtk_widget_set_visible(image_icon, is_image_icon);
    if (window_item_get_is_suspended(window_item)) {
        gtk_widget_add_css_class(image_icon, "dim-icon");
    }

    GtkWidget* label_icon = gtk_widget_get_next_sibling(image_icon);
    gtk_label_set_text(GTK_LABEL(label_icon), window_item_get_icon_name(window_item));
    gtk_widget_set_visible(label_icon, !is_image_icon);
    if (window_item_get_is_suspended(window_item)) {
        gtk_widget_add_css_class(label_icon, "dim-icon");
    }

    GtkWidget* title = gtk_widget_get_next_sibling(label_icon);
    const char* title_c_string = window_item_get_title(window_item);
    gtk_label_set_text(GTK_LABEL(title), title_c_string);
    if (window_item_get_is_suspended(window_item)) {
        gtk_widget_add_css_class(title, "dim-label");
    }
}

static void
on_window_list_view_unmap(GtkListView* self, gpointer user_data)
{
    orion::log_verbose("window_list_box: on_window_list_view_unmap");
    gtk_list_view_set_model(self, NULL);
    g_signal_handlers_disconnect_by_data(self, user_data);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;

    WindowListBox* window_list_box = WINDOW_LIST_BOX(gtk_widget_get_ancestor(GTK_WIDGET(self), WINDOW_LIST_BOX_TYPE));
    if (!window_list_box) {
        orion::log_error("window_list_box: on_window_list_view_unmap: could not find WindowListBox ancestor");
        return;
    }
    disconnect_notify_window_id_handler(window_list_box, win);
}

namespace {
    struct StringAndNullTerm {
        const char* string;
        const char* term = NULL;
    };

    struct ItemFactoryBindData {

    };
}

static void
on_window_list_view_map(GtkListView* self, gpointer user_data)
{
    orion::log_verbose("window_list_box: on_window_list_view_map");
    WindowListBox* window_list_box = WINDOW_LIST_BOX(gtk_widget_get_ancestor(GTK_WIDGET(self), WINDOW_LIST_BOX_TYPE));
    if (!window_list_box) {
        orion::log_error("window_list_box: on_window_list_view_map: could not find WindowListBox ancestor");
        return;
    }

    GtkListItemFactory* item_factory = gtk_signal_list_item_factory_new();
    defer([item_factory]{ g_object_unref(item_factory); });

    auto item_factory_bind_data = new ItemFactoryBindData{};

    g_signal_connect(item_factory, "setup", G_CALLBACK(item_factory_setup), NULL);
    g_signal_connect(item_factory, "unbind", G_CALLBACK(item_factory_unbind), NULL);
    g_signal_connect_data(item_factory, "bind", G_CALLBACK(item_factory_bind), item_factory_bind_data,
        [](gpointer data, GClosure* closure){
            delete static_cast<ItemFactoryBindData*>(data);
        }, G_CONNECT_DEFAULT);

    gtk_list_view_set_factory(self, item_factory);

    GtkFlattenListModel* section_list = gtk_flatten_list_model_new(NULL);

    GListStore* sections = g_list_store_new(G_TYPE_LIST_MODEL);
    window_list_box->sections = sections;

    GListStore* tab_groups = g_list_store_new(WINDOW_ITEM_TYPE);
    GListStore* saved_windows = g_list_store_new(WINDOW_ITEM_TYPE);

    g_list_store_append(sections, tab_groups);
    g_object_unref(tab_groups);
    g_list_store_append(sections, saved_windows);
    g_object_unref(saved_windows);
    gtk_flatten_list_model_set_model(section_list, G_LIST_MODEL(sections));

    GtkSingleSelection* selection = gtk_single_selection_new(G_LIST_MODEL(section_list));
    window_list_box->n_selections++;
    orion::log_verbose("window_list_box: selection created: %d", window_list_box->n_selections);
    orion::gtk::WeakRefWrapper selection_weak_ref(selection);
    gtk_list_view_set_model(self, GTK_SELECTION_MODEL(selection));

    //I need to always keep selection on current main window id, shall just bind the property ig but that's so random
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper window_weak_ref(win);
    orion::gtk::WeakRefWrapper window_list_box_weak_ref(window_list_box);
    global_core_api->get_window_ids({.profile_id=profile_id},
        [selection_weak_ref,window_weak_ref,window_list_box_weak_ref](orion::GetWindowIdsReply reply){

        VERIFY_LOG(reply.err);

        global_core_api->get_windows(reply.ids, [selection_weak_ref,window_weak_ref,window_list_box_weak_ref](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            auto selection_ref = selection_weak_ref.lock();
            if(!selection_ref) return;
            auto window_ref = window_weak_ref.lock();
            if(!window_ref) return;
            auto window_list_box_ref = window_list_box_weak_ref.lock();
            if(!window_list_box_ref) return;
            // selection will be unreffed in disconnect_existing_selection
            GtkSingleSelection* selection = GTK_SINGLE_SELECTION(selection_ref.get());
            MainWindow* win = MAIN_WINDOW(window_ref.get());
            WindowListBox* window_list_box = WINDOW_LIST_BOX(window_list_box_ref.get());
            
            GtkFlattenListModel* section_list = GTK_FLATTEN_LIST_MODEL(gtk_single_selection_get_model(selection));
            GListModel* sections = G_LIST_MODEL(gtk_flatten_list_model_get_model(section_list));
            GListStore* tab_groups = G_LIST_STORE(g_list_model_get_item(sections, 0));
            defer([tab_groups]{ g_object_unref(tab_groups); });
            GListStore* saved_windows = G_LIST_STORE(g_list_model_get_item(sections, 1));
            defer([saved_windows]{ g_object_unref(saved_windows); });

            //TODO: check whether this is the correct order of operation,
            //I want the notify signal to only ever trigger after the list is full
            g_object_freeze_notify(G_OBJECT(win));
            
            disconnect_notify_window_id_handler(window_list_box, win);

            auto notify_data = new OnNotifyWindowIdData{
                .selection = G_LIST_MODEL(selection),
                .window_list_box = g_object_ref(window_list_box), // ref to be sure it is not destroyed before we disconnect
            };

            // store the handler id so that we can disconnect it later, note that the lifetime of this handler is tied
            // to the lifetime of MainWindow
            window_list_box->on_notify_window_id_handler_id = g_signal_connect_data(win, "notify::window-id", G_CALLBACK(on_notify_window_id), notify_data,
                [](gpointer data, GClosure* closure){
                    auto notify_data = static_cast<OnNotifyWindowIdData*>(data);
                    notify_data->window_list_box->n_selections--;
                    orion::log_verbose("window_list_box: selection destroyed: %d", notify_data->window_list_box->n_selections);
                    g_object_unref(notify_data->window_list_box);
                    delete notify_data;
                }, G_CONNECT_DEFAULT);

            // called when user selects an item after the new item is already selected
            g_signal_connect(selection, "notify::selected-item", G_CALLBACK(on_selected_item_changed_by_user), window_list_box);

            g_signal_handlers_block_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);

            g_list_store_remove_all(tab_groups);
            g_list_store_remove_all(saved_windows);

            guint num_tab_groups = 0;
            guint num_saved_windows = 0;

            for(auto window : reply.windows)
            {
                WindowItem* item = window_item_new(window);
                guint insertion_pos;
                if(window.is_tab_group)
                {
                    insertion_pos = g_list_store_insert_sorted(tab_groups, item, window_item_seq_num_compare, NULL);
                    num_tab_groups++;
                }
                else
                {
                    insertion_pos = g_list_store_insert_sorted(saved_windows, item, window_item_seq_num_compare, NULL);
                    insertion_pos += g_list_model_get_n_items(G_LIST_MODEL(tab_groups));
                    num_saved_windows++;
                }
                g_object_unref(item);
                if(window.id == main_window_get_window_id(win))
                {
                    gtk_selection_model_select_item(GTK_SELECTION_MODEL(selection), insertion_pos, true);
                    window_list_box->last_selected_window_item_index = insertion_pos;
                }
            }
            
            window_list_box->num_tab_groups = num_tab_groups;
            window_list_box->num_saved_windows = num_saved_windows;

            refresh_headers(window_list_box);

            g_object_thaw_notify(G_OBJECT(win));
            g_signal_handlers_unblock_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
        });
    });

    // this cleanup struct can be declared locally which is nice
    struct ObserverRemoveData {
        orion::ObserverId window_observer_id;
        orion::ObserverId ui_window_observer_id;
    };
    ObserverRemoveData* remove_data = new ObserverRemoveData;

    remove_data->window_observer_id = global_core_api->windows_change().add_observer(
        [selection_weak_ref,profile_id,window_list_box_weak_ref](const orion::WindowsChange& change)
        {
            if (change.created.empty() && change.updated.empty() && change.deleted.empty()) {
                // this probably means that Observable class is broken - check implementation of its notify method
                orion::log_warning("window_list_box: windows_change observer called with empty change");
            }
            
            auto selection_ref = selection_weak_ref.lock();
            if (!selection_ref) return;
            auto window_list_box_ref = window_list_box_weak_ref.lock();
            if (!window_list_box_ref) return;
            GtkSingleSelection* selection = GTK_SINGLE_SELECTION(selection_ref.get());
            WindowListBox* window_list_box = WINDOW_LIST_BOX(window_list_box_ref.get());

            GtkFlattenListModel* list = GTK_FLATTEN_LIST_MODEL(gtk_single_selection_get_model(selection));

            // careful: may be null
            WindowItem* selected_item = WINDOW_ITEM(gtk_single_selection_get_selected_item(selection));

            orion::WindowId selected_window_id{};
            if (selected_item) {
                selected_window_id = window_item_get_id(selected_item);
            }
            
            g_signal_handlers_block_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);
            
            handle_change(change, profile_id, list, window_list_box);

            if (selected_window_id != orion::WindowId{}) {
                // restoring selection is needed because updates are implemented as delete + create
                select_item_by_window_id(selection, selected_window_id, window_list_box);
            }

            g_signal_handlers_unblock_by_func(selection, (gpointer)on_selected_item_changed_by_user, window_list_box);

            GListModel* sections = G_LIST_MODEL(gtk_flatten_list_model_get_model(list));
            GListStore* tab_groups = G_LIST_STORE(g_list_model_get_item(sections, 0));
            defer([tab_groups]{ g_object_unref(tab_groups); });
            guint num_tab_groups = g_list_model_get_n_items(G_LIST_MODEL(tab_groups));

            (void)window_list_box;
            (void)num_tab_groups;
        });

    remove_data->ui_window_observer_id = global_core_api->ui_windows_change().add_observer(
        [window_list_box_weak_ref](const orion::UiWindowsChange& change){
            auto window_list_box_ref = window_list_box_weak_ref.lock();
            if (!window_list_box_ref) return;
            WindowListBox* self = WINDOW_LIST_BOX(window_list_box_ref.get());
            rebuild_all_menu_button_menus(self);
        });

    g_signal_connect_data(self, "unmap", G_CALLBACK(on_window_list_view_unmap), remove_data,
        [](gpointer data, GClosure* closure){
            ObserverRemoveData* remove_data = static_cast<ObserverRemoveData*>(data);
            global_core_api->windows_change().remove_observer(remove_data->window_observer_id);
            global_core_api->ui_windows_change().remove_observer(remove_data->ui_window_observer_id);
            delete remove_data;
        }, G_CONNECT_DEFAULT);
}

// The caller of the function takes ownership of the data, and is responsible for freeing it.
static GMenu*
create_window_item_menu(
    const orion::WindowDto& window, MainWindow* main_window,
    const std::vector<orion::TabDto>& tabs, bool switch_saved_windows_in_place)
{
    GMenu* window_menu = g_menu_new();
    orion::WindowId window_id = window.id;
    orion::WindowId current_window_id = main_window_get_window_id(main_window);

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });

        if (window.is_suspended) {
            // Open (basically just selects the window) - visible only if the window is suspended
            orion::gtk::menu_append_action_with_int64_target(
                section, _("Open"), "win.open-window", window_id.value
            );
        }

        // "Open in Separate Window" must only be visible if "Switch Tab Groups In-Place" is enabled and
        // the window is not already an owner of a main window. It does not matter if this is the current main
        // window or not.
        std::vector<orion::UiWindowId> ui_ids = global_core_api->get_ui_window_ids_owned_by_window_id(window_id);
        if (switch_saved_windows_in_place && ui_ids.empty()) {
            orion::gtk::menu_append_action_with_int64_target(
                section, _("Open in Separate Window"), "win.open-window-in-separate-window", window_id.value
            );
        }

        g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
    }

    if (window.is_tab_group) {
        // Unsaved Tab Group

        if (!window.is_private) {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Save…"), "win.edit-window-group", window_id.value
            );

            g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
        }
        
        {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Duplicate"), "win.duplicate-window", window_id.value
            );

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Close Window"), "win.close-window", window_id.value
            );

            g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
        }
    } else {
        // Saved Tab Group
        {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Edit…"), "win.edit-window-group", window_id.value
            );

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Duplicate"), "win.duplicate-window", window_id.value
            );
            
            g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
        }
        {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            // "Close Window" is visible only if the window is owner of a main window
            std::vector<orion::UiWindowId> owned_ui_ids = global_core_api->get_ui_window_ids_owned_by_window_id(window_id);

            if (!owned_ui_ids.empty()) {
                orion::gtk::menu_append_action_with_int64_target(
                    section, _("Close Window"), "win.close-window", window_id.value
                );
            } else if (!window.is_suspended) {
                orion::gtk::menu_append_action_with_int64_target(
                    section, _("Suspend"), "win.suspend-window", window_id.value
                );
            }

            orion::gtk::menu_append_action_with_int64_target(
                section, _("Delete"), "win.delete-window", window_id.value
            );
            
            g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
        }
    }
    {
        GMenu* submenu = g_menu_new();
        defer([submenu]{ g_object_unref(submenu); });

        std::vector<int64_t> tab_ids;
        tab_ids.reserve(tabs.size());
        std::transform(tabs.begin(), tabs.end(), std::back_inserter(tab_ids),
            [](const orion::TabDto& tab){ return tab.id.value; }
        );

        orion::gtk::menu_append_action_with_int64_target(
            submenu, _("Copy Tab Links"), "win.copy-tab-links-of-window", window_id.value
        );

        std::string bookmark_label = orion::text::ngettext_string(
            "Bookmark %" PRId64 " Tab", "Bookmark %" PRId64 " Tabs", window.num_tabs
        );

        orion::gtk::menu_append_action_with_int64_target(
            submenu, bookmark_label.c_str(), "win.bookmark-tabs", window_id.value
        );

        // "Suspend N Tabs" should only be visible if the window is not suspended
        if (!window.is_suspended) {
            std::string suspend_label = orion::text::ngettext_string(
                "Suspend %" PRId64 " Tab", "Suspend %" PRId64 " Tabs", window.num_tabs
            );

            orion::gtk::menu_append_action_with_int64_vector_target(
                submenu, suspend_label.c_str(), "win.suspend-tabs", tab_ids
            );
        }

        {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            g_menu_append_section(submenu, NULL, G_MENU_MODEL(section));

            for (const auto& tab : tabs) {
                bool is_start_page = tab.uri.empty() || tab.uri == "orion://newtab/";
                std::string label;
                if (is_start_page) {
                    label = _("Start Page");
                } else {
                    label = tab.name.empty() ? _("<no-title>") : tab.name.c_str();
                }
                orion::gtk::menu_append_action_with_int64_target(
                    section, label.c_str(), "win.switch-to-tab", tab.id.value
                );
            }
        }

        {
            GMenu* section = g_menu_new();
            defer([section]{ g_object_unref(section); });

            // The name (title) of this submenu should always be "N Tabs" even for Private Tab Groups
            std::string tabs_label = orion::text::ngettext_string("%" PRId64 " Tab", "%" PRId64 " Tabs", window.num_tabs);

            g_menu_append_submenu(
                section, tabs_label.c_str(), G_MENU_MODEL(submenu)
            );

            g_menu_append_section(window_menu, NULL, G_MENU_MODEL(section));
        }
    }
    return window_menu;
}

void
create_window_context_menu_async(
    orion::WindowId window_id, MainWindow* main_window, std::function<void(GMenu*)> callback)
{
    orion::gtk::WeakRefWrapper main_window_weak(main_window);

    global_core_api->get_windows({window_id}, [window_id,main_window_weak,callback](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);

        orion::WindowDto window{};
        if (reply.windows.empty()) {
            orion::log_warning("window_list_box: create_window_context_menu_async: window id %lld does not exist", window_id.value);
            return;
        }
        window = reply.windows.at(0);

        orion::GetTabIdsQuery query{};
        query.window_id = window_id;
        global_core_api->get_tab_ids(query, [window,main_window_weak,callback](orion::GetTabIdsReply reply){
            VERIFY_LOG(reply.err);

            global_core_api->get_tabs(reply.ids, [window,main_window_weak,callback](orion::GetTabsReply reply){
                VERIFY_LOG(reply.err);

                orion::ProfileId profile_id = window.profile_id;
                std::vector<orion::TabDto> tabs = reply.tabs;

                global_core_api->get_setting(profile_id, orion::settings::windows::switch_saved_windows_in_place,
                    [window,main_window_weak,tabs,callback](orion::GetSettingReply<bool> reply){
                        VERIFY_LOG(reply.err);
                        bool switch_saved_windows_in_place = reply.value;
    
                        auto main_window_ref = main_window_weak.lock();
                        if(!main_window_ref) return;
                        MainWindow* main_window = MAIN_WINDOW(main_window_ref.get());

                        GMenu* window_menu = create_window_item_menu(window, main_window, tabs, switch_saved_windows_in_place);
                        defer([window_menu]{ g_object_unref(window_menu); });

                        callback(window_menu);
                    });
            });
        });
    });
}

static void
on_window_menu_button_map(GtkMenuButton* button, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    WindowItem* window_item = WINDOW_ITEM(gtk_list_item_get_item(list_item));
    orion::WindowId window_id = window_item_get_id(window_item);

    orion::gtk::WeakRefWrapper button_weak(button);
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));

    create_window_context_menu_async(window_id, main_window, [button_weak](GMenu* window_menu){
        auto button_strong = button_weak.lock();
        if(!button_strong) return;
        GtkMenuButton* button = GTK_MENU_BUTTON(button_strong.get());

        gtk_menu_button_set_menu_model(button, G_MENU_MODEL(window_menu));
    });
}

static char*
get_window_title(GObject* obj)
{
    GtkListItem* list_item = GTK_LIST_ITEM(obj);
    WindowItem* item = WINDOW_ITEM(gtk_list_item_get_item(list_item));
    if(!item) return g_strdup("");
    const char* title = window_item_get_title(item);
    if(!title) return g_strdup("");
    return g_strdup(title);
}

static void
on_new_saved_window_button_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    g_action_group_activate_action(G_ACTION_GROUP(main_window), "new-saved-window", NULL);
}

void
on_windows_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    WindowItem* window_item = WINDOW_ITEM(gtk_list_item_get_item(list_item));
    if (!window_item) return;
    orion::WindowId window_id = window_item_get_id(window_item);

    GtkWidget* row = gtk_list_item_get_child(list_item);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(row));
    if (!win) return;

    GdkRectangle rect = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};

    GtkPopover* popover = GTK_POPOVER(gtk_widget_get_last_child(row));
    if (!GTK_IS_POPOVER(popover)) {
        orion::log_error("window_list_box: on_windows_list_view_row_secondary_released: popover not found as last child of list item row");
        return;
    }

    orion::gtk::WeakRefWrapper popover_weak(popover);

    create_window_context_menu_async(window_id, win, [popover_weak,rect](GMenu* window_menu){
        auto popover_strong = popover_weak.lock();
        if (!popover_strong) return;
        GtkPopover* popover = GTK_POPOVER(popover_strong.get());

        gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
        gtk_popover_menu_set_menu_model(GTK_POPOVER_MENU(popover), G_MENU_MODEL(window_menu));
        gtk_popover_popup(GTK_POPOVER(popover));
    });
}

GtkBox*
window_list_box_new()
{
	auto box = g_object_new(WINDOW_LIST_BOX_TYPE, NULL);
	return GTK_BOX(box);
}

static void
window_list_box_finalize(GObject* gobject)
{
    WindowListBox* self = WINDOW_LIST_BOX(gobject);
    if (self->n_selections != 0) {
        orion::log_warning("window_list_box_finalize: remaining selections: %d", self->n_selections);
    }
    G_OBJECT_CLASS(window_list_box_parent_class)->finalize(gobject);

    global_core_api->single_setting_change().clear_observer(&self->switch_saved_windows_in_place_setting_observer_id);

    std::destroy_at(&self->switch_saved_windows_in_place_setting_observer_id);
}

static void
window_list_box_init(WindowListBox* self)
{
    std::construct_at(&self->switch_saved_windows_in_place_setting_observer_id);

    g_type_ensure(window_item_get_type());

	gtk_widget_init_template(GTK_WIDGET(self));

    //window_list_box_register_list_box_right_clicks(self, self->tab_window_list);
    (void)window_list_box_register_list_box_right_clicks;

    self->switch_saved_windows_in_place_setting_observer_id = global_core_api->add_single_setting_change_observer(
        orion::settings::windows::switch_saved_windows_in_place,
        [self](orion::SingleSettingChange<bool> change){
            // rebuild all menus to reflect the new setting value
            orion::log_verbose("window_list_box: switch_saved_windows_in_place changed to %d, rebuilding all menus", change.new_value);
            rebuild_all_menu_button_menus(self);
        }
    );

    setup_context_menus(self);

    {
        // prepare drag label as drag-and-drop target
        GtkDropTarget* drop_target = gtk_drop_target_new(WINDOW_ITEM_TYPE, GDK_ACTION_MOVE);
        gtk_widget_add_controller(GTK_WIDGET(self->drag_label), GTK_EVENT_CONTROLLER(drop_target));
        g_signal_connect(drop_target, "drop", G_CALLBACK(on_window_item_drop_label), self->drag_label);
        g_signal_connect(drop_target, "motion", G_CALLBACK(on_window_item_motion_label), self->drag_label);
    }
}

static void
window_list_box_class_init(WindowListBoxClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = window_list_box_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(GTK_WIDGET_CLASS(klass), GRESOURCE_PREFIX "/src/window/window_list_box.ui");

	gtk_widget_class_bind_template_child(widget_class, WindowListBox, window_menu);
	gtk_widget_class_bind_template_child(widget_class, WindowListBox, list_view);
    gtk_widget_class_bind_template_child(widget_class, WindowListBox, header_label);
    gtk_widget_class_bind_template_child(widget_class, WindowListBox, bottom_header);
    gtk_widget_class_bind_template_child(widget_class, WindowListBox, drag_label);

    gtk_widget_class_bind_template_callback(widget_class, on_window_list_view_map);
    gtk_widget_class_bind_template_callback(widget_class, on_window_list_view_unmap);
    gtk_widget_class_bind_template_callback(widget_class, on_window_menu_button_map);
    gtk_widget_class_bind_template_callback(widget_class, get_window_title);
    gtk_widget_class_bind_template_callback(widget_class, on_new_saved_window_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, get_middle_header_label);
    gtk_widget_class_bind_template_callback(widget_class, on_windows_list_view_row_secondary_released);
}