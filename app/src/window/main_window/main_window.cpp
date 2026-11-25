#define ENABLE_VERBOSE_LOGS 0
#define OPEN_TEST_DIALOG 0
#define ENABLE_ITP_SUMMARY 0
#define ENABLE_KAGI_SERVICE 1

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <adwaita.h>

#include <rfl/json.hpp>

#include <vector>
#include <list>
#include <string>
#include <iostream>
#include <functional>
#include <sstream>
#include <set>
#include <format>
#include <chrono>

#include <common/src/log.h>
#include <common/src/uri.h>
#include <common/src/error.h>
#include <core/settings.h>
#include <common/src/defer.h>
#include <common/src/uuid.h>
#include <common/src/string_util.h>
#include <common/src/filter_list.h>
#include <core/webextensions/runtime.h>

#include "main_window.h"
#include "app.h"
#include "orion_window_iface.h"
#include "action/main_window/action.h"
#include "action/main_window/action_map.h"
#include "window/settings_for_website_dialog.h"
#include "window/privacy_popover.h"
#include "window/rss_feeds_popover.h"
#include "window/page_tweaks_popover.h"
#include "window/save_window_popover.h"
#include "window/search_popover.h"
#include "window/search_completion_popover.h"
#include "window/add_bookmark_dialog.h"
#include "window/bookmarks_dialog.h"
#include "window/window_list_popover.h"
#include "window/certificate_info_dialog.h"
#include "window/main_window/header/header_bar.h"
#include "window/main_window/header/bookmarks_bar.h"
#include "window/main_window/sidebar/reading_list_toggle_group.h"
#include "window/main_window/sidebar/sidebar.h"
#include "window/main_window/js_sidebar/js_sidebar.h"
#include "window/search_recommendations.h"
#include "window/window_list_box.h"
#include "window/profiles_icon_selection_box.h"
#include "window/main_window/menu/profile_windows_submenu.h"
#include "window/main_window/menu/new_tab_menu.h"

#include "util/click_util.h"
#include "util/dialog_util.h"
#include "util/text_util.h"
#include "util/toolbar_util.h"
#include "util/bookmarks_util.h"
#include "util/tab_context_menu.h"
#include "util/url_or_search_detector.h"
#include "util/file_util.h"
#include "util/ptr_util.h"
#include "util/css_util.h"
#include "util/action_util.h"
#include "util/menu_util.h"
#include "util/profile_manager.h"
#include "util/enum_util.h"
#include "util/settings_util.h"

#include "window/accelerator_widget/orion_accelerator_entry.h"
#include "window/editable_cell_widget/orion_editable_cell_entry.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"
#include "window/orion_webview_tabview_widgets/orion_address_entry.h"
#include "window/programmable_button_popover.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_container.h"
#include "window/main_menu/main_menu.h"
#include "window/main_window/menu/profiles_button_menu.h"
#include "window/scaled_container/scaled_container.h"
#include "window/import_csv_passwords_dialog.h"
#include "window/delete_csv_passwords_dialog.h"
#include "window/main_window/header/tab_bar.h"
#include "window/fill_password_dialog.h"
#include "window/tab_switcher.h"
#include "window/set_default_search_engine_dialog.h"
#include "content_block/embedded_filter_lists.h"

//#define ENABLE_WEBKIT 1 //TODO: temporary hack to enable code autocompletion, remove 

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

namespace //force internal linkage to avoid potential ODR violations
{
    struct MainWindowCppWrapper
    {
        orion::ProfileId profile_id; // current profile ID of this main window
        orion::WindowId window_id;
        orion::ObserverId profiles_change_observer;
        orion::ObserverId programmable_buttons_observer;
        orion::ObserverId tab_layout_setting_observer;
        orion::ObserverId bookmarks_bar_visibility_observer;
        orion::ObserverId focus_mode_observer;
        orion::ObserverId bookmarks_bar_style_observer;
        orion::ObserverId toolbar_item_style_observer;
        orion::ObserverId window_list_button_style_observer;
        orion::ObserverId profiles_button_style_observer;
        orion::ObserverId show_tab_preview_on_hover_observer;
        orion::ObserverId recently_bookmarked_change_observer;
        orion::ObserverId all_bookmarked_change_observer;
        orion::ObserverId all_folders_change_observer;
        orion::ObserverId show_recently_opened_on_top_observer;
        orion::ObserverId switch_saved_windows_in_place_observer;
        orion::ObserverId use_tab_switcher_ui_for_ctrl_tab_observer;
        orion::ObserverId tab_switch_order_observer;
        orion::ObserverId windows_change_observer;
        orion::ObserverId bookmarks_change_observer;
        orion::ObserverId recently_closed_tabs_observer;
        orion::ObserverId show_favicons_inside_tabs_observer;
        orion::ObserverId always_show_website_titles_in_tabs_observer;
        orion::ObserverId open_new_tab_next_to_current_tab_observer;
        orion::ObserverId file_download_location_observer;
        orion::ObserverId file_download_custom_path_observer;
        orion::ObserverId show_sidebar_observer;
        orion::ObserverId auto_show_sidebar_observer;
        orion::ObserverId auto_show_sidebar_in_fullscreen_observer;
        orion::ObserverId always_show_toolbar_in_fullscreen_observer;
        orion::ObserverId show_status_bar_on_link_hover_observer;
        orion::ObserverId web_views_observer;
        orion::ObserverId when_opening_link_in_new_tab_make_it_active_observer;
        orion::ObserverId reader_mode_font_size_index_observer;
        orion::ObserverId reader_mode_background_type_observer;
        orion::ObserverId reader_mode_font_name_observer;
        orion::ObserverId reader_mode_document_width_observer;
        orion::ObserverId reader_mode_text_alignment_observer;
        orion::ObserverId reader_mode_border_shadow_enabled_observer;
        orion::ObserverId low_power_mode_setting_observer;
        orion::ObserverId page_tweaks_observer;

        ContextMenuData context_menu_data;
        
        std::shared_ptr<orion::bridge::ProfileWorld> profile_world;
        std::shared_ptr<orion::KagiService> kagi_service;

        // We cannot embed std::map directly (or indirectly using this wrapper struct) into MainWindow
        // (which inherits from AdwApplicationWindow) because it would trigger "‘offsetof’ within
        // non-standard-layout type" compile warning (= error), so we use a pointer to std::map instead.
        // The map holds strong references to the tab views. In the future, this map can be refactored
        // to class container that will have better encapsulation and be less error-prone.
        std::map<orion::WindowId, OrionWebviewTabView*>* tab_view_map;
        std::map<orion::ProgrammableButtonId, orion::gtk::StrongRefWrapper>* programmable_button_shortcut_map;

        std::string current_website;

        std::unique_ptr<MainWindowNewFromBackendCb> done_cb;
    };
}

struct _MainWindow
{
	AdwApplicationWindow parent;

    HeaderBar* header_bar;
    EraseElementsBar* erase_elements_bar;
    BookmarksBar* bookmarks_bar;
    FindBar* find_bar;
    AdwOverlaySplitView* overlay_split_view;
    Sidebar* sidebar;
    AdwOverlaySplitView* js_sidebar_split_view;

    GtkPopover* test_popover;

	// Context menus (presented typically after a right-click)
	GtkPopoverMenu* sidebar_button_popover;
	GtkPopoverMenu* window_list_button_popover;
	GtkPopoverMenu* profiles_button_popover;
    GtkPopoverMenu* tabbar_tab_popover; // reused for each tab in the tab bar
    GtkPopover* programmable_button_popover;
    GMenuModel* main_menu_model;
    GMenu* profiles_list_menu;
    GMenu* current_profile_menu;
    GMenu* profile_windows_submenu;
    GMenu* recently_bookmarked_menu;
    GMenu* all_bookmarks_section;
    gboolean profiles_button_popover_is_open;
	GtkPopoverMenu* address_focus_mode_button_popover;
	GtkPopoverMenu* reader_mode_button_popover;
	GtkPopoverMenu* rss_feeds_button_popover;
	GtkPopoverMenu* certificates_button_popover;
	GtkPopoverMenu* address_entry_popover;
	GtkPopoverMenu* reload_page_button_popover;
	GtkPopoverMenu* share_button_popover; // on left-click

	GtkBox* page_content_box;
	GtkPopoverMenu* page_content_box_popover;
    GtkPopoverMenu* toolbar_box_popover;

	GtkPopover* privacy_popover;
	GtkPopover* rss_feeds_popover;
	GtkPopover* page_tweaks_popover;
	GtkPopover* save_window_popover;
	GtkPopover* reader_mode_unavailable_popover;
	GtkPopover* downloads_popover;
	GtkPopover* window_list_popover;

    TabSwitcher* tab_switcher;

	GtkScrolledWindow* page_content_window;
	AdwTabPage* page_content_tabpage;
	OrionWebviewTabView* page_content_tabview;
    AdwTabOverview* tab_overview;

    GtkWidget* last_right_clicked_widget;
    const char* profile_name;

    //Borderless state can be obtained from window.
    gboolean webkit_fullscreen = FALSE; // JavaScript-induced fullscreen
    gboolean window_borderless; // tracks desired UI Fullscreen state of the window (ignores webkit fullscreen)
    gboolean ui_hidden = FALSE; // tracks focus mode
    gboolean is_displaying_link_overlay = FALSE; // status bar

    GtkRevealer* main_ui_revealer;
    GtkRevealer* link_overlay_revealer;
    AdwToolbarView* main_toolbar_view;

    GtkLabel* link_label;

    MainWindowCppWrapper wrapper;
    gboolean is_active = FALSE; // whether this window has focus
    GtkBox* page_tabview_box;
    GtkBox* main_box;
    GtkBox* tab_bar_box;
    TabBar* tab_bar;

    gboolean settings_dialog_open = FALSE; // whether Settings dialog is currently presented

    GtkDropTarget* drop_target;
    GtkShortcutController* programmable_button_shortcut_controller;

    guint dnd_hover_timeout_handler;

    GBinding* tab_view_title_binding;
    GtkRevealer* header_bar_revealer;
    GtkOverlay* top_bar_overlay;
    orion::PrivateMode private_mode; //will be set to 0 (== orion::PrivateMode::no) upon gobject creation, glib does not respect default initialisation specified here

    double mouse_x;
    double mouse_y;
    guint low_power_mode_timer;

    GtkExpressionWatch* reader_mode_enabled_watch;

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitUserScript* reader_template_script;
#endif
    ProfileDataHolder* profile_data_holder;
};

static void
main_window_orion_window_iface_init(OrionWindowInterface* iface) {} //noop, iface provides only properties which are overriden in class_init

G_DEFINE_TYPE_WITH_CODE(MainWindow, main_window, ADW_TYPE_APPLICATION_WINDOW,
    G_IMPLEMENT_INTERFACE(ORION_TYPE_WINDOW, main_window_orion_window_iface_init))

// This is needed for custom properties of main window
enum {
    PROP_0,
    PROP_TAB_VIEW,
    PROP_NEW_FROM_BACKEND_CB,
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    PROP_READER_TEMPLATE_SCRIPT,
#endif

    //property overrides
    PROP_WINDOW_ID,
    PROP_PROFILE_ID,
    PROP_PROFILE_DATA_HOLDER,
    PROP_PRIVATE_MODE,
    PROP_WEB_VIEW_FACTORY,
    PROP_URI,
    PROP_TLS_STATUS,
    PROP_BACK_LIST,
    PROP_FORWARD_LIST,
    N_PROPS,
};

enum {
    SIG_PRIMARY_PRESSED,
    N_SIGNALS,
};

static GParamSpec* obj_properties[N_PROPS] = { NULL, };
static guint signals[N_SIGNALS] = {0};

namespace orion::gtk {

MainWindow*
get_root_main_window(GtkWidget* widget)
{
    if (!widget) return NULL;
    GtkRoot* root = gtk_widget_get_root(widget);
    if (!MAIN_IS_WINDOW(root)) {
        return NULL;
    }
    return MAIN_WINDOW(root);
}

} // namespace orion::gtk

OrionWebviewTabContent*
main_window_get_current_tab_content(MainWindow* self)
{
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(self);
    if (!tab_view) {
        orion::log_warning("main_window_get_current_tab_content: failed to get current tab view");
        return NULL;
    }
    return orion_webview_tab_view_get_current_tab_content(tab_view);
}

static void
main_window_sync_page_zoom_button(MainWindow* self)
{
    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
    if (!content) return;
    orion_webview_tab_content_sync_webview_zoom_level(content);
}

static void
main_window_sync_popup_blocked_button(MainWindow* self)
{
    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    guint count = content ? (guint)orion_webview_tab_content_get_blocked_popup_urls(content).size() : 0;
#else
    guint count = 0;
#endif
    main_window_update_popup_blocked_button(self, count);
}

