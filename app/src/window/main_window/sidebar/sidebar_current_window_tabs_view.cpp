#define ENABLE_VERBOSE_LOGS 0

#include "sidebar_current_window_tabs_view.h"
#include "window/main_window/main_window.h"
#include "data_item/window_item.h"
#include "window/window_list_box.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"
#include "window/main_window/sidebar/sidebar.h"
#include "util/click_util.h"
#include "util/tab_context_menu.h"
#include "util/menu_util.h"
#include "util/action_util.h"
#include "util/text_util.h"
#include "util/custom_uri_util.h"
#include "window/columnview/orion_tab_selection_model.h"
#include "window/columnview/orion_filter_selection_model.h"

#include <common/src/defer.h>
#include <core/settings.h>

static gboolean
is_page_pinned_filter_func(gpointer item, gpointer user_data)
{
    bool is_pinned = GPOINTER_TO_INT(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(item);
    return adw_tab_page_get_pinned(page) == is_pinned;
}

// Caller of this function takes ownership of the returned selection model and is responsible for freeing it
static OrionFilterSelectionModel*
create_pinned_selection(Sidebar* sidebar, OrionWebviewTabView* orion_webview_tab_view)
{
    AdwTabView* tab_view = orion_webview_tab_view_get_actual_view(orion_webview_tab_view);
    GtkSelectionModel* all_pages_model = adw_tab_view_get_pages(tab_view);

    GtkCustomFilter* pinned_filter = gtk_custom_filter_new(is_page_pinned_filter_func, GINT_TO_POINTER(TRUE), NULL);
    defer([pinned_filter]{ g_object_unref(pinned_filter); });
    OrionFilterSelectionModel* pinned_selection = orion_filter_selection_model_new(all_pages_model);
    orion_filter_selection_model_set_filter(pinned_selection, GTK_FILTER(pinned_filter));


    return pinned_selection;
}

// Caller of this function takes ownership of the returned selection model and is responsible for freeing it
static OrionFilterSelectionModel*
create_unpinned_selection(Sidebar* sidebar, OrionWebviewTabView* orion_webview_tab_view)
{
    AdwTabView* tab_view = orion_webview_tab_view_get_actual_view(orion_webview_tab_view);
    GtkSelectionModel* all_pages_model = adw_tab_view_get_pages(tab_view);

    GtkCustomFilter* unpinned_filter = gtk_custom_filter_new(is_page_pinned_filter_func, GINT_TO_POINTER(FALSE), NULL);
    defer([unpinned_filter]{ g_object_unref(unpinned_filter); });
    OrionFilterSelectionModel* unpinned_selection = orion_filter_selection_model_new(all_pages_model);
    orion_filter_selection_model_set_filter(unpinned_selection, GTK_FILTER(unpinned_filter));

    return unpinned_selection;
}

void
sidebar_tabs_list_view_setup_tabs_lists(Sidebar* self, OrionWebviewTabView* orion_webview_tab_view)
{   
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));

    {
        GtkGridView* pinned_grid_view = sidebar_get_pinned_tabs_grid_view(self);
        OrionFilterSelectionModel* pinned_selection = create_pinned_selection(self, orion_webview_tab_view);
        defer([pinned_selection]{ g_object_unref(pinned_selection); });
        gtk_grid_view_set_model(pinned_grid_view, GTK_SELECTION_MODEL(pinned_selection));
    }
    {
        GtkListView* unpinned_list_view = sidebar_get_unpinned_tabs_list_view(self);
        OrionFilterSelectionModel* unpinned_selection = create_unpinned_selection(self, orion_webview_tab_view);
        defer([unpinned_selection]{ g_object_unref(unpinned_selection); });
        
        OrionTabSelectionModel* model = orion_tab_selection_model_new(G_LIST_MODEL(unpinned_selection));
        defer([model]{ g_object_unref(model); });
        
        GAction* show_nested_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
        GVariant* state = g_action_get_state(show_nested_setting);
        defer([state]{ g_variant_unref(state); });
        g_object_set(model, "show-nested-tabs", g_variant_get_boolean(state), NULL);
        g_object_bind_property_full(show_nested_setting, "state", model, "show-nested-tabs", G_BINDING_DEFAULT, orion::gtk::action_state_to_bool_transform, NULL, NULL, NULL);

        sidebar_set_unpinned_tabs_selection(self, GTK_SELECTION_MODEL(unpinned_selection));
        gtk_list_view_set_model(unpinned_list_view, GTK_SELECTION_MODEL(model));

    }
}

