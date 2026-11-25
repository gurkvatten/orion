#define ENABLE_VERBOSE_LOGS 0

#include "sidebar.h"

#include <vector>
#include <map>
#include <string>
#include <memory>
#include <libintl.h>

#include <common/src/log.h>
#include <core/settings.h>
#include <common/src/defer.h>
#include <common/src/uri.h>
#include <common/src/string_util.h>

#include "sidebar_bookmarks_columnview.h"
#include "sidebar_current_window_tabs_view.h"
#include "sidebar_reading_list_list_view.h"
#include "sidebar_windows_list_view.h"
#include "window/window_list_popover.h"
#include "config/default_config.h"
#include "util/click_util.h"
#include "util/text_util.h"
#include "util/ref_util.h"
#include "util/menu_util.h"
#include "util/action_util.h"
#include "util/tab_context_menu.h"
#include "util/window_observer_util.h"
#include "window/main_window/main_window.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_view.h"
#include "window/orion_webview_tabview_widgets/orion_webview_tab_content.h"
#include "data_item/window_item.h"
#include "window/columnview/orion_window_selection_model.h"
#include "reading_list_toggle_group.h"

namespace { // always wrap CppWrapper struct into anonymous namespace to avoid nasty ODR violations

struct CppWrapper {
    orion::ObserverId show_nested_tabs_observer;
    orion::ObserverId show_favicons_inside_tabs_observer;
    orion::ObserverId sidebar_page_observer;
    orion::ObserverId tab_layout_observer;
};

}

struct _Sidebar {
    GtkBox parent;

    GtkButton* sidebar_button;
    GtkMenuButton* sidebar_menu_button;

    SidebarButtonClickedCallback sidebar_button_clicked_cb;
    gpointer sidebar_button_clicked_cb_user_data;

    GtkButton* new_tab_button;
    GtkPopoverMenu* new_tab_button_popover_menu;

    GtkStack* page_stack;

    GtkBox* windows_page;
    GtkBox* tabs_page;
    GtkBox* bookmarks_page;
    GtkBox* reading_list_page;

    GtkEntry* bookmarks_entry;
    GtkEntry* reading_list_entry;

    GtkColumnView* bookmarks_columnview;
    GtkPopover* window_list_popover;
    GtkListView* tabs_list_view;
    GtkGridView* pinned_tabs_grid_view;
    GtkLabel* tabs_label;
    GtkListBox* windows_list_box;
    GtkListView* reading_list_list_view;
    GtkListView* windows_list_view;
    GtkLabel* sidebar_label;

    GtkPopoverMenu* windows_row_popover;
    GtkPopoverMenu* tabs_row_popover;
    GtkPopoverMenu* bookmarks_folder_popover_menu;
    GtkPopoverMenu* bookmarks_internal_folder_popover_menu;
    GtkPopoverMenu* bookmarks_bookmark_popover_menu;
    GtkPopoverMenu* bookmarks_column_view_popover_menu;
    GtkPopoverMenu* tabs_list_view_popover_menu;
    ReadingListToggleGroup* reading_list_toggle_group;

    GtkMenuButton* current_window_menu_button;
    GtkSelectionModel* pinned_tabs_selection;
    GtkSelectionModel* unpinned_tabs_selection;
    gboolean show_favicons_inside_tabs;

    CppWrapper cpp_wrapper;
};

G_DEFINE_TYPE(Sidebar, sidebar, GTK_TYPE_BOX)

static void
update_toolbar_item_label_visibility(Sidebar* self)
{
    MainWindow* win = orion::gtk::get_root_main_window((GTK_WIDGET(self)));
    if (!win) {
        orion::log_debug("update_toolbar_item_label_visibility: could not find root main window");
        return;
    }

    std::string toolbar_item_style_str = orion::gtk::get_string_action_state(G_ACTION_MAP(win), "toolbar-item-style");

    int64_t toolbar_item_style_num = 0;
    VERIFY_LOG(orion::str::try_parse_int64(toolbar_item_style_str, &toolbar_item_style_num));
    orion::settings::ToolbarItemStyle toolbar_item_style = orion::settings::ToolbarItemStyle(toolbar_item_style_num);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [profile_id,self_weak,toolbar_item_style](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);

        bool is_compact_tab_layout = (reply.value == orion::settings::TabLayout::compact);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        Sidebar* self = APP_SIDEBAR(self_strong.get());

        gboolean show_label = !is_compact_tab_layout && 
            toolbar_item_style == orion::settings::ToolbarItemStyle::icon_and_text;
        gtk_widget_set_visible(GTK_WIDGET(self->sidebar_label), show_label);
    });
}

