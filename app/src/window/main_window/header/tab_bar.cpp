#define ENABLE_VERBOSE_LOGS 0
#define ENABLE_TAB_DEBUG_INFO !NDEBUG && 0

#include <cmath>

#include <common/src/defer.h>
#include <core/settings.h>

#include "tab_bar.h"
#include "window/columnview/orion_filter_selection_model.h"
#include "window/main_window/main_window.h"
#include "util/tab_context_menu.h"
#include "window/main_window/sidebar/sidebar_current_window_tabs_view.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"

#include "window/scaled_container/forced_size_container.h"

//check tab_bar_update_tab_width for usage, meaning
#define MAX_TABS_WITHOUT_SCROLL 5
#define TAB_WIDTH_EXPONENTIAL_COEFFICIENT 1/10
#define MIN_TITLE_WIDTH 80 //value selected to add up to 120 with MIN_FAVICON_SIZE, 
#define MIN_FAVICON_SIZE 16
#define BUTTON_WIDTH 24 //obtained from gtk inspector, inaccuracy shouldn't be fatal, will just cause an overly low minimum
//size request that will be ignored in favour of the actual minimum width of the tab widget
//recommend making a custom "tab box" widget that will handle this more gracefully
#define SPACING 8

struct _TabBar
{
    GtkBox parent_type;

    GtkListView* pinned_view;
    GtkListView* unpinned_view;
    GtkScrolledWindow* unpinned_tabs_scrolled_window;

    OrionWebviewTabView* tab_view;
    int prev_unpinned_tab_list_width;
    int tab_width;
    GBinding* show_favicons_setting_binding;
    GBinding* show_titles_setting_binding;
    bool show_favicons_inside_tabs;
    bool always_show_titles_setting;
    bool show_titles;
};

enum {
    PROP_0,
    PROP_TAB_VIEW,
    PROP_TAB_WIDTH,
    PROP_SHOW_FAVICONS,
    PROP_ALWAYS_SHOW_TITLES_SETTING,
    PROP_SHOW_TITLES,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

G_DEFINE_TYPE(TabBar, tab_bar, GTK_TYPE_BOX)


static void tab_bar_update_tab_width(TabBar* tab_bar);

static void
on_filter_item_notify_pinned(AdwTabPage* page, GParamSpec* pspec, gpointer user_data)
{
    GtkFilter* filter = GTK_FILTER(user_data);
    gtk_filter_changed(filter, GTK_FILTER_CHANGE_DIFFERENT);
}

static gboolean
pinned_filter_func(gpointer item, gpointer user_data)
{
    g_signal_handlers_disconnect_by_data(item, user_data);
    g_signal_connect_object(item, "notify::pinned", G_CALLBACK(on_filter_item_notify_pinned), user_data, G_CONNECT_DEFAULT);
    return adw_tab_page_get_pinned(ADW_TAB_PAGE(item));
}

static gboolean
unpinned_filter_func(gpointer item, gpointer user_data)
{
    g_signal_handlers_disconnect_by_data(item, user_data);
    g_signal_connect_object(item, "notify::pinned", G_CALLBACK(on_filter_item_notify_pinned), user_data, G_CONNECT_DEFAULT);
    return !adw_tab_page_get_pinned(ADW_TAB_PAGE(item));
}

static void
on_tab_bar_tab_model_items_changed(TabBar* self)
{
    orion::log_verbose("on_tab_bar_tab_model_items_changed");
    if (!(self->tab_view)) {
        // tab bar was probably disposed already
        return;
    }
    tab_bar_update_tab_width(self);
}

static void
tab_bar_scroll_to_selection(TabBar* tab_bar)
{
    GtkSelectionModel* model = gtk_list_view_get_model(tab_bar->unpinned_view);
    if(!model) return;
    GtkBitset* selection = gtk_selection_model_get_selection(model);
    defer([selection]{ gtk_bitset_unref(selection); });
    if(gtk_bitset_get_size(selection))
    {
        uint n_items = g_list_model_get_n_items(G_LIST_MODEL(model));
        //orion::log_debug("items selected: %llu, first selected item: %llu, model length: %u", gtk_bitset_get_size(selection), gtk_bitset_get_nth(selection, 0), n_items);
        
        
        if(gtk_bitset_get_nth(selection, 0)<n_items)
        {
            //scroll is done inside of an idle callback as listview might not have the list item constructed at this point
            orion::gtk::WeakRefWrapper* view_weak_ref_ptr = new orion::gtk::WeakRefWrapper(tab_bar->unpinned_view);
            auto scroll_to_idle_cb = [](gpointer data){
                orion::gtk::WeakRefWrapper* view_weak_ref_ptr = static_cast<orion::gtk::WeakRefWrapper*>(data);
                orion::gtk::GObjectPtr view_strong_ref = view_weak_ref_ptr->lock();
                if(!view_strong_ref) return G_SOURCE_REMOVE;

                GtkListView* view = GTK_LIST_VIEW(view_strong_ref.get());


                GtkBitset* selection = gtk_selection_model_get_selection(gtk_list_view_get_model(view));
                defer([selection]{ gtk_bitset_unref(selection); });
                if(gtk_bitset_get_size(selection))
                {
                    gtk_list_view_scroll_to(view, gtk_bitset_get_nth(selection, 0), GTK_LIST_SCROLL_FOCUS, NULL);
                }
                return G_SOURCE_REMOVE;
            };
            auto user_data_destroy = [](gpointer data){
                delete static_cast<orion::gtk::WeakRefWrapper*>(data);
            };
            g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, scroll_to_idle_cb, view_weak_ref_ptr, user_data_destroy);
            //gtk_list_view_scroll_to(tab_bar->unpinned_view, gtk_bitset_get_nth(selection, 0), GTK_LIST_SCROLL_FOCUS, NULL);
        }
        else
        {
            orion::log_debug("on_tab_bar_unpinned_tab_model_selection_changed(): gtk_bitset_get_nth(selection, 0) < n_items failed");
            orion::log_debug("this likely means something is wrong with OrionTabSelectionModel's implementation of items_changed, selection_changed handling");
        }
    }
    else if(g_list_model_get_n_items(G_LIST_MODEL(model)))
    {
        gtk_list_view_scroll_to(tab_bar->unpinned_view, 0, GTK_LIST_SCROLL_NONE, NULL);
    }
}