void
sidebar_tabs_list_view_map(GtkListView* view, gpointer user_data)
{
    orion::log_verbose("sidebar: sidebar_tabs_list_view_map");

    Sidebar* sidebar = APP_SIDEBAR(gtk_widget_get_ancestor(GTK_WIDGET(view), APP_SIDEBAR_TYPE));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));

    orion::gtk::WeakRefWrapper sidebar_weak(sidebar);
    orion::WindowId current_window_id = main_window_get_window_id(win);

    create_window_context_menu_async(current_window_id, win, [sidebar_weak](GMenu* menu){
        auto sidebar_strong = sidebar_weak.lock();
        if (!sidebar_strong) return;
        Sidebar* sidebar = APP_SIDEBAR(sidebar_strong.get());

        GtkMenuButton* current_window_menu_button = sidebar_get_current_window_menu_button(sidebar);
        gtk_menu_button_set_menu_model(current_window_menu_button, G_MENU_MODEL(menu));
    });
}

static void
blank_space_secondary_released_cb(GtkGestureClick* click, gdouble x, gdouble y, Sidebar* sidebar, GtkPopoverMenu* popover_menu)
{
    orion::log_verbose("sidebar: blank_space_secondary_released_cb");
    
    GdkRectangle rect = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(sidebar)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper popover_menu_weak(popover_menu);

    orion::GetRecentlyClosedTabIdsQuery query{
        .profile_id = profile_id,
        .window_id = std::nullopt, // do not filter by window
    };

    global_core_api->get_recently_closed_tab_ids(query, [rect,popover_menu_weak](orion::GetRecentlyClosedTabIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_recently_closed_tabs(reply.ids, [rect,popover_menu_weak](orion::GetRecentlyClosedTabsReply reply){
            VERIFY_LOG(reply.err);

            auto popover_menu_strong = popover_menu_weak.lock();
            if (!popover_menu_strong) return;
            GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(popover_menu_strong.get());

            GMenu* root = g_menu_new();
            defer([root]{ g_object_unref(root); });
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                GMenuItem* item = g_menu_item_new(_("New Tab"), "win.new-tab");
                defer([item]{ g_object_unref(item); });
                g_menu_append_item(section, item);
                g_menu_append_section(root, NULL, G_MENU_MODEL(section));
            }
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                GMenuItem* item = g_menu_item_new(_("Show Nested Tabs"), "win.show-nested-tabs");
                defer([item]{ g_object_unref(item); });
                g_menu_append_item(section, item);
                g_menu_append_section(root, NULL, G_MENU_MODEL(section));
            }
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                for (const auto& tab : reply.tabs) {
                    std::string title = tab.name;
                    if (orion::Uri::parse_strict(tab.uri.c_str()).known_scheme() == orion::Uri::Scheme::orion) {
                        title = orion::custom_uri_get_corresponding_title(tab.uri);
                    } else if (tab.name.empty()) {
                        title = _("<no-title>");
                    }
                    orion::gtk::menu_append_action_with_int64_target(
                        section, orion::text::ellipsize_string(title, MAX_CONTEXT_MENU_USER_STRING_LENGTH).c_str(), "win.open-recently-closed-tab", tab.id.value
                    );
                }
                g_menu_append_section(root, _("Recently Closed Tabs"), G_MENU_MODEL(section));
            }
            gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu), &rect);
            gtk_popover_set_has_arrow(GTK_POPOVER(popover_menu), FALSE);
            gtk_popover_menu_set_menu_model(popover_menu, G_MENU_MODEL(root));
            gtk_popover_popup(GTK_POPOVER(popover_menu));
        });
    });
}

void
on_tabs_list_view_secondary_released(Sidebar* sidebar, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    orion::log_verbose("on_tabs_list_view_secondary_released: click=%p user_data=%p x=%f y=%f", click, sidebar, x, y);
    GtkPopoverMenu* popover_menu = sidebar_get_tabs_list_view_popover_menu(sidebar);
    blank_space_secondary_released_cb(click, x, y, sidebar, popover_menu);
}

static void
close_tab_page_of_list_item(MainWindow* win, GtkListItem* list_item)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(list_item));
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    adw_tab_view_close_page(orion_webview_tab_view_get_actual_view(view), page);
}