static void
update_toolbar_item_label_visibility_cb(GAction* action, GParamSpec* pspec, gpointer user_data)
{
    Sidebar* self = APP_SIDEBAR(user_data);
    update_toolbar_item_label_visibility(self);
}

void
sidebar_set_sidebar_button_clicked_callback(Sidebar* self, SidebarButtonClickedCallback cb, gpointer user_data)
{
    self->sidebar_button_clicked_cb = cb;
    self->sidebar_button_clicked_cb_user_data = user_data;
}

static void
on_sidebar_button_clicked(GtkButton* button, gpointer user_data)
{
    Sidebar* self = APP_SIDEBAR(user_data);
    if (self->sidebar_button_clicked_cb) {
        self->sidebar_button_clicked_cb(self->sidebar_button_clicked_cb_user_data);
    }
}

orion::settings::SidebarPage
sidebar_page_name_to_page_type(std::string page_name)
{
    if (page_name == "windows") {
        return orion::settings::SidebarPage::windows;
    } else if (page_name == "tabs") {
        return orion::settings::SidebarPage::tabs;
    } else if (page_name == "bookmarks") {
        return orion::settings::SidebarPage::bookmarks;
    } else if (page_name == "reading_list") {
        return orion::settings::SidebarPage::reading_list;
    } else {
        orion::log_warning("unknown page name: %s", page_name);
        return orion::settings::SidebarPage::bookmarks;
    }
}

static std::string
sidebar_page_type_to_page_name(orion::settings::SidebarPage page)
{
    switch (page) {
        case orion::settings::SidebarPage::windows:
            return "windows";
        case orion::settings::SidebarPage::tabs:
            return "tabs";
        case orion::settings::SidebarPage::bookmarks:
            return "bookmarks";
        case orion::settings::SidebarPage::reading_list:
            return "reading_list";
        default:
            orion::log_warning("unknown page type: %d", static_cast<int>(page));
            return "bookmarks";
    }
}

std::string
sidebar_page_type_to_toggle_action(orion::settings::SidebarPage page)
{
    switch (page) {
        case orion::settings::SidebarPage::windows:
            return "toggle-windows-sidebar";
        case orion::settings::SidebarPage::tabs:
            return "toggle-vertical-tabs-sidebar";
        case orion::settings::SidebarPage::bookmarks:
            return "toggle-bookmarks-sidebar";
        case orion::settings::SidebarPage::reading_list:
            return "toggle-reading-list-sidebar";
        default:
            orion::log_warning("unknown page type: %d", static_cast<int>(page));
            return "toggle-bookmarks-sidebar";
    }
}

void
sidebar_switch_to_page(Sidebar* self, const char* page_name)
{
    g_return_if_fail(APP_IS_SIDEBAR(self));
    gtk_stack_set_visible_child_name(self->page_stack, page_name);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    orion::gtk::set_string_action_state(G_ACTION_MAP(win), "sidebar-type", page_name);

    orion::settings::SidebarPage page = sidebar_page_name_to_page_type(page_name);
    
    global_core_api->set_setting(profile_id, orion::settings::appearance::sidebar_page, page,
        [self,page_name](orion::SetSettingReply<orion::settings::SidebarPage> reply){
            VERIFY_LOG(reply.err);
        });
}

const char*
sidebar_get_visible_page(Sidebar* self)
{
    g_return_val_if_fail(APP_IS_SIDEBAR(self), NULL);

    return gtk_stack_get_visible_child_name(self->page_stack);
}