static void
on_tab_bar_unpinned_tab_model_selection_changed(GtkSelectionModel* self, guint pos, guint n_items, TabBar* tab_bar)
{
    //attempts to scroll to newly selected item, if there is one
    tab_bar_scroll_to_selection(tab_bar);
}

static void
tab_bar_unbind_tab_view(TabBar* self)
{
    if(self->tab_view)
    {
        g_signal_handlers_disconnect_by_data(gtk_list_view_get_model(self->unpinned_view), self);
        g_signal_handlers_disconnect_by_data(gtk_list_view_get_model(self->pinned_view), self);
        self->tab_view = NULL;
    }
}

void
tab_bar_set_tab_view(TabBar* self, OrionWebviewTabView* tab_view)
{
    g_assert(APP_IS_TAB_BAR(self));
    if(self->tab_view != tab_view)
    {
        tab_bar_unbind_tab_view(self);
        //TODO: unbind old
        self->tab_view = tab_view;
        AdwTabView* view = orion_webview_tab_view_get_actual_view(tab_view);

        OrionFilterSelectionModel* pinned_model = orion_filter_selection_model_new(GTK_SELECTION_MODEL(adw_tab_view_get_pages(view)));
        GtkCustomFilter* pinned_filter = gtk_custom_filter_new(NULL, NULL, NULL);
        defer([pinned_filter]{ g_object_unref(pinned_filter); });
        gtk_custom_filter_set_filter_func(pinned_filter, pinned_filter_func, pinned_filter, NULL);
        orion_filter_selection_model_set_filter(pinned_model, GTK_FILTER(pinned_filter));
        gtk_list_view_set_model(self->pinned_view, GTK_SELECTION_MODEL(pinned_model));

        
        OrionFilterSelectionModel* unpinned_model = orion_filter_selection_model_new(GTK_SELECTION_MODEL(adw_tab_view_get_pages(view)));
        GtkCustomFilter* unpinned_filter = gtk_custom_filter_new(NULL, NULL, NULL);
        defer([unpinned_filter]{ g_object_unref(unpinned_filter); });
        gtk_custom_filter_set_filter_func(unpinned_filter, unpinned_filter_func, unpinned_filter, NULL);
        orion_filter_selection_model_set_filter(unpinned_model, GTK_FILTER(unpinned_filter));
        g_signal_connect_object(unpinned_model, "items-changed", G_CALLBACK(on_tab_bar_tab_model_items_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_data(unpinned_model, "selection-changed", G_CALLBACK(on_tab_bar_unpinned_tab_model_selection_changed), self, NULL, G_CONNECT_AFTER);
        g_signal_connect_object(pinned_model, "items-changed", G_CALLBACK(on_tab_bar_tab_model_items_changed), self, G_CONNECT_SWAPPED);
        gtk_list_view_set_model(self->unpinned_view, GTK_SELECTION_MODEL(unpinned_model));
        tab_bar_update_tab_width(self);

        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TAB_VIEW]);
    }
}