void
on_close_page_button_clicked(GtkButton* self, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    close_tab_page_of_list_item(main_window, GTK_LIST_ITEM(user_data));
}

void
on_sidebar_tab_audio_button_clicked(GtkButton* self, gpointer user_data)
{
    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion_webview_tab_container_set_is_muted(container, !orion_webview_tab_container_get_is_muted(container));
}

GdkContentProvider*
on_pinned_tab_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    GdkContentProvider* prov = gdk_content_provider_new_typed(ADW_TYPE_TAB_PAGE, page);
    return prov;
}

void
on_pinned_tab_drag_source_drag_begin(GtkDragSource* self, GdkDrag* drag, gpointer user_data)
{
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget)));
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);

    GWeakRef* weak_ref = g_new(GWeakRef, 1);
    g_weak_ref_init(weak_ref, orion_webview_tab_view_get_actual_view(view));
    
    g_object_set_data(G_OBJECT(drag), "orion-sidebar-pinned-tab-page-dnd-source", weak_ref);
}

void
on_pinned_tab_drag_source_drag_end(GtkDragSource* self, GdkDrag* drag, gpointer user_data)
{
    GWeakRef* weak_ref = (GWeakRef*)g_object_get_data(G_OBJECT(drag), "orion-sidebar-pinned-tab-page-dnd-source");
    g_weak_ref_clear(weak_ref);
    g_free(weak_ref);
}

gboolean
on_pinned_tab_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    //relies on existing in the same address space as all other windows
    GdkDrag* drag = gdk_drop_get_drag(gtk_drop_target_get_current_drop(self));
    GWeakRef* weak_ref = (GWeakRef*)g_object_get_data(G_OBJECT(drag), "orion-sidebar-pinned-tab-page-dnd-source");
    if (!weak_ref) return FALSE;

    AdwTabView* original_view = ADW_TAB_VIEW(g_weak_ref_get(weak_ref));
    if(original_view)
    {
        uint pos = gtk_list_item_get_position(GTK_LIST_ITEM(user_data));
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
        AdwTabPage* page = ADW_TAB_PAGE(g_value_get_object(val));
        adw_tab_view_transfer_page(original_view, page, orion_webview_tab_view_get_actual_view(view), pos);
        
        g_object_unref(original_view);
        return TRUE;
    }
    return FALSE;
}

GdkContentProvider*
on_unpinned_tab_drag_source_prepare(GtkDragSource* self, gdouble x, gdouble y, gpointer user_data)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(GTK_LIST_ITEM(user_data)));
    GdkContentProvider* prov = gdk_content_provider_new_typed(ADW_TYPE_TAB_PAGE, page);
    return prov;
}

static void
on_unpinned_tab_drag_cancel(GdkDrag* drag, GdkDragCancelReason reason, gpointer user_data)
{
    //FIXME: commented out here is the ideal way we'd like to handle this with a note, for now just replaced by code
    //that is unaware of user cancel and error cancel, just setting the value every time
    //
    // //NOTE: this code, while structured different, should behave similarly to Adwaita's TabBox tab_drag_cancel_cb,
    // //but for some reason, this receives only GDK_DRAG_CANCEL_ERROR as the reason, which is unusable
    // //as we have no reliable way to distinguish between the user cancelling the drag using esc and dropping outside of valid areas
    // //comparing this to the behaviour of epiphany dynamically linked against mostly the same libraries,
    // //epiphany's AdwTabBar (and by extension AdwTabBox) detects NO_TARGET when dropping over the gnome side/topbar
    // if(reason == GDK_DRAG_CANCEL_NO_TARGET)
    // {
    //     gboolean* val = static_cast<gboolean*>(g_object_get_data(G_OBJECT(drag), "should-create-new-window"));
    //     if(val)
    //     {
    //         *val = true;
    //     }
    //     else
    //     {
    //         orion::log_debug("on_unpinned_tab_drag_cancel(): drag does not have should-create-new-window set");
    //     }
    // }
    
    gboolean* val = static_cast<gboolean*>(g_object_get_data(G_OBJECT(drag), "should-create-new-window"));
    if(val)
    {
        *val = true;
    }
}