static void
main_window_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    MainWindow* self = MAIN_WINDOW(object);

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);

    switch (property_id)
    {
        case PROP_PROFILE_ID:
            self->wrapper.profile_id = orion::ProfileId(g_value_get_int64(value));
            self->profile_data_holder = orion_app_ref_profile_data_holder(self->wrapper.profile_id);
            gtk_widget_insert_action_group(GTK_WIDGET(self), "profile", G_ACTION_GROUP(self->profile_data_holder));
            g_object_notify_by_pspec(object, obj_properties[PROP_PROFILE_DATA_HOLDER]);
            
            global_core_api->get_profiles({self->wrapper.profile_id}, [wrapped_ref](orion::GetProfilesReply reply){
                VERIFY_LOG(reply.err);
                if(reply.profiles.size()==0)
                {
                    orion::log_error("invalid profile id for main window set, changing to default");
                    //TODO: finish this sanity check
                    global_core_api->get_default_profile_id([wrapped_ref](orion::GetDefaultProfileIdReply reply){
                        MainWindow* win = MAIN_WINDOW(wrapped_ref->get());
                        if(reply.err)
                        {
                            if(win)
                            {
                                gtk_window_close(GTK_WINDOW(win));
                                g_object_unref(win);
                            }
                            VERIFY_LOG(reply.err);
                        }
                        if(win)
                        {
                            GValue* val = g_new(GValue, 1);
                            *val = G_VALUE_INIT;
                            g_value_set_int64(val, reply.profile_id.value);
                            //sadly a recursive algorithm but if get default profile id fails, this will just exit
                            g_object_set_property(G_OBJECT(val), "", val);
                            g_free(val);
                            g_object_unref(win);
                        }
                    });
                }
            });

            break;
        case PROP_WINDOW_ID:
            {
                self->wrapper.window_id = orion::WindowId(g_value_get_int64(value));
                GWeakRef* main_window_weak = g_new(GWeakRef, 1);
                g_weak_ref_init(main_window_weak, self);
                g_idle_add_full(G_PRIORITY_DEFAULT, [](gpointer data){
                    GWeakRef* main_window_weak = static_cast<GWeakRef*>(data);
                    MainWindow* self = MAIN_WINDOW(g_weak_ref_get(main_window_weak));
                    if(!self)
                    {
                        return G_SOURCE_REMOVE;
                    }
                    defer([self]{ g_object_unref(self); });
                    orion::SetSelectedWindowIdQuery query{};
                    query.profile_id = self->wrapper.profile_id;
                    query.window_id = self->wrapper.window_id;
                    orion::log_verbose("main_window_set_property: set_selected_window_id(%lld)", self->wrapper.window_id.value);
                    orion::gtk::WeakRefWrapper self_weak(self);
                    global_core_api->set_selected_window_id(query, [self_weak](orion::Error err){
                        VERIFY_LOG(err);
                        auto self_strong = self_weak.lock();
                        if (!self_strong) return;
                        MainWindow* self = MAIN_WINDOW(self_strong.get());
                        main_window_refresh_bookmark_icon(self);
                        main_window_sync_page_zoom_button(self);
                        main_window_sync_popup_blocked_button(self);
                    });
                    main_window_set_current_tab_view_by_window_id(self, self->wrapper.window_id, true);
                    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_WINDOW_ID]);
                    return G_SOURCE_REMOVE;
                }, main_window_weak, [](gpointer object){
                    g_weak_ref_clear(static_cast<GWeakRef*>(object));
                    g_free(object);
                });
            }
            break;
        case PROP_NEW_FROM_BACKEND_CB:
            if(g_value_get_pointer(value))
            {
                self->wrapper.done_cb.reset(static_cast<MainWindowNewFromBackendCb*>(g_value_get_pointer(value)));
            }
            break;
        case PROP_PRIVATE_MODE:
            {
                self->private_mode = static_cast<orion::PrivateMode>(g_value_get_int64(value));
                g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PRIVATE_MODE]);
            }
            break;
        case PROP_URI:
            {
                const char* c_str = g_value_get_string(value);
                OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
                orion::Uri uri = orion::Uri::parse_user_input(c_str?c_str:"");
                if(view) orion_webview_tab_view_load_uri(view, uri);
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
main_window_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    MainWindow* self = MAIN_WINDOW(object);

    switch (property_id)
    {
        case PROP_PROFILE_ID:
            g_value_set_int64(value, self->wrapper.profile_id.value);
            break;
        case PROP_WINDOW_ID:
            g_value_set_int64(value, self->wrapper.window_id.value);
            break;
        case PROP_PRIVATE_MODE:
            g_value_set_int64(value, static_cast<gint64>(self->private_mode));
            break;
        case PROP_TAB_VIEW:
            g_value_set_object(value, main_window_get_webview_tab_view(self));
            break;
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        case PROP_READER_TEMPLATE_SCRIPT:
            g_value_set_boxed(value, main_window_get_reader_template_script(self));
            break;
#endif
        case PROP_PROFILE_DATA_HOLDER:
            g_value_set_object(value, self->profile_data_holder);
            break;
        case PROP_WEB_VIEW_FACTORY:
            g_value_set_pointer(
                value, orion_app_get_web_view_factory(*self->wrapper.profile_world).lock().get());
            break;
        case PROP_URI:
            {
                OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
                if(view)
                {
                    orion::Uri uri = orion_webview_tab_view_get_visual_uri(view);
                    g_value_set_string(value, uri.uri().c_str());
                }
            }
            break;
        case PROP_TLS_STATUS:
            {
                OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
                if(view)
                {
                    g_object_get_property(G_OBJECT(view), "tls-status", value);
                }
                else
                {
                    g_value_set_int64(value, gint64(orion::TlsStatus::none));
                }
            }
            break;
        case PROP_BACK_LIST:
            {
                OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
                if(view)
                {
                    g_object_get_property(G_OBJECT(view), "back-list", value);
                }
                else
                {
                    g_value_set_boxed(value, NULL);
                }
            }
            break;
        case PROP_FORWARD_LIST:
            {
                OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
                if(view)
                {
                    g_object_get_property(G_OBJECT(view), "forward-list", value);
                }
                else
                {
                    g_value_set_boxed(value, NULL);
                }
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static MainWindow*
main_window_new_internal(
    GApplication* app, orion::ProfileId profile_id, orion::WindowId window_id,
    orion::PrivateMode private_mode, MainWindowNewFromBackendCb* done_cb = nullptr)
{
	MainWindow* win = nullptr;
    if(done_cb)
    {
        win = MAIN_WINDOW(g_object_new(MAIN_WINDOW_TYPE,
            "application", app,
            "profile-id", profile_id.value, // additional property to get this value to main_window_init()
            "done-cb", done_cb, //this takes ownership of the raw pointer
            "window-id", window_id.value,
            "private-mode", static_cast<gint64>(private_mode),
            NULL
        ));
    }
    else
    {
        win = MAIN_WINDOW(g_object_new(MAIN_WINDOW_TYPE,
            "application", app,
            "profile-id", profile_id.value, // additional property to get this value to main_window_init()
            "window-id", window_id.value,
            "private-mode", static_cast<gint64>(private_mode),
            NULL
        ));
    }

    orion::log_verbose("main_window_new_internal: creating window_id=%lld ui_window=%p",
        window_id.value, win
    );

    orion::UiWindowId ui_window_id = static_cast<orion::UiWindowId>(win);
    global_core_api->set_window_id_for_ui_window(ui_window_id, window_id);
    
    // we are creating a separate UI window, so its window_id is the owner of the UI window
    global_core_api->set_owner_for_ui_window(ui_window_id, window_id);

    orion::UpdateWindowQuery query{
        .id = window_id,
        .has_ui_window = true,
        .update_has_ui_window = true,
    };
    global_core_api->update_window(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
    
    return win;   
}

MainWindow*
main_window_new(GApplication* app, orion::ProfileId profile_id, orion::WindowId window_id,
    orion::PrivateMode private_mode)
{
	MainWindow* win = main_window_new_internal(app, profile_id, window_id, private_mode);
    return win;
}

// done_cb takes MainWindow* as parameter as a convenience for the caller
MainWindow*
main_window_new_from_backend(
    GApplication* app, orion::ProfileId profile_id, orion::WindowId window_id,
    orion::PrivateMode private_mode, MainWindowNewFromBackendCb done_cb)
{
    MainWindow* win = main_window_new_internal(
        app,
        profile_id,
        window_id,
        private_mode,
        done_cb ? new MainWindowNewFromBackendCb(std::move(done_cb)) : nullptr
    );
    return win;
}

orion::TabId
main_window_get_current_tab_id(MainWindow* win)
{
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    if (!page) {
        orion::log_warning("main_window_get_current_tab_id: no selected page");
        return orion::TabId(0);
    }
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    return orion_webview_tab_container_get_id(container);
}

static void
try_unsuspend_window(orion::WindowId window_id)
{
    global_core_api->get_windows({window_id}, [window_id](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_error("try_unsuspend_window: window not found");
            return;
        }
        const orion::WindowDto& window = reply.windows.at(0);
        if (window.is_suspended) {
            orion::UpdateWindowQuery query{};
            query.id = window_id;
            query.is_suspended = false;
            query.update_is_suspended = true;
            global_core_api->update_window(query, [window_id](orion::Error err){
                VERIFY_LOG(err);
            });
        }
    });
}

void
main_window_switch_to_window_id_in_place(MainWindow* win, orion::WindowId window_id)
{
    g_object_set(G_OBJECT(win), "window-id", window_id.value, NULL);

    // update selected window id in backend if main window is active
    if (win->is_active) {
        orion::SetSelectedWindowIdQuery query{
            .profile_id = main_window_get_current_profile_id(win),
            .window_id = window_id,
        };
        global_core_api->set_selected_window_id(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
    }

    // mark the window as not suspended if needed
    try_unsuspend_window(window_id);
}

void
main_window_set_main_menu_model(MainWindow* self, GMenuModel* model)
{
    self->main_menu_model = model;
}

GMenuModel*
main_window_get_main_menu_model(MainWindow* self)
{
    return self->main_menu_model;
}

void
main_window_set_recently_bookmarked_menu(MainWindow* self, GMenu* menu)
{
    self->recently_bookmarked_menu = menu;
}

GMenu*
main_window_get_recently_bookmarked_menu(MainWindow* self)
{
    return self->recently_bookmarked_menu;
}

void
main_window_set_recently_bookmarked_change_observer(MainWindow* self, orion::ObserverId id)
{
    self->wrapper.recently_bookmarked_change_observer = id;
}

orion::ObserverId
main_window_get_recently_bookmarked_change_observer(MainWindow* self)
{
    return self->wrapper.recently_bookmarked_change_observer;
}

void
main_window_set_all_folders_change_observer(MainWindow* self, orion::ObserverId id)
{
    self->wrapper.all_folders_change_observer = id;
}

orion::ObserverId
main_window_get_all_folders_change_observer(MainWindow* self)
{
    return self->wrapper.all_folders_change_observer;
}

void
main_window_set_all_bookmarked_change_observer(MainWindow* self, orion::ObserverId id)
{
    self->wrapper.all_bookmarked_change_observer = id;
}

orion::ObserverId
main_window_get_all_bookmarked_change_observer(MainWindow* self)
{
    return self->wrapper.all_bookmarked_change_observer;
}

void
main_window_set_all_bookmarks_section(MainWindow* self, GMenu* section)
{
    self->all_bookmarks_section = section;
}

GMenu*
main_window_get_all_bookmarks_section(MainWindow* self)
{
    return self->all_bookmarks_section;
}

HeaderBar*
main_window_get_header_bar(MainWindow* self)
{
    return self->header_bar;
}

EraseElementsBar*
main_window_get_erase_elements_bar(MainWindow* self)
{
    return self->erase_elements_bar;
}

ProfileDataHolder*
main_window_get_profile_data_holder(MainWindow* self)
{
    return self->profile_data_holder;
}


static void
setup_context_menu(GtkBuilder* builder, MainWindow* win, GtkPopoverMenu** popover_out, const char* object_name)
{
	GMenuModel* context_menu = G_MENU_MODEL(gtk_builder_get_object(builder, object_name));
    //Needs to be sliding to properly show custom widgets
	GtkWidget* popover = gtk_popover_menu_new_from_model_full(context_menu, GTK_POPOVER_MENU_SLIDING);
	gtk_widget_set_parent(popover, GTK_WIDGET(win));
	*popover_out = GTK_POPOVER_MENU(popover);
}

static GMenu*
create_address_bar_context_menu(MainWindow* win)
{
    GMenu* menu = g_menu_new();

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Copy"), "win.copy-page-url");
        orion::gtk::menu_append_item(section, _("Paste and Go"), "win.paste-and-go");
        orion::gtk::menu_append_item(
            section, _("Create QR Code for This Page"), "win.create-qr-code-for-this-page");
    }

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_item(section, _("Add Bookmark…"), "win.add-bookmark");
    }

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

        std::string website = main_window_get_current_website(win);
        std::string label = website.empty()
            ? _("Settings for This Website")
            : orion::text::translatef(_("Settings for %1$s"), website.c_str());
        orion::gtk::menu_append_item(section, label, "win.settings-for-current-page");
    }

    return menu;
}

static void
show_address_bar_context_menu(
    MainWindow* win, GtkGestureClick* click, gdouble x, gdouble y, GtkPopoverMenu** popover_out,
    GtkWidget* pointing_widget)
{
    GMenu* menu = create_address_bar_context_menu(win);
    defer([menu]{ g_object_unref(menu); });

    if (*popover_out) {
        gtk_widget_unparent(GTK_WIDGET(*popover_out));
    }

    GtkWidget* popover = gtk_popover_menu_new_from_model_full(
        G_MENU_MODEL(menu), GTK_POPOVER_MENU_SLIDING);
    gtk_widget_set_parent(popover, GTK_WIDGET(win));

    *popover_out = GTK_POPOVER_MENU(popover);

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper click_weak(click);
    orion::gtk::WeakRefWrapper pointing_widget_weak(pointing_widget);
    orion::gtk::WeakRefWrapper popover_weak(popover);

    // We must dispatch button_released_handler asynchronously to avoid measurement issues
    // resulting in force vertical scroll bar in the context menu
    global_core_api->async([win_weak,click_weak,pointing_widget_weak,popover_weak,x,y]{
        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        auto click_strong = click_weak.lock();
        if (!click_strong) return;
        GtkGestureClick* click = GTK_GESTURE_CLICK(click_strong.get());

        auto pointing_widget_strong = pointing_widget_weak.lock();
        if (!pointing_widget_strong) return;
        GtkWidget* pointing_widget = GTK_WIDGET(pointing_widget_strong.get());

        auto popover_strong = popover_weak.lock();
        if (!popover_strong) return;
        GtkPopoverMenu* popover = GTK_POPOVER_MENU(popover_strong.get());

        orion::gtk::button_released_handler(
            GTK_WIDGET(win), click, x, y, popover, pointing_widget);
    });
}

static void
on_sidebar_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
	orion::gtk::button_released_handler(
        parent, click, x, y, win->sidebar_button_popover, GTK_WIDGET(header_bar_get_sidebar_button(win->header_bar))
    );
}

static void
on_window_list_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
	orion::gtk::button_released_handler(
        parent, click, x, y, win->window_list_button_popover,
        GTK_WIDGET(header_bar_get_window_list_button(win->header_bar))
    );
}

void
on_profiles_button_icon_selection_box_map(ProfilesIconSelectionBox* self, gpointer user_data)
{
    profiles_icon_selection_bind_to_profile_id(self, main_window_get_current_profile_id(MAIN_WINDOW(user_data)));
}

static void
on_profiles_button_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    // recreate the menu (avoids issues with duplicate child names in GtkStack)
    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });

    gtk_popover_menu_set_menu_model(win->profiles_button_popover, G_MENU_MODEL(menu));

    orion::gtk::WeakRefWrapper click_weak(click);
    orion::gtk::WeakRefWrapper win_weak(win);

    recreate_profiles_menu(menu, profile_id, win, [x,y,click_weak,win_weak](){
        auto click_strong = click_weak.lock();
        if (!click_strong) return;
        GtkGestureClick* click = GTK_GESTURE_CLICK(click_strong.get());

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        GtkWidget* profiles_button = GTK_WIDGET(header_bar_get_profiles_button(win->header_bar));

        ProfilesIconSelectionBox* icon_selection = profiles_icon_selection_box_new();

        g_signal_connect(icon_selection, "map", G_CALLBACK(on_profiles_button_icon_selection_box_map), win);
        gtk_popover_menu_add_child(
            win->profiles_button_popover, GTK_WIDGET(icon_selection), "profiles_icon_selection"
        );

        // using orion::gtk::button_released_handler could cause positioning issues with the popover
	    orion::gtk::button_released_handler_manual_pos(click, win->profiles_button_popover, profiles_button);
    });
}

static void
on_address_focus_mode_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    show_address_bar_context_menu(
        win, click, x, y, &win->address_focus_mode_button_popover,
        GTK_WIDGET(header_bar_get_address_focus_mode_button(win->header_bar)));
}

static GMenu*
create_reader_mode_button_context_menu(MainWindow* win)
{
    GMenu* menu = g_menu_new();

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

        std::string website = main_window_get_current_website(win);
        std::string label = orion::text::translatef(
            _("Automatically Use Reader Mode on “%1$s”"), website.c_str());
        orion::gtk::menu_append_item(section, label, "win.toggle-reader");
    }

    {
        GMenu* section = g_menu_new();
        defer([section]{ g_object_unref(section); });
        g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
        orion::gtk::menu_append_action_with_int64_target(
            section, _("Reader Mode Websites Preferences…"), "win.orion-settings-on-page", 8);
    }

    return menu;
}

static void
on_reader_mode_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);

    GMenu* menu = create_reader_mode_button_context_menu(win);
    defer([menu]{ g_object_unref(menu); });

    if (win->reader_mode_button_popover) {
        gtk_widget_unparent(GTK_WIDGET(win->reader_mode_button_popover));
    }

    win->reader_mode_button_popover = GTK_POPOVER_MENU(
        gtk_popover_menu_new_from_model_full(G_MENU_MODEL(menu), GTK_POPOVER_MENU_SLIDING));
    gtk_widget_set_parent(GTK_WIDGET(win->reader_mode_button_popover), GTK_WIDGET(win));

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper click_weak(click);

    // We must dispatch button_released_handler asynchronously to avoid measurement issues
    // resulting in force vertical scroll bar in the context menu
    global_core_api->async([win_weak,click_weak,x,y]{
        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        auto click_strong = click_weak.lock();
        if (!click_strong) return;
        GtkGestureClick* click = GTK_GESTURE_CLICK(click_strong.get());
        GtkWidget* pointing_widget = GTK_WIDGET(header_bar_get_reader_mode_button(win->header_bar));

        orion::gtk::button_released_handler(
            GTK_WIDGET(win), click, x, y, win->reader_mode_button_popover, pointing_widget);
    });
}

static void
on_rss_feeds_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    show_address_bar_context_menu(
        win, click, x, y, &win->rss_feeds_button_popover,
        GTK_WIDGET(header_bar_get_rss_feeds_button(win->header_bar)));
}

static void
on_certificates_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    show_address_bar_context_menu(
        win, click, x, y, &win->certificates_button_popover,
        GTK_WIDGET(header_bar_get_certificates_button(win->header_bar)));
}

static void
on_address_entry_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    show_address_bar_context_menu(
        win, click, x, y, &win->address_entry_popover,
        GTK_WIDGET(header_bar_get_address_entry(win->header_bar)));
}

static void
on_reload_page_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
	orion::gtk::button_released_handler(
        parent, click, x, y, win->reload_page_button_popover,
        GTK_WIDGET(header_bar_get_reload_page_button(win->header_bar))
    );
}

static void
on_new_tab_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);

    GtkPopoverMenu* popover_menu = header_bar_get_new_tab_button_popover_menu(win->header_bar);

    if (!popover_menu) {
        orion::log_warning("on_new_tab_button_secondary_released: popover menu is NULL");
        return;
    }

    new_tab_button_context_menu(click, x, y, popover_menu);
}

static void
on_share_button_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
	gint id = gtk_gesture_single_get_button(GTK_GESTURE_SINGLE(click));

	orion::log_debug("on_share_button_released: id=%d", id);

	switch (id) {
		case GDK_BUTTON_PRIMARY:   // left-click
			orion::gtk::button_released_handler(
                parent, click, x, y, win->share_button_popover,
                GTK_WIDGET(header_bar_get_share_button(win->header_bar))
            );
			break;
	}
}

static void
on_toolbar_box_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    MainWindow* win = MAIN_WINDOW(parent);
    orion::gtk::button_released_handler(parent, click, x, y, win->toolbar_box_popover, GTK_WIDGET(win->header_bar));
}

static void
setup_context_menus(MainWindow* win)
{
	GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_window_context_menus.ui");
    defer([builder]{ g_object_unref(builder); });

	setup_context_menu(builder, win, &win->sidebar_button_popover, "show_sidebar_context_menu");
	setup_context_menu(builder, win, &win->window_list_button_popover, "window_list_button_context_menu");
	setup_context_menu(builder, win, &win->reload_page_button_popover, "reload_button_context_menu");
	setup_context_menu(builder, win, &win->share_button_popover, "share_button_menu"); // left-click
	setup_context_menu(builder, win, &win->page_content_box_popover, "page_context_menu");
    setup_context_menu(builder, win, &win->toolbar_box_popover, "toolbar_context_menu");

    //TODO: improve size allocation to do a proper size request upon opening
    GMenu* profiles_menu = g_menu_new();
    defer([profiles_menu]{ g_object_unref(profiles_menu); });

    GtkWidget* popover = gtk_popover_menu_new_from_model_full(G_MENU_MODEL(profiles_menu), GTK_POPOVER_MENU_SLIDING);
    win->profiles_button_popover = GTK_POPOVER_MENU(popover);

    // Do not use MainWindow as profiles button popover's parent - it would cause issues
    // with popover's size allocation. Do not use profiles button as popover's parent
    // directly because the button is styled and its styling would interfere with styling
    // of the popover. Use for example header bar's profiles button box instead.
    GtkWidget* parent = GTK_WIDGET(header_bar_get_profiles_button_box(win->header_bar));
    gtk_widget_set_parent(GTK_WIDGET(win->profiles_button_popover), parent);

    gtk_popover_set_cascade_popdown(GTK_POPOVER(win->profiles_button_popover), FALSE);

    win->profiles_list_menu = g_menu_new();
    g_menu_insert_section(G_MENU(gtk_popover_menu_get_menu_model(win->profiles_button_popover)),
        1, NULL, G_MENU_MODEL(win->profiles_list_menu));

    win->current_profile_menu = G_MENU(gtk_builder_get_object(builder, "current_profile_menu"));
    win->profile_windows_submenu = G_MENU(gtk_builder_get_object(builder, "profile_windows_submenu"));
}

void
main_window_close_all_windows(MainWindow* self)
{
    //get profile id
    orion::ProfileId profile_id = self->wrapper.profile_id;
    orion::profile::quit(profile_id);
}

void
main_window_close(MainWindow* self)
{
    gtk_window_close(GTK_WINDOW(self));
}

static gboolean
main_window_close_request(GtkWindow* self)
{
    return false;
}

static void
main_window_finalize(GObject* object)
{
    MainWindow* self = MAIN_WINDOW(object);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_clear_pointer(&self->reader_template_script, webkit_user_script_unref);
#endif

    orion::ProfileId profile_id = self->wrapper.profile_id;

    // TODO: These ref counts should all be 1 at this point, otherwise we have a memory leak
    if (self->all_bookmarks_section) {
        orion::log_verbose("main_window_finalize: all_bookmarks_section.ref_count=%u",
            G_OBJECT(self->all_bookmarks_section)->ref_count
        );
    }
    if (self->recently_bookmarked_menu) {
        orion::log_verbose("main_window_finalize: recently_bookmarked_menu.ref_count=%u",
            G_OBJECT(self->recently_bookmarked_menu)->ref_count
        );
    }

    global_core_api->profiles_change().clear_observer(&self->wrapper.profiles_change_observer);
    global_core_api->windows_change().clear_observer(&self->wrapper.windows_change_observer);
    global_core_api->bookmarks_change().clear_observer(&self->wrapper.bookmarks_change_observer);

    delete self->wrapper.tab_view_map;
    delete self->wrapper.programmable_button_shortcut_map;


    // Capture the window id before destroying the wrapper
    orion::WindowId window_id = self->wrapper.window_id;
    orion::PrivateMode private_mode = self->private_mode;

    orion::UiWindowId ui_window_id = static_cast<orion::UiWindowId>(self);

    // For unsaved tab groups, the current window_id is always the same as owner_window_id but for
    // saved tab groups, the owner can be a different backend window.
    orion::WindowId owner_window_id = global_core_api->get_owner_window_id_for_ui_window(ui_window_id);

    orion::settings::AppOpensWith app_opens_with = orion::profile::get_app_opens_with(profile_id);

    orion::log_verbose(
        "main_window_finalize: window_id=%lld owner_window_id=%lld private=%d app_opens_with=%d ptr=%p",
        window_id.value, owner_window_id.value, static_cast<int>(private_mode), static_cast<int>(app_opens_with), self
    );

    OrionApp* app = ORION_APP(g_application_get_default());
    orion::gtk::WeakRefWrapper app_weak(app);
    bool has_windows_for_profile = orion::profile::has_windows_for_profile(profile_id);

    orion::profile::should_preserve_windows_on_quit(profile_id);

    bool should_clear_session =
           app_opens_with == orion::settings::AppOpensWith::new_private_window
        || app_opens_with == orion::settings::AppOpensWith::new_window
        || app_opens_with == orion::settings::AppOpensWith::not_set;
    
    bool should_close_window = should_clear_session || (
           app_opens_with == orion::settings::AppOpensWith::all_non_private_windows_from_last_session
        && private_mode == orion::PrivateMode::yes
    );

    if (should_close_window) {
        orion::log_verbose("main_window_finalize: closing backend window %lld", owner_window_id.value);
        global_core_api->close_window(owner_window_id, [](orion::Error err){VERIFY_LOG(err);});
    }
    orion::profile::on_window_finalized(profile_id, window_id, has_windows_for_profile);
    global_core_api->unregister_ui_window(ui_window_id);

    int64_t main_window_count = global_core_api->unref_ui_window();
    if (main_window_count == 0) {
        orion::log_verbose("main_window_finalize(): no more main windows, quitting application");
        orion_app_quit();
    }

    std::destroy_at(&self->wrapper);
    G_OBJECT_CLASS(main_window_parent_class)->finalize(object);
}