OrionWebviewTabView*
tab_bar_get_tab_view(TabBar* self)
{
    return self->tab_view;
}

static void
tab_bar_update_tab_width(TabBar* tab_bar)
{
    GListModel* model = G_LIST_MODEL(gtk_list_view_get_model(tab_bar->unpinned_view));
    if(!model) return;
    int px_width = gtk_widget_get_width(GTK_WIDGET(tab_bar->unpinned_tabs_scrolled_window));
    int n_pages = g_list_model_get_n_items(model);


    bool force_show_titles = tab_bar->always_show_titles_setting || (!tab_bar->show_favicons_inside_tabs);
    bool actually_show_titles = true;
    int min_width = (force_show_titles?(MIN_TITLE_WIDTH + SPACING):0)+(tab_bar->show_favicons_inside_tabs?(MIN_FAVICON_SIZE + SPACING):0)+BUTTON_WIDTH;

    //holds the potential maximum tab size that would be output throught he px_width division codepath
    //0.985 constant to guarantee that error caused by converting between floating point and integer doesn't cause scrollbar to appear
    int result = px_width*(0.985/std::min(n_pages, MAX_TABS_WITHOUT_SCROLL));

    if(result>min_width)
    {
        if(n_pages>MAX_TABS_WITHOUT_SCROLL)
        {
            //reverse exponential going down to safe minimum
            //since exponential will never be negative, this will always be >= MIN_TAB_PX_WIDTH_WITH_TITLE
            result = (
                (result-min_width) * std::exp(-((float)n_pages-MAX_TABS_WITHOUT_SCROLL)*TAB_WIDTH_EXPONENTIAL_COEFFICIENT) 
                + min_width);
        }
    }
    else
    {
        result = min_width;
    }

    if(!force_show_titles)
    {
        int min_width_for_title = BUTTON_WIDTH + SPACING + MIN_TITLE_WIDTH + (tab_bar->show_favicons_inside_tabs?(SPACING + MIN_FAVICON_SIZE):0);
        if(result < min_width_for_title)
        {
            actually_show_titles = false;
        }
    }

    if(tab_bar->show_titles != actually_show_titles)
    {
        tab_bar->show_titles = actually_show_titles;
        g_object_notify_by_pspec(G_OBJECT(tab_bar), obj_properties[PROP_SHOW_TITLES]);
    }

    tab_bar->tab_width = result;
    g_object_notify_by_pspec(G_OBJECT(tab_bar), obj_properties[PROP_TAB_WIDTH]);
}