static void
on_tabs_menu_button_clicked(GtkButton* button, gpointer user_data)
{
    AdwDialog* dialog = window_list_popover_new();
    adw_dialog_present(dialog, GTK_WIDGET(button));
}

void
sidebar_setup_tabs_lists(Sidebar* self, OrionWebviewTabView* tab_view)
{
    sidebar_tabs_list_view_setup_tabs_lists(self, tab_view);
}

void
sidebar_popdown_all_popovers(Sidebar* self)
{
    if (self->window_list_popover) {
        gtk_popover_popdown(self->window_list_popover);
    }
}

GtkColumnView*
sidebar_get_bookmarks_column_view(Sidebar* self)
{
    return self->bookmarks_columnview;
}

GtkPopoverMenu*
sidebar_get_bookmarks_internal_folder_popover_menu(Sidebar* self)
{
    return self->bookmarks_internal_folder_popover_menu;
}

void
sidebar_set_bookmarks_internal_folder_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu)
{
    self->bookmarks_internal_folder_popover_menu = popover_menu;
}


GtkPopoverMenu*
sidebar_get_bookmarks_folder_popover_menu(Sidebar* self)
{
    return self->bookmarks_folder_popover_menu;
}

void
sidebar_set_bookmarks_folder_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu)
{
    self->bookmarks_folder_popover_menu = popover_menu;
}

GtkPopoverMenu*
sidebar_get_bookmarks_bookmark_popover_menu(Sidebar* self)
{
    return self->bookmarks_bookmark_popover_menu;
}

GtkPopoverMenu*
sidebar_get_bookmarks_column_view_popover_menu(Sidebar* self)
{   
    return self->bookmarks_column_view_popover_menu;
}

void
sidebar_set_bookmarks_bookmark_popover_menu(Sidebar* self, GtkPopoverMenu* popover_menu)
{
    self->bookmarks_bookmark_popover_menu = popover_menu;
}

GtkMenuButton*
sidebar_get_current_window_menu_button(Sidebar* self)
{
    return self->current_window_menu_button;
}

GtkLabel*
sidebar_get_tabs_label(Sidebar* self)
{
    return self->tabs_label;
}

GtkSelectionModel*
sidebar_get_pinned_tabs_selection(Sidebar* self)
{
    return self->pinned_tabs_selection;
}

void
sidebar_set_pinned_tabs_selection(Sidebar* self, GtkSelectionModel* selection)
{
    self->pinned_tabs_selection = selection;
}

GtkSelectionModel*
sidebar_get_unpinned_tabs_selection(Sidebar* self)
{
    return self->unpinned_tabs_selection;
}

void
sidebar_set_unpinned_tabs_selection(Sidebar* self, GtkSelectionModel* selection)
{
    self->unpinned_tabs_selection = selection;
}

GtkPopoverMenu*
sidebar_get_tabs_list_view_popover_menu(Sidebar* self)
{
    return self->tabs_list_view_popover_menu;
}

GtkPopoverMenu*
sidebar_get_new_tab_button_popover_menu(Sidebar* self)
{
    return self->new_tab_button_popover_menu;
}

GtkListView*
sidebar_get_tabs_list_view(Sidebar* self)
{
    return self->tabs_list_view;
}

GtkGridView*
sidebar_get_pinned_tabs_grid_view(Sidebar* self)
{
    return self->pinned_tabs_grid_view;
}

GtkListView*
sidebar_get_unpinned_tabs_list_view(Sidebar* self)
{
    return self->tabs_list_view;
}

static void on_tabs_window_selection_list_view_map(GtkListView* windows_list_view, gpointer user_data);

static void
on_main_window_notify_window_id(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    GtkListView* windows_list_view = GTK_LIST_VIEW(user_data);
    on_tabs_window_selection_list_view_map(windows_list_view, NULL);
}

static void
on_tabs_window_selection_list_view_unmap(GtkListView* windows_list_view, gpointer user_data)
{
    g_signal_handlers_disconnect_by_data(windows_list_view, user_data);
    GtkRoot* win = gtk_widget_get_root(GTK_WIDGET(windows_list_view));
    g_signal_handlers_disconnect_matched(win, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_main_window_notify_window_id, NULL);
}