static void
main_window_dispose(GObject* object)
{
    MainWindow* self = MAIN_WINDOW(object);
    
    gtk_widget_insert_action_group(GTK_WIDGET(self), "profile", NULL);

    g_clear_pointer(&self->sidebar_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->window_list_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->profiles_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->address_focus_mode_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->reader_mode_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->rss_feeds_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->certificates_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->address_entry_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->reload_page_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->share_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->page_content_box_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->toolbar_box_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->privacy_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->rss_feeds_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->page_tweaks_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->save_window_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->reader_mode_unavailable_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->downloads_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->window_list_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->test_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->tabbar_tab_popover, orion::gtk::widget_unparent_wrapper);

    g_clear_pointer(&self->reader_mode_enabled_watch, gtk_expression_watch_unwatch);

    if(self->profile_data_holder)
    {
        orion_app_unref_profile_data_holder(self->wrapper.profile_id);
        self->profile_data_holder = NULL;
    }
	
    g_clear_object(&self->profiles_list_menu);

    orion::log_verbose("tab_view_map: deleting tab view map with %zu entries", self->wrapper.tab_view_map->size());
    for (const auto& [_, tab_view] : *self->wrapper.tab_view_map) {
        g_object_unref(tab_view);
    }
    self->wrapper.tab_view_map->clear();
    self->wrapper.programmable_button_shortcut_map->clear();

    if (self->low_power_mode_timer) {
        g_source_remove(self->low_power_mode_timer);
        self->low_power_mode_timer = 0;
    }

    G_OBJECT_CLASS(main_window_parent_class)->dispose(G_OBJECT(self));
}

static std::string
get_profile_icon_label_markup(orion::settings::ProfilesButtonStyle style, const orion::ProfileDto& profile)
{
    switch (style) {
        case orion::settings::ProfilesButtonStyle::show_name_only:
            return "<span font='Sans 10'>" + profile.name + "</span>";
        case orion::settings::ProfilesButtonStyle::show_icon_and_name:
            return profile.icon_name + " <span font='Sans 10'>" + profile.name + "</span>";
        default:
            return profile.icon_name;
    }
}

static void
refresh_profile_icon_label(MainWindow* win)
{
    GtkButton* profiles_button = header_bar_get_profiles_button(win->header_bar);
    GtkLabel* profile_icon_label = GTK_LABEL(gtk_button_get_child(profiles_button));

    orion::gtk::WeakRefWrapper profile_icon_label_weak(profile_icon_label);
    global_core_api->get_profiles({win->wrapper.profile_id}, [profile_icon_label_weak](orion::GetProfilesReply reply){
        VERIFY_LOG(reply.err);

        if (reply.profiles.empty()) {
            orion::log_error("MainWindow refresh_profile_icon_label: profile id does not exist");
            return;
        }

        orion::ProfileDto profile = reply.profiles.at(0);

        global_core_api->get_setting(profile.id, orion::settings::appearance::profiles_button_style,
            [profile_icon_label_weak,profile](orion::GetSettingReply<orion::settings::ProfilesButtonStyle> reply){
                VERIFY_LOG(reply.err);
                orion::settings::ProfilesButtonStyle style = reply.value;
               
                auto profile_icon_label_strong = profile_icon_label_weak.lock();
                if (!profile_icon_label_strong) return;
                GtkLabel* profile_icon_label = GTK_LABEL(profile_icon_label_strong.get());

                std::string label = get_profile_icon_label_markup(style, profile);

                //TODO: possibly sanity check this
                gtk_label_set_markup(profile_icon_label, label.c_str());
            });
    });
}

// Respects the current borderless (fullscreen) state of the window
static gboolean
main_window_get_auto_show_sidebar_in_current_screen_mode(MainWindow* self)
{
    // New Kagi policy: always use the same setting for auto-show-sidebar regardless of fullscreen state
    return orion::gtk::get_boolean_action_state(G_ACTION_MAP(self), "auto-show-sidebar");
    
    // return main_window_get_is_borderless(self)
    //     ? orion::gtk::get_boolean_action_state(G_ACTION_MAP(self), "auto-show-sidebar-in-fullscreen")
    //     : orion::gtk::get_boolean_action_state(G_ACTION_MAP(self), "auto-show-sidebar");
}

// Does not persist the setting
static void
change_sidebar_visibility(MainWindow* self, gboolean is_visible)
{
    adw_overlay_split_view_set_show_sidebar(self->overlay_split_view, is_visible);

    gboolean auto_show_sidebar = main_window_get_auto_show_sidebar_in_current_screen_mode(self);

    // If Auto Show Sidebar is enabled, then never hide sidebar toggle button in the top header bar
    header_bar_set_sidebar_button_visible(self->header_bar, auto_show_sidebar || !is_visible);
}

// Persists the setting
void
main_window_set_show_sidebar_setting(MainWindow* self, gboolean show_sidebar)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);

    // Persist show_sidebar setting state
    global_core_api->set_setting(profile_id, orion::settings::appearance::show_sidebar, show_sidebar,
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

// Persists the setting
void
main_window_toggle_show_sidebar_setting(MainWindow* self)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);

    // Persist show_sidebar setting state
    global_core_api->toggle_setting(profile_id, orion::settings::appearance::show_sidebar,
        [profile_id](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            if (reply.new_value) {
                global_core_api->set_setting(profile_id, orion::settings::appearance::auto_show_sidebar, false,
                    [](orion::SetSettingReply<bool> reply){
                        VERIFY_LOG(reply.err);
                    }
                );
            }
        }
    );
}

static void
main_window_sync_sidebar(MainWindow* self)
{
    if (self->webkit_fullscreen) {
        // Fullscreen was triggered from WebKit, always hide sidebar
        change_sidebar_visibility(self, FALSE);
        return;
    }

    gboolean show_sidebar = orion::gtk::get_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar");
    gboolean auto_show_sidebar = main_window_get_auto_show_sidebar_in_current_screen_mode(self);

    adw_overlay_split_view_set_collapsed(self->overlay_split_view, auto_show_sidebar);
    
    change_sidebar_visibility(self, show_sidebar);

    // New policy: never hide Tag Groups button (Window List button) even if Sidebar is open
    // GtkButton* window_list_button = header_bar_get_window_list_button(self->header_bar);
    // // If Auto Show Sidebar is enabled, then never hide Window List Button in the top header bar
    // gtk_widget_set_visible(GTK_WIDGET(window_list_button), auto_show_sidebar || !show_sidebar);
}

static void
main_window_on_sidebar_toggled(gpointer user_data, gboolean active)
{
    MainWindow* self = MAIN_WINDOW(user_data);
    gboolean auto_show_sidebar = main_window_get_auto_show_sidebar_in_current_screen_mode(self);
    if (!auto_show_sidebar) {
        main_window_set_show_sidebar_setting(self, active);
    }
    change_sidebar_visibility(self, active);
}

static void
set_top_bar_visible(MainWindow* self, gboolean is_visible)
{
    orion::log_verbose("set_top_bar_visible: is_visible=%b", is_visible);
    gtk_revealer_set_reveal_child(self->main_ui_revealer, is_visible);

    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "always-show-toolbar-in-fullscreen");
    gboolean always_show_toolbar_in_fullscreen = g_variant_get_boolean(g_action_get_state(action));
    gboolean is_borderless = main_window_get_is_borderless(self);
    gboolean extend_content = is_borderless && !always_show_toolbar_in_fullscreen;

    adw_toolbar_view_set_extend_content_to_top_edge(self->main_toolbar_view, extend_content);

    // Do not do this (it would disrespect the user's system-wide setting):
    // g_object_set(gtk_settings_get_default(), "gtk-enable-animations", TRUE, NULL);

    gboolean animations_enabled{};
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &animations_enabled, NULL);

    if (!animations_enabled) {
        orion::log_verbose("set_top_bar_visible: cannot animate visibility change smoothly because system-wide"
            " 'gtk-enable-animations' setting is disabled"
        );
    }
}

static void
main_window_sync_borderless_state(MainWindow* self)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);
    orion::gtk::WeakRefWrapper self_weak(self);

    // We need to get persistent state of show_sidebar, not transient UI action state
    global_core_api->get_setting(profile_id, orion::settings::appearance::show_sidebar,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            bool show_sidebar = reply.value;

            // Sync UI action state with backend
            orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar", show_sidebar);

            gboolean always_show_toolbar_in_fullscreen
                = orion::gtk::get_boolean_action_state(G_ACTION_MAP(self), "always-show-toolbar-in-fullscreen");

            gboolean borderless = main_window_get_is_borderless(self);

            if (borderless) {
                orion::log_debug(
                    "main_window_sync_borderless_state: entering borderless mode: webkit_fullscreen=%b always_show_toolbar_in_fullscreen=%b",
                    self->webkit_fullscreen,
                    always_show_toolbar_in_fullscreen
                );
                set_top_bar_visible(self, !self->webkit_fullscreen && always_show_toolbar_in_fullscreen);
                main_window_sync_sidebar(self);
            } else {
                orion::log_debug(
                    "main_window_sync_borderless_state: leaving borderless mode: webkit_fullscreen=%b show_sidebar=%b",
                    self->webkit_fullscreen,
                    show_sidebar
                );
                if (self->webkit_fullscreen) {
                    main_window_set_is_webkit_fullscreen(self, FALSE);
                }
                set_top_bar_visible(self, TRUE);

                if (show_sidebar) {
                    change_sidebar_visibility(self, TRUE);
                }
                main_window_sync_sidebar(self);
            }
        }
    );
}

static void
main_window_on_borderless_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    MainWindow* self = MAIN_WINDOW(object);
    main_window_sync_borderless_state(self);
}