void
on_unpinned_tab_drag_source_drag_begin(GtkDragSource* self, GdkDrag* drag, gpointer user_data)
{
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget)));
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);

    GWeakRef* weak_ref = g_new(GWeakRef, 1);
    g_weak_ref_init(weak_ref, orion_webview_tab_view_get_actual_view(view));
    
    g_object_set_data_full(G_OBJECT(drag), "orion-sidebar-tab-page-dnd-source", weak_ref, 
        [](gpointer data)
            {
                GWeakRef* weak_ref = static_cast<GWeakRef*>(data);
                g_weak_ref_clear(weak_ref);
                g_free(weak_ref);
            });

    //this signal only runs when dropping onto a valid target
    g_signal_connect(drag, "cancel", G_CALLBACK(on_unpinned_tab_drag_cancel), user_data);
    
    gboolean* val = new gboolean(FALSE);
    g_object_set_data_full(G_OBJECT(drag), "should-create-new-window", val,
        [](gpointer data){ delete static_cast<gboolean*>(data); });
}

void
on_unpinned_tab_drag_source_drag_end(GtkDragSource* self, GdkDrag* drag, gpointer user_data)
{
    gboolean* val = static_cast<gboolean*>(g_object_get_data(G_OBJECT(drag), "should-create-new-window"));
    if(val)
    {
        if(*val)
        {
            MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self))));
            if(win)
            {
                orion::ProfileId profile_id = main_window_get_current_profile_id(win);

                // The newly created window should never be private in this case
                orion::CreateWindowQuery query{
                    .profile_id = profile_id,
                    .icon_name = "window-flat-symbolic",
                    .is_tab_group = true,
                };

                GValue drop_val = G_VALUE_INIT;
                g_value_init(&drop_val, ADW_TYPE_TAB_PAGE);
                GError* err = NULL;
                if(!gdk_content_provider_get_value(gdk_drag_get_content(drag), &drop_val, &err))
                {
                    g_error_free(err);
                    g_value_unset(&drop_val);
                }
                else
                {
                    AdwTabPage* page = ADW_TAB_PAGE(g_value_get_object(&drop_val));
                    if(page)
                    {
                        //orion::log_debug("%i",G_OBJECT(page)->ref_count);
                        AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));

                        orion::gtk::WeakRefWrapper view_ref(view);
                        orion::gtk::WeakRefWrapper page_ref(page);

                        global_core_api->create_window(query,
                            [view_ref,page_ref,profile_id](orion::CreateWindowReply reply){

                            VERIFY_LOG(reply.err);
                            // The new window should never be private in this case
                            MainWindow* new_win = main_window_new_from_backend(
                                g_application_get_default(), profile_id, reply.id, orion::PrivateMode::no,
                                [page_ref, view_ref](MainWindow* new_win){
                                    OrionWebviewTabView* orion_webview_tab_view = main_window_get_webview_tab_view(new_win);
                                    
                                    orion::gtk::GObjectPtr view_strong = view_ref.lock();
                                    orion::gtk::GObjectPtr page_strong = page_ref.lock();
                                    AdwTabView* old_view = ADW_TAB_VIEW(view_strong.get());
                                    AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

                                    if(old_view && page)
                                    {
                                        orion_webview_tab_view_transfer_page(orion_webview_tab_view, page, 0, old_view, false, false);
                                    }
                                });

                            gtk_window_present(GTK_WINDOW(new_win));
                        });
                    }
                }
            }
        }
    }
}