static void
on_tabs_window_window_selection_model_window_selected(OrionWindowSelectionModel* model, guint pos, gpointer user_data)
{
    orion::log_verbose("sidebar: on_tabs_window_window_selection_model_window_selected: pos=%u", pos);
    WindowItem* item = WINDOW_ITEM(g_list_model_get_item(G_LIST_MODEL(model), pos));
    defer([item]{ g_object_unref(item); });
    MainWindow* current_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(user_data)));
    orion::WindowId window_id = window_item_get_id(item);

    // window switch from Sidebar->Tabs is always in-place switch
    main_window_switch_to_window_id_in_place(current_win, window_id);
}

static void
tabs_window_selection_list_view_reload(GtkListView* view, orion::ProfileId profile_id, orion::WindowId this_window_id)
{
    orion::gtk::WeakRefWrapper view_weak(view);

    global_core_api->get_window_ids({.profile_id=profile_id},
        [this_window_id,view_weak](orion::GetWindowIdsReply reply){

        VERIFY_LOG(reply.err);
        global_core_api->get_windows(reply.ids, [this_window_id,view_weak](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            auto view_strong = view_weak.lock();
            if (!view_strong) return;
            GtkListView* view = GTK_LIST_VIEW(view_strong.get());
            
            GListStore* store = g_list_store_new(G_TYPE_OBJECT);
            OrionWindowSelectionModel* selection = orion_window_selection_model_new(G_LIST_MODEL(store));
            size_t pos = 0;
            for(size_t i = 0; i<reply.windows.size(); i++)
            {
                WindowItem* item = window_item_new(reply.windows.at(i));
                defer([item]{ g_object_unref(item); });
                g_list_store_append(store, item);
                if(reply.windows.at(i).id == this_window_id) pos = i;
            }
            orion_window_selection_model_actual_select_item(selection, pos);
            
            g_signal_connect(selection, "window-selected", G_CALLBACK(on_tabs_window_window_selection_model_window_selected), view);
            gtk_list_view_set_model(view, GTK_SELECTION_MODEL(selection));
        });
    });
}

static void
on_tabs_window_selection_list_view_map(GtkListView* view, gpointer user_data)
{
    orion::log_verbose("sidebar: on_tabs_window_selection_list_view_map");
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
    orion::WindowId this_window_id = main_window_get_window_id(win);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    tabs_window_selection_list_view_reload(view, profile_id, this_window_id);


    orion::ObserverId* old_observer_id = static_cast<orion::ObserverId*>(g_object_get_data(G_OBJECT(view), "windows-change-observer"));
    if(old_observer_id)
    {
        global_core_api->windows_change().remove_observer(*old_observer_id);
        delete old_observer_id;
    }

    orion::gtk::WeakRefWrapper view_weak(view);
    orion::ObserverId* observer_id = new orion::ObserverId;
    *observer_id = global_core_api->windows_change().add_observer([view_weak](const orion::WindowsChange& change){
        auto view_strong = view_weak.lock();
        if (!view_strong) return;
        GtkListView* view = GTK_LIST_VIEW(view_strong.get());

        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
        orion::ProfileId profile_id = main_window_get_current_profile_id(win);
        orion::WindowId this_window_id = main_window_get_window_id(win);
        
        tabs_window_selection_list_view_reload(view, profile_id, this_window_id);
    });
    g_object_set_data(G_OBJECT(view), "windows-change-observer", observer_id);

    g_signal_handlers_disconnect_by_func(win, (void*)on_main_window_notify_window_id, view);
    g_signal_connect_object(win, "notify::window-id", G_CALLBACK(on_main_window_notify_window_id), view, G_CONNECT_AFTER);
    g_signal_connect_data(view, "unmap", G_CALLBACK(on_tabs_window_selection_list_view_unmap), view,
        [](gpointer data, GClosure* closure){
            orion::ObserverId* id = static_cast<orion::ObserverId*>(g_object_get_data(G_OBJECT(data), "windows-change-observer"));
            if(id)
            {
                g_object_set_data(G_OBJECT(data), "windows-change-observer", NULL);
                global_core_api->windows_change().remove_observer(*id);
                delete id;
            }
        }, G_CONNECT_DEFAULT);
}