static void
main_window_setup_settings(MainWindow* self)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);
    orion::WindowId window_id = self->wrapper.window_id;
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
        [self_weak,profile_id](orion::GetSettingReply<orion::settings::TabLayout> reply){
            VERIFY_LOG(reply.err);
            orion::settings::TabLayout tab_layout = reply.value;

            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            if(tab_layout == orion::settings::TabLayout::vertical)
            {
                // Show sidebar in vertical tab layout but only if persisted show_sidebar state is true
                main_window_set_show_sidebar_setting(self, TRUE);
            }
            gtk_widget_set_visible(GTK_WIDGET(self->tab_bar), tab_layout == orion::settings::TabLayout::standard);

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "tab-layout-setting");
            std::string target;
            switch(tab_layout)
            {
                case orion::settings::TabLayout::standard:
                    target = "standard";
                    break;
                case orion::settings::TabLayout::compact:
                    target = "compact";
                    break;
                case orion::settings::TabLayout::vertical:
                    target = "vertical";
                    break;
                default:
                    target = "standard";
                    break;
            }
            g_action_change_state(action, g_variant_new_string(target.c_str()));
        }
    );

    self->wrapper.tab_layout_setting_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::tab_layout,
        [self_weak,profile_id](orion::SingleSettingChange<orion::settings::TabLayout> change){
            if(change.profile_id!=profile_id) return;
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            main_window_setup_main_menu_button(self);
            
            if(change.new_value == orion::settings::TabLayout::vertical)
            {
                gtk_widget_activate_action(GTK_WIDGET(self), "win.sidebar-type", "s", "tabs");
                main_window_set_show_sidebar_setting(self, TRUE);
            }
            else
            {
                if(strcmp(sidebar_get_visible_page(self->sidebar), "tabs")==0)
                {
                    main_window_set_show_sidebar_setting(self, FALSE);
                }
            }
            gtk_widget_set_visible(GTK_WIDGET(self->tab_bar), change.new_value == orion::settings::TabLayout::standard);
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "tab-layout-setting");
            std::string target;
            switch(change.new_value)
            {
                case orion::settings::TabLayout::standard:
                    target = "standard";
                    break;
                case orion::settings::TabLayout::compact:
                    target = "compact";
                    break;
                case orion::settings::TabLayout::vertical:
                    target = "vertical";
                    break;
                default:
                    target = "standard";
                    break;
            }
            g_action_change_state(action, g_variant_new_string(target.c_str()));
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::show_bookmarks_bar,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            gtk_widget_set_visible(GTK_WIDGET(self->bookmarks_bar), reply.value);
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "bookmarks-bar-visibility");
            g_action_change_state(action, g_variant_new_boolean(reply.value));
        }
    );

    self->wrapper.bookmarks_bar_visibility_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::show_bookmarks_bar,
        [self_weak,profile_id](orion::SingleSettingChange<bool> change){
            if(change.profile_id!=profile_id) return;
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            gtk_widget_set_visible(GTK_WIDGET(self->bookmarks_bar), change.new_value);
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "bookmarks-bar-visibility");
            g_action_change_state(action, g_variant_new_boolean(change.new_value));
        }
    );

    auto focus_mode_handler = [self_weak,profile_id](bool enabled){
        global_core_api->get_setting(profile_id, orion::settings::appearance::show_bookmarks_bar,
            [=](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                bool show_bookmarks_bar = reply.value;

                global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
                    [=](orion::GetSettingReply<orion::settings::TabLayout> reply){
                        VERIFY_LOG(reply.err);
                        bool use_standard_tabs = reply.value == orion::settings::TabLayout::standard;
                        bool use_compact_tabs = reply.value == orion::settings::TabLayout::compact;

                        global_core_api->get_setting(profile_id, orion::settings::appearance::show_sidebar,
                            [=](orion::GetSettingReply<bool> reply){
                                VERIFY_LOG(reply.err);
                                bool show_sidebar = reply.value;

                                auto self_strong = self_weak.lock();
                                if (!self_strong) return;
                                MainWindow* self = MAIN_WINDOW(self_strong.get());

                                GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toggle-focus-mode");
                                g_action_change_state(action, g_variant_new_boolean(enabled));
                                TabBar* compact_tab_bar = header_bar_get_compact_page_content_tabbar(self->header_bar);

                                if (enabled) {
                                    gtk_widget_add_css_class(GTK_WIDGET(self), "focus-mode-enabled");
                                    gtk_widget_set_visible(GTK_WIDGET(self->bookmarks_bar), FALSE);
                                    gtk_widget_set_visible(GTK_WIDGET(self->tab_bar), FALSE);
                                    gtk_widget_set_visible(GTK_WIDGET(compact_tab_bar), FALSE);

                                    change_sidebar_visibility(self, FALSE);
                                    orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar", FALSE);
                                    
                                    // This causes the overlay to be drawn over the web page content
                                    gtk_overlay_set_measure_overlay(self->top_bar_overlay,
                                        GTK_WIDGET(self->main_ui_revealer), FALSE
                                    );

                                    set_top_bar_visible(self, FALSE);
                                    self->ui_hidden = TRUE;
                                } else {
                                    gtk_widget_remove_css_class(GTK_WIDGET(self), "focus-mode-enabled");
                                    gtk_widget_set_visible(GTK_WIDGET(self->bookmarks_bar), show_bookmarks_bar);
                                    gtk_widget_set_visible(GTK_WIDGET(self->tab_bar), use_standard_tabs);
                                    gtk_widget_set_visible(GTK_WIDGET(compact_tab_bar), use_compact_tabs);

                                    if (show_sidebar) {
                                        orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar", TRUE);
                                        main_window_sync_sidebar(self);
                                    }

                                    // This causes the overlay to be drawn above the web page content
                                    gtk_overlay_set_measure_overlay(self->top_bar_overlay,
                                        GTK_WIDGET(self->main_ui_revealer), TRUE
                                    );
                                    
                                    set_top_bar_visible(self, TRUE);
                                    self->ui_hidden = FALSE;
                                }
                            }
                        );
                    }
                );
            }
        );
    };

    global_core_api->get_windows({window_id}, [focus_mode_handler](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) {
            orion::log_error("main_window_setup_settings: window id does not exist");
            return;
        }
        bool is_focus_mode = reply.windows.at(0).is_focus_mode;
        focus_mode_handler(is_focus_mode);
    });

    self->wrapper.focus_mode_observer = global_core_api->windows_change().add_observer(
        [focus_mode_handler,window_id](const orion::WindowsChange& change){
            for (const auto& updated_window : change.updated) {
                if (updated_window.id == window_id && change.updated_is_focus_mode) {
                    focus_mode_handler(updated_window.is_focus_mode);
                }
            }
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::bookmarks_bar_style,
        [self_weak](orion::GetSettingReply<orion::settings::BookmarksBarStyle> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "bookmarks-bar-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(reply.value))));
        }
    );

    self->wrapper.bookmarks_bar_style_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::bookmarks_bar_style,
        [self_weak, profile_id](orion::SingleSettingChange<orion::settings::BookmarksBarStyle> change){
            if(change.profile_id!=profile_id) return;
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "bookmarks-bar-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(change.new_value))));
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::toolbar_item_style,
        [self_weak](orion::GetSettingReply<orion::settings::ToolbarItemStyle> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toolbar-item-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(reply.value))));
        }
    );

    self->wrapper.toolbar_item_style_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::toolbar_item_style,
        [self_weak, profile_id](orion::SingleSettingChange<orion::settings::ToolbarItemStyle> change){
            if(change.profile_id!=profile_id) return;
            auto self_strong = self_weak.lock();
            if(!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toolbar-item-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(change.new_value))));
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::profiles_button_style,
        [self_weak](orion::GetSettingReply<orion::settings::ProfilesButtonStyle> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "profiles-button-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i", int32_t(reply.value))));
        }
    );

    self->wrapper.profiles_button_style_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::profiles_button_style,
        [self_weak,profile_id](orion::SingleSettingChange<orion::settings::ProfilesButtonStyle> change){
            if (change.profile_id != profile_id) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "profiles-button-style");
            g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i", int32_t(change.new_value))));

            refresh_profile_icon_label(self);
        }
    );
        
    global_core_api->get_setting(profile_id, orion::settings::tabs::show_tab_preview_on_hover,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "show-tab-preview-on-hover");
            g_action_change_state(action, g_variant_new_boolean(reply.value));
        }
    );

    self->wrapper.show_tab_preview_on_hover_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::show_tab_preview_on_hover,
        [self_weak,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "show-tab-preview-on-hover");
            g_action_change_state(action, g_variant_new_boolean(change.new_value));

            refresh_profile_icon_label(self);
        }
    );

    auto show_recently_opened_on_top_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "show-recently-opened-on-top");
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::windows::show_recently_opened_on_top,
        [show_recently_opened_on_top_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            show_recently_opened_on_top_handler(reply.value);
        }
    );

    self->wrapper.show_recently_opened_on_top_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::windows::show_recently_opened_on_top,
        [show_recently_opened_on_top_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            show_recently_opened_on_top_handler(change.new_value);
        }
    );

    auto switch_saved_windows_in_place_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "switch-saved-windows-in-place");
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::windows::switch_saved_windows_in_place,
        [switch_saved_windows_in_place_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            switch_saved_windows_in_place_handler(reply.value);
        }
    );

    self->wrapper.switch_saved_windows_in_place_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::windows::switch_saved_windows_in_place,
        [switch_saved_windows_in_place_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            switch_saved_windows_in_place_handler(change.new_value);
        }
    );

    auto show_tab_switcher_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "show-tab-switcher");
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::use_tab_switcher_ui_for_ctrl_tab,
        [show_tab_switcher_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            show_tab_switcher_handler(reply.value);
        }
    );

    self->wrapper.use_tab_switcher_ui_for_ctrl_tab_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::use_tab_switcher_ui_for_ctrl_tab,
        [show_tab_switcher_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            show_tab_switcher_handler(change.new_value);
        }
    );
        
    auto open_new_tab_next_to_current_tab_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        //orion::log_debug("%s", std::format("obtained open_new_tab_next_to_current_tab setting value: {}", new_value).c_str());
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "open-new-tab-next-to-current-tab");
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::open_new_tab_next_to_current_tab,
        [open_new_tab_next_to_current_tab_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            open_new_tab_next_to_current_tab_handler(reply.value);
        }
    );

    self->wrapper.open_new_tab_next_to_current_tab_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::open_new_tab_next_to_current_tab,
        [open_new_tab_next_to_current_tab_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            open_new_tab_next_to_current_tab_handler(change.new_value);
        }
    );

    auto tab_switch_order_handler = [self_weak](orion::settings::TabSwitchOrder reply){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "tab-switch-order");
        g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(reply))));
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_switch_order,
        [tab_switch_order_handler](orion::GetSettingReply<orion::settings::TabSwitchOrder> reply){
            VERIFY_LOG(reply.err);
            tab_switch_order_handler(reply.value);
        }
    );
        
    auto show_favicons_inside_tabs_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "show-favicons-inside-tabs");
        orion::log_verbose("show_favicons_inside_tabs: new_value=%d", (int)new_value); 
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::show_favicons_inside_tabs,
        [show_favicons_inside_tabs_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            show_favicons_inside_tabs_handler(reply.value);
        }
    );

    self->wrapper.show_favicons_inside_tabs_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::show_favicons_inside_tabs,
        [show_favicons_inside_tabs_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            show_favicons_inside_tabs_handler(change.new_value);
        }
    );

    auto always_show_website_titles_in_tabs_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "always-show-website-titles-in-tabs");
        orion::log_verbose("always_show_website_titles_in_tabs: new_value=%d", (int)new_value); 
        g_action_change_state(action, g_variant_new_boolean(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::always_show_website_titles_in_tabs,
        [always_show_website_titles_in_tabs_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            always_show_website_titles_in_tabs_handler(reply.value);
        }
    );

    self->wrapper.always_show_website_titles_in_tabs_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::always_show_website_titles_in_tabs,
        [always_show_website_titles_in_tabs_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            always_show_website_titles_in_tabs_handler(change.new_value);
        }
    );
    
    auto file_download_location_handler = [self_weak](orion::settings::FileDownloadLocation new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "file-download-location");
        orion::log_verbose("file_download_location: new_value=%d", (int)new_value); 
        g_action_change_state(action, g_variant_new_take_string(g_strdup_printf("%i",int32_t(new_value))));
    };

    global_core_api->get_setting(profile_id, orion::settings::general::file_download_location,
        [self_weak,file_download_location_handler](orion::GetSettingReply<orion::settings::FileDownloadLocation> reply){
            VERIFY_LOG(reply.err);
            file_download_location_handler(reply.value);
        }
    );

    self->wrapper.file_download_location_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::general::file_download_location,
        [file_download_location_handler,profile_id]
        (orion::SingleSettingChange<orion::settings::FileDownloadLocation> change){
            if (change.profile_id != profile_id) return;
            file_download_location_handler(change.new_value);
        }
    );

    auto file_download_custom_path_handler = [self_weak](std::string new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "file-download-custom-path");
        orion::log_verbose("file_download_custom_path: new_value=%s", new_value.c_str()); 
        g_action_change_state(action, g_variant_new_string(new_value.c_str()));
    };

    global_core_api->get_setting(profile_id, orion::settings::general::file_download_custom_path,
        [file_download_custom_path_handler](orion::GetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
            file_download_custom_path_handler(reply.value);
        }
    );

    self->wrapper.file_download_custom_path_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::general::file_download_custom_path,
        [file_download_custom_path_handler,profile_id](orion::SingleSettingChange<std::string> change){
            if (change.profile_id != profile_id) return;
            file_download_custom_path_handler(change.new_value);
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::show_sidebar,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar", reply.value);
            main_window_sync_sidebar(self);
        }
    );

    self->wrapper.show_sidebar_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::show_sidebar,
        [profile_id,self_weak](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "toggle-sidebar", change.new_value);
            main_window_sync_sidebar(self);
        }
    );

    auto auto_show_sidebar_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "auto-show-sidebar", new_value);
        
        main_window_sync_sidebar(self);
    };

    global_core_api->get_setting(profile_id, orion::settings::appearance::auto_show_sidebar,
        [auto_show_sidebar_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto_show_sidebar_handler(reply.value);
        }
    );

    self->wrapper.auto_show_sidebar_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::auto_show_sidebar,
        [auto_show_sidebar_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            auto_show_sidebar_handler(change.new_value);
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::auto_show_sidebar_in_fullscreen,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "auto-show-sidebar-in-fullscreen");
            g_action_change_state(action, g_variant_new_boolean(reply.value));
        }
    );

    self->wrapper.auto_show_sidebar_in_fullscreen_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::auto_show_sidebar_in_fullscreen,
        [self_weak,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "auto-show-sidebar-in-fullscreen");
            g_action_change_state(action, g_variant_new_boolean(change.new_value));
            
            // Never show top bar if in webkit fullscreen
            if (!self->webkit_fullscreen && main_window_get_is_borderless(self)) {
                change_sidebar_visibility(self, change.new_value);
            }
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::always_show_toolbar_in_fullscreen,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "always-show-toolbar-in-fullscreen");
            g_action_change_state(action, g_variant_new_boolean(reply.value));
        }
    );

    self->wrapper.always_show_toolbar_in_fullscreen_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::always_show_toolbar_in_fullscreen,
        [self_weak,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "always-show-toolbar-in-fullscreen");
            g_action_change_state(action, g_variant_new_boolean(change.new_value));
            
            // Never show top bar if in webkit fullscreen
            if (!self->webkit_fullscreen && main_window_get_is_borderless(self)) {
                set_top_bar_visible(self, change.new_value);
            }
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::appearance::show_status_bar_on_link_hover,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toggle-show-status-bar-on-link-hover");
            g_action_change_state(action, g_variant_new_boolean(reply.value));
        }
    );

    self->wrapper.show_status_bar_on_link_hover_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::show_status_bar_on_link_hover,
        [self_weak,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toggle-show-status-bar-on-link-hover");
            g_action_change_state(action, g_variant_new_boolean(change.new_value));
        }
    );

    auto window_list_button_style_handler = [self_weak](orion::settings::WindowListButtonStyle new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_string_action_state(G_ACTION_MAP(self), "window-list-button-style",
            std::to_string(orion::enums::to_underlying(new_value)).c_str());
    };

    global_core_api->get_setting(profile_id, orion::settings::appearance::window_list_button_style,
        [window_list_button_style_handler](orion::GetSettingReply<orion::settings::WindowListButtonStyle> reply){
            VERIFY_LOG(reply.err);
            window_list_button_style_handler(reply.value);
        }
    );

    self->wrapper.window_list_button_style_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::appearance::window_list_button_style,
        [window_list_button_style_handler,profile_id](orion::SingleSettingChange<orion::settings::WindowListButtonStyle> change){
            if (change.profile_id != profile_id) return;
            window_list_button_style_handler(change.new_value);
        }
    );

    auto when_opening_link_in_new_tab_make_it_active_handler = [self_weak](bool new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "when-opening-link-in-new-tab-make-it-active", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::tabs::when_opening_link_in_new_tab_make_it_active,
        [when_opening_link_in_new_tab_make_it_active_handler](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            when_opening_link_in_new_tab_make_it_active_handler(reply.value);
        }
    );

    self->wrapper.when_opening_link_in_new_tab_make_it_active_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::when_opening_link_in_new_tab_make_it_active,
        [when_opening_link_in_new_tab_make_it_active_handler,profile_id](orion::SingleSettingChange<bool> change){
            if (change.profile_id != profile_id) return;
            when_opening_link_in_new_tab_make_it_active_handler(change.new_value);
        }
    );
    
    auto reader_mode_font_size_index_handler = [self_weak](int64_t new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_int64_action_state(G_ACTION_MAP(self), "reader-mode-font-size-index", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::font_size_index,
        [reader_mode_font_size_index_handler](orion::GetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
            reader_mode_font_size_index_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_font_size_index_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::font_size_index,
        [reader_mode_font_size_index_handler,profile_id](orion::SingleSettingChange<int64_t> change){
            if (change.profile_id != profile_id) return;
            reader_mode_font_size_index_handler(change.new_value);
        }
    );
    
    auto reader_mode_background_type_handler = [self_weak](orion::settings::ReaderModeBackgroundType new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_enum_action_state<orion::settings::ReaderModeBackgroundType>(G_ACTION_MAP(self), "reader-mode-background-type", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::background_type,
        [reader_mode_background_type_handler](orion::GetSettingReply<orion::settings::ReaderModeBackgroundType> reply){
            VERIFY_LOG(reply.err);
            reader_mode_background_type_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_background_type_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::background_type,
        [reader_mode_background_type_handler,profile_id](orion::SingleSettingChange<orion::settings::ReaderModeBackgroundType> change){
            if (change.profile_id != profile_id) return;
            reader_mode_background_type_handler(change.new_value);
        }
    );
    
    auto reader_mode_font_name_handler = [self_weak](orion::settings::Font new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_int64_action_state(G_ACTION_MAP(self), "reader-mode-font-name", int64_t(new_value));
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::font_name,
        [reader_mode_font_name_handler](orion::GetSettingReply<orion::settings::Font> reply){
            VERIFY_LOG(reply.err);
            reader_mode_font_name_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_font_name_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::font_name,
        [reader_mode_font_name_handler,profile_id](orion::SingleSettingChange<orion::settings::Font> change){
            if (change.profile_id != profile_id) return;
            reader_mode_font_name_handler(change.new_value);
        }
    );
    
    auto reader_mode_document_width_handler = [self_weak](int64_t new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_int64_action_state(G_ACTION_MAP(self), "reader-mode-document-width", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::document_width,
        [reader_mode_document_width_handler](orion::GetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
            reader_mode_document_width_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_document_width_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::document_width,
        [reader_mode_document_width_handler,profile_id](orion::SingleSettingChange<int64_t> change){
            if (change.profile_id != profile_id) return;
            reader_mode_document_width_handler(change.new_value);
        }
    );
    
    auto reader_mode_text_alignment_handler = [self_weak](orion::settings::TextAlignment new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_enum_action_state<orion::settings::TextAlignment>(G_ACTION_MAP(self), "reader-mode-text-alignment", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::text_alignment,
        [reader_mode_text_alignment_handler](orion::GetSettingReply<orion::settings::TextAlignment> reply){
            VERIFY_LOG(reply.err);
            reader_mode_text_alignment_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_text_alignment_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::text_alignment,
        [reader_mode_text_alignment_handler,profile_id](orion::SingleSettingChange<orion::settings::TextAlignment> change){
            if (change.profile_id != profile_id) return;
            reader_mode_text_alignment_handler(change.new_value);
        }
    );
    
    auto reader_mode_border_shadow_enabled_handler = [self_weak](orion::settings::ReaderModeBorderShadowEnabled new_value){
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        
        orion::gtk::set_enum_action_state<orion::settings::ReaderModeBorderShadowEnabled>(G_ACTION_MAP(self), "reader-mode-border-shadow-enabled", new_value);
    };

    global_core_api->get_setting(profile_id, orion::settings::reader_mode::border_shadow_enabled,
        [reader_mode_border_shadow_enabled_handler](orion::GetSettingReply<orion::settings::ReaderModeBorderShadowEnabled> reply){
            VERIFY_LOG(reply.err);
            reader_mode_border_shadow_enabled_handler(reply.value);
        }
    );

    self->wrapper.reader_mode_border_shadow_enabled_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::reader_mode::border_shadow_enabled,
        [reader_mode_border_shadow_enabled_handler,profile_id](orion::SingleSettingChange<orion::settings::ReaderModeBorderShadowEnabled> change){
            if (change.profile_id != profile_id) return;
            reader_mode_border_shadow_enabled_handler(change.new_value);
        }
    );

    global_core_api->get_setting(profile_id, orion::settings::general::low_power_mode,
    [self_weak](orion::GetSettingReply<bool> reply){
        VERIFY_LOG(reply.err);
        
        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());

        orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "low-power-mode", reply.value);

        if (reply.value) {
            bool only_inactive_for_some_time = false;
            orion::gtk::main_window::suspend_non_active_non_pinned_tabs(self, only_inactive_for_some_time);
        }

        struct LowPowerModeTimerData {
            orion::gtk::WeakRefWrapper win_weak;
        };

        LowPowerModeTimerData* timer_data = new LowPowerModeTimerData{
            .win_weak = self_weak,
        };
        
        self->low_power_mode_timer = g_timeout_add_seconds_full(
            G_PRIORITY_DEFAULT,
            1,
            [](gpointer user_data) -> gboolean {
                auto* timer_data = static_cast<LowPowerModeTimerData*>(user_data);
                auto win_strong = timer_data->win_weak.lock();
                if (!win_strong) return G_SOURCE_REMOVE;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                if (!orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "low-power-mode")) {
                    // Low Power Mode is disabled
                    return G_SOURCE_CONTINUE;
                }
                
                orion::log_verbose("low_power_mode_timer: triggered");
                bool only_inactive_for_some_time = true;
                orion::gtk::main_window::suspend_non_active_non_pinned_tabs(win, only_inactive_for_some_time);
                return G_SOURCE_CONTINUE;
            },
            timer_data, // user_data
            [](gpointer user_data) {
                delete static_cast<LowPowerModeTimerData*>(user_data);
            }
        );
    });

    self->wrapper.low_power_mode_setting_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::general::low_power_mode,
    [profile_id,self_weak](orion::SingleSettingChange<bool> change){
        if (change.profile_id != profile_id) return;

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());

        orion::log_debug(
            "main_window_constructed: low_power_mode changed: %d->%d",
            change.old_value, change.new_value
        );
        orion::gtk::set_boolean_action_state(G_ACTION_MAP(self), "low-power-mode", change.new_value);

        if (change.new_value) {
            bool only_inactive_for_some_time = false;
            orion::gtk::main_window::suspend_non_active_non_pinned_tabs(self, only_inactive_for_some_time);
        }
    });
}

static void
main_window_unrealize(GtkWidget* widget, gpointer user_data)
{
    MainWindow* self = MAIN_WINDOW(widget);

    global_core_api->single_setting_change().clear_observer(&self->wrapper.tab_layout_setting_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.bookmarks_bar_visibility_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.bookmarks_bar_style_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.toolbar_item_style_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.window_list_button_style_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.profiles_button_style_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.show_tab_preview_on_hover_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.show_recently_opened_on_top_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.switch_saved_windows_in_place_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.use_tab_switcher_ui_for_ctrl_tab_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.tab_switch_order_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.show_favicons_inside_tabs_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.always_show_website_titles_in_tabs_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.open_new_tab_next_to_current_tab_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.file_download_location_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.file_download_custom_path_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.show_sidebar_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.auto_show_sidebar_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.auto_show_sidebar_in_fullscreen_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.always_show_toolbar_in_fullscreen_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.show_status_bar_on_link_hover_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.when_opening_link_in_new_tab_make_it_active_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_font_size_index_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_background_type_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_font_name_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_document_width_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_text_alignment_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.reader_mode_border_shadow_enabled_observer);
    global_core_api->single_setting_change().clear_observer(&self->wrapper.low_power_mode_setting_observer);
    if(self->wrapper.page_tweaks_observer != orion::ObserverId(0))
    {
        global_core_api->settings_change().clear_observer(&self->wrapper.page_tweaks_observer);
    }
    
    global_core_api->bookmarks_change().clear_observer(&self->wrapper.recently_bookmarked_change_observer);
    global_core_api->bookmarks_change().clear_observer(&self->wrapper.all_bookmarked_change_observer);
    global_core_api->bookmark_folders_change().clear_observer(&self->wrapper.all_folders_change_observer);
    global_core_api->recently_closed_tabs_change().clear_observer(&self->wrapper.recently_closed_tabs_observer);
    global_core_api->windows_change().clear_observer(&self->wrapper.focus_mode_observer);
    global_core_api->programmable_buttons_change().clear_observer(&self->wrapper.programmable_buttons_observer);
}

// Note: Be careful, realize() can be called multiple times during the lifetime of the same GtkWidget
static void
main_window_realize(GtkWidget* widget, gpointer user_data)
{
    MainWindow* self = MAIN_WINDOW(widget);
    main_window_setup_settings(self);
    main_window_setup_main_menu_button(self);
}

static void
on_show(GtkWidget* win, gpointer user_data)
{
}

static void
on_window_id_notify(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    // update the backend with the new window ID
    orion::WindowId window_id = win->wrapper.window_id;
    orion::UiWindowId ui_window_id = static_cast<orion::UiWindowId>(win);
    global_core_api->set_window_id_for_ui_window(ui_window_id, window_id);
    // leave the owner window ID as is
}

static void
on_profile_id_notify(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    orion::ProfileId old_id =
        main_window_get_current_webview_factory(win).profile_id();
    orion::ProfileId new_id = win->wrapper.profile_id;

    if (old_id != new_id) {
        // Legacy: This should never happen anymore - profile id of a main window can never change
        // during the lifetime of the window
        orion::log_warning(
            "main_window: on_profile_id_notify: suspicious profile id change %lld->%lld in main window %lld",
            old_id, new_id, win->wrapper.window_id);
        
        return;
    }

    refresh_profile_icon_label(win);

    if(gtk_widget_get_visible(GTK_WIDGET(win->bookmarks_bar)))
    {
        gtk_widget_set_visible(GTK_WIDGET(win->bookmarks_bar), false);
        gtk_widget_set_visible(GTK_WIDGET(win->bookmarks_bar), true);
    }
}

static void
main_window_force_close_all_dialogs(MainWindow* self)
{
    GListModel* dialogs = adw_application_window_get_dialogs(ADW_APPLICATION_WINDOW(self));
    defer([dialogs](){ g_object_unref(dialogs); });

    guint n = g_list_model_get_n_items(dialogs);

    for (guint i = 0; i < n; i++) {
        AdwDialog* dialog = ADW_DIALOG(g_list_model_get_item(dialogs, i));
        defer([dialog](){ g_object_unref(dialog); });
        
        adw_dialog_force_close(dialog);
    }
}

static void
main_window_on_profiles_change(MainWindow* self, const orion::ProfilesChange& change)
{
    orion::ProfileId current_profile_id = main_window_get_current_profile_id(self);

    auto search_result_deleted = std::find(change.deleted.cbegin(), change.deleted.cend(), current_profile_id);
    if(search_result_deleted != change.deleted.cend())
    {
        // Close Bookmarks dialog and Settings dialog if they are open and belong to the deleted profile,
        // otherwise the main window would not be closed
        main_window_force_close_all_dialogs(self);

        gtk_window_close(GTK_WINDOW(self));
        return;
    }
    
    
    auto search_result_updated = std::find_if(
        change.updated.cbegin(),
        change.updated.cend(),
        [current_profile_id](orion::ProfileDto a){return a.id == current_profile_id;}
    );
    if(search_result_updated == change.updated.cend()) {
        return;
    }
    
    orion::ProfileDto updated_profile = *search_result_updated;

    GtkButton* profiles_button = header_bar_get_profiles_button(self->header_bar);

    orion::gtk::WeakRefWrapper label_weak(gtk_button_get_child(profiles_button));

    global_core_api->get_setting(
        current_profile_id,
        orion::settings::appearance::profiles_button_style,
        [label_weak,updated_profile](orion::GetSettingReply<orion::settings::ProfilesButtonStyle> reply){
            VERIFY_LOG(reply.err);

            auto label_strong = label_weak.lock();
            if(!label_strong) return;
            GtkLabel* label = GTK_LABEL(label_strong.get());
            
            orion::settings::ProfilesButtonStyle style = reply.value;
            
            std::string text = get_profile_icon_label_markup(style, updated_profile);
            gtk_label_set_markup(label, text.c_str());
        }
    );
}

void
main_window_refresh_bookmark_icon(MainWindow* self)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::IsCurrentTabBookmarkedQuery query{
        .window_id = main_window_get_window_id(self),
    };

    global_core_api->is_current_tab_bookmarked(query, [self_weak](orion::IsCurrentTabBookmarkedReply reply){
        VERIFY_LOG(reply.err);
        orion::log_verbose("main_window_refresh_bookmark_icon: is_bookmarked=%d current_tab_id=%lld uri=%s",
            reply.is_bookmarked, reply.current_tab_id, reply.address.c_str()
        );

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());

        GtkButton* button = header_bar_get_add_page_to_button(self->header_bar);
        gtk_button_set_icon_name(button, reply.is_bookmarked ? "star-large-symbolic" : "star-outline-thin-symbolic");
    });
}