gboolean
on_unpinned_tab_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    //relies on existing in the same address space as all other windows
    GdkDrag* drag = gdk_drop_get_drag(gtk_drop_target_get_current_drop(self));
    GWeakRef* weak_ref = (GWeakRef*)g_object_get_data(G_OBJECT(drag), "orion-sidebar-tab-page-dnd-source");
    if (!weak_ref) return FALSE;

    AdwTabView* original_view = ADW_TAB_VIEW(g_weak_ref_get(weak_ref));

    if(original_view)
    {
        defer([original_view]{ g_object_unref(original_view); });
        
        GtkListItem* list_item = GTK_LIST_ITEM(user_data);
        AdwTabPage* target_page = ADW_TAB_PAGE(gtk_list_item_get_item(list_item));
        AdwTabPage* dropped_page = ADW_TAB_PAGE(g_value_get_object(val));
        //if the target item is the same as what is being dropped, we ignore the drop
        if(gtk_list_item_get_item(list_item) == dropped_page) return true;

        // Sidebar* sidebar = APP_SIDEBAR(gtk_widget_get_ancestor(gtk_list_item_get_child(list_item), APP_SIDEBAR_TYPE));

        // // we have to take into account the pinned tabs at the start of the list
        // GtkSelectionModel* pinned_model = sidebar_get_pinned_tabs_selection(sidebar);
        // guint n_pinned_tabs = g_list_model_get_n_items(G_LIST_MODEL(pinned_model));

        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);

        // this position should already take into account pinned tabs
        int dropped_pos = adw_tab_view_get_page_position(
            orion_webview_tab_view_get_actual_view(view), ADW_TAB_PAGE(gtk_list_item_get_item(list_item))
        );


        GAction* show_nested_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
        GVariant* state = g_action_get_state(show_nested_setting);
        defer([state]{ g_variant_unref(state); });
        //TODO: better logic for when I should be grouping with the tab I am dropping onto
        //has to calculate whether to put into group, put after or put before,
        //has to calculate the pos based on whether the drop is before the item being dropped or not

        bool show_nested_tabs = g_variant_get_boolean(state);
        bool drop_into_group = false;

        GtkListView* listview = GTK_LIST_VIEW(gtk_widget_get_ancestor(widget, GTK_TYPE_LIST_VIEW));
        if(!listview) return false;

        bool is_horizontal = gtk_orientable_get_orientation(GTK_ORIENTABLE(listview)) == GTK_ORIENTATION_HORIZONTAL;
        int pos = is_horizontal?x:y;
        int max_pos = is_horizontal?gtk_widget_get_width(widget):gtk_widget_get_height(widget);

        if(!is_horizontal && show_nested_tabs && pos>max_pos/3 && pos<max_pos*2/3)
        {
            //dropped pos stays
            OrionWebviewTabContainer* dropped_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(dropped_page));
            orion::TabId dropped_page_parent_id = orion_webview_tab_container_get_parent_id(dropped_container);
            OrionWebviewTabContainer* target_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(target_page));
            orion::TabId target_page_id = orion_webview_tab_container_get_id(target_container);
            if(dropped_page_parent_id == target_page_id) return true;
            drop_into_group = true;
        }
        else
        {
            //if we are dropping from a different tab view or an item after the currently hovered one,
            //we want the target pos to be one after the target position
            if((original_view != orion_webview_tab_view_get_actual_view(view)) ||
               (adw_tab_view_get_page_position(original_view, dropped_page)>dropped_pos))
            {
                dropped_pos++;
            }
            //when dropping onto the first third of an item vertically, the target is before this current item
            //prior logic should mean that even when dropping onto the first item, dropped pos should be >0 at this point
            if((!is_horizontal && show_nested_tabs && pos<max_pos/3) || pos<max_pos/2)
            {
                dropped_pos--;
            }
            
        
            if((original_view == orion_webview_tab_view_get_actual_view(view)) &&
                (adw_tab_view_get_page_position(original_view, dropped_page)==dropped_pos))
            {
                return true;
            }
        }


        // n_pinned_tabs should already be accounted for in dropped_pos
        orion_webview_tab_view_transfer_page(
            view, dropped_page, dropped_pos, original_view, show_nested_tabs, drop_into_group
        );
        
        return TRUE;
    }
    return FALSE;
}

struct StringAndNullTerm {
    const char* string;
    const char* term = NULL;
};

GdkDragAction
on_tab_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);


    GtkWidget* widget = gtk_list_item_get_child(listitem);
    graphene_rect_t row_rectangle;

    StringAndNullTerm css_class;

    if(gtk_widget_compute_bounds(widget, widget, &row_rectangle))
    {
        css_class.string = "dnd-highlight-whole";
    }

    //uses the fact that StringAndNullTerm* is equivalent to a 2 element array of const char*, last one being a NULL terminator
    gtk_widget_set_css_classes(widget, reinterpret_cast<const char**>(&css_class));

    return GDK_ACTION_MOVE;
}

GdkDragAction
on_tab_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* listitem = GTK_LIST_ITEM(user_data);

    const char* empty_string_array[1] = {NULL};

    gtk_widget_set_css_classes(gtk_list_item_get_child(listitem), empty_string_array);

    return GDK_ACTION_MOVE;
}