static void
setup_context_menus(Sidebar* self)
{
	GtkBuilder* builder = gtk_builder_new_from_resource(GRESOURCE_PREFIX "/src/window/main_window/sidebar/context_menus.ui");

	orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_folder_popover_menu, "bookmarks_dialog_bookmark_folder_context_menu");
    orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_internal_folder_popover_menu, "bookmarks_dialog_bookmark_internal_folder_context_menu");
    orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_bookmark_popover_menu, "bookmarks_dialog_bookmark_context_menu");
    orion::gtk::setup_context_menu(builder, GTK_WIDGET(self), &self->bookmarks_column_view_popover_menu, "bookmarks_dialog_bookmarks_column_view_context_menu");

	g_object_unref(builder);
}

static void
sidebar_finalize(GObject* gobject)
{
    Sidebar* self = APP_SIDEBAR(gobject);
    G_OBJECT_CLASS(sidebar_parent_class)->finalize(gobject);

    std::destroy_at(&self->cpp_wrapper);
}

void
sidebar_deactivate_other_page_toggles(Sidebar* self, const char* action_name)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;

    const char* all_actions[] = {
        "toggle-windows-sidebar", 
        "toggle-vertical-tabs-sidebar",
        "toggle-bookmarks-sidebar",
        "toggle-reading-list-sidebar",
    };

    // Disable all other page toggles
    for (const char* action : all_actions) {
        if (strcmp(action, action_name) != 0) {
            orion::gtk::set_boolean_action_state(G_ACTION_MAP(win), action, FALSE);
        }
    }
}

static void
sidebar_setup_settings(Sidebar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    {
        auto handler = [win_weak](bool new_value){
            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
            g_action_change_state(action, g_variant_new_boolean(new_value));
        };

        global_core_api->get_setting(profile_id, orion::settings::tabs::show_nested_vertical_tabs,
            [handler](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                handler(reply.value);
            });

        self->cpp_wrapper.show_nested_tabs_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::tabs::show_nested_vertical_tabs,
            [handler,profile_id](orion::SingleSettingChange<bool> change){
                if (change.profile_id != profile_id) return;
                handler(change.new_value);
            });
    }
    {
        auto handler = [win_weak](bool new_value){
            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "show-favicons-inside-tabs");
            g_action_change_state(action, g_variant_new_boolean(new_value));
        };

        global_core_api->get_setting(profile_id, orion::settings::tabs::show_favicons_inside_tabs,
            [handler](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                handler(reply.value);
            });

        self->cpp_wrapper.show_favicons_inside_tabs_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::tabs::show_favicons_inside_tabs,
            [handler,profile_id](orion::SingleSettingChange<bool> change){
                if (change.profile_id != profile_id) return;
                handler(change.new_value);
            });
    }
    {
        global_core_api->get_setting(profile_id, orion::settings::appearance::sidebar_page,
            [self_weak](orion::GetSettingReply<orion::settings::SidebarPage> reply){
                VERIFY_LOG(reply.err);
                auto self_strong = self_weak.lock();
                if(!self_strong) return;
                Sidebar* self = APP_SIDEBAR(self_strong.get());
                
                std::string page_name = sidebar_page_type_to_page_name(reply.value);
                gtk_widget_activate_action(GTK_WIDGET(self), "win.sidebar-type", "s", page_name.c_str());

                MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
                if (!win) return;

                orion::gtk::set_string_action_state(G_ACTION_MAP(win), "sidebar-type", page_name.c_str());
                std::string toggle_action = sidebar_page_type_to_toggle_action(reply.value);

                sidebar_deactivate_other_page_toggles(self, toggle_action.c_str());
                orion::gtk::set_boolean_action_state(G_ACTION_MAP(win), toggle_action.c_str(), TRUE);
            }
        );

        self->cpp_wrapper.sidebar_page_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::appearance::sidebar_page,
            [self_weak,profile_id](orion::SingleSettingChange<orion::settings::SidebarPage> change){
                if (change.profile_id != profile_id) return;
                auto self_strong = self_weak.lock();
                if (!self_strong) return;
                Sidebar* self = APP_SIDEBAR(self_strong.get());
                MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
                if (!win) return;

                std::string page_name = sidebar_page_type_to_page_name(change.new_value);
                orion::gtk::set_string_action_state(G_ACTION_MAP(win), "sidebar-type", page_name.c_str());
                std::string toggle_action = sidebar_page_type_to_toggle_action(change.new_value);

                sidebar_deactivate_other_page_toggles(self, toggle_action.c_str());
                orion::gtk::set_boolean_action_state(G_ACTION_MAP(win), toggle_action.c_str(), TRUE);
            }
        );

        self->cpp_wrapper.tab_layout_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::tabs::tab_layout,
        [self_weak,profile_id](orion::SingleSettingChange<orion::settings::TabLayout> change){
            if (change.profile_id != profile_id) return;

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            Sidebar* self = APP_SIDEBAR(self_strong.get());
            
            update_toolbar_item_label_visibility(self);
        });

        update_toolbar_item_label_visibility(self);
    }
}