static void
on_bookmarks_change(MainWindow* self, const orion::BookmarksChange& change)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);

    bool should_refresh = !change.deleted.empty();

    for (const auto& bookmark : change.created) {
        if (bookmark.profile_id == profile_id) {
            // The bookmarked state of the current tab might have changed
            should_refresh = true;
            break;
        }
    }

    for (const auto& bookmark_dto : change.updated) {
        if (bookmark_dto.profile_id == profile_id) {
            // The bookmarked state of the current tab might have changed
            should_refresh = true;
            break;
        }
    }

    if (should_refresh) {
        main_window_refresh_bookmark_icon(self);
    }
}

static void
on_windows_change(MainWindow* self, const orion::WindowsChange& change)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);

    for (const auto& window_dto : change.updated) {
        if (profile_id == window_dto.profile_id && window_dto.selected_tab_id != window_dto.prev_selected_tab_id) {
            // The selected tab in this window has changed
            orion::TabId tab_id = window_dto.selected_tab_id;
            orion::log_verbose("on_windows_change: selected_tab_id changed: %lld -> %lld",
                window_dto.prev_selected_tab_id, tab_id
            );
            main_window_refresh_bookmark_icon(self);
            main_window_sync_page_zoom_button(self);
            main_window_sync_popup_blocked_button(self);
            main_window_on_current_website_change(self);
        }

        if (change.updated_is_suspended && window_dto.is_suspended) {
            auto it = self->wrapper.tab_view_map->find(window_dto.id);
            if (it == self->wrapper.tab_view_map->end()) continue;

            OrionWebviewTabView* tab_view = it->second;
            g_object_unref(tab_view);
            self->wrapper.tab_view_map->erase(it);

            orion::log_verbose("tab_view_map: tab view %p with window_id %lld deleted (total %zu tab views in map)",
                tab_view, window_dto.id, self->wrapper.tab_view_map->size()
            );
        }
    }
    for (const auto& deleted : change.deleted) {
        if(self->wrapper.window_id == deleted.id)
        {
            gtk_window_close(GTK_WINDOW(self));
        }
    }
}

static void
show_test_popover(gpointer user_data)
{
    auto win = MAIN_WINDOW(user_data);
    gtk_widget_set_parent(GTK_WIDGET(win->test_popover), GTK_WIDGET(win));

    graphene_point_t zero{};
	graphene_point_t pos{};
    GtkWidget* list_button = GTK_WIDGET(header_bar_get_window_list_button(win->header_bar));
	if (!gtk_widget_compute_point(list_button, GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(list_button) / 2),
		.y = int(pos.y + gtk_widget_get_height(list_button)),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(win->test_popover, &rect);
	gtk_popover_set_has_arrow(win->test_popover, TRUE);

    gtk_popover_popup(win->test_popover);
}

void
main_window_popup_settings_for_website(MainWindow* self)
{
    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
    if (!content) return;

    orion::Uri current_uri = orion_webview_tab_content_get_uri(content);
    std::string website = current_uri.authority();
    AdwDialog* dialog = settings_for_website_popover_new(website);
    adw_dialog_present(dialog, GTK_WIDGET(self));
}

orion::ProfileId
main_window_get_current_profile_id(MainWindow* win)
{
    return win->wrapper.profile_id;
}

WebExtensionsRuntime&
main_window_get_current_extensions_runtime(MainWindow* win)
{
    return *win->wrapper.profile_world->runtime();
}

WebExtensionWebViewFactory&
main_window_get_current_webview_factory(MainWindow* win)
{
    return *orion_app_get_web_view_factory(*win->wrapper.profile_world).lock();
}

orion::WindowId
main_window_get_window_id(MainWindow* win)
{
    return win->wrapper.window_id;
}

GtkPopover*
main_window_get_reader_unavailable_popover(MainWindow* win)
{
    if (win->reader_mode_unavailable_popover == NULL) {
        GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/reader_mode_unavailable_popover.ui");
        win->reader_mode_unavailable_popover = GTK_POPOVER(gtk_builder_get_object(builder, "reader_mode_unavailable_popover"));
        gtk_widget_set_parent(GTK_WIDGET(win->reader_mode_unavailable_popover), GTK_WIDGET(win));
        g_object_unref(builder);
    }
    return win->reader_mode_unavailable_popover;
}

static void
on_privacy_button_clicked(GtkButton* button, gpointer user_data)
{
	MainWindow* win = MAIN_WINDOW(user_data);
	if (win->privacy_popover == NULL) {
		win->privacy_popover = privacy_popover_new();
		gtk_widget_set_parent(GTK_WIDGET(win->privacy_popover), GTK_WIDGET(win));
	}
	privacy_popover_refresh(PRIVACY_POPOVER(win->privacy_popover), win);
	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("on_privacy_button_clicked: failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(win->privacy_popover, &rect);
	gtk_popover_set_has_arrow(win->privacy_popover, TRUE);
	gtk_popover_popup(win->privacy_popover);
}

void
main_window_show_privacy_popover(MainWindow* self)
{
    on_privacy_button_clicked(header_bar_get_privacy_button(self->header_bar), self);
}

static void
on_rss_feeds_button_clicked(GtkButton* button, gpointer user_data)
{
	MainWindow* win = MAIN_WINDOW(user_data);
	if (win->rss_feeds_popover == NULL) {
		win->rss_feeds_popover = rss_feeds_popover_new();
		gtk_widget_set_parent(GTK_WIDGET(win->rss_feeds_popover), GTK_WIDGET(win));
	}
	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("on_rss_feeds_button_clicked: failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(win->rss_feeds_popover, &rect);
	gtk_popover_set_has_arrow(win->rss_feeds_popover, TRUE);
	gtk_popover_popup(win->rss_feeds_popover);
}

static void
on_page_tweaks_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = page_tweaks_popover_new();
    adw_dialog_present(dialog, GTK_WIDGET(user_data));
}

SaveWindowPopover*
main_window_on_save_window(MainWindow* win)
{
	GtkButton* button = header_bar_get_window_list_button(win->header_bar);
	if (win->save_window_popover == NULL) {
		win->save_window_popover = save_window_popover_new();
		gtk_widget_set_parent(GTK_WIDGET(win->save_window_popover), GTK_WIDGET(win));
	}
	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("main_window_on_save_window: failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(win->save_window_popover, &rect);
	gtk_popover_set_has_arrow(win->save_window_popover, TRUE);
	gtk_popover_popup(win->save_window_popover);
    return SAVE_WINDOW_POPOVER(win->save_window_popover);
}

static void
on_add_page_to_button_clicked(GtkButton* button, gpointer user_data)
{
	adw_dialog_present(add_bookmark_dialog_new_init_from_current_page(MAIN_WINDOW(user_data)), GTK_WIDGET(user_data));
}

static void
on_history_button_clicked(GtkButton* button, MainWindow* win)
{
	adw_dialog_present(bookmarks_dialog_new(), GTK_WIDGET(win));
}

void
main_window_toggle_reader(MainWindow* self)
{
    //on_reader_mode_button_clicked(header_bar_get_reader_mode_button(self->header_bar), self);
    orion_webview_tab_view_toggle_reader_mode(main_window_get_webview_tab_view(self));
}

void
main_window_exit_reader_mode(MainWindow* self)
{
    orion_webview_tab_view_exit_reader_mode(main_window_get_webview_tab_view(self));
}

std::string
main_window_get_current_website(MainWindow* self)
{
    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
    if (!content) return std::string();

    orion::Uri current_uri = orion_webview_tab_content_get_uri(content);
    return current_uri.authority();
}

static void
on_window_list_button_clicked(GtkButton* button, MainWindow* win)
{
    AdwDialog* dialog = window_list_popover_new();
    adw_dialog_present(dialog, GTK_WIDGET(win));
}

void
main_window_on_edit_programmable_button(MainWindow* win, orion::ProgrammableButtonId id)
{
    GtkEntry* address_entry = header_bar_get_address_entry(win->header_bar);

    GtkPopover* programmable_button_popover = programmable_button_popover_new(id);
    gtk_widget_set_parent(GTK_WIDGET(programmable_button_popover), GTK_WIDGET(win));

	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(address_entry), GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("on_edit_programmable_button: failed to compute point");
	}

	// Point the popover arrow to the widget's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(address_entry)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(address_entry))),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(programmable_button_popover, &rect);
	gtk_popover_set_has_arrow(programmable_button_popover, TRUE);
    g_signal_connect(programmable_button_popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
	gtk_popover_popup(programmable_button_popover);
    win->programmable_button_popover = programmable_button_popover;
}

void
main_window_activate_programmable_button_action(MainWindow* win, orion::ProgrammableButtonId id)
{
    if(win->wrapper.programmable_button_shortcut_map->contains(id))
    {
        auto& shortcut_strong = win->wrapper.programmable_button_shortcut_map->at(id);
        GtkShortcut* shortcut = GTK_SHORTCUT(shortcut_strong.get());
        GtkShortcutAction* action = gtk_shortcut_get_action(shortcut);
        GVariant* args = gtk_shortcut_get_arguments(shortcut);
        gtk_shortcut_action_activate(action, GtkShortcutActionFlags(0), GTK_WIDGET(win), args);
    }
}

void
main_window_show_certificates_dialog(MainWindow* self)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    GTlsCertificate* certificate = NULL;
    GTlsCertificateFlags tls_errors = static_cast<GTlsCertificateFlags>(0);
    const char* uri = NULL;

    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
    if (content) {
        WebKitWebView* webview = orion_webview_tab_content_get_webview(content);
        if (webview) {
            gboolean has_tls_info = webkit_web_view_get_tls_info(webview, &certificate, &tls_errors);
            uri = webkit_web_view_get_uri(webview);
            if (!has_tls_info) {
                certificate = orion_webkit_webview_get_last_tls_certificate(ORION_WEBKIT_WEBVIEW(webview));
                tls_errors = orion_webkit_webview_get_last_tls_errors(ORION_WEBKIT_WEBVIEW(webview));
                uri = orion_webkit_webview_get_last_tls_uri(ORION_WEBKIT_WEBVIEW(webview));
            }
        }
    }

    adw_dialog_present(certificate_info_dialog_new(certificate, tls_errors, uri), GTK_WIDGET(self));
#else
    adw_dialog_present(certificate_info_dialog_new(), GTK_WIDGET(self));
#endif
}

void
main_window_show_popup_blocked_message(MainWindow* self)
{
    header_bar_show_popup_blocked_message(self->header_bar);
}

static void
on_certificates_button_clicked(GtkButton* button, MainWindow* win)
{
    main_window_show_certificates_dialog(win);
}

// Status Bar
void
main_window_show_link_overlay(MainWindow* window, const char* url)
{
    if (!orion::gtk::get_boolean_action_state(G_ACTION_MAP(window), "toggle-show-status-bar-on-link-hover")) {
        return;
    }

    gtk_label_set_text(window->link_label, url);
    if (!window->is_displaying_link_overlay)
    {
        gtk_revealer_set_transition_duration(window->link_overlay_revealer, 0);//How long to show
        gtk_revealer_set_reveal_child(window->link_overlay_revealer, TRUE);
        window->is_displaying_link_overlay = TRUE;
    }
}

void
main_window_hide_link_overlay(MainWindow* window)
{
    if (window->is_displaying_link_overlay)
    {
        gtk_revealer_set_transition_duration(window->link_overlay_revealer, 500);//How long to hide
        gtk_revealer_set_reveal_child(window->link_overlay_revealer, FALSE);

        window->is_displaying_link_overlay = FALSE;
    }
}

// Find in Page
void
main_window_show_find_bar(MainWindow* self)
{
    gtk_widget_set_visible(GTK_WIDGET(self->find_bar), TRUE);
    find_bar_focus_entry(self->find_bar);
}

void
main_window_show_find_bar_with_text(MainWindow* self, const std::string& text)
{
    gtk_widget_set_visible(GTK_WIDGET(self->find_bar), TRUE);
    find_bar_set_text_and_search(self->find_bar, text);
    find_bar_focus_entry(self->find_bar);
}

void
main_window_hide_find_bar(MainWindow* self)
{
    gtk_widget_set_visible(GTK_WIDGET(self->find_bar), FALSE);

    OrionWebviewTabContent* content = main_window_get_current_tab_content(self);
    if (content) {
        orion_webview_tab_content_find_finish(content);
    }
}

gboolean
main_window_get_find_bar_visible(MainWindow* self)
{
    return gtk_widget_get_visible(GTK_WIDGET(self->find_bar));
}

void main_window_refresh_find_search(MainWindow* self)
{
    find_bar_refresh_search(self->find_bar);
}

// Call this when JavaScript requested fullscreen (eg., YouTube video)
void
main_window_set_is_webkit_fullscreen(MainWindow* window, gboolean is_fullscreened)
{
    window->webkit_fullscreen = is_fullscreened;
    main_window_set_is_borderless(window, is_fullscreened, TRUE);
}

void
main_window_set_is_borderless(MainWindow* self, gboolean is_borderless, gboolean from_webkit)
{
    if (!from_webkit) {
        self->window_borderless = is_borderless;
    }
    gboolean current_state = main_window_get_is_borderless(self);
    orion::log_debug(
        "main_window_set_is_borderless: window_borderless=%d is_borderless=%d from_webkit=%d current_state=%d",
        self->window_borderless, is_borderless, from_webkit, current_state
    );
    if (is_borderless) {
        if (current_state) {
            // Already in borderless mode, sync webkit fullscreen mode (that possibly changed)
            main_window_sync_borderless_state(self);
        } else {
            gtk_window_fullscreen(GTK_WINDOW(self));
        }
    } else {
        if (self->window_borderless) {
            main_window_sync_borderless_state(self);
        } else {
            gtk_window_unfullscreen(GTK_WINDOW(self));
        }
    }
}

void
main_window_load_bookmark(MainWindow* win, std::string bookmark_address)
{
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(view);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    global_core_api->get_setting(profile_id, orion::settings::browsing::open_bookmarks_in_new_tabs,
        [wrapped_ref, bookmark_address](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(wrapped_ref->get());
            if(view)
            {
                if(reply.value) orion_webview_tab_view_new_tab_with_uri(view, bookmark_address.c_str());
                else orion_webview_tab_view_load_uri(view, orion::Uri::parse_strict(bookmark_address.c_str()));
                g_object_unref(view);
            }
        });
}

OrionWebviewTabView*
main_window_get_webview_tab_view(MainWindow* win)
{
    return win->page_content_tabview;
}

Sidebar*
main_window_get_sidebar(MainWindow* win)
{
    return win->sidebar;
}

AdwOverlaySplitView*
main_window_get_js_sidebar_split_view(MainWindow* win)
{
    return win->js_sidebar_split_view;
}

TabBar*
main_window_get_tab_bar(MainWindow* win)
{
    return win->tab_bar;
}

GtkPopover*
main_window_get_window_list_popover(MainWindow* win)
{
	return win->window_list_popover;
}

GtkWidget*
main_window_get_last_right_clicked_widget(MainWindow* win)
{
    return MAIN_WINDOW(win)->last_right_clicked_widget;
}

void
main_window_set_last_right_clicked_widget(MainWindow* win, GtkWidget* widget)
{
    MAIN_WINDOW(win)->last_right_clicked_widget = widget;
}

ContextMenuData*
main_window_get_context_menu_data(MainWindow* win)
{
    return &MAIN_WINDOW(win)->wrapper.context_menu_data;
}

double
main_window_get_mouse_x(MainWindow* win)
{
    return MAIN_WINDOW(win)->mouse_x;
}

double
main_window_get_mouse_y(MainWindow* win)
{
    return MAIN_WINDOW(win)->mouse_y;
}

void
main_window_set_context_menu_data(MainWindow* win, ContextMenuData data)
{
    MAIN_WINDOW(win)->wrapper.context_menu_data = std::move(data);
}

void
main_window_set_test_popover(MainWindow* win, GtkPopover* popover)
{
    win->test_popover = popover;
    
    if (win->test_popover != NULL) {
        // We have to popup the test popover after some time the main window is shown.
        // Otherwise, it will not be shown at all. If this does not seem to work, try
        // increasing the delay (timeout) value. Note that this kind of 'once' timer
        // is cleaned automatically.
        g_timeout_add_once(1000, show_test_popover, win);
    }
}

static void
on_sidebar_button_clicked(gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    main_window_set_show_sidebar_setting(win, FALSE);
}

static AdwTabPage*
on_create_tab(AdwTabOverview* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);

    orion_webview_tab_view_new_tab(win->page_content_tabview);

    // Note that this callback must not return void but has to return AdwTabPage* (as per documentation) and it must
    // return a valid Adwaita page (we must not return NULL). Otherwise, Adwaita critical error is triggered and
    // a crash in flatpak app may happen. But we create our tabs asynchronously so we cannot return the actual
    // new tab page here synchronously. As a workaround, we return the currently selected page, which should always
    // be valid. It seems to work fine with our tabs change observers.

    return orion_webview_tab_view_get_selected_page(win->page_content_tabview);
}

static void
on_main_window_drop_controller_motion_enter(GtkDropControllerMotion* self, gdouble x, gdouble y, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    if(win->dnd_hover_timeout_handler) return;
    if(gtk_window_is_active(GTK_WINDOW(win))) return;

    //main_window_focus(win, [](){});
    GWeakRef* ref = g_new(GWeakRef, 1);
    g_weak_ref_init(ref, win);
    
    win->dnd_hover_timeout_handler = g_timeout_add_full(G_PRIORITY_DEFAULT, 1500, [](gpointer user_data){
        GWeakRef* ref = (GWeakRef*)user_data;
        MainWindow* win = MAIN_WINDOW(g_weak_ref_get(ref));
        if(win)
        {
            defer([win]{g_object_unref(win);});
            //sadly does not actually bring the window to the front on ubuntu 25.04
            gtk_window_present(GTK_WINDOW(win));
        }
        return FALSE;
    }, ref, [](gpointer data){
        GWeakRef* ref = (GWeakRef*)data;
        MainWindow* win = MAIN_WINDOW(g_weak_ref_get(ref));
        if(win)
        {
            defer([win]{g_object_unref(win);});
            win->dnd_hover_timeout_handler = 0;
        }
        g_weak_ref_clear(ref);
        g_free(data);
    });
}

static void
on_main_window_drop_controller_motion_leave(AdwTabOverview* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    if(win->dnd_hover_timeout_handler)
    {
        g_source_remove(win->dnd_hover_timeout_handler);
    }
}


static gboolean
on_main_window_drop_target_accept(GtkDropTarget* target, GdkDrop* drop, gpointer user_data)
{
    GdkContentFormats* formats = gdk_drop_get_formats(drop);
    return gdk_content_formats_contain_gtype(formats, ADW_TYPE_TAB_PAGE);
}

static gboolean
on_main_window_drop_target_drop(GtkDropTarget* target, const GValue* value, gdouble x, gdouble y, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    AdwTabPage* page = ADW_TAB_PAGE(g_value_get_object(value));
    GtkWidget* child = adw_tab_page_get_child(page);
    OrionWebviewTabView* original_view = ORION_WEBVIEW_TAB_VIEW(gtk_widget_get_ancestor(child, ORION_WEBVIEW_TAB_VIEW_TYPE));
    AdwTabView* original_tabview = orion_webview_tab_view_get_actual_view(original_view);
    if(original_view != win->page_content_tabview)
    {
        AdwTabView* this_view = orion_webview_tab_view_get_actual_view(win->page_content_tabview);
        int pos = adw_tab_view_get_n_pages(this_view);

        GAction* setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
        GVariant* state = g_action_get_state(setting);
        defer([state]{ g_variant_unref(state); });
        orion_webview_tab_view_transfer_page(
            win->page_content_tabview, page, pos, original_tabview, g_variant_get_boolean(state), false
        );
    }

    return TRUE;
}

static void
on_mouse_motion(MainWindow* win, gdouble x, gdouble y, gpointer user_data)
{
    win->mouse_x = x;
    win->mouse_y = y;
}

static gboolean
on_main_window_key_pressed(TabSwitcher* tab_switcher, guint keyval, guint keycode, GdkModifierType state,
    GtkEventControllerKey* self)
{
    if(keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
    {
        tab_switcher_increment_ctrl_count(tab_switcher, keyval == GDK_KEY_Control_L);
    }
    return FALSE;
}

static gboolean
on_main_window_key_released(TabSwitcher* tab_switcher, guint keyval, guint keycode, GdkModifierType state,
    GtkEventControllerKey* self)
{
    if(keyval == GDK_KEY_Control_L || keyval == GDK_KEY_Control_R)
    {
        tab_switcher_decrement_ctrl_count(tab_switcher, keyval == GDK_KEY_Control_L);
    }
    return FALSE;
}

static void
on_main_window_notify_is_active(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    if(!gtk_window_is_active(GTK_WINDOW(win)))
    {
        tab_switcher_reset_ctrl_count(win->tab_switcher);
    }
}

static void
on_main_window_primary_pressed(MainWindow* win, gint n_press, gdouble x, gdouble y)
{
    g_signal_emit(win, signals[SIG_PRIMARY_PRESSED], 0, x, y);
    //main_window_popdown_all_popovers(win);
}

static gboolean
tab_switcher_overlay_get_child_position(GtkOverlay* overlay, GtkWidget* widget, GdkRectangle* allocation,
    gpointer user_data)
{
    int overlay_width = gtk_widget_get_width(GTK_WIDGET(overlay));
    int overlay_height = gtk_widget_get_height(GTK_WIDGET(overlay));

    int hor_min, hor_nat, hor_min_baseline, hor_nat_baseline;
    int ver_min, ver_nat, ver_min_baseline, ver_nat_baseline;
    gtk_widget_measure(widget, GTK_ORIENTATION_HORIZONTAL, -1, &hor_min, &hor_nat, &hor_min_baseline, &hor_nat_baseline);

    if(hor_nat<overlay_width)
    {
        allocation->width = hor_nat;
        allocation->x = overlay_width/2 - hor_nat/2;
    }
    else if(hor_min<overlay_width)
    {
        allocation->width = overlay_width;
        allocation->x = 0;
    }
    else
    {
        return FALSE;
    }

    gtk_widget_measure(
        widget, GTK_ORIENTATION_VERTICAL, allocation->width, &ver_min, &ver_nat, &ver_min_baseline, &ver_nat_baseline
    );

    if(ver_nat<overlay_height)
    {
        allocation->height = ver_nat;
        allocation->y = overlay_height/2 - ver_nat/2;
    }
    else if(ver_min<overlay_height)
    {
        allocation->height = overlay_height;
        allocation->y = 0;
    }
    else
    {
        return FALSE;
    }

    return TRUE;
}

BookmarksBar*
main_window_get_bookmarks_bar(MainWindow* self)
{
    return self->bookmarks_bar;
}

OrionWebviewTabContainer*
main_window_get_orion_webview_tab_container_by_tab_id(MainWindow* win, orion::TabId tab_id)
{
    OrionWebviewTabView* tab_view = main_window_get_webview_tab_view(win);
    if (!tab_view) return NULL;
    AdwTabPage* page = orion_webview_tab_view_get_page_with_tab_id(tab_view, tab_id);
    if (!page) return NULL;
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    return container;
}

OtherTabsAudioState
main_window_get_other_tabs_audio_state(MainWindow* win, orion::TabId current_tab_id)
{
    OtherTabsAudioState state{};
    AdwTabView* tab_view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    int n_pages = adw_tab_view_get_n_pages(tab_view);
    for(int i = 0; i < n_pages; i++)
    {
        AdwTabPage* page = adw_tab_view_get_nth_page(tab_view, i);
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        if(orion_webview_tab_container_get_id(container).value == current_tab_id.value) continue;
        if(orion_webview_tab_container_get_is_playing_audio(container))
        {
            state.any_playing = true;
            if(!orion_webview_tab_container_get_is_muted(container))
            {
                state.any_playing_unmuted = true;
            }
        }
    }
    return state;
}

OrionWebviewTabContent*
main_window_get_orion_webview_tab_content_by_tab_id(MainWindow* win, orion::TabId tab_id)
{
    OrionWebviewTabContainer* container = main_window_get_orion_webview_tab_container_by_tab_id(win, tab_id);
    if (!container) return NULL;
    return orion_webview_tab_container_get_content(container);
}

void
on_new_tab_button_clicked(MainWindow* win)
{
    g_return_if_fail(MAIN_IS_WINDOW(win));
    on_create_tab(win->tab_overview, win);
}

void
main_window_switch_to_sidebar_page(MainWindow* win, const char* page_name)
{
    sidebar_switch_to_page(win->sidebar, page_name);
}

void
main_window_popdown_all_popovers(MainWindow* win)
{
    GtkPopover* popovers[] = {
        win->window_list_popover,
        win->save_window_popover,
        win->programmable_button_popover,
    };

    for (GtkPopover* p : popovers) {
        if (p) {
            gtk_popover_popdown(p);
        }
    }
    
    sidebar_popdown_all_popovers(win->sidebar);
    header_bar_popdown_all_popovers(win->header_bar);
}

void
main_window_show_tab_overview(MainWindow* self)
{
    gtk_widget_activate_action(GTK_WIDGET(self->tab_overview), "overview.open", NULL);
}

void
main_window_hide_tab_overview(MainWindow* self)
{
    gtk_widget_activate_action(GTK_WIDGET(self->tab_overview), "overview.close", NULL);
}

void
main_window_stop_loading_current_tab(MainWindow* self)
{
    orion::TabId tab_id = main_window_get_current_tab_id(self);
    OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(self, tab_id);
    if (!content) return;
    orion_webview_tab_content_stop_loading(content);
}

static void
change_zoom(MainWindow* self, int64_t delta, bool reset)
{
    OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
    OrionWebviewTabContent* content = orion_webview_tab_view_get_current_tab_content(view);
    orion_webview_tab_content_change_zoom(content, delta, reset);
}

void
main_window_zoom_in(MainWindow* self)
{
    change_zoom(self, 1, false);
}

void
main_window_zoom_out(MainWindow* self)
{
    change_zoom(self, -1, false);
}

void
main_window_reset_zoom(MainWindow* self)
{
    change_zoom(self, 0, true);
}

void
main_window_update_page_zoom_button(MainWindow* self, int64_t zoom_percent)
{
    GtkButton* button = header_bar_get_page_zoom_button(self->header_bar);
    std::string label = std::to_string(zoom_percent) + "%";
    gtk_button_set_label(button, label.c_str());
    gtk_widget_set_visible(GTK_WIDGET(button), zoom_percent != 100);
}

void
main_window_update_popup_blocked_button(MainWindow* self, guint count)
{
    GtkButton* button = header_bar_get_popup_blocked_button(self->header_bar);
    gtk_widget_set_visible(GTK_WIDGET(button), count > 0);
}

orion::PrivateMode
main_window_get_private_mode(MainWindow* self)
{
    return self->private_mode;
}

gboolean
main_window_get_is_borderless(MainWindow* self)
{
    gboolean borderless = FALSE;
    g_object_get(ADW_APPLICATION_WINDOW(self), "fullscreened", &borderless, NULL);
    return borderless;
}

gboolean
main_window_get_is_webkit_fullscreen(MainWindow* self)
{
    return self->webkit_fullscreen;
}

static void
handle_focus_mode_and_fullscreen_motion(MainWindow* win, gdouble x, gdouble y)
{
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "toggle-focus-mode");
    gboolean is_focus_mode = g_variant_get_boolean(g_action_get_state(action));
    gboolean is_borderless_window = gtk_window_is_fullscreen(GTK_WINDOW(win));

    GAction* always_show_toolbar_in_fullscreen_action = g_action_map_lookup_action(
        G_ACTION_MAP(win), "always-show-toolbar-in-fullscreen");
    gboolean always_show_toolbar_in_fullscreen = g_variant_get_boolean(
        g_action_get_state(always_show_toolbar_in_fullscreen_action));

    if (!is_focus_mode && (win->webkit_fullscreen || !is_borderless_window || always_show_toolbar_in_fullscreen)) {
        return;
    }

    if (win->ui_hidden && y <= 4)
    {
        set_top_bar_visible(win, TRUE);
        win->ui_hidden = FALSE;
    }
    else if (!win->ui_hidden && y >= gtk_widget_get_height(GTK_WIDGET(win->tab_bar)) 
                + gtk_widget_get_height(GTK_WIDGET(win->header_bar))
                + gtk_widget_get_height(GTK_WIDGET(win->bookmarks_bar))
                + 10)
    {
        set_top_bar_visible(win, FALSE);
        win->ui_hidden = TRUE;
    }
}

static void
handle_auto_show_sidebar_motion(MainWindow* self, gdouble x, gdouble y)
{
    gboolean auto_show_sidebar = main_window_get_auto_show_sidebar_in_current_screen_mode(self);
    if (!auto_show_sidebar) return;

    gboolean show_sidebar = adw_overlay_split_view_get_show_sidebar(self->overlay_split_view);

    if (!show_sidebar && x <= 10) // on Mac, this condition is basically x == 0
    {
        change_sidebar_visibility(self, TRUE);
    }
    else if (show_sidebar && x >= gtk_widget_get_width(GTK_WIDGET(self->sidebar)) + 10)
    {
        change_sidebar_visibility(self, FALSE);
    }
}

static void 
motion_evaluator(GtkEventControllerMotion* self, gdouble x, gdouble y, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(user_data);
    handle_focus_mode_and_fullscreen_motion(win, x, y);
    handle_auto_show_sidebar_motion(win, x, y);
}

static char*
get_window_id_label(MainWindow* win, int64_t id)
{
    return g_strdup_printf("%li", id);
}

// The cb callback is called after the window is asynchronously focused.
void
main_window_focus(MainWindow* win, std::function<void()> cb)
{
    orion::gtk::WeakRefWrapper win_weak(win);

    // This has to be done asynchronously because it would not work if there is still
    // some popover open (eg., a popover menu). Also, async() and async_after(0ms)
    // may still not work properly, so we use a small non-zero delay. Even using
    // g_idle_add() is not reliable in some cases unfortunately. Theoretically,
    // we can react to "closed" signal of a potential popover menu being open.
    global_core_api->async_after(std::chrono::milliseconds{50}, [win_weak,cb=std::move(cb)]{
        auto win_ptr = win_weak.lock();
        if (!win_ptr) return;
        MainWindow* win = MAIN_WINDOW(win_ptr.get());
        
        gtk_window_present(GTK_WINDOW(win));
        if (cb) {
            cb();
        }
    });
}

static void
on_active_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    MainWindow* self = MAIN_WINDOW(object);
    if (!self->is_active && gtk_window_is_active(GTK_WINDOW(self))) {
        // window got focus
        orion::log_verbose("on_active_changed: got focus: profile_id=%lld window_id=%lld",
            self->wrapper.profile_id, self->wrapper.window_id, gtk_window_is_active(GTK_WINDOW(self))
        );
        self->is_active = TRUE;

        OrionApp* app = ORION_APP(gtk_window_get_application(GTK_WINDOW(self)));
        orion_app_update_theme(app, self->wrapper.profile_id);

        orion::SetSelectedWindowIdQuery query{};
        query.profile_id = self->wrapper.profile_id;
        query.window_id = self->wrapper.window_id;
        orion::log_verbose("main_window: on_active_changed: set_selected_window_id(%lld)",
            self->wrapper.window_id.value
        );
        global_core_api->set_selected_window_id(query, orion::Cb<orion::Error>{[](orion::Error err){
            VERIFY_LOG(err);
        }});
    } else {
        // window lost focus
        self->is_active = FALSE;
    }
}