gboolean
on_tabs_list_view_drop_target_drop(GtkDropTarget* self, const GValue* val, gdouble x, gdouble y, gpointer user_data)
{
    //relies on existing in the same address space as all other windows
    GdkDrag* drag = gdk_drop_get_drag(gtk_drop_target_get_current_drop(self));
    GWeakRef* weak_ref = (GWeakRef*)g_object_get_data(G_OBJECT(drag), "orion-sidebar-tab-page-dnd-source");
    if (!weak_ref) return FALSE;

    AdwTabView* original_view = ADW_TAB_VIEW(g_weak_ref_get(weak_ref));
    if(original_view)
    {
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);

        AdwTabView* this_view = orion_webview_tab_view_get_actual_view(view);
        int pos = adw_tab_view_get_n_pages(this_view);
        //index needs to be at the end of the tab list instead of one past when dropping in the same window due to reordering
        if(this_view==original_view) pos--;

        AdwTabPage* page = ADW_TAB_PAGE(g_value_get_object(val));

        GAction* show_nested_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
        GVariant* state = g_action_get_state(show_nested_setting);
        defer([state]{ g_variant_unref(state); });
        bool show_nested_tabs = g_variant_get_boolean(state);

        orion_webview_tab_view_transfer_page(view, page, pos, original_view, show_nested_tabs, false);
        
        g_object_unref(original_view);
        return TRUE;
    }
    return FALSE;
}

GdkDragAction
on_tabs_list_view_drop_target_motion(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    return GDK_ACTION_MOVE;
}

GdkDragAction
on_tabs_list_view_drop_target_leave(GtkDropTarget* self, gdouble x, gdouble y, gpointer user_data)
{
    return GDK_ACTION_MOVE;
}

void
on_tab_event_controller_motion_enter(GtkListItem* item)
{
    GtkWidget* box = gtk_list_item_get_child(item);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(box)));
    if(!win) return;

    GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-tab-preview-on-hover");
    GVariant* var = g_action_get_state(setting);
    defer([var]{ g_variant_unref(var); });
    if(!g_variant_get_boolean(var)) return;

    GtkPopover* popover = GTK_POPOVER(gtk_widget_get_prev_sibling(gtk_widget_get_last_child(box)));
    orion::gtk::WeakRefWrapper popover_ref(popover);
    orion::gtk::WeakRefWrapper box_ref(box);

    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId id = orion_webview_tab_container_get_id(container);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    
    if(adw_tab_page_get_pinned(page))
    {
        gtk_popover_set_offset(popover, 0, 8);
    }
    else
    {
        gtk_popover_set_offset(popover, 24, 0);
    }
    if(!gtk_list_item_get_selected(item))
    {
        global_core_api->get_setting(profile_id, orion::settings::tabs::show_tab_preview_on_hover,
            [box_ref, popover_ref,id](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                if(!reply.value) return;

                global_core_api->get_tabs({id},
                    [box_ref, popover_ref](orion::GetTabsReply reply){
                        VERIFY_LOG(reply.err);

                        if(reply.tabs.empty()) return;

                        std::vector<uint8_t>& image_data = reply.tabs.at(0).snapshot_image; 

                        auto popover_strong = popover_ref.lock();
                        if(!popover_strong) return;
                        GtkPopover* popover = GTK_POPOVER(popover_strong.get());

                        auto box_strong = box_ref.lock();
                        if(!box_strong) return;
                        GtkWidget* box = GTK_WIDGET(box_strong.get());
                        if(!gtk_widget_is_drawable(box)) return;

                        if(!gtk_widget_get_root(GTK_WIDGET(popover)))
                        {
                            //NOTE: this is known to be reached when cancelling a tab dnd operation over itself,
                            //the tab will get transferred, which will cause the popover to lose its root widget while our async callbacks go through
                            orion::log_debug("on_tab_event_controller_motion_enter: attempting to popup popover, but it is not inside the widget structure");
                            return;
                        }

                        
                        GtkWidget* inner_box = gtk_popover_get_child(popover);
                        g_return_if_fail(GTK_IS_BOX(inner_box));
                        GtkWidget* clamp = gtk_widget_get_first_child(inner_box);
                        g_return_if_fail(ADW_IS_CLAMP(clamp));
                        GtkPicture* picture = GTK_PICTURE(gtk_widget_get_first_child(clamp));
                        g_return_if_fail(GTK_IS_PICTURE(picture));
                        if(!(image_data.size()))
                        {
                            //gtk_widget_set_visible(GTK_WIDGET(picture), false);
                        }
                        else
                        {

                            GBytes* image_bytes = g_bytes_new_static(image_data.data(), image_data.size());
                            defer([image_bytes]{ g_bytes_unref(image_bytes); });
                            GError* err = NULL;
                            GdkTexture* texture = gdk_texture_new_from_bytes(image_bytes, &err);
                            if(err)
                            {
                                orion::log_debug("on_tab_event_controller_motion_enter while attempting to create texture from backend data: %s ", err->message);
                                g_error_free(err);
                                return;
                            }
                            defer([texture]{ g_object_unref(texture); });
                            gtk_picture_set_paintable(picture, GDK_PAINTABLE(texture));
                            //gtk_widget_set_visible(GTK_WIDGET(picture), true);
                        }
                        gtk_popover_popup(popover);
                    });
            });
    }
    else
    {
        gtk_popover_popup(popover);
    }
}