static void
sidebar_realize(Sidebar* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    g_signal_connect_data(self->new_tab_button, "clicked", G_CALLBACK(on_new_tab_button_clicked), win, NULL, G_CONNECT_SWAPPED);

    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    auto sidebar_weak_ref = orion::gtk::WeakRefWrapper(self);

    sidebar_setup_settings(self);
}

static void
sidebar_unrealize(Sidebar* self, gpointer user_data)
{
    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.show_nested_tabs_observer);
    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.show_favicons_inside_tabs_observer);
    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.sidebar_page_observer);
    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.tab_layout_observer);
}

static void
on_reading_list_entry_changed(GtkEditable *editable, gpointer user_data)
{
    Sidebar* sidebar = APP_SIDEBAR(user_data);

    const char *text = gtk_editable_get_text(editable);
    std::string text_str(text);
    orion::log_debug("on_bookmarks_entry_changed: text: '%s'", text);

    bool show_read = get_sidebar_readinglist_show_read(sidebar);
    if (text_str.length() > 0)
    {
        reading_list_list_view_refetch_data(sidebar->reading_list_list_view, text_str, show_read);
    }else
    {
        reading_list_list_view_refetch_data(sidebar->reading_list_list_view, "", show_read);
    }
}

static void
on_bookmarks_entry_changed(GtkEditable *editable, gpointer user_data)
{
    Sidebar* sidebar = APP_SIDEBAR(user_data);

    const char *text = gtk_editable_get_text(editable);
    std::string text_str(text);
    orion::log_debug("on_bookmarks_entry_changed: text: '%s'", text);
    if (text_str.length() > 0)
    {
        sidebar_bookmarks_column_view_map_flat_query(sidebar->bookmarks_columnview, sidebar, std::move(text_str));
    }else
    {
        sidebar_bookmarks_column_view_map(sidebar->bookmarks_columnview, sidebar);
    }
}
/*
static void
callback(GtkColumnView* bookmarks_columnview, gpointer user_data){
    sidebar_bookmarks_column_view_map_flat_query(bookmarks_columnview, NULL, "");
}
*/

static void
on_toggle_group_active_changed(AdwToggleGroup *group, GParamSpec *pspec, Sidebar *self)
{
    guint index = adw_toggle_group_get_active(group);
    if (index == GTK_INVALID_LIST_POSITION) {
        orion::log_debug("No toggle selected");
        return;
    }
    switch (index)
    {
    case 0://All
        reading_list_list_view_refetch_data(self->reading_list_list_view, get_sidebar_readinglist_query(self), true);
        break;
    case 1://Unread
        reading_list_list_view_refetch_data(self->reading_list_list_view, get_sidebar_readinglist_query(self), false);
        break;
    default:
        break;
    }
}

bool
get_sidebar_readinglist_show_read(Sidebar* self){
    guint index = adw_toggle_group_get_active(ADW_TOGGLE_GROUP(reading_list_toggle_group_get_unread_toggle_group(self->reading_list_toggle_group)));
    if (index == GTK_INVALID_LIST_POSITION) {
        orion::log_debug("No toggle selected");
        return true;// What can we do D:
    }

    switch (index)
    {
    case 0://All
        return true;
        break;
    case 1://Unread
        return false;
        break;
    default:
        return true;// What can we do D:
        break;
    }
}

std::string
get_sidebar_readinglist_query(Sidebar* self){
    const char *text = gtk_editable_get_text(GTK_EDITABLE(self->reading_list_entry));
    return std::string(text);
}

bool
sidebar_get_show_favicons_inside_tabs(Sidebar* self)
{
    return self->show_favicons_inside_tabs;
}

void
sidebar_set_mini_toolbar_enabled(Sidebar* self, bool enabled)
{
    auto add_or_remove_func = enabled
        ? gtk_widget_add_css_class
        : gtk_widget_remove_css_class;

    add_or_remove_func(GTK_WIDGET(self->sidebar_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->sidebar_menu_button), "mini-toolbar-menu-button");
}

static void
sidebar_init(Sidebar* self)
{
    std::construct_at(&self->cpp_wrapper);

    g_type_ensure(window_item_get_type());
    g_type_ensure(ORION_WEBVIEW_TAB_CONTENT_TYPE);
    gtk_widget_init_template(GTK_WIDGET(self));

    // select the first row
    //gtk_list_box_select_row(self->windows_list_box, gtk_list_box_get_row_at_index(self->windows_list_box, 0));
    
    //gtk_column_view_set_header_factory(self->bookmarks_columnview, NULL);

    g_signal_connect(self->bookmarks_columnview, "realize", G_CALLBACK(sidebar_bookmarks_column_view_realize), self);
    g_signal_connect(self->bookmarks_columnview, "map", G_CALLBACK(sidebar_bookmarks_column_view_map), self);

    g_signal_connect(self->bookmarks_entry, "changed", G_CALLBACK(on_bookmarks_entry_changed), self);
    g_signal_connect(self->reading_list_entry, "changed", G_CALLBACK(on_reading_list_entry_changed), self);

    g_signal_connect(self->reading_list_list_view, "map", G_CALLBACK(on_reading_list_list_view_map), self);
    g_signal_connect(reading_list_toggle_group_get_unread_toggle_group(self->reading_list_toggle_group),
                    "notify::active",
                    G_CALLBACK(on_toggle_group_active_changed),
                    self);

    g_signal_connect(self->windows_list_view, "map", G_CALLBACK(on_tabs_window_selection_list_view_map), NULL);

    g_signal_connect(self, "realize", G_CALLBACK(sidebar_realize), NULL);
    g_signal_connect(self, "unrealize", G_CALLBACK(sidebar_unrealize), NULL);

    g_signal_connect(self->tabs_list_view, "map", G_CALLBACK(sidebar_tabs_list_view_map), NULL);


    setup_context_menus(self);
}

static float
bool_to_float(GtkListItem* item, gboolean val)
{
    return val ? 1.0 : 0.0;
}

static int
indent_to_margin(GtkListItem* item, int indent)
{
    return 16*indent;
}

//debug functions
/*
static char*
bool_to_string(GtkListItem* item, gboolean val)
{
    return g_strdup_printf("%i", val);
}
*/
static char*
double_to_string(GtkListItem* item, double val)
{
    return g_strdup_printf("%f", val);
}
static char*
int64_to_string(GtkListItem* item, int64_t val)
{
    return g_strdup_printf("%" PRId64, val);
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

static char*
id_to_string(GtkListItem* item, int64_t id)
{
    return g_strdup_printf("%ld", id);
}

static char*
seq_num_to_string(GtkListItem* item, double val)
{
    return g_strdup_printf("%lf", val);
}

static void
sidebar_root(GtkWidget* self)
{
    GTK_WIDGET_CLASS(sidebar_parent_class)->root(self); // always call up

    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_debug("sidebar: could not find root main window");
        return;
    }

    GAction* toolbar_item_style_action = g_action_map_lookup_action(G_ACTION_MAP(win), "toolbar-item-style");
    g_signal_connect(toolbar_item_style_action, "notify::state", G_CALLBACK(update_toolbar_item_label_visibility_cb), self);
}