static void
on_maximized_changed(GObject* object, GParamSpec* pspec, gpointer user_data)
{
    GtkWindow* window = GTK_WINDOW(object);
    gboolean maximized = gtk_window_is_maximized(window);
    orion::log_verbose("on_maximized_changed: maximized=%d", maximized);
    MainWindow* main_window = MAIN_WINDOW(window);
    orion::ProfileId profile_id = main_window_get_current_profile_id(main_window);

    // Persist the maximized state in the current profile
    global_core_api->set_setting(profile_id, orion::settings::appearance::start_maximized, maximized,
        [](orion::SetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
        });
}

void
main_window_switch_to_previous_tab(MainWindow* win)
{
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    int len = adw_tab_view_get_n_pages(view);
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    if(adw_tab_view_get_page_position(view, page) == 0)
    {
        adw_tab_view_set_selected_page(view, adw_tab_view_get_nth_page(view, len-1));
    }
    else
    {
        adw_tab_view_select_previous_page(view);
    }
}

void
main_window_switch_to_next_tab(MainWindow* win)
{
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    int len = adw_tab_view_get_n_pages(view);
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    if(adw_tab_view_get_page_position(view, page) == len-1)
    {
        adw_tab_view_set_selected_page(view, adw_tab_view_get_nth_page(view, 0));
    }
    else
    {
        adw_tab_view_select_next_page(view);
    }
}

void
main_window_switch_to_tab(MainWindow* win, orion::TabDto tab)
{
    AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    int64_t n_pages = adw_tab_view_get_n_pages(view);
    if (tab.index < 0 || tab.index >= n_pages) {
        orion::log_warning("main_window_switch_to_tab: invalid tab index %d (n_pages=%lld) in tab id %lld",
            tab.index, n_pages, tab.id.value
        );
        return;
    }
    adw_tab_view_set_selected_page(view, adw_tab_view_get_nth_page(view, (int)tab.index));
}

void
main_window_show_next_tab_accel(MainWindow* win)
{
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "show-tab-switcher");
    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    if(g_variant_get_boolean(state))
    {
        main_window_show_tab_switcher(win);
        tab_switcher_select_next_tab(win->tab_switcher);
    }
    else
    {
        main_window_switch_to_next_tab(win);
    }
}

void
main_window_show_previous_tab_accel(MainWindow* win)
{
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "show-tab-switcher");
    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    if(g_variant_get_boolean(state))
    {
        main_window_show_tab_switcher(win);
        tab_switcher_select_prev_tab(win->tab_switcher);
    }
    else
    {
        main_window_switch_to_previous_tab(win);
    }
}

void
main_window_show_tab_switcher(MainWindow* win)
{
    GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "show-tab-switcher");
    GVariant* state = g_action_get_state(action);
    defer([state]{ g_variant_unref(state); });
    if(g_variant_get_boolean(state))
    {
        gtk_widget_set_visible(GTK_WIDGET(win->tab_switcher), true);
    }
}

// Workaround for Adwaita bug where pinned tabs are not shown correctly in tab bar.
// If there are any pinned pages, then after calling Adwaita's adw_tab_bar_set_view(),
// all unpinned pages will be shown in reverse order in the tab bar. To workaround this,
// we temporarily unpin all pinned pages, call adw_tab_bar_set_view(), and then re-pin
// the pinned pages back.
// static void
// fixed_adw_tab_bar_set_view(AdwTabBar* tab_bar, AdwTabView* tab_view)
// {
//     // temporarily unpin all pinned pages
//     int n_pages = adw_tab_view_get_n_pages(tab_view);
//     std::vector<AdwTabPage*> pinned_pages;
//     for (int i = 0; i < n_pages; i++) {
//         AdwTabPage* page = adw_tab_view_get_nth_page(tab_view, i);
//         if (adw_tab_page_get_pinned(page)) {
//             pinned_pages.push_back(page);
//         }
//         adw_tab_view_set_page_pinned(tab_view, adw_tab_view_get_nth_page(tab_view, i), FALSE);
//     }

//     // this is where the bug happens if there are pinned pages
//     adw_tab_bar_set_view(tab_bar, tab_view);

//     // re-pin the pinned pages
//     for (auto page : pinned_pages) {
//         adw_tab_view_set_page_pinned(tab_view, page, TRUE);
//     }
// }

static gboolean
title_binding_transform_to(GBinding* binding, const GValue* from, GValue* to, gpointer user_data)
{
    const char* tab_title = g_value_get_string(from);
    MainWindow* win = MAIN_WINDOW(user_data);

    // If Settings dialog is open, show "Settings — Orion"
    if (win && win->settings_dialog_open) {
        std::string settings_title = std::string("Settings — ") + std::string(_("Orion"));
        g_value_set_string(to, settings_title.c_str());
        return true;
    }

    std::string title = std::string(_("Orion"));
    if (tab_title && tab_title[0] != '\0') {
        title = std::string(tab_title) + " — " + std::string(_("Orion"));
    }
    g_value_set_string(to, title.c_str());
    return true;
}

static void notify_uri(MainWindow* self) { g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_URI]); }
static void notify_tls_status(MainWindow* self) { g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TLS_STATUS]); }
static void notify_back_list(MainWindow* self) { g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_BACK_LIST]); }
static void notify_forward_list(MainWindow* self) { g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_FORWARD_LIST]); }

static void
main_window_set_actual_tab_view(MainWindow* self, OrionWebviewTabView* tab_view)
{
    if(self->page_content_tabview)
    {
        g_signal_handlers_disconnect_by_func(self->page_content_tabview, (gpointer)notify_uri, self);
    }
    self->page_content_tabview = tab_view;
    AdwTabView* actual_tab_view = orion_webview_tab_view_get_actual_view(self->page_content_tabview);
    TabBar* tab_bar = APP_TAB_BAR(header_bar_get_compact_page_content_tabbar(self->header_bar));

    tab_bar_set_tab_view(tab_bar, self->page_content_tabview);
    tab_bar_set_tab_view(self->tab_bar, self->page_content_tabview);
    adw_tab_overview_set_view(self->tab_overview, actual_tab_view);
    tab_switcher_set_tab_view(self->tab_switcher, actual_tab_view);
    sidebar_setup_tabs_lists(self->sidebar, self->page_content_tabview);

    orion_webview_tab_view_set_back_button(self->page_content_tabview, header_bar_get_back_button(self->header_bar));
    orion_webview_tab_view_set_forward_button(self->page_content_tabview, header_bar_get_forward_button(self->header_bar));
    orion_webview_tab_view_set_reload_button(self->page_content_tabview, header_bar_get_reload_page_button(self->header_bar));

    if(self->tab_view_title_binding)
    {
        g_clear_object(&self->tab_view_title_binding);
    }
    self->tab_view_title_binding = g_object_bind_property_full(
        tab_view, "title", self, "title",
        G_BINDING_SYNC_CREATE,
        title_binding_transform_to,
        NULL, /* transform_from */
        self, /* user_data passed to transform */
        NULL /* destroy_notify */);

    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_TAB_VIEW]);

    g_signal_connect_object(tab_view, "notify::uri", G_CALLBACK(notify_uri), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(tab_view, "notify::tls-status", G_CALLBACK(notify_tls_status), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(tab_view, "notify::back_list", G_CALLBACK(notify_back_list), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(tab_view, "notify::forward-list", G_CALLBACK(notify_forward_list), self, G_CONNECT_SWAPPED);
    notify_uri(self);
    notify_tls_status(self);
    notify_back_list(self);
    notify_forward_list(self);
}

void
main_window_set_settings_dialog_open(MainWindow* self, gboolean open)
{
    if (!self) return;
    self->settings_dialog_open = open;
    if (open) {
        std::string settings_title = std::string("Settings — ") + std::string(_("Orion"));
        gtk_window_set_title(GTK_WINDOW(self), settings_title.c_str());
    } else {
        // restore title from the active tab
        // compute title from current tab and set it directly to ensure immediate update
        OrionWebviewTabView* view = main_window_get_webview_tab_view(self);
        const char* tab_title = NULL;
        if (view) tab_title = orion_webview_tab_view_get_title(view);

        std::string title = std::string(_("Orion"));
        if (tab_title && tab_title[0] != '\0') {
            // strip existing suffixes to avoid duplication
            std::string base = std::string(tab_title);
            const std::vector<std::string> suffixes = {" — Orion", " - Orion", " \u2014 Orion"};
            for (const auto& suf : suffixes) {
                if (base.size() > suf.size() && base.compare(base.size() - suf.size(), suf.size(), suf) == 0) {
                    base = base.substr(0, base.size() - suf.size());
                    break;
                }
            }
            while (!base.empty() && base.back() == ' ') base.pop_back();
            if (!base.empty()) title = base + " — " + std::string(_("Orion"));
        }
        gtk_window_set_title(GTK_WINDOW(self), title.c_str());
    }
}

// Called when switching between Tab Groups (saved windows) in-place (in the same main window).
// Note that it hides the old tab view and shows the new one, it does not destroy anything,
// so "unmap" signal is emitted for the old tab view. In this mode, the old tab views still
// consume memory and other resources - this can be improved in the future if needed.
OrionWebviewTabView*
main_window_set_current_tab_view_by_window_id(MainWindow* self, orion::WindowId window_id, bool load_from_backend)
{
    OrionWebviewTabView* tab_view = nullptr;
    bool creating_new = false;
    
    // hide the old tab view
    if (self->page_content_tabview) {
        orion::log_verbose("main_window_set_current_tab_view_by_window_id: hiding old tab view %p",
            self->page_content_tabview
        );
        gtk_widget_set_visible(GTK_WIDGET(self->page_content_tabview), FALSE);
    }

    auto it = self->wrapper.tab_view_map->find(window_id);

    if (it == self->wrapper.tab_view_map->end()) {
        orion::log_verbose("main_window_set_current_tab_view_by_window_id: creating new tab view with window_id %lld",
            window_id.value
        );
        creating_new = true;

        // visible by default
        tab_view = orion_webview_tab_view_new();
        gtk_box_append(self->page_tabview_box, GTK_WIDGET(tab_view));

        self->wrapper.tab_view_map->insert({window_id, g_object_ref(tab_view)}); // we hold a reference in the map

        orion::log_verbose("tab_view_map: new tab view %p with window_id %lld created (total %zu tab views in map)",
            tab_view, window_id.value, self->wrapper.tab_view_map->size()
        );
    } else {
        orion::log_verbose(
            "main_window_set_current_tab_view_by_window_id: switching to existing tab view with window_id %lld",
            window_id.value
        );

        // When switching tab views, we want to still keep the old tab view in the widget hierarchy
        // (but hidden) so that its children still can find their corresponding main window calling
        // gtk_widget_get_root() or similar functions. Also, we do not need to ref/unref the tab views
        // to manage their lifetimes properly, since they are all children of page_tabview_box.

        // show the new tab view
        orion::log_verbose(
            "main_window_set_current_tab_view_by_window_id: showing existing tab view %p with window_id %lld",
            it->second, window_id.value
        );
        gtk_widget_set_visible(GTK_WIDGET(it->second), TRUE);

        tab_view = it->second;
    }

    main_window_set_actual_tab_view(self, tab_view);

    if (creating_new && load_from_backend) {
        // load the new tab view from backend
        if(self->wrapper.done_cb)
        {
            orion::gtk::WeakRefWrapper self_weak(self);

            auto* cb_ptr = self->wrapper.done_cb.release();
            auto handler = [self_weak, cb = std::move(*cb_ptr)](void*){
                auto self_strong = self_weak.lock();
                if(self_strong) cb(MAIN_WINDOW(self_strong.get()));
            };
            delete cb_ptr;

            orion_webview_tab_view_load_from_backend(
                tab_view,
                window_id,
                main_window_get_current_profile_id(self),
                std::move(handler),
                NULL
            );
        }
        else
        {
            orion_webview_tab_view_load_from_backend(
                tab_view,
                window_id,
                main_window_get_current_profile_id(self),
                NULL,
                NULL
            );
        }
    }

    if(self->tab_bar)
    {
        tab_bar_set_tab_view(self->tab_bar, tab_view);
    }
    return tab_view;
}

void
main_window_autoselect_next_created_tab_with_id(MainWindow* win, orion::TabId id)
{
    orion_webview_tab_view_select_next_created_tab_with_id(main_window_get_webview_tab_view(win), id);
}

void
main_window_reload_tab_by_tab_id(MainWindow* win, orion::TabId id)
{
    orion::gtk::WeakRefWrapper win_weak(win);

    // Just to check the existence of the tab
    global_core_api->get_tabs({id}, [win_weak,id](orion::GetTabsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.tabs.empty()) return;

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        AdwTabView* view = orion_webview_tab_view_get_actual_view(main_window_get_webview_tab_view(win));
    
        int len = adw_tab_view_get_n_pages(view);
        for(int i = 0; i<len; i++)
        {
            AdwTabPage* page = adw_tab_view_get_nth_page(view, i);
            OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
            if(orion_webview_tab_container_get_id(container) == id)
            {
                OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
                if(content)
                {
                    orion_webview_tab_content_reload(content);
                }

                // unsuspend the tab if it was suspended
                orion::UpdateTabQuery query{
                    .id = id,
                    .is_suspended = false,
                    .update_is_suspended = true,
                };
                global_core_api->update_tab(query, [](orion::Error err){
                    VERIFY_LOG(err);
                });
                break;
            }
        }
    });
}

void
main_window_reload_current_tab_bypass_cache(MainWindow* self)
{
    orion::TabId tab_id = main_window_get_current_tab_id(self);
    OrionWebviewTabContent* content = main_window_get_orion_webview_tab_content_by_tab_id(self, tab_id);
    if (!content) return;
    orion_webview_tab_content_reload_bypass_cache(content);
}

void
main_window_reload_all_tabs(MainWindow* self)
{
    orion::GetTabIdsQuery query{
        .window_id = self->wrapper.window_id,
    };
    global_core_api->get_tab_ids(query, [self](orion::GetTabIdsReply reply){
        VERIFY_LOG(reply.err);
        for (const auto& tab_id : reply.ids) {
            main_window_reload_tab_by_tab_id(self, tab_id);
        }
    });
}

void
main_window_update_enable_compatibility_mode_action_state(MainWindow* self, std::string website)
{
    orion::ProfileId profile_id = main_window_get_current_profile_id(self);
    orion::gtk::WeakRefWrapper self_weak(self);

    orion::log_verbose(
        "main_window_update_enable_compatibility_mode_action_state: website=%s",
        website.c_str());

    global_core_api->get_setting_for_website_or_default(
        orion::GetSettingForWebsiteOrDefaultQuery{
            .profile_id = profile_id,
            .website = website,
            .key_name = orion::settings::website::specific::enable_compatibility_mode.key_name(),
        },
        [self_weak](orion::GetSettingForWebsiteOrDefaultReply reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            bool enabled{};
            VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &enabled));
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "enable-compatibility-mode");
            g_action_change_state(action, g_variant_new_boolean(enabled));
        }
    );
}

void
main_window_on_current_website_change(MainWindow* self)
{
    std::string website = main_window_get_current_website(self);
    if (self->wrapper.current_website == website) {
        // Nothing has changed
        return;
    }
    orion::log_verbose(
        "main_window_on_current_website_change: old_website=%s new_website=%s",
        self->wrapper.current_website.c_str(), website.c_str()
    );
    self->wrapper.current_website = website;

    main_window_update_enable_compatibility_mode_action_state(self, website);
    
    // orion::gtk::WeakRefWrapper self_weak(self);

    // orion::GetSettingForWebsiteOrDefaultQuery query{
    //     .profile_id = self->wrapper.profile_id,
    //     .website = website,
    //     .key_name = orion::settings::website::specific::enable_reader_mode.key_name(),
    // };

    // global_core_api->get_setting_for_website_or_default(query,
    // [self_weak](orion::GetSettingForWebsiteOrDefaultReply reply){
    //     VERIFY_LOG(reply.err);
    //     auto self_strong = self_weak.lock();
    //     if (!self_strong) return;
    //     MainWindow* self = MAIN_WINDOW(self_strong.get());

    //     orion::settings::ReaderModeWebsiteSetting setting{};
    //     VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &setting));
    //     bool enabled{};
    //     if (setting == orion::settings::ReaderModeWebsiteSetting::on) {
    //         enabled = true;
    //     } else if (setting == orion::settings::ReaderModeWebsiteSetting::off) {
    //         enabled = false;
    //     } else {
    //         orion::log_warning(
    //             "main_window_on_current_website_change: unexpected ReaderModeWebsiteSetting value %d",
    //             static_cast<int>(setting)
    //         );
    //         enabled = false;
    //     }
        
    //     GAction* action = g_action_map_lookup_action(G_ACTION_MAP(self), "toggle-reader");
    //     if (action) {
    //         g_action_change_state(action, g_variant_new_boolean(enabled));
    //     }
    // });
}

#if ENABLE_ITP_SUMMARY
static void
get_itp_summary_ready_cb(GObject* source_object, GAsyncResult* res, gpointer user_data)
{
    WebKitNetworkSession* network_session = WEBKIT_NETWORK_SESSION(source_object);
    GError* error = NULL;
    GList* summary_list = webkit_network_session_get_itp_summary_finish(network_session, res, &error);
    if (error) {
        orion::log_warning("get_itp_summary_ready_cb: failed to get ITP summary: %s", error->message);
        g_error_free(error);
        return;
    }
    defer([summary_list]{ g_list_free(summary_list); }); // can be called even on NULL

    if (!summary_list) {
        orion::log_info("get_itp_summary_ready_cb: ITP summary is empty");
        return;
    }
    
    for (GList* sl = summary_list; sl != NULL; sl = sl->next) {
        // There is no smart cast macro defined for boxed types like WebKitITPThirdParty, use static_cast
        auto third_party = static_cast<WebKitITPThirdParty*>(sl->data);
        defer([third_party]{ webkit_itp_third_party_unref(third_party); });

        const char* domain_third = webkit_itp_third_party_get_domain(third_party);
        GList* first_parties = webkit_itp_third_party_get_first_parties(third_party); // must not free this list

        for (GList* fp = first_parties; fp != NULL; fp = fp->next) {
            auto first_party = static_cast<WebKitITPFirstParty*>(fp->data); // do not unref this pointer
            const char* domain_first = webkit_itp_first_party_get_domain(first_party);
            gboolean access_allowed = webkit_itp_first_party_get_website_data_access_allowed(first_party);
            GDateTime* last_update_time = webkit_itp_first_party_get_last_update_time(first_party);
            orion::log_info(
                "get_itp_summary_ready_cb: domain_third=%s domain_first=%s access_allowed=%d last_update_time=%lld",
                domain_third, domain_first, access_allowed, g_date_time_to_unix(last_update_time)
            );
        }
    }
}
#endif // ENABLE_ITP_SUMMARY

static void
open_test_dialog(MainWindow* win)
{
#if OPEN_TEST_DIALOG && !defined(NDEBUG)
    ImportCsvPasswordsDialogOptions options{
        .n_imported = 7,
        .not_imported_items = std::vector<orion::PasswordItemDto>{
            orion::PasswordItemDto{.url="example.com", .username="user1"},
            orion::PasswordItemDto{.url="atlas.com", .username="user2"},
            orion::PasswordItemDto{.url="wikipedia.org", .username="user3"},
        },
        .show_open_passwords_button = true,
    };
    AdwDialog* dialog = import_csv_passwords_dialog_new_for_import_result(options);
    dialog = delete_csv_passwords_dialog_new_for_import_result(6, "/home/user/passwords.csv");
    dialog = fill_password_dialog_new(nullptr);
    adw_dialog_present(ADW_DIALOG(dialog), GTK_WIDGET(win));
#endif // OPEN_TEST_DIALOG && !defined(NDEBUG)
}

static void
main_window_setup_kagi_service(MainWindow* self)
{
    self->wrapper.kagi_service =
        orion_app_get_or_create_kagi_service(
            *self->wrapper.profile_world);

    self->wrapper.kagi_service->get_or_create_kagi_client_id([](std::string kagi_client_id, orion::Error err){
        VERIFY_LOG(err);
        orion::log_verbose("kagi_client_id: %s", kagi_client_id.c_str());
    });

    self->wrapper.kagi_service->set_onboarding_status(orion::KagiService::OnboardingStatus::pending);
    self->wrapper.kagi_service->try_kagi_search_onboarding([](orion::Error err){
        VERIFY_LOG(err);
    });
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static WebKitUserScript*
main_window_construct_reader_template_script(MainWindow* self)
{
    GError* err = NULL;
    std::string desktop_scroller;
    {
        GBytes* desktop_scroller_bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/desktop_scroller.js", G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
        if(err)
        {
            orion::log_error("%s", err->message);
            g_error_free(err);
            return NULL;
        }
        defer([desktop_scroller_bytes]{ g_bytes_unref(desktop_scroller_bytes); });

        gsize len = 0;
        const char* bytes_data = static_cast<const char*>(g_bytes_get_data(desktop_scroller_bytes, &len));
        std::string_view desktop_scroller_template(bytes_data, len);
        orion::str::PlaceholderMap desktop_scroller_dict = {
            {"READER-BODY-CLICK-EVENT", "window.webkit.messageHandlers.readerMode.postMessage([\"bodyClicked\"]);"}
        };
        desktop_scroller = orion::str::parse_string_prepend_append_and_replace_placeholders(desktop_scroller_template, "", "", desktop_scroller_dict, false);
    }

    GBytes* bytes = g_resources_lookup_data(GRESOURCE_PREFIX "/scripts/reader_template_script.js",
        G_RESOURCE_LOOKUP_FLAGS_NONE, &err);
    if(err)
    {
        orion::log_error("%s", err->message);
        g_error_free(err);
        return NULL;
    }

    defer([bytes]{ g_bytes_unref(bytes); });
    gsize len = 0;
    const char* bytes_data = static_cast<const char*>(g_bytes_get_data(bytes, &len));
    std::string_view template_str(bytes_data, len);

    orion::str::PlaceholderMap map = {
        {"READER-FONTSIZE-INDEX", std::to_string(orion::gtk::get_int64_action_state(G_ACTION_MAP(self), "reader-mode-font-size-index"))},
        {"READER-SCRIPT-1", R"(
                document.getElementById('h1-title').style.color = reader.bodyFontColor();
                document.getElementsByClassName('metadata singleline')[0].style.color = reader.bodyFontColor();
                document.querySelectorAll("table").forEach(item => item.style.color = reader.bodyFontColor());
                document.getElementById("article").querySelectorAll(".page, .metadata a[href]").forEach(item => item.style.color = reader.bodyFontColor());
            )"},
        {"READER-SCRIPT-2", R"(
                var figures = document.getElementsByTagName('figure');
                for (var i = 0; i < figures.length; i++) {
                    figures[i].style.fontFamily = document.body.style.fontFamily;
                }
            )"},
        {"READER-SCRIPT-3", R"(
                var figures = document.getElementsByTagName('figure');
                for (var i = 0; i < figures.length; i++) {
                    figures[i].style.textAlign = document.body.style.textAlign;
                }
            )"},
        {"READER-TEXT-INITIAL-OFFSET", "window.textForInitialOffset"},
        {"READER-TECHCRUNCH-CONDITION", "window.location.host.endsWith('techcrunch.com')"},
        {"READER-BODY-CLICK-EVENT", "window.webkit.messageHandlers.readerMode.postMessage([\"bodyClicked\"]);"},
        {"READER-SCRIPT-4", orion::reader_mode_width_script(orion::gtk::get_int64_action_state(G_ACTION_MAP(self), "reader-mode-document-width"))},
        {"DESKTOP-SCROLLER-SCRIPT", desktop_scroller},
    };

    std::string script_string = orion::str::parse_string_prepend_append_and_replace_placeholders(template_str, "", "", map, false);

    return webkit_user_script_new_for_world(script_string.c_str(), WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, READABILITY_SCRIPT_WORLD, NULL, NULL);
}

static void
main_window_notify_reader_script_data(GAction* action, GParamSpec* pspec, MainWindow* self)
{
    g_clear_pointer(&self->reader_template_script, webkit_user_script_unref);
    self->reader_template_script = main_window_construct_reader_template_script(self);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_READER_TEMPLATE_SCRIPT]);
}

WebKitUserScript*
main_window_get_reader_template_script(MainWindow* self)
{
    if(!self->reader_template_script)
    {
        self->reader_template_script = main_window_construct_reader_template_script(self);
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_READER_TEMPLATE_SCRIPT]);
        GAction* font_size_index_action = g_action_map_lookup_action(G_ACTION_MAP(self), "reader-mode-font-size-index");
        GAction* document_width_action = g_action_map_lookup_action(G_ACTION_MAP(self), "reader-mode-document-width");
        g_signal_connect(font_size_index_action, "notify::state", G_CALLBACK(main_window_notify_reader_script_data), self);
        g_signal_connect(document_width_action, "notify::state", G_CALLBACK(main_window_notify_reader_script_data), self);
    }
    return self->reader_template_script;
}
#endif

static gboolean
key_pressed_cb(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
    if (keyval == GDK_KEY_Escape) {
        orion::log_verbose("orion_webkit_webview: escape key pressed");
        GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(controller));
        MainWindow* win = orion::gtk::get_root_main_window(widget);
        if (!win) {
            orion::log_warning("key_pressed_cb: could not find root main window for widget %p", widget);
            return FALSE;
        }
        if (win->webkit_fullscreen) {
            // Leave webkit fullscreen
            OrionWebviewTabContent* content = main_window_get_current_tab_content(win);
            
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
            WebKitWebView* webview = orion_webview_tab_content_get_webview(content);
            orion_webkit_webview_leave_webkit_fullscreen(ORION_WEBKIT_WEBVIEW(webview));
#endif
        }
    }
    return FALSE; // always propagate the event further
}

// profile_id and window_id of main window are already set here, but
// note that when gtk_widget_init_template() is called, each child widget
// is not a descendant of the main window yet, so the main window pointer
// cannot be retrieved using gtk_widget_get_root() or similar in the child
static void
main_window_constructed(GObject* object)
{
    // always chain up first
    G_OBJECT_CLASS(main_window_parent_class)->constructed(object);

    MainWindow* self = MAIN_WINDOW(object);

    orion::log_verbose("main_window_constructed: profile_id=%d private=%d",
        self->wrapper.profile_id, static_cast<int>(self->private_mode)
    );

    self->wrapper.profile_world = orion_app_activate_profile_world(
        self->wrapper.profile_id, self->private_mode).lock();

    global_core_api->ref_ui_window();
    global_core_api->register_ui_window(static_cast<orion::UiWindowId>(self));

    g_type_ensure(APP_HEADER_BAR_TYPE);
    g_type_ensure(APP_BOOKMARKS_BAR_TYPE);
    g_type_ensure(APP_FIND_BAR_TYPE);
    g_type_ensure(READING_LIST_TOGGLE_GROUP_TYPE);
    g_type_ensure(APP_SIDEBAR_TYPE);
    g_type_ensure(SEARCH_RECOMMENDATIONS_TYPE);
    g_type_ensure(WINDOW_LIST_BOX_TYPE);
    g_type_ensure(ORION_ACCELERATOR_ENTRY_TYPE);
    g_type_ensure(ORION_EDITABLE_CELL_ENTRY_TYPE);
    g_type_ensure(ORION_WEBVIEW_TAB_VIEW_TYPE);
    g_type_ensure(ORION_ADDRESS_ENTRY_TYPE);
    g_type_ensure(APP_SCALED_CONTAINER_TYPE);
    g_type_ensure(APP_TAB_BAR_TYPE);
    g_type_ensure(APP_TAB_SWITCHER_TYPE);
    g_type_ensure(APP_TYPE_ERASE_ELEMENTS_BAR);
    g_type_ensure(APP_TYPE_JS_SIDEBAR);

	g_signal_connect(self, "notify::profile-id", G_CALLBACK(on_profile_id_notify), NULL);

	gtk_widget_init_template(GTK_WIDGET(self));

    GType types[1] = {ADW_TYPE_TAB_PAGE};
    gtk_drop_target_set_gtypes(self->drop_target, types, 1);

    // GListModel* controllers = gtk_widget_observe_controllers(GTK_WIDGET(self->tab_bar_box));
    // defer([controllers]{g_object_unref(controllers);});
    // GtkDropTarget* drop_target = GTK_DROP_TARGET(g_list_model_get_item(controllers, 0));
    // defer([drop_target]{g_object_unref(drop_target);});
    // GType types[] = {ADW_TYPE_TAB_PAGE};
    // gtk_drop_target_set_gtypes(drop_target, types, 1);


    
    // We need to call this at least once on startup to update profile button icon
    on_profile_id_notify(self, NULL, NULL);

	setup_context_menus(self);
    
#if ENABLE_KAGI_SERVICE
    // WebView factory must already be setup
    main_window_setup_kagi_service(self);
#endif

	g_signal_connect(self, "show", G_CALLBACK(on_show), NULL);
    g_signal_connect(self, "notify::window-id", G_CALLBACK(on_window_id_notify), NULL);
    g_signal_connect(self, "notify::fullscreened", G_CALLBACK(main_window_on_borderless_changed), NULL);
    g_signal_connect(self, "notify::is-active", G_CALLBACK(on_active_changed), NULL);
    g_signal_connect(self, "notify::maximized", G_CALLBACK(on_maximized_changed), NULL);

    GtkEventController* motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(motion_evaluator), self);

    gtk_widget_add_controller(GTK_WIDGET(self), motion_controller);

    g_signal_connect(
        header_bar_get_window_list_button(self->header_bar), "clicked", G_CALLBACK(on_window_list_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_privacy_button(self->header_bar), "clicked", G_CALLBACK(on_privacy_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_rss_feeds_button(self->header_bar), "clicked", G_CALLBACK(on_rss_feeds_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_page_tweaks_button(self->header_bar), "clicked", G_CALLBACK(on_page_tweaks_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_add_page_to_button(self->header_bar), "clicked", G_CALLBACK(on_add_page_to_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_history_button(self->header_bar), "clicked", G_CALLBACK(on_history_button_clicked), self
    );
    g_signal_connect(
        header_bar_get_certificates_button(self->header_bar), "clicked", G_CALLBACK(on_certificates_button_clicked), self
    );

    GtkEventController* key_controller = gtk_event_controller_key_new();
    g_signal_connect(key_controller, "key-pressed", G_CALLBACK(key_pressed_cb), NULL);
    // Takes ownership of the controller object
    gtk_widget_add_controller(GTK_WIDGET(self), key_controller);
    
    header_bar_set_sidebar_button_toggled_callback(self->header_bar, main_window_on_sidebar_toggled, self);

    sidebar_set_sidebar_button_clicked_callback(self->sidebar, on_sidebar_button_clicked, self);

    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_sidebar_button(self->header_bar)),
        on_sidebar_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_window_list_button(self->header_bar)),
        on_window_list_button_secondary_released
    );
	orion::gtk::register_left_and_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_profiles_button(self->header_bar)),
        on_profiles_button_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_address_focus_mode_button(self->header_bar)),
        on_address_focus_mode_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_reader_mode_button(self->header_bar)),
        on_reader_mode_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_rss_feeds_button(self->header_bar)),
        on_rss_feeds_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_certificates_button(self->header_bar)),
        on_certificates_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_reload_page_button(self->header_bar)),
        on_reload_page_button_secondary_released
    );
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_new_tab_button(self->header_bar)),
        on_new_tab_button_secondary_released
    );
    // We want the same behavior as right-click
    orion::gtk::register_left_long_click(
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_new_tab_button(self->header_bar)),
        on_new_tab_button_secondary_released
    );
	orion::gtk::register_left_click( // only left-click is needed here
        GTK_WIDGET(self), GTK_WIDGET(header_bar_get_share_button(self->header_bar)),
        on_share_button_released
    );
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->header_bar),
        on_toolbar_box_secondary_released
    );

    orion::gtk::WeakRefWrapper self_weak(self);
    self->wrapper.profiles_change_observer = global_core_api->profiles_change().add_observer(
        [self_weak](const orion::ProfilesChange& change){
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            main_window_on_profiles_change(self, change);
        }
    );

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
//TODO: do we want to use the default webkit network session?
    WebKitNetworkSession* session = webkit_network_session_get_default();
    WebKitWebsiteDataManager* data_manager = webkit_network_session_get_website_data_manager(session);
    webkit_website_data_manager_set_favicons_enabled (data_manager, TRUE);
#endif

    self->wrapper.windows_change_observer = global_core_api->windows_change().add_observer(
        [self_weak](const orion::WindowsChange& change){
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            on_windows_change(self, change);
        }
    );

    self->wrapper.bookmarks_change_observer = global_core_api->bookmarks_change().add_observer(
        [self_weak](const orion::BookmarksChange& change){
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            on_bookmarks_change(self, change);
        }
    );

    open_test_dialog(self);

#if ENABLE_ITP_SUMMARY
    WebExtensions::WebExtensionWebViewFactory& factory = main_window_get_current_webview_factory(self);
    WebKitNetworkSession* network_session = WEBKIT_NETWORK_SESSION(factory.network_session());
    webkit_network_session_get_itp_summary(network_session, NULL, get_itp_summary_ready_cb, NULL);
#endif // ENABLE_ITP_SUMMARY

    // Restore the last maximized state
    global_core_api->get_setting(self->wrapper.profile_id, orion::settings::appearance::start_maximized,
        [self_weak](orion::GetSettingReply<bool> reply){
            VERIFY_LOG(reply.err);
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            MainWindow* self = MAIN_WINDOW(self_strong.get());

            if (reply.value) {
                gtk_window_maximize(GTK_WINDOW(self));
            }
        }
    );

    global_core_api->get_programmable_buttons({.profile_id = self->wrapper.profile_id}, [self_weak](orion::GetProgrammableButtonsReply reply){
        VERIFY_LOG(reply.err);
        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());
        for(auto& button: reply.items)
        {
            GtkShortcut* shortcut = orion::gtk::programmable_button_parse_shortcut(button);
            self->wrapper.programmable_button_shortcut_map->insert_or_assign(button.id.value(), shortcut);
            //this takes ownership of shortcut, meaning we transfer the ref local to this scope
            gtk_shortcut_controller_add_shortcut(self->programmable_button_shortcut_controller, shortcut);
        }
    });

    self->wrapper.programmable_buttons_observer = global_core_api->programmable_buttons_change().add_observer(
    [self_weak](const orion::ProgrammableButtonsChange& change){
        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        MainWindow* self = MAIN_WINDOW(self_strong.get());

        for(auto& created : change.created)
        {
            GtkShortcut* shortcut = orion::gtk::programmable_button_parse_shortcut(created);
            self->wrapper.programmable_button_shortcut_map->insert_or_assign(created.id.value(), shortcut);
            //this takes ownership of shortcut, meaning we transfer the ref local to this scope
            gtk_shortcut_controller_add_shortcut(self->programmable_button_shortcut_controller, shortcut);
        }
        for(auto& updated : change.updated)
        {
            if(self->wrapper.programmable_button_shortcut_map->contains(updated.id.value()))
            {
                auto shortcut_strong = self->wrapper.programmable_button_shortcut_map->at(updated.id.value());
                GtkShortcut* shortcut = GTK_SHORTCUT(shortcut_strong.get());

                GtkShortcutAction* action = orion::gtk::programmable_button_parse_action(updated);
                gtk_shortcut_set_action(shortcut, action);
                
                GtkShortcutTrigger* trigger = orion::gtk::programmable_button_parse_trigger(updated);
                gtk_shortcut_set_trigger(shortcut, trigger);

                GVariant* args = orion::gtk::programmable_button_parse_args(updated);
                gtk_shortcut_set_arguments(shortcut, args);
            }
            else
            {
                GtkShortcut* shortcut = orion::gtk::programmable_button_parse_shortcut(updated);
                self->wrapper.programmable_button_shortcut_map->insert_or_assign(updated.id.value(), shortcut);
                gtk_shortcut_controller_add_shortcut(self->programmable_button_shortcut_controller, shortcut);
            }
        }
        for(auto& deleted : change.deleted)
        {
            auto shortcut_map_node = self->wrapper.programmable_button_shortcut_map->extract(deleted);
            if(shortcut_map_node)
            {
                GtkShortcut* shortcut = GTK_SHORTCUT(shortcut_map_node.mapped().get());

                gtk_shortcut_controller_remove_shortcut(self->programmable_button_shortcut_controller, shortcut);
            }
        }
    });
}

// profile_id and window_id of main window are not set here yet
static void
main_window_init(MainWindow* win)
{
    std::construct_at(&win->wrapper);
    win->wrapper.tab_view_map = new std::map<orion::WindowId, OrionWebviewTabView*>();
    win->wrapper.programmable_button_shortcut_map = new std::map<orion::ProgrammableButtonId, orion::gtk::StrongRefWrapper>();

    orion::gtk::main_window::register_actions(win);


    GActionMap* action_map = G_ACTION_MAP(win);

    //when touching any even seemingly unrelated expression usages in this function, please make sure to check whether ref counting is not messed up 
    GtkExpression* tab_view_exp = gtk_property_expression_new_for_pspec(NULL, obj_properties[PROP_TAB_VIEW]);
    defer([tab_view_exp]{ gtk_expression_unref(tab_view_exp); });
    GtkExpression* can_go_back_exp = gtk_property_expression_new(ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_exp), "can-go-back");
    GtkExpression* can_go_forward_exp = gtk_property_expression_new(ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_exp), "can-go-forward");
    GtkExpression* reader_mode_enabled_exp = gtk_property_expression_new(ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_exp), "reader-mode-enabled");

    GAction* action = g_action_map_lookup_action(action_map, "back");
    gtk_expression_bind(can_go_back_exp, action, "enabled", win);
    
    action = g_action_map_lookup_action(action_map, "forward");
    gtk_expression_bind(can_go_forward_exp, action, "enabled", win);

    action = g_action_map_lookup_action(action_map, "exit-reader-mode");
    gtk_expression_bind(gtk_expression_ref(reader_mode_enabled_exp), action, "enabled", win);

    auto state_handler = [](gpointer data){
        MainWindow* win = MAIN_WINDOW(data);

        GValue val = G_VALUE_INIT;
        g_value_init(&val, G_TYPE_BOOLEAN);
        if(!gtk_expression_watch_evaluate(win->reader_mode_enabled_watch, &val))
        {
            return;
        }

        orion::gtk::set_boolean_action_state(G_ACTION_MAP(data), "toggle-reader", g_value_get_boolean(&val));
        g_value_unset(&val);
    };

    win->reader_mode_enabled_watch = gtk_expression_watch(reader_mode_enabled_exp, win, state_handler, win, NULL);
    gtk_expression_unref(reader_mode_enabled_exp);
    // do all the setup of children widgets in constructed() to be able to use profile_id and window_id easily
}

static void
main_window_class_init(MainWindowClass* klass)
{
    GTK_WINDOW_CLASS(klass)->close_request = main_window_close_request;
    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = main_window_dispose;
    object_class->finalize = main_window_finalize;
    object_class->constructed = main_window_constructed;

    
    object_class->set_property = main_window_set_property;
    object_class->get_property = main_window_get_property;

    obj_properties[PROP_TAB_VIEW] = g_param_spec_object(
        "tab-view",
        NULL,
        NULL,
        orion_webview_tab_view_get_type(),
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS)
    );
    obj_properties[PROP_NEW_FROM_BACKEND_CB] = g_param_spec_pointer( //expects to take ownership of a raw pointer
        "done-cb",
        NULL,
        NULL,
        GParamFlags(G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS)
    );
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    obj_properties[PROP_READER_TEMPLATE_SCRIPT] = g_param_spec_boxed(
        "reader-template-script",
        NULL,
        NULL,
        WEBKIT_TYPE_USER_SCRIPT,
        GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS)
    );
#endif

    gpointer iface = g_type_interface_peek(object_class, ORION_TYPE_WINDOW);

    obj_properties[PROP_WINDOW_ID] = g_param_spec_override(
        "window-id", g_object_interface_find_property(iface, "window-id"));
    obj_properties[PROP_PROFILE_ID] = g_param_spec_override(
        "profile-id", g_object_interface_find_property(iface, "profile-id"));
    obj_properties[PROP_PROFILE_DATA_HOLDER] = g_param_spec_override(
        "profile-data-holder", g_object_interface_find_property(iface, "profile-data-holder"));
    obj_properties[PROP_PRIVATE_MODE] = g_param_spec_override(
        "private-mode", g_object_interface_find_property(iface, "private-mode"));
    obj_properties[PROP_WEB_VIEW_FACTORY] = g_param_spec_override(
        "web-view-factory", g_object_interface_find_property(iface, "web-view-factory"));
    obj_properties[PROP_URI] = g_param_spec_override(
        "uri", g_object_interface_find_property(iface, "uri"));
    obj_properties[PROP_TLS_STATUS] = g_param_spec_override(
        "tls-status", g_object_interface_find_property(iface, "tls-status"));
    obj_properties[PROP_BACK_LIST] = g_param_spec_override(
        "back-list", g_object_interface_find_property(iface, "back-list"));
    obj_properties[PROP_FORWARD_LIST] = g_param_spec_override(
        "forward-list", g_object_interface_find_property(iface, "forward-list"));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);


    signals[SIG_PRIMARY_PRESSED] = g_signal_new(
        "primary-pressed", MAIN_WINDOW_TYPE, GSignalFlags(0), 0, NULL, NULL,
        g_cclosure_marshal_generic, G_TYPE_NONE, 2, G_TYPE_DOUBLE, G_TYPE_DOUBLE
    );

    
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/main_window.ui");

	GtkCssProvider* provider = gtk_css_provider_new();

	// Load system-wide CSS file (applies to all windows, dialogs, popovers, etc.)
	gtk_css_provider_load_from_resource(provider, GRESOURCE_PREFIX "/data/themes.css");
	GdkDisplay* display = gdk_display_get_default();
	gtk_style_context_add_provider_for_display(display, GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);

    gtk_widget_class_bind_template_child(widget_class, MainWindow, header_bar);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, bookmarks_bar);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, find_bar);
	gtk_widget_class_bind_template_child(widget_class, MainWindow, page_content_box);
	gtk_widget_class_bind_template_child(widget_class, MainWindow, tab_overview);
	// TODO: bind right-click context menu dynamically to Previous and Next page toolbar buttons
    gtk_widget_class_bind_template_child(widget_class, MainWindow, overlay_split_view);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, sidebar);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, main_ui_revealer);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, link_overlay_revealer);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, link_label);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, page_tabview_box);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, main_box);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, tab_bar);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, drop_target);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, tab_switcher);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, top_bar_overlay);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, programmable_button_shortcut_controller);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, erase_elements_bar);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, main_toolbar_view);
    gtk_widget_class_bind_template_child(widget_class, MainWindow, js_sidebar_split_view);

    gtk_widget_class_bind_template_callback(widget_class, on_create_tab);
    gtk_widget_class_bind_template_callback(widget_class, main_window_realize);
    gtk_widget_class_bind_template_callback(widget_class, main_window_unrealize);
    gtk_widget_class_bind_template_callback(widget_class, get_window_id_label);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_drop_controller_motion_enter);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_drop_controller_motion_leave);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_drop_target_accept);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_key_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_key_released);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_notify_is_active);
    gtk_widget_class_bind_template_callback(widget_class, on_main_window_primary_pressed);
    gtk_widget_class_bind_template_callback(widget_class, tab_switcher_overlay_get_child_position);
    gtk_widget_class_bind_template_callback(widget_class, on_mouse_motion);
}