void
on_new_tab_button_secondary_pressed(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

void
on_new_tab_button_secondary_released(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    Sidebar* sidebar = APP_SIDEBAR(user_data);
    GtkPopoverMenu* popover_menu = sidebar_get_new_tab_button_popover_menu(sidebar);
    // the same context menu as for blank space in tabs list view
    blank_space_secondary_released_cb(click, x, y, sidebar, popover_menu);
}

void
on_new_window_button_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    g_action_group_activate_action(G_ACTION_GROUP(main_window), "new-saved-window", NULL);
}

void
on_tabs_list_view_row_secondary_pressed(GtkGestureClick* click, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("sidebar: on_tabs_list_view_row_secondary_pressed");
    gtk_gesture_set_state(GTK_GESTURE(click), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
tab_list_item_right_click_cb(GtkListItem* list_item, gdouble x, gdouble y)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(list_item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);

    GtkWidget* row = gtk_list_item_get_child(list_item);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(row));

    GdkRectangle rect = {.x = (int)x, .y = (int)y, .width = 1, .height = 1};

    // See popup_tabbar_tab_popover_menu in main_window.cpp for very similar code
    orion::GetOtherTabIdsQuery other_tab_ids_query{};
    other_tab_ids_query.exclude_tab_id = tab_id;

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper tab_page_weak(page);
    //relies on popover menu being the last child of the row widget
    orion::gtk::WeakRefWrapper popover_weak(gtk_widget_get_last_child(row));

    global_core_api->get_other_tab_ids(other_tab_ids_query, [win_weak,tab_page_weak,popover_weak,rect](orion::GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        auto tab_page_strong = tab_page_weak.lock();
        if (!tab_page_strong) return;
        AdwTabPage* tab_page = ADW_TAB_PAGE(tab_page_strong.get());

        OrionWebviewTabView* orion_webview_tab_view = main_window_get_webview_tab_view(win);
        AdwTabView* tab_view = orion_webview_tab_view_get_actual_view(orion_webview_tab_view);

        // create menu on the heap and do not unref it until inside the async callback
        GMenu* menu = g_menu_new();
        
        orion::gtk::fill_tab_context_menu(menu, win, tab_view, tab_page, reply, orion::gtk::CreateTabContextMenuType::sidebar,
            [popover_weak,rect,menu](orion::Error err){
                defer([menu]{ g_object_unref(menu); });
                VERIFY_LOG(err);

                auto popover_strong = popover_weak.lock();
                if (!popover_strong) return;
                GtkWidget* popover = GTK_WIDGET(popover_strong.get());

                gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
                gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
                gtk_popover_menu_set_menu_model(GTK_POPOVER_MENU(popover), G_MENU_MODEL(menu));
                gtk_popover_popup(GTK_POPOVER(popover));
            });
    });
}

void
on_tabs_list_view_row_secondary_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("sidebar: on_tabs_list_view_row_secondary_released");
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    tab_list_item_right_click_cb(list_item, x, y);
}

void
on_pinned_tabs_list_view_item_secondary_pressed(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    orion::log_verbose("sidebar: on_pinned_tabs_list_view_item_secondary_pressed");
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    tab_list_item_right_click_cb(list_item, x, y);
}

void
on_tabs_list_view_row_middle_released(GtkGestureClick* gesture, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkListItem* list_item = GTK_LIST_ITEM(user_data);
    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    MainWindow* main_window = MAIN_WINDOW(gtk_widget_get_root(widget));
    close_tab_page_of_list_item(main_window, list_item);
}