static void
sidebar_class_init(SidebarClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = sidebar_finalize;
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->root = sidebar_root;
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/sidebar/sidebar.ui");
    gtk_widget_class_bind_template_callback(widget_class, on_sidebar_button_clicked);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, tabs_label);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, windows_page);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, tabs_page);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, bookmarks_page);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, reading_list_page);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, bookmarks_columnview);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, tabs_list_view);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, pinned_tabs_grid_view);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, reading_list_list_view);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, new_tab_button);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, page_stack);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, reading_list_toggle_group);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, windows_list_view);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, tabs_list_view_popover_menu);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, bookmarks_entry);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, reading_list_entry);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, new_tab_button_popover_menu);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, current_window_menu_button);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, sidebar_button);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, sidebar_menu_button);
    gtk_widget_class_bind_template_child(widget_class, Sidebar, sidebar_label);

    gtk_widget_class_bind_template_callback(widget_class, on_sidebar_bookmarks_column_view_activate);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_row_secondary_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_row_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_row_middle_released);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_expander_notify_expanded);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_expander_notify_root);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_favicon_notify_root);
    gtk_widget_class_bind_template_callback(widget_class, get_tab_expander_expanded);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_title_notify_root);
    gtk_widget_class_bind_template_callback(widget_class, get_tab_icon_visible);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_menu_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_close_page_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, is_audio_indicator_visible);
    gtk_widget_class_bind_template_callback(widget_class, audio_indicator_icon_name);
    gtk_widget_class_bind_template_callback(widget_class, audio_indicator_tooltip);
    gtk_widget_class_bind_template_callback(widget_class, on_sidebar_tab_audio_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_prepare);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_drag_begin);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drag_source_drag_end);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tab_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_prepare);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_drag_begin);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drag_source_drag_end);
    gtk_widget_class_bind_template_callback(widget_class, on_unpinned_tab_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drop_target_motion);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_drop_target_drop);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_drop_target_motion);
    gtk_widget_class_bind_template_callback(widget_class, on_tabs_list_view_drop_target_leave);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_drop_target_leave);
    gtk_widget_class_bind_template_callback(widget_class, load_image_from_reading_list_item);
    gtk_widget_class_bind_template_callback(widget_class, get_reading_list_title_from_item);
    gtk_widget_class_bind_template_callback(widget_class, get_reading_list_address_from_item);
    gtk_widget_class_bind_template_callback(widget_class, get_reading_list_description_from_item);
    gtk_widget_class_bind_template_callback(widget_class, on_reading_list_list_view_map);
    gtk_widget_class_bind_template_callback(widget_class, on_reading_list_list_view_row_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_reading_list_list_view_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_new_window_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_new_tab_button_secondary_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_new_tab_button_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_pinned_tabs_list_view_item_secondary_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_event_controller_motion_enter);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_state_flags_changed);
    gtk_widget_class_bind_template_callback(widget_class, bool_to_float);
    gtk_widget_class_bind_template_callback(widget_class, indent_to_margin);
    //gtk_widget_class_bind_template_callback(widget_class, bool_to_string);
    gtk_widget_class_bind_template_callback(widget_class, double_to_string);
    gtk_widget_class_bind_template_callback(widget_class, int64_to_string);
    gtk_widget_class_bind_template_callback(widget_class, invert_bool);
    gtk_widget_class_bind_template_callback(widget_class, get_uri_authority);
    gtk_widget_class_bind_template_callback(widget_class, window_item_factory_setup);
    gtk_widget_class_bind_template_callback(widget_class, window_item_factory_bind);
    gtk_widget_class_bind_template_callback(widget_class, bind_object_with_label_to_window);
    gtk_widget_class_bind_template_callback(widget_class, id_to_string);
    gtk_widget_class_bind_template_callback(widget_class, seq_num_to_string);
}