static void
tab_bar_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    TabBar *self = APP_TAB_BAR(object);

    switch (property_id) {
    case PROP_TAB_VIEW:
        tab_bar_set_tab_view(self, ORION_WEBVIEW_TAB_VIEW(g_value_get_object(value)));
        break;
    case PROP_SHOW_FAVICONS:
        self->show_favicons_inside_tabs = g_value_get_boolean(value);
        g_object_notify_by_pspec(object, obj_properties[PROP_SHOW_FAVICONS]);
        tab_bar_update_tab_width(self);
        break;
    case PROP_ALWAYS_SHOW_TITLES_SETTING:
        self->always_show_titles_setting = g_value_get_boolean(value);
        orion::log_verbose("%s", std::format("always shot titles setting new value: {}", self->always_show_titles_setting).c_str());
        g_object_notify_by_pspec(object, obj_properties[PROP_ALWAYS_SHOW_TITLES_SETTING]);
        tab_bar_update_tab_width(self);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
tab_bar_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    TabBar *self = APP_TAB_BAR(object);

    switch (property_id) {
    case PROP_TAB_VIEW:
        g_value_set_object(value, tab_bar_get_tab_view(self));
        break;
    case PROP_TAB_WIDTH:
        g_value_set_int(value, self->tab_width);
        break;
    case PROP_SHOW_FAVICONS:
        g_value_set_boolean(value, self->show_favicons_inside_tabs);
        break;
    case PROP_ALWAYS_SHOW_TITLES_SETTING:
        g_value_set_boolean(value, self->always_show_titles_setting | (!self->show_favicons_inside_tabs));
        break;
    case PROP_SHOW_TITLES:
        orion::log_verbose("%s", std::format("show titles: {}", self->show_titles).c_str());
        g_value_set_boolean(value, self->show_titles);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
tab_bar_init(TabBar* self)
{
    g_type_ensure(ORION_TYPE_FILTER_SELECTION_MODEL);
    g_type_ensure(APP_TYPE_FORCED_SIZE_CONTAINER);
    gtk_widget_init_template(GTK_WIDGET(self));
}

static void
tab_bar_dispose(GObject* object)
{
    orion::log_verbose("tab_bar_dispose");
    TabBar* self = APP_TAB_BAR(object);
    tab_bar_unbind_tab_view(self);
    G_OBJECT_CLASS(tab_bar_parent_class)->dispose(object);
}

static GVariant*
int64_target_from_id(GtkListItem* item, int64_t id)
{
    return g_variant_new_int64(id);
}

static gchar*
tab_debug_info(GtkListItem* item, double seq_num)
{
    AdwTabPage* adw_tab_page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(adw_tab_page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);
    std::string id_str = "[" + std::to_string(tab_id.value) + "; " + std::to_string(seq_num) + "]";
    return g_strdup(id_str.c_str());
}

static void
on_context_menu_closed(GtkPopoverMenu* self, gpointer user_data)
{
    g_idle_add_once((GSourceOnceFunc)gtk_widget_unparent, self);
}

static void
on_tab_middle_pressed(GtkGestureClick* self)
{
    gtk_gesture_set_state(GTK_GESTURE(self), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_tab_middle_released(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkWidget* row = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(row));
    if(!win) return;
    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

    GAction* close_tab_action = g_action_map_lookup_action(G_ACTION_MAP(win), "close-tab");
    g_return_if_fail(close_tab_action);
    GVariant* id = g_variant_new_int64(orion_webview_tab_container_get_id(container).value);
    g_action_activate(close_tab_action, id);
}

static gboolean
is_audio_indicator_visible(GtkListItem* item, gboolean is_playing_audio, gboolean is_muted)
{
    return is_playing_audio || is_muted;
}

static gchar*
audio_indicator_icon_name(GtkListItem* item, gboolean is_muted)
{
    return g_strdup(is_muted ? "speaker-0-symbolic" : "speaker-3-symbolic");
}

static gchar*
audio_indicator_tooltip(GtkListItem* item, gboolean is_muted)
{
    return g_strdup(is_muted ? _("Unmute Tab") : _("Mute Tab"));
}


static void
on_tab_audio_button_clicked(GtkButton* button, gpointer user_data)
{
    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion_webview_tab_container_set_is_muted(container, !orion_webview_tab_container_get_is_muted(container));
}

static void
on_tab_secondary_released(GtkGestureClick* self, gint n_press, gdouble x, gdouble y, gpointer user_data)
{
    GtkWidget* row = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self));
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(row));
    if(!win) return;
    GtkListItem* item = GTK_LIST_ITEM(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));


    orion::GetOtherTabIdsQuery other_tab_ids_query{};
    other_tab_ids_query.exclude_tab_id = orion_webview_tab_container_get_id(container);

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper tab_page_weak(page);
    orion::gtk::WeakRefWrapper row_weak(row);

    GdkRectangle rect{.x = (int)x, .y = (int)y, .width = 1, .height = 1};
    

    global_core_api->get_other_tab_ids(other_tab_ids_query, [win_weak,tab_page_weak,row_weak,rect](orion::GetOtherTabIdsReply reply){
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
            [row_weak,rect,menu](orion::Error err){
                defer([menu]{ g_object_unref(menu); });
                VERIFY_LOG(err);
                auto row_strong = row_weak.lock();
                if (!row_strong) return;
                GtkWidget* row = GTK_WIDGET(row_strong.get());
                
                GtkWidget* popover = gtk_popover_menu_new_from_model(G_MENU_MODEL(menu));
                //g_object_unref(menu);
                gtk_widget_set_parent(popover, row);

                gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
                gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
                
                g_signal_connect(popover, "closed", G_CALLBACK(on_context_menu_closed), NULL);
                gtk_popover_popup(GTK_POPOVER(popover));
            });
    });
}

static void
on_tab_secondary_pressed(GtkGestureClick* self)
{
    gtk_gesture_set_state(GTK_GESTURE(self), GTK_EVENT_SEQUENCE_CLAIMED);
}

static gboolean
on_event_controller_scroll_scroll(GtkScrolledWindow* scrolled_win, gdouble dx, gdouble dy, GtkEventControllerScroll* self)
{
    GtkAdjustment* hadjustment = gtk_scrolled_window_get_hadjustment(scrolled_win);
    GdkDevice* device = gtk_event_controller_get_current_event_device(GTK_EVENT_CONTROLLER(self));
    if(gdk_device_get_source(device) == GDK_SOURCE_MOUSE)
    {
        gtk_adjustment_set_value(hadjustment, CLAMP(gtk_adjustment_get_value(hadjustment)+gtk_adjustment_get_step_increment(hadjustment)*dy, gtk_adjustment_get_lower(hadjustment), gtk_adjustment_get_upper(hadjustment)));
        return true;
    }
    else
    {
        return false;
    }
}


static gboolean
invert_bool(GtkListItem* item, gboolean val)
{
    return !val;
}

static char*
get_uri_authority(GtkListItem* item, const char* uri)
{
    return g_strdup(orion::Uri::parse_strict(uri).authority().c_str());
}

static void
on_preview_picture_map(GtkPicture* picture, GtkListItem* item)
{
    if(!gtk_list_item_get_selected(item))
    {
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(gtk_list_item_get_child(item)));
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);

        AdwTabPage* page = ADW_TAB_PAGE(gtk_list_item_get_item(item));
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        orion::TabId id = orion_webview_tab_container_get_id(container);

        orion::gtk::WeakRefWrapper picture_weak(picture);
        global_core_api->get_setting(profile_id, orion::settings::tabs::show_tab_preview_on_hover,
            [picture_weak,id](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                if(!reply.value) return;

                global_core_api->get_tabs({id},
                    [picture_weak](orion::GetTabsReply reply){
                        VERIFY_LOG(reply.err);

                        if(reply.tabs.empty()) return;

                        std::vector<uint8_t>& image_data = reply.tabs.at(0).snapshot_image; 

                        auto picture_strong = picture_weak.lock();
                        if(!picture_strong) return;

                        GtkPicture* picture = GTK_PICTURE(picture_strong.get());
                        /*
                        if(!gtk_widget_get_root(GTK_WIDGET(popover)))
                        {
                            //NOTE: this is known to be reached when cancelling a tab dnd operation over itself,
                            //the tab will get transferred, which will cause the popover to lose its root widget while our async callbacks go through
                            orion::log_debug("on_tab_event_controller_motion_enter: attempting to popup popover, but it is not inside the widget structure");
                            return;
                        }
                            */

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
                    });
            });
    }
    else
    {
        orion::log_verbose("on_preview_picture_map: mapped supposedly invisible picture");
    }
}

static TabBar*
get_tab_bar(GtkListItem* item, MainWindow* win)
{
    if(!win) return NULL;
    TabBar* tab_bar = APP_TAB_BAR(gtk_widget_get_ancestor(gtk_list_item_get_child(item), APP_TAB_BAR_TYPE));
    return tab_bar ? g_object_ref(tab_bar) : NULL;
}

// static gboolean
// get_models_have_items(GtkListItem* item, GListModel* unpinned_model, GListModel* pinned_model)
// {
//     if(!unpinned_model) return false;
//     if(!pinned_model) return false;
    
//     return g_list_model_get_n_items(unpinned_model) && g_list_model_get_n_items(pinned_model);
// }

static gboolean
get_models_have_items(GtkListItem* item, uint unpinned_n_items, uint pinned_n_items)
{   
    return unpinned_n_items && pinned_n_items;
}

// static GtkPropagationPhase
// get_preview_event_controller_propagation_phase(GVariant* state)
// {
//     if(g_variant_get_boolean(state))
//     {
//         return GTK_PHASE_TARGET;
//     }
//     else
//     {
//         return GTK_PHASE_NONE;
//     }
// }

// static int
// transform_preview_event_controller_propagation_phase(GBinding* binding, const GValue* from_value, GValue* to_value, gpointer user_data)
// {
//     GVariant* var = g_value_get_variant(from_value);
//     if(var)
//     {
//         g_value_set_enum(to_value, get_preview_event_controller_propagation_phase(var));
//         return TRUE;
//     }
//     else
//     {
//         return false;
//     }
// }


// static void
// preview_event_controller_bind_propagation_phase(GtkEventController* preview_event_controller, GParamSpec* pspec, GtkWidget* box)
// {
//     GtkRoot* root = gtk_widget_get_root(box);
//     if(!MAIN_IS_WINDOW(root))
//     {
//         GBinding* binding = G_BINDING(g_object_get_data(G_OBJECT(preview_event_controller), "phase-binding"));
//         if(binding)
//         {
//             g_binding_unbind(binding);
//             g_object_set_data(G_OBJECT(preview_event_controller), "phase-binding", NULL);
//         }
//         return;
//     }

//     GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(root), "show-tab-preview-on-hover");
//     GBinding* binding = g_object_bind_property_full(setting, "state", preview_event_controller, "propagation-phase",
//         GBindingFlags(G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE), transform_preview_event_controller_propagation_phase, NULL, NULL, NULL);
//     g_object_set_data(G_OBJECT(preview_event_controller), "phase_binding", binding);
// }

static void
preview_popover_notify_setting(GAction* setting, GParamSpec* pspec, GtkPopover* preview_popover)
{
    GVariant* state = g_action_get_state(setting);
    defer([state]{ g_variant_unref(state); });
    if(!g_variant_get_boolean(state))
    {
        gtk_popover_popdown(preview_popover);
    }
}

static void
preview_popover_bind_to_setting(GtkPopover* preview_popover, GParamSpec* pspec, GtkWidget* box)
{
    GtkRoot* root = gtk_widget_get_root(box);
    if(!MAIN_IS_WINDOW(root))
    {
        //gtk_popover_popdown(preview_popover); should be unnecessary
        return;
    }

    GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(root), "show-tab-preview-on-hover");

    GVariant* state = g_action_get_state(setting);
    defer([state]{ g_variant_unref(state); });
    if(!g_variant_get_boolean(state))
    {
        gtk_popover_popdown(preview_popover);
    }

    g_signal_connect_object(setting, "notify::state", G_CALLBACK(preview_popover_notify_setting), preview_popover, G_CONNECT_DEFAULT);
}

static void
on_preview_event_controller_enter(GtkPopover* preview_popover, gdouble x, gdouble y, GtkEventController* controller)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(preview_popover));
    g_assert(MAIN_IS_WINDOW(root));

    GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(root), "show-tab-preview-on-hover");

    GVariant* state = g_action_get_state(setting);
    defer([state]{ g_variant_unref(state); });
    if(g_variant_get_boolean(state))
    {
        gtk_popover_popup(preview_popover);
    }
}


static void
tab_bar_snapshot(GtkWidget* widget, GtkSnapshot* snapshot)
{
    TabBar* tab_bar = APP_TAB_BAR(widget);
    int unpinned_tab_list_width = gtk_widget_get_width(GTK_WIDGET(tab_bar->unpinned_tabs_scrolled_window));
    if(tab_bar->prev_unpinned_tab_list_width != unpinned_tab_list_width)
    {
        orion::gtk::WeakRefWrapper tab_bar_weak(tab_bar);
        global_core_api->async([tab_bar_weak]{
            auto tab_bar_strong = tab_bar_weak.lock();
            if(!tab_bar_strong) return;

            TabBar* tab_bar = APP_TAB_BAR(tab_bar_strong.get());
            tab_bar_update_tab_width(tab_bar);
        });        
        tab_bar->prev_unpinned_tab_list_width = unpinned_tab_list_width;
    }
    //GTK_WIDGET_CLASS(tab_bar_parent_class)->snapshot(widget, snapshot);


    graphene_rect_t bounds{};
    graphene_point_t next_origin{};
    for(GtkWidget* child = gtk_widget_get_first_child(widget); GTK_IS_WIDGET(child); child = gtk_widget_get_next_sibling(child))
    {
        if(!GTK_IS_SCROLLED_WINDOW(child)) 
        {
            gtk_widget_snapshot_child(widget, child, snapshot);
            next_origin.x += gtk_widget_get_width(child) + gtk_box_get_spacing(GTK_BOX(widget));
        }
        else
        {
            break;
        }
    }
    if(gtk_widget_compute_bounds(widget, GTK_WIDGET(tab_bar->unpinned_tabs_scrolled_window), &bounds))
    {
        //NOTE: these are calculated manually because the box based nature of this widget class causes compute bounds to fail
        bounds.origin = next_origin;
        bounds.size.width = gtk_widget_get_width(GTK_WIDGET(tab_bar->unpinned_tabs_scrolled_window));
        GskColorStop stops_right[] = {
            { 0.00f, { 1, 0, 0, 1 } },
            //{ 0.95f, { 1, 1, 1, 1 } },
            { 0.965f, { 1, 0, 0, 1 } },
            { 1.00f, { 1, 0, 0, 0 } },
        };
        GskColorStop stops_left[] = {
            { 0.00f, { 1, 0, 0, 0 } },
            { 0.035f, { 1, 0, 0, 1 } },
            //{ 0.05f, { 1, 1, 1, 1 } },
            { 1.00f, { 1, 0, 0, 1 } },
        };
        GdkRGBA opaque = { 1, 1, 1, 1 };


        graphene_point_t start{bounds.origin.x, bounds.origin.y + bounds.size.height/2}, end{bounds.origin.x + bounds.size.width, bounds.origin.y + bounds.size.height/2};
        GskRenderNode *mask_right, *mask_left;
        
        GtkAdjustment* adjustment = gtk_scrolled_window_get_hadjustment(tab_bar->unpinned_tabs_scrolled_window);
        if((gtk_adjustment_get_upper(adjustment)-gtk_adjustment_get_page_size(adjustment)) != gtk_adjustment_get_value(adjustment))
        {
            mask_right = gsk_linear_gradient_node_new(&bounds, &start, &end, stops_right, 3);
        }
        else
        {
            mask_right = gsk_color_node_new(&opaque, &bounds);
        }
        defer([mask_right]{ gsk_render_node_unref(mask_right); });
        
        if(gtk_adjustment_get_lower(adjustment) != gtk_adjustment_get_value(adjustment))
        {
            mask_left = gsk_linear_gradient_node_new(&bounds, &start, &end, stops_left, 3);
        }
        else
        {
            mask_left = gsk_color_node_new(&opaque, &bounds);
        }
        defer([mask_left]{ gsk_render_node_unref(mask_left); });

        //NOTE: structured like this to better show the stucture of what gtk_snapshot_pop corresponds to what
        gtk_snapshot_save(snapshot);

        gtk_snapshot_push_mask(snapshot, GSK_MASK_MODE_ALPHA);
        {
            gtk_snapshot_append_node(snapshot, mask_right);
        }
        gtk_snapshot_pop(snapshot);
        {
            gtk_snapshot_push_mask(snapshot, GSK_MASK_MODE_ALPHA);
            {
                gtk_snapshot_append_node(snapshot, mask_left);
            }
            gtk_snapshot_pop(snapshot);
            {
                gtk_widget_snapshot_child(widget, GTK_WIDGET(tab_bar->unpinned_tabs_scrolled_window), snapshot);
            }
            gtk_snapshot_pop(snapshot);
        }
        gtk_snapshot_pop(snapshot);

        gtk_snapshot_restore(snapshot);
    }
}


static void
tab_bar_map(GtkWidget* widget)
{
    TabBar* tab_bar = APP_TAB_BAR(widget);
    GTK_WIDGET_CLASS(tab_bar_parent_class)->map(widget);
    tab_bar_scroll_to_selection(tab_bar);
}

static void
tab_bar_root(GtkWidget* widget)
{
    TabBar* tab_bar = APP_TAB_BAR(widget);
    GTK_WIDGET_CLASS(tab_bar_parent_class)->root(widget);
    
    g_clear_pointer(&tab_bar->show_favicons_setting_binding, g_binding_unbind);
    g_clear_pointer(&tab_bar->show_titles_setting_binding, g_binding_unbind);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(widget));
    auto transform_to = [](GBinding* binding, const GValue* from, GValue* to, gpointer data){
        g_value_set_boolean(to, g_variant_get_boolean(g_value_get_variant(from)));
        return 1;
    };
    // ? Could be error
    GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-favicons-inside-tabs");
    tab_bar->show_favicons_setting_binding = g_object_bind_property_full(setting, "state", tab_bar, "show-favicons", G_BINDING_SYNC_CREATE, transform_to, NULL, NULL, NULL);
    setting = g_action_map_lookup_action(G_ACTION_MAP(win), "always-show-website-titles-in-tabs");
    tab_bar->show_titles_setting_binding = g_object_bind_property_full(setting, "state", tab_bar, "always-show-titles-setting", G_BINDING_SYNC_CREATE, transform_to, NULL, NULL, NULL);
}

struct point_double {
    double x;
    double y;
};

static void
on_tab_drag_source_gesture_begin(GtkGesture* self, GdkEventSequence* sequence, gpointer user_data)
{
    point_double* start = g_new0(point_double, 1);
    if(gtk_gesture_get_point(self, NULL, &start->x, &start->y))
    {
        g_object_set_data_full(G_OBJECT(self), "start-point", start, [](gpointer data){ 
            g_free(data); 
        });
    }
    else
    {
        g_free(start);
    }
}

static void
on_tab_drag_source_gesture_end(GtkGesture* self, GdkEventSequence* sequence, gpointer user_data)
{
    g_object_set_data_full(G_OBJECT(self), "start-point", NULL, [](gpointer data){ g_free(data); });
}

static void
on_tab_drag_source_gesture_update(GtkGesture* self, GdkEventSequence* sequence, gpointer user_data)
{
    point_double* start = static_cast<point_double*>(g_object_get_data(G_OBJECT(self), "start-point"));
    if(!start) return;

    point_double end;
    gtk_gesture_get_point(self, NULL, &end.x, &end.y);

    int drag_threshold;
    GdkDisplay* display = gtk_widget_get_display(gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(self)));
    GtkSettings* settings = gtk_settings_get_for_display(display);
    g_object_get(settings, "gtk-dnd-drag-threshold", &drag_threshold, NULL);


    if((ABS(end.x - start->x) > drag_threshold) || (ABS(end.y - start->y) > drag_threshold))
    {
        gtk_gesture_set_state(self, GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static char*
id_to_string(GtkListItem* item, int64_t id)
{
    return g_strdup_printf("%ld", id);
}

static char*
double_to_string(GtkListItem* item, double val)
{
    return g_strdup_printf("%lf", val);
}

static float
bool_to_opacity(GtkListItem* item, gboolean value)
{
    return value?1:0;
}

static gboolean
get_is_tab_debug_info_visible()
{
#if ENABLE_TAB_DEBUG_INFO
    return TRUE;
#else
    return FALSE;
#endif
}

static void
tab_bar_class_init(TabBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = tab_bar_dispose;
    //object_class->finalize = tab_bar_finalize;

    object_class->set_property = tab_bar_set_property;
    object_class->get_property = tab_bar_get_property;

    obj_properties[PROP_TAB_VIEW] = g_param_spec_object("tab-view", "tab-view", NULL, ORION_WEBVIEW_TAB_VIEW_TYPE,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_TAB_WIDTH] = g_param_spec_int("tab-width", "tab-width", NULL, std::min(MIN_FAVICON_SIZE, MIN_TITLE_WIDTH), G_MAXINT, MIN_FAVICON_SIZE + MIN_TITLE_WIDTH,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_SHOW_FAVICONS] = g_param_spec_boolean("show-favicons", "show-favicons", NULL, false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_ALWAYS_SHOW_TITLES_SETTING] = g_param_spec_boolean("always-show-titles-setting", "always-show-titles-setting", NULL, false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_SHOW_TITLES] = g_param_spec_boolean("show-titles", "show-titles", NULL, false,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS));
    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->snapshot = tab_bar_snapshot;
    widget_class->map = tab_bar_map;
    widget_class->root = tab_bar_root;
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/header/tab_bar.ui");
    gtk_widget_class_bind_template_child(widget_class, TabBar, pinned_view);
    gtk_widget_class_bind_template_child(widget_class, TabBar, unpinned_view);
    gtk_widget_class_bind_template_child(widget_class, TabBar, unpinned_tabs_scrolled_window);
    gtk_widget_class_bind_template_callback(widget_class, int64_target_from_id);
    gtk_widget_class_bind_template_callback(widget_class, tab_debug_info);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_secondary_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_middle_released);
    gtk_widget_class_bind_template_callback(widget_class, is_audio_indicator_visible);
    gtk_widget_class_bind_template_callback(widget_class, audio_indicator_icon_name);
    gtk_widget_class_bind_template_callback(widget_class, audio_indicator_tooltip);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_audio_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_middle_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_event_controller_scroll_scroll);
    gtk_widget_class_bind_template_callback(widget_class, invert_bool);
    gtk_widget_class_bind_template_callback(widget_class, get_uri_authority);
    gtk_widget_class_bind_template_callback(widget_class, on_preview_picture_map);
    gtk_widget_class_bind_template_callback(widget_class, get_tab_bar);
    gtk_widget_class_bind_template_callback(widget_class, get_models_have_items);
    gtk_widget_class_bind_template_callback(widget_class, preview_popover_bind_to_setting);
    gtk_widget_class_bind_template_callback(widget_class, on_preview_event_controller_enter);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_prepare);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_drag_begin);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_drag_end);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_prepare);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_drag_begin);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_drag_end);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drag_source_gesture_begin);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drag_source_gesture_end);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drag_source_gesture_update);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drop_target_motion);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drop_target_leave);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_state_flags_changed);
    gtk_widget_class_bind_template_callback(widget_class, id_to_string);
    gtk_widget_class_bind_template_callback(widget_class, double_to_string);
    gtk_widget_class_bind_template_callback(widget_class, bool_to_opacity);
    gtk_widget_class_bind_template_callback(widget_class, get_is_tab_debug_info_visible);
}