void
on_tab_expander_notify_expanded(GtkExpander* expander, GParamSpec* pspec, gpointer user_data)
{
    bool expanded = gtk_expander_get_expanded(expander);

    //roundabout way of obtaining a reference to the selection model but I would have to refactor for a signal list item factory otherwise
    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    if(!page) return;
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(container)));
    if(!win) return;
    Sidebar* sidebar = main_window_get_sidebar(win);
    GtkListView* view = sidebar_get_tabs_list_view(sidebar);
    OrionTabSelectionModel* model = ORION_TAB_SELECTION_MODEL(gtk_list_view_get_model(view));

    orion_tab_selection_model_set_item_expanded(model, gtk_list_item_get_position(item), expanded);
}

void
on_tab_expander_notify_root(GtkExpander* expander, GParamSpec* pspec, gpointer user_data)
{
    GActionMap* win = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(expander)));
    if(win)
    {
        GBinding* old_binding = G_BINDING(g_object_get_data(G_OBJECT(expander), "visible-binding"));
        if(old_binding) g_binding_unbind(old_binding);
        GAction* action = g_action_map_lookup_action(win, "show-nested-tabs");
        GBinding* binding = g_object_bind_property_full(action, "state", expander, "visible", G_BINDING_DEFAULT, orion::gtk::action_state_to_bool_transform, NULL, NULL, NULL);


        g_object_set_data(G_OBJECT(expander), "visible-binding", binding);

        GVariant* state = g_action_get_state(action);
        defer([state]{ g_variant_unref(state); });
        gtk_widget_set_visible(GTK_WIDGET(expander), g_variant_get_boolean(state));
    }
}

void
on_tab_favicon_notify_root(GtkImage* favicon, GParamSpec* pspec, gpointer user_data)
{
    GActionMap* win = G_ACTION_MAP(gtk_widget_get_root(GTK_WIDGET(favicon)));
    if(win)
    {
        GBinding* old_binding = G_BINDING(g_object_get_data(G_OBJECT(favicon), "visible-binding"));
        if(old_binding) g_binding_unbind(old_binding);
        GAction* action = g_action_map_lookup_action(win, "show-favicons-inside-tabs");
        GBinding* binding = g_object_bind_property_full(action, "state", favicon, "visible", G_BINDING_DEFAULT, orion::gtk::action_state_to_bool_transform, NULL, NULL, NULL);


        g_object_set_data(G_OBJECT(favicon), "visible-binding", binding);

        GVariant* state = g_action_get_state(action);
        defer([state]{ g_variant_unref(state); });
        gtk_widget_set_visible(GTK_WIDGET(favicon), g_variant_get_boolean(state));
    }
}

void
on_tab_title_notify_root(GtkLabel* label, GParamSpec* pspec, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(label)));
    if (!win) return;

    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    if (!page) return;

    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

    const char* title = adw_tab_page_get_title(page);

    orion::log_verbose("sidebar: on_tab_title_notify_root: page_title=%s content=%p label=%p",
        title, container, label
    );

    GBinding* old_binding = G_BINDING(g_object_get_data(G_OBJECT(label), "title-binding"));
    if (old_binding) g_binding_unbind(old_binding);

    GBinding* binding = g_object_bind_property_full(
        container, "tab-title",
        label, "label",
        GBindingFlags(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE), // SYNC_CREATE is needed to work correctly
        orion::gtk::string_transform_to,
        NULL, // no "from" transform
        NULL, // user data
        NULL  // destroy notify
    );

    g_object_set_data(G_OBJECT(label), "title-binding", binding);
}

bool
get_tab_expander_expanded(GtkListItem* item)
{
    if(item)
    {
        AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
        if(!page) return false;
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

        
        return orion_webview_tab_container_get_group_expanded(container);
    }
    else
    {
        return false;
    }
}

bool
get_tab_icon_visible(GtkListItem* item)
{
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    if(!page) return true;
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(adw_tab_page_get_child(page)));
    Sidebar* sidebar = main_window_get_sidebar(win);

    return sidebar_get_show_favicons_inside_tabs(sidebar);
}

void
on_tab_state_flags_changed(GtkPopover* tab_preview, GtkStateFlags old_flags, GtkWidget* tab_box)
{
    GtkStateFlags flags = gtk_widget_get_state_flags(tab_box);
    if(!(flags & GTK_STATE_FLAG_PRELIGHT))
    {
        gtk_popover_popdown(tab_preview);
    }
}