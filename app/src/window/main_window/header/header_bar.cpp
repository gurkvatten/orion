#include <string>

#include <common/src/log.h>
#include <common/src/string_util.h>
#include <common/src/defer.h>
#include <core/settings.h>
#include <core/core_api.h>
#include <core/webextensions/extension.h>
#include <core/toolbar_item_util.h>

#include "header_bar.h"
#include "window/main_window/main_window.h"
#include "window/main_window/header/bookmarks_bar.h"
#include "window/reader_mode_settings_popover.h"
#include "window/downloads_dialog.h"
#include "window/extension_popover.h"
#include "window/scaled_container/max_width_container.h"
#include "window/main_window/menu/toolbar_menu.h"
#include "window/main_window/header/programmable_button.h"
#include "util/text_util.h"
#include "util/action_util.h"
#include "util/dialog_util.h"
#include "util/window_observer_util.h"
#include "util/menu_util.h"
#include "util/ref_util.h"
#include "util/favicon_util.h"
#include "downloads_button_content.h"
#include "data_item/toolbar_item.h"

struct ExtensionButton {
    std::weak_ptr<WebExtension> extension;
    orion::gtk::GObjectPtr widget;
};

namespace {
    struct CppWrapper {
        orion::ObserverId use_mini_toolbar_observer;
        orion::ObserverId tab_layout_setting_observer;
        orion::ObserverId toolbar_items_change_observer;

        std::vector<ExtensionButton> extension_buttons;
        std::shared_ptr<WebExtensionsRuntime::OnExtensionsUpdateDelegate> on_extensions_update_delegate;
    };
}

struct _HeaderBar
{
    GtkBox parent_type;

    GtkWindowControls* window_controls_start; // visible when gsettings button-layout is set to "start"
    GtkWindowControls* window_controls_end;   // visible when gsettings button-layout is set to "end" (default)
    GtkBox* header_bar_box;
    GtkWindowHandle* window_handle;
    GtkBox* start_box;
    GtkBox* middle_box;
    GtkBox* end_box;
    GtkBox* available_box;

    // Toolbar Item widgets
    GtkBox* sidebar_button_box;
    GtkBox* tab_groups_box;
    GtkBox* back_forward_box;
    GtkBox* privacy_box;
    GtkBox* page_tweaks_box;
    GtkBox* website_settings_box;
    GtkOverlay* address_entry_overlay;
    GtkBox* add_page_to_box;
    GtkBox* new_tab_box;
    GtkBox* downloads_box;
    GtkBox* share_box;
    GtkBox* tab_overview_box;
    GtkBox* overflow_menu_box;
    GtkBox* history_box;
    GtkBox* focus_mode_box;
    GtkBox* home_box;
    GtkBox* compact_tab_bar_box;
    GtkBox* summarize_page_box;
    GtkBox* start_page_box;
    GtkBox* notes_box;
    GtkBox* new_window_box;
    GtkBox* new_private_window_box;
    GtkBox* zoom_box;
    GtkBox* print_box;
    GtkBox* web_inspector_box;
    GtkBox* manage_extensions_box;
    GtkBox* show_reader_mode_box;
    GtkBox* kagi_privacy_pass_box;

    GtkBox* static_box;

    gboolean show_compact_tab_bar;

    GtkWidget* toolbar_box;
    GtkButton* window_list_button;
    GtkButton* sidebar_button;
    GtkButton* back_button;
    GtkButton* forward_button;
    GtkButton* summarize_page_button;
    GtkButton* home_button;
    GtkButton* start_page_button;
    GtkButton* notes_button;
    GtkButton* new_window_button;
    GtkButton* new_private_window_button;
    GtkButton* zoom_in_button;
    GtkButton* zoom_out_button;
    GtkButton* print_button;
    GtkButton* web_inspector_button;
    GtkButton* manage_extensions_button;
    GtkButton* show_reader_mode_button;
    GtkButton* kagi_privacy_pass_button;

    GtkLabel* sidebar_label;
    GtkLabel* back_forward_label;
    GtkLabel* privacy_label;
    GtkLabel* page_tweaks_label;
    GtkLabel* website_settings_label;
    GtkLabel* add_page_to_label;
    GtkLabel* new_tab_label;
    GtkLabel* history_label;
    GtkLabel* focus_mode_label;
    GtkLabel* share_label;
    GtkLabel* tab_overview_label;
    GtkLabel* downloads_label;
    GtkLabel* overflow_menu_label;
    GtkLabel* tab_groups_label;
    GtkLabel* home_label;
    GtkLabel* summarize_page_label;
    GtkLabel* window_title_label;
    GtkLabel* start_page_label;
    GtkLabel* notes_label;
    GtkLabel* new_window_label;
    GtkLabel* new_private_window_label;
    GtkLabel* zoom_label;
    GtkLabel* print_label;
    GtkLabel* web_inspector_label;
    GtkLabel* manage_extensions_label;
    GtkLabel* show_reader_mode_label;
    GtkLabel* kagi_privacy_pass_label;

    ToggleSidebarCallback toggle_sidebar_cb;
    gpointer toggle_sidebar_cb_user_data;
    int64_t n_tabs;

    CppWrapper cpp_wrapper;

    GtkButton* privacy_button;
    GtkButton* page_tweaks_button;
    GtkButton* website_settings_button;
    GtkButton* profiles_button;
    GtkBox* profiles_button_box;
    GtkButton* address_focus_mode_button;
    GtkButton* reader_mode_button; // mini button in address entry
    GtkButton* tab_audio_button;
    GtkButton* rss_feeds_button;
    GtkButton* certificates_button;
    GtkEntry* address_entry;
    GtkButton* reload_page_button;
    GtkButton* add_page_to_button;
    GtkButton* new_tab_button;
    GtkButton* history_button;
    GtkButton* page_zoom_button;
    GtkButton* popup_blocked_button;
    GtkLabel* popup_blocked_message;
    GtkBox* extension_box;

    guint popup_blocked_message_timeout_id;

    TabBar* compact_tab_bar;

    GtkExpressionWatch* reader_enabled_watch;
    GtkExpressionWatch* is_readerable_watch;
    GtkExpressionWatch* tab_audio_unmuted_watch;
    GtkExpressionWatch* tab_audio_visible_watch;

    GtkMenuButton* menu_button;
    GtkButton* focus_mode_button;
    GtkButton* share_button;
    GtkButton* tab_overview_button;
    GtkButton* downloads_button;
    GtkMenuButton* overflow_menu_button;
    GtkButton* more_toolbar_items_button;
    GtkBox* back_forward_buttons_box;

    GtkPopover* extension_popover;

    gboolean focus_mode_collapsed;
    gboolean share_collapsed;
    gboolean tab_overview_collapsed;

    GtkPopoverMenu* new_tab_button_popover_menu;
    GtkPopoverMenu* downloads_button_popover;
    GtkPopoverMenu* add_page_to_button_popover;
	GtkPopoverMenu* privacy_button_popover;
	GtkPopoverMenu* page_tweaks_button_popover;
    GtkPopoverMenu* website_settings_button_popover;
    GtkPopoverMenu* history_button_popover;
    GtkPopoverMenu* focus_mode_button_popover;
    GtkPopoverMenu* share_button_context_popover; // on right-click
	GtkPopoverMenu* tab_overview_button_popover;
    GtkPopoverMenu* back_button_popover;
    GtkPopoverMenu* forward_button_popover;
    GtkPopoverMenu* flexible_space_popover;
    GtkPopoverMenu* summarize_page_button_popover;
    GtkPopoverMenu* home_button_popover;
    GtkPopoverMenu* start_page_button_popover;
    GtkPopoverMenu* notes_button_popover;
    GtkPopoverMenu* new_window_button_popover;
    GtkPopoverMenu* new_private_window_button_popover;
    GtkPopoverMenu* zoom_box_popover;
    GtkPopoverMenu* overflow_menu_button_popover;
    GtkPopoverMenu* print_button_popover;
    GtkPopoverMenu* web_inspector_button_popover;
    GtkPopoverMenu* manage_extensions_button_popover;
    GtkPopoverMenu* show_reader_mode_button_popover;
    GtkPopoverMenu* kagi_privacy_pass_button_popover;
};

enum {
    PROP_0,
    PROP_SHOW_COMPACT_TAB_BAR,
    PROP_FOCUS_MODE_COLLAPSED,
    PROP_SHARE_COLLAPSED,
    PROP_TAB_OVERVIEW_COLLAPSED,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS] = {NULL, };

// Use GtkBox instead of AdwBin just because of an issue with
// disposing the template by calling gtk_widget_dispose_template
G_DEFINE_TYPE(HeaderBar, header_bar, GTK_TYPE_BOX)


static void
header_bar_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    HeaderBar *self = APP_HEADER_BAR(object);

    switch (property_id) {
    case PROP_SHOW_COMPACT_TAB_BAR:
        if (!!(self->show_compact_tab_bar) != !!(g_value_get_boolean(value))) {
            self->show_compact_tab_bar = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, obj_properties[PROP_SHOW_COMPACT_TAB_BAR]);
        }
        break;
    case PROP_FOCUS_MODE_COLLAPSED:
        if (!!(self->focus_mode_collapsed) != !!(g_value_get_boolean(value))) {
            self->focus_mode_collapsed = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, obj_properties[PROP_SHOW_COMPACT_TAB_BAR]); // is this correct?
        }
        break;
    case PROP_SHARE_COLLAPSED:
        if (!!(self->share_collapsed) != !!(g_value_get_boolean(value))) {
            self->share_collapsed = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, obj_properties[PROP_SHOW_COMPACT_TAB_BAR]); // is this correct?
        }
        break;
    case PROP_TAB_OVERVIEW_COLLAPSED:
        if (!!(self->tab_overview_collapsed) != !!(g_value_get_boolean(value))) {
            self->tab_overview_collapsed = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, obj_properties[PROP_SHOW_COMPACT_TAB_BAR]); // is this correct?
        }
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void
header_bar_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    HeaderBar* self = APP_HEADER_BAR(object);

    switch (property_id) {
    case PROP_SHOW_COMPACT_TAB_BAR:
        g_value_set_boolean(value, self->show_compact_tab_bar);
        break;

    case PROP_FOCUS_MODE_COLLAPSED:
        g_value_set_boolean(value, self->focus_mode_collapsed);
        break;

    case PROP_SHARE_COLLAPSED:
        g_value_set_boolean(value, self->share_collapsed);
        break;

    case PROP_TAB_OVERVIEW_COLLAPSED:
        g_value_set_boolean(value, self->tab_overview_collapsed);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

GtkButton*
header_bar_get_window_list_button(HeaderBar* self)
{
    return self->window_list_button;
}

GtkButton*
header_bar_get_sidebar_button(HeaderBar* self)
{
    return self->sidebar_button;
}

GtkButton*
header_bar_get_back_button(HeaderBar* self)
{
    return self->back_button;
}

GtkButton*
header_bar_get_forward_button(HeaderBar* self)
{
    return self->forward_button;
}

GtkMenuButton*
header_bar_get_menu_button(HeaderBar* self)
{
    return self->menu_button;
}

GtkButton*
header_bar_get_focus_mode_button(HeaderBar* self)
{
    return self->focus_mode_button;
}

GtkButton*
header_bar_get_share_button(HeaderBar* self)
{
    return self->share_button;
}

GtkButton*
header_bar_get_tab_overview_button(HeaderBar* self)
{
    return self->tab_overview_button;
}

GtkButton*
header_bar_get_downloads_button(HeaderBar* self)
{
    return self->downloads_button;
}

GtkButton*
header_bar_get_privacy_button(HeaderBar* self)
{
    return self->privacy_button;
}

GtkButton*
header_bar_get_page_tweaks_button(HeaderBar* self)
{
    return self->page_tweaks_button;
}

GtkButton*
header_bar_get_website_settings_button(HeaderBar* self)
{
    return self->website_settings_button;
}

GtkButton*
header_bar_get_profiles_button(HeaderBar* self)
{
    return self->profiles_button;
}

GtkBox*
header_bar_get_profiles_button_box(HeaderBar* self)
{
    return self->profiles_button_box;
}

GtkButton*
header_bar_get_address_focus_mode_button(HeaderBar* self)
{
    return self->address_focus_mode_button;
}

GtkButton*
header_bar_get_reader_mode_button(HeaderBar* self)
{
    return self->reader_mode_button;
}

GtkButton*
header_bar_get_rss_feeds_button(HeaderBar* self)
{
    return self->rss_feeds_button;
}

GtkButton*
header_bar_get_certificates_button(HeaderBar* self)
{
    return self->certificates_button;
}

GtkEntry*
header_bar_get_address_entry(HeaderBar* self)
{
    return self->address_entry;
}

GtkButton*
header_bar_get_reload_page_button(HeaderBar* self)
{
    return self->reload_page_button;
}

GtkButton*
header_bar_get_add_page_to_button(HeaderBar* self)
{
    return self->add_page_to_button;
}

GtkButton*
header_bar_get_new_tab_button(HeaderBar* self)
{
    return self->new_tab_button;
}

GtkPopoverMenu*
header_bar_get_new_tab_button_popover_menu(HeaderBar* self)
{
    return self->new_tab_button_popover_menu;
}

GtkButton*
header_bar_get_history_button(HeaderBar* self)
{
    return self->history_button;
}

TabBar*
header_bar_get_compact_page_content_tabbar(HeaderBar* self)
{
    return self->compact_tab_bar;
}

void
header_bar_set_sidebar_button_toggled_callback(HeaderBar* self, ToggleSidebarCallback cb, gpointer user_data)
{
    self->toggle_sidebar_cb = cb;
    self->toggle_sidebar_cb_user_data = user_data;
}

void
header_bar_set_sidebar_button_visible(HeaderBar* self, gboolean visible)
{
    gtk_widget_set_visible(GTK_WIDGET(self->sidebar_button), visible);
}

GtkButton*
header_bar_get_page_zoom_button(HeaderBar* self)
{
    return self->page_zoom_button;
}

GtkButton*
header_bar_get_popup_blocked_button(HeaderBar* self)
{
    return self->popup_blocked_button;
}

static void
header_bar_hide_popup_blocked_message(HeaderBar* self)
{
    gtk_widget_set_visible(GTK_WIDGET(self->popup_blocked_message), FALSE);
    g_clear_handle_id(&self->popup_blocked_message_timeout_id, g_source_remove);
}

static void
on_address_entry_overlay_pressed(GtkGestureClick* gesture, gint n_press,
                                 gdouble x, gdouble y, HeaderBar* self)
{
    if (gtk_widget_get_visible(GTK_WIDGET(self->popup_blocked_message)))
        header_bar_hide_popup_blocked_message(self);
}

void
header_bar_show_popup_blocked_message(HeaderBar* self)
{
    orion::log_verbose("header_bar_show_popup_blocked_message: showing label");
    gtk_widget_set_visible(GTK_WIDGET(self->popup_blocked_message), TRUE);

    g_clear_handle_id(&self->popup_blocked_message_timeout_id, g_source_remove);
    self->popup_blocked_message_timeout_id = g_timeout_add_once(4000, [](gpointer user_data) {
        HeaderBar* self = APP_HEADER_BAR(user_data);
        self->popup_blocked_message_timeout_id = 0;
        orion::log_verbose("header_bar_show_popup_blocked_message: hiding label after timeout");
        header_bar_hide_popup_blocked_message(self);
    }, self);
}

static void
on_sidebar_button_clicked(GtkButton* self, gpointer user_data)
{
    HeaderBar* header = APP_HEADER_BAR(user_data);

    if (header->toggle_sidebar_cb) {
        header->toggle_sidebar_cb(header->toggle_sidebar_cb_user_data, TRUE);
    }
}

void
header_bar_set_mini_toolbar_enabled(HeaderBar* self, bool enabled)
{
    orion::log_verbose("header_bar_set_mini_toolbar_enabled: enabled=%d", enabled);

    //TODO: it may be worth instead just having the style class on the whole header bar and using css selectors instead of this?
    auto add_or_remove_func = enabled
        ? gtk_widget_add_css_class
        : gtk_widget_remove_css_class;

    auto inverse_add_or_remove_func = enabled
        ? gtk_widget_remove_css_class
        : gtk_widget_add_css_class;

    add_or_remove_func(GTK_WIDGET(self->window_list_button), "mini-toolbar-window-list-button");
    add_or_remove_func(GTK_WIDGET(self->sidebar_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->back_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->forward_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->privacy_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->page_tweaks_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->website_settings_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->add_page_to_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->new_tab_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->history_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->address_entry), "mini-toolbar-entry");
    add_or_remove_func(GTK_WIDGET(self->profiles_button), "mini-toolbar-profiles-icon");
    add_or_remove_func(GTK_WIDGET(self->focus_mode_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->share_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->tab_overview_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->downloads_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->overflow_menu_button), "mini-toolbar-menu-button");
    add_or_remove_func(GTK_WIDGET(self->more_toolbar_items_button), "mini-toolbar-menu-button");
    add_or_remove_func(GTK_WIDGET(self->menu_button), "mini-toolbar-menu-button");
    add_or_remove_func(GTK_WIDGET(self->home_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->start_page_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->overflow_menu_button), "mini-toolbar-menu-button");
    add_or_remove_func(GTK_WIDGET(self->notes_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->new_window_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->new_private_window_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->zoom_in_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->zoom_out_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->print_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->web_inspector_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->manage_extensions_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->show_reader_mode_button), "mini-toolbar-icon-button");
    add_or_remove_func(GTK_WIDGET(self->kagi_privacy_pass_button), "mini-toolbar-icon-button");

    inverse_add_or_remove_func(GTK_WIDGET(self->address_entry), "normal-toolbar-entry");
    inverse_add_or_remove_func(GTK_WIDGET(self->profiles_button), "normal-toolbar-profiles-icon");
}

static gboolean
bind_window_icon_visibility(HeaderBar* self, MainWindow* win)
{
    if(MAIN_IS_WINDOW(win))
    {
        //technically should undo previous binding but window_list_button should never move
    }
    return FALSE;
}

static gboolean
bind_window_true_visibility(HeaderBar* self, MainWindow* win)
{
    if(MAIN_IS_WINDOW(win))
    {
        //technically should undo previous binding but window_list_button should never move
    }
    return TRUE;
}

static GAction*
lookup_window_list_button_style_action(HeaderBar* self, MainWindow* win)
{
    if(!G_IS_ACTION_MAP(win))
    {
        return NULL;
    }
    else
    {
        return g_object_ref(g_action_map_lookup_action(G_ACTION_MAP(win), "window-list-button-style"));
    }
}

static gboolean
window_list_button_style_setting_to_title_visibility(HeaderBar* self, GVariant* state)
{
    if(!state)
    {
        return false;
    }
    size_t len = 0;
    const char* state_string = g_variant_get_string(state, &len);
    if(state_string)
    {
        char* end;
        int val = strtol(state_string, &end, 10);
        if((size_t)(end-state_string)!=len)
        {
            //this should've already been validated and errored out earlier so let's stay silent
            orion::log_verbose("window_list_button_style_setting_to_title_visibility: bad string");
            return true;
        }
        orion::settings::WindowListButtonStyle style = orion::settings::WindowListButtonStyle(val);
        switch(style)
        {
            case orion::settings::WindowListButtonStyle::icon_and_text:
                return true;
            case orion::settings::WindowListButtonStyle::text_only:
                return true;
            case orion::settings::WindowListButtonStyle::icon_only:
                return false;
            case orion::settings::WindowListButtonStyle::not_set:
                return true;
        }
        return true;
    }
    else
    {
        return true;
    }
}

static gboolean
window_list_button_style_setting_to_icon_visibility(HeaderBar* self, GVariant* state)
{
    if(!state)
    {
        return false;
    }
    size_t len = 0;
    const char* state_string = g_variant_get_string(state, &len);
    if(state_string)
    {
        char* end;
        int val = strtol(state_string, &end, 10);
        if((size_t)(end-state_string)!=len)
        {
            //this should've already been validated and errored out earlier so let's stay silent
            orion::log_verbose("window_list_button_style_setting_to_title_visibility: bad string");
            return false;
        }
        orion::settings::WindowListButtonStyle style = orion::settings::WindowListButtonStyle(val);
        switch(style)
        {
            case orion::settings::WindowListButtonStyle::icon_and_text:
                return true;
            case orion::settings::WindowListButtonStyle::text_only:
                return false;
            case orion::settings::WindowListButtonStyle::icon_only:
                return true;
            case orion::settings::WindowListButtonStyle::not_set:
                return false;
        }
        return false;
    }
    else
    {
        return false;
    }
}

static gchar*
get_certificates_button_icon(HeaderBar* self, gint64 raw_tls_status)
{
    orion::TlsStatus status = orion::TlsStatus(raw_tls_status);

    switch(status)
    {
        case orion::TlsStatus::secure:
        case orion::TlsStatus::local: // use the secure icon for local as well (similar to Firefox behavior)
            return g_strdup("padlock2-symbolic");
        case orion::TlsStatus::none:
        case orion::TlsStatus::insecure:
        case orion::TlsStatus::mixed:
        case orion::TlsStatus::failure:
            return g_strdup("padlock2-slash-symbolic");
    }
    orion::log_error("get_certificates_button_icon(): got invalid enum value %li", raw_tls_status);
    return g_strdup("INVALID");
}

static gboolean
tls_status_to_certificates_button_visibility(HeaderBar* self, gint64 raw_tls_status)
{
    orion::TlsStatus status = orion::TlsStatus(raw_tls_status);

    switch(status)
    {
        case orion::TlsStatus::secure:
        case orion::TlsStatus::mixed:
        case orion::TlsStatus::insecure:
        case orion::TlsStatus::failure:
            return true;
        case orion::TlsStatus::none:
        case orion::TlsStatus::local:
            return false;
    }
    orion::log_error("tls_status_to_certificates_button_visibility(): got invalid enum value %li", raw_tls_status);
    return true;
}

static gboolean
toolbar_item_label_visibility(HeaderBar* self, GVariant* state, const gchar* item_type_str)
{
    if (!state) return false;

    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_debug("toolbar_item_label_visibility: could not find root main window");
        return false;
    }
    gboolean show_sidebar = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "toggle-sidebar");

    size_t len = 0;
    const char* state_string = g_variant_get_string(state, &len);
    if(state_string)
    {
        char* end;
        int val = strtol(state_string, &end, 10);
        if((size_t)(end-state_string)!=len)
        {
            //this should've already been validated and errored out earlier so let's stay silent
            orion::log_verbose("toolbar_item_label_visibility: bad string");
            return true;
        }
        orion::settings::ToolbarItemStyle style = orion::settings::ToolbarItemStyle(val);
        switch(style)
        {
            case orion::settings::ToolbarItemStyle::icon_and_text:
                return true;
            case orion::settings::ToolbarItemStyle::icon_only:
                return false;
            case orion::settings::ToolbarItemStyle::not_set:
                return true;
        }
        return true;
    }
    else
    {
        return true;
    }
}

static void
update_toolbar_item_label_visibility(HeaderBar* self)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_debug("update_toolbar_item_label_visibility: could not find root main window");
        return;
    }

    gboolean show_sidebar = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "toggle-sidebar");
    std::string toolbar_item_style_str = orion::gtk::get_string_action_state(G_ACTION_MAP(win), "toolbar-item-style");

    int64_t toolbar_item_style_num = 0;
    VERIFY_LOG(orion::str::try_parse_int64(toolbar_item_style_str, &toolbar_item_style_num));
    orion::settings::ToolbarItemStyle toolbar_item_style = orion::settings::ToolbarItemStyle(toolbar_item_style_num);

    gboolean show_sidebar_label = !show_sidebar && toolbar_item_style == orion::settings::ToolbarItemStyle::icon_and_text;
    gtk_widget_set_visible(GTK_WIDGET(self->sidebar_label), show_sidebar_label);

    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [profile_id,self_weak,toolbar_item_style](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        HeaderBar* self = APP_HEADER_BAR(self_strong.get());

        bool is_compact_tab_layout = (reply.value == orion::settings::TabLayout::compact);

        std::vector<GtkWidget*> widgets{
            GTK_WIDGET(self->back_forward_label),
            GTK_WIDGET(self->privacy_label),
            GTK_WIDGET(self->page_tweaks_label),
            GTK_WIDGET(self->website_settings_label),
            GTK_WIDGET(self->add_page_to_label),
            GTK_WIDGET(self->new_tab_label),
            GTK_WIDGET(self->history_label),
            GTK_WIDGET(self->focus_mode_label),
            GTK_WIDGET(self->share_label),
            GTK_WIDGET(self->tab_overview_label),
            GTK_WIDGET(self->downloads_label),
            GTK_WIDGET(self->overflow_menu_label),
            GTK_WIDGET(self->tab_groups_label),
            GTK_WIDGET(self->home_label),
            GTK_WIDGET(self->summarize_page_label),
            GTK_WIDGET(self->start_page_label),
            GTK_WIDGET(self->notes_label),
            GTK_WIDGET(self->new_window_label),
            GTK_WIDGET(self->new_private_window_label),
            GTK_WIDGET(self->zoom_label),
            GTK_WIDGET(self->print_label),
            GTK_WIDGET(self->web_inspector_label),
            GTK_WIDGET(self->manage_extensions_label),
            GTK_WIDGET(self->show_reader_mode_label),
            GTK_WIDGET(self->kagi_privacy_pass_label),
        };

        gboolean show_label =
            !is_compact_tab_layout &&
            toolbar_item_style == orion::settings::ToolbarItemStyle::icon_and_text;

        for (auto widget : widgets) {
            if (!widget) {
                orion::log_warning("update_toolbar_item_label_visibility: widget is NULL, skipping");
                continue;
            }
            gtk_widget_set_visible(widget, show_label);
        }

        for (GtkWidget* widget = gtk_widget_get_first_child(GTK_WIDGET(self->extension_box));
             widget != NULL;
             widget = gtk_widget_get_next_sibling(widget))
        {
            if (!GTK_IS_BOX(widget)) {
                // Suspicious
                continue;
            }
            GtkWidget* label = gtk_widget_get_next_sibling(gtk_widget_get_first_child(widget));
            if (!label || !GTK_IS_LABEL(label)) {
                // Suspicious
                continue;
            }
            gtk_widget_set_visible(label, show_label);
        }
    });
}

static void
update_toolbar_item_label_visibility_cb(GAction* action, GParamSpec* pspec, gpointer user_data)
{
    HeaderBar* self = APP_HEADER_BAR(user_data);
    update_toolbar_item_label_visibility(self);
}

static void
reader_enabled_notify(HeaderBar* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_BOOLEAN);
    defer([val = &val]{ g_value_unset(val); });
    if(gtk_expression_watch_evaluate(self->reader_enabled_watch, &val))
    {
        gboolean enabled = g_value_get_boolean(&val);
        if(enabled)
        {
            gtk_widget_add_css_class(GTK_WIDGET(self->reader_mode_button), "reader-mode-enabled");
            gtk_widget_set_tooltip_text(GTK_WIDGET(self->reader_mode_button), _("Hide Reader Mode"));
        }
        else
        {
            gtk_widget_remove_css_class(GTK_WIDGET(self->reader_mode_button), "reader-mode-enabled");
            gtk_widget_set_tooltip_text(GTK_WIDGET(self->reader_mode_button), _("Show Reader Mode"));
        }
    }
}

static void
is_readerable_notify(HeaderBar* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_BOOLEAN);
    defer([val = &val]{ g_value_unset(val); });
    if(gtk_expression_watch_evaluate(self->is_readerable_watch, &val))
    {
        gboolean readerable = g_value_get_boolean(&val);
        if(readerable)
        {
            gtk_button_set_icon_name(self->reader_mode_button, "open-book-symbolic");
        }
        else
        {
            gtk_button_set_icon_name(self->reader_mode_button, "open-book-slash-symbolic");
        }
    }
}

static void
tab_audio_unmuted_notify(HeaderBar* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_BOOLEAN);
    defer([val = &val]{ g_value_unset(val); });
    if(gtk_expression_watch_evaluate(self->tab_audio_unmuted_watch, &val))
    {
        gboolean any_unmuted = g_value_get_boolean(&val);
        if(any_unmuted)
        {
            gtk_button_set_icon_name(self->tab_audio_button, "speaker-3-symbolic");
        }
        else
        {
            gtk_button_set_icon_name(self->tab_audio_button, "speaker-0-symbolic");
        }
    }
}

static void
tab_audio_visible_notify(HeaderBar* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_BOOLEAN);
    defer([val = &val]{ g_value_unset(val); });
    if(gtk_expression_watch_evaluate(self->tab_audio_visible_watch, &val))
    {
        gboolean any_playing = g_value_get_boolean(&val);
        if(any_playing)
        {
            gtk_widget_add_css_class(GTK_WIDGET(self->address_entry), "has-audio-icon");
        }
        else
        {
            gtk_widget_remove_css_class(GTK_WIDGET(self->address_entry), "has-audio-icon");
        }
    }
}

static void
on_page_zoom_button_clicked(GtkButton* button, gpointer user_data)
{
    HeaderBar* self = APP_HEADER_BAR(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    g_action_group_activate_action(G_ACTION_GROUP(win), "reset-zoom", NULL);
}

static void
on_popup_blocked_button_clicked(GtkButton* button, gpointer user_data)
{
    HeaderBar* self = APP_HEADER_BAR(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    g_action_group_activate_action(G_ACTION_GROUP(win), "show-blocked-popups", NULL);
}

static void
on_reader_button_clicked(GtkButton* button, HeaderBar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    gboolean reader_mode_enabled = TRUE;
    g_object_get(view, "reader-mode-enabled", &reader_mode_enabled, NULL);
    if(reader_mode_enabled) return;

    g_signal_connect_object(view, "notify::is-readerable", GCallback(+[](OrionWebviewTabView* view, GParamSpec* pspec, GtkButton* button){
        defer([view, button]{ g_signal_handlers_disconnect_by_data(view, button); });

        gboolean val = FALSE;
        g_object_get(view, "is-readerable", &val, NULL);
        if(val) return;
        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
        if(!win) return;
        //here to protect us against NULL focus child issues that cause the popover to emit a critical on popup
        if(!gtk_widget_get_focus_child(GTK_WIDGET(win))) 
        {
            if(!gtk_widget_grab_focus(GTK_WIDGET(button))) return;
        }


        GtkPopover* popover = main_window_get_reader_unavailable_popover(win);

        graphene_point_t zero{};
        graphene_point_t pos{};
        if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(win), &zero, &pos)) {
            orion::log_warning("setup_reader_mode_popover: failed to compute point");
        }

        // Point the popover arrow to the button's bottom (vertically) center (horizontally)
        GdkRectangle rect{
            .x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
            .y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
            .width = 1,
            .height = 1,
        };
        gtk_popover_set_pointing_to(popover, &rect);
        gtk_popover_set_has_arrow(popover, TRUE);
        gtk_popover_set_autohide(popover, TRUE);

        
        //error already prevented by force grabbing focus
        //if(!gtk_widget_get_focus_child(GTK_WIDGET(win))) orion::log_error("no focus?");
        gtk_popover_popup(popover);
    }), button, G_CONNECT_DEFAULT);
}

static void
on_tab_audio_button_clicked(GtkButton* button, HeaderBar* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;
    AdwTabPage* current_page = orion_webview_tab_view_get_selected_page(view);
    OrionWebviewTabContainer* current_container = current_page ? ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(current_page)) : NULL;

    // if the current tab is audible, toggle just that tab; otherwise toggle every other playing tab
    // (mirrors the "Mute Tab" / "Mute Other Tabs" actions from the Window menu)
    if(current_container && orion_webview_tab_container_get_is_playing_audio(current_container))
    {
        g_action_group_activate_action(G_ACTION_GROUP(win), "mute-current-tab", NULL);
    }
    else
    {
        g_action_group_activate_action(G_ACTION_GROUP(win), "mute-other-tabs-than-current", NULL);
    }
}

void
header_bar_popdown_all_popovers(HeaderBar* self)
{
    gtk_menu_button_popdown(self->menu_button);
    if(self->extension_popover)
    {
        gtk_popover_popdown(self->extension_popover);
    }
}

static void
on_extension_button_clicked(GtkButton* button, ExtensionButton* extension_button)
{
    HeaderBar* self = nullptr;
    GtkWidget* widget = GTK_WIDGET(button);
    while(widget && !APP_IS_HEADER_BAR(widget)) { // TODO: check this
        widget = gtk_widget_get_parent(widget);
    }
    if(!widget) {
        return;
    }
    self = APP_HEADER_BAR(widget);
    
    if(self->extension_popover) {
        gtk_widget_unparent(GTK_WIDGET(self->extension_popover));
    }
    std::shared_ptr<WebExtension> extension = extension_button->extension.lock();
    if(!extension) {
        return;
    }
    self->extension_popover = extension_popover_new(extension->extension_id());
    gtk_widget_set_parent(GTK_WIDGET(self->extension_popover), GTK_WIDGET(self));

    graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(self), &zero, &pos)) {
		orion::log_warning("on_extension_button_clicked: failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
		.width = 1,
		.height = 1,
	};
	gtk_popover_set_pointing_to(self->extension_popover, &rect);
	gtk_popover_set_has_arrow(self->extension_popover, TRUE);
	gtk_popover_popup(self->extension_popover);
}

static bool
set_button_image(GtkButton* button, const guint8* data, gsize size)
{
    UNIQUE_PTR(GBytes, g_bytes_unref) bytes(g_bytes_new(data, size));
    UNIQUE_PTR(GdkTexture, g_object_unref) texture(gdk_texture_new_from_bytes(bytes.get(), nullptr));
    if (!texture) {
        return false;
    }
    UNIQUE_PTR(GtkWidget, g_object_unref) image(gtk_image_new_from_paintable(GDK_PAINTABLE(texture.get())));
    gtk_image_set_pixel_size(GTK_IMAGE(image.get()), 24);
    gtk_button_set_child(button, image.release());
    gtk_button_set_has_frame(button, false);
    return true;
}

static orion::gtk::GObjectPtr
extension_button_new(const WebExtension& extension)
{
    GtkBox* box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));
    gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_CENTER);
    GtkButton* extension_button = GTK_BUTTON(gtk_button_new_with_label("?"));
    gtk_widget_set_tooltip_text(GTK_WIDGET(extension_button), extension.default_title().value_or("<null>").c_str());
    std::optional<WebExtensionResourceData> icon_data = extension.default_icon_data();
    if(!icon_data || !set_button_image(GTK_BUTTON(extension_button), icon_data->data, icon_data->datalen)) {
        // TODO: set error image
    }
    gtk_widget_add_css_class(GTK_WIDGET(extension_button), "toolbar-button");
    gtk_widget_add_css_class(GTK_WIDGET(extension_button), "extension-button");
    gtk_box_append(box, GTK_WIDGET(extension_button));
    GtkLabel* label = GTK_LABEL(gtk_label_new(extension.default_title().value_or("<null>").c_str()));
    gtk_widget_add_css_class(GTK_WIDGET(label), "toolbar-item-label");
    gtk_widget_set_visible(GTK_WIDGET(label), FALSE);
    gtk_box_append(box, GTK_WIDGET(label));
    return orion::gtk::GObjectPtr((GObject*)g_object_ref(box));
}

static void
on_extensions_change(HeaderBar* self, WebExtensionList extension_list)
{
    for(auto& extension_button : self->cpp_wrapper.extension_buttons) {
        gtk_box_remove(self->extension_box, GTK_WIDGET(extension_button.widget.get()));
    }
    self->cpp_wrapper.extension_buttons.clear();
    self->cpp_wrapper.extension_buttons.reserve(extension_list.size());
    for(const auto& extension : extension_list) {
        std::shared_ptr<WebExtension> ref = extension.lock();
        if(!ref) {
            continue;
        }
        ExtensionButton& extension_button = self->cpp_wrapper.extension_buttons.emplace_back(
            ExtensionButton {
                .extension = extension,
                .widget = extension_button_new(*ref.get())
            }
        );
        GtkWidget* button_widget = gtk_widget_get_first_child(GTK_WIDGET(extension_button.widget.get()));
        if (!GTK_IS_BUTTON(button_widget)) {
            orion::log_warning("on_extensions_change: extension button widget is not a button");
        } else {
            g_signal_connect(button_widget, "clicked", G_CALLBACK(on_extension_button_clicked), &extension_button);
        }
    }

    // TODO: fix extension buttons placement
    for(auto& extension_button : self->cpp_wrapper.extension_buttons) {
        gtk_box_append(self->extension_box, GTK_WIDGET(extension_button.widget.get()));
    }

    update_toolbar_item_label_visibility(self);
}

static void
header_bar_reload_profile(HeaderBar* self)
{
    orion::gtk::WeakRefWrapper weak_ref(self);
    auto on_extensions_update_cb = [weak_ref](WebExtensionList extension_list){
        auto ref = weak_ref.lock();
        if(!ref) {
            return;
        }
        HeaderBar* self = APP_HEADER_BAR(ref.get());
        on_extensions_change(self, extension_list);
    };
    WebExtensionsRuntime& runtime = main_window_get_current_extensions_runtime(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self))));
    self->cpp_wrapper.on_extensions_update_delegate = std::make_shared<WebExtensionsRuntime::OnExtensionsUpdateDelegate>(on_extensions_update_cb);
    runtime.on_extensions_update += self->cpp_wrapper.on_extensions_update_delegate;
    std::optional<WebExtensionList> extension_list = runtime.get_extensions();
    if(extension_list) {
        on_extensions_change(self, extension_list.value());
    }
}

static void
on_profile_id_notify(MainWindow* win, GParamSpec* pspec, gpointer user_data)
{
    HeaderBar* self = APP_HEADER_BAR(user_data);
    header_bar_reload_profile(self);
}

static void
on_focus_mode_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    // Toggle focus mode setting
    g_action_group_activate_action(G_ACTION_GROUP(win), "toggle-focus-mode", NULL);
}

static GMenuModel*
header_bar_create_overflow_menu(HeaderBar* self)
{
    GMenu* menu = g_menu_new();

    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(self));
    if (!MAIN_IS_WINDOW(root)) {
        orion::log_error("header_bar_create_overflow_menu: root widget is not MainWindow");
        return G_MENU_MODEL(menu);
    }
    MainWindow* win = MAIN_WINDOW(root);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper menu_weak(menu);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [profile_id,menu_weak](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);

        bool is_compact_tab_layout = (reply.value == orion::settings::TabLayout::compact);

        orion::GetToolbarItemsQuery query{
            .profile_id = profile_id,
            .is_compact_tab_layout = is_compact_tab_layout,
        };

        global_core_api->get_toolbar_items(query, [menu_weak](orion::GetToolbarItemsReply reply){
            VERIFY_LOG(reply.err);

            auto menu_strong = menu_weak.lock();
            if (!menu_strong) return;
            GMenu* menu = G_MENU(menu_strong.get());

            // Dynamic section
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
                std::multiset<std::string> menu_names;

                std::vector<orion::ProgrammableButtonId> programmable_button_ids{};
                for (const orion::ToolbarItemDto& dto : reply.items) {
                    if (!dto.in_overflow_menu) continue;

                    ToolbarItem* item = toolbar_item_new_from_dto(dto);
                    defer([item]{ g_object_unref(item); });

                    switch (dto.item_type) { //should continue instead of break, to skip the code after this (acting as a default statement while preserving exhaustiveness check)
                        case orion::ToolbarItemType::not_set:
                        case orion::ToolbarItemType::flexible_space:
                        case orion::ToolbarItemType::profile:
                        case orion::ToolbarItemType::address_and_search:
                        case orion::ToolbarItemType::back_forward:
                        case orion::ToolbarItemType::sidebar:
                        case orion::ToolbarItemType::zoom:
                        case orion::ToolbarItemType::overflow_menu: //having this as an item type sounds weird...
                        case orion::ToolbarItemType::tab_groups:
                            orion::log_warning("header_bar_create_overflow_menu(): wrong item type got into overflow menu for id %ld", dto.id.value);
                            continue;
                        case orion::ToolbarItemType::summarize_page:
                        case orion::ToolbarItemType::focus_mode:
                        case orion::ToolbarItemType::home:
                        case orion::ToolbarItemType::tab_overview:
                        case orion::ToolbarItemType::start_page:
                        case orion::ToolbarItemType::web_inspector:
                        case orion::ToolbarItemType::extensions:
                        case orion::ToolbarItemType::bookmark:
                        case orion::ToolbarItemType::new_tab:
                        case orion::ToolbarItemType::history:
                        case orion::ToolbarItemType::website_settings:
                        case orion::ToolbarItemType::downloads:
                        case orion::ToolbarItemType::page_tweaks:
                        case orion::ToolbarItemType::privacy:
                        case orion::ToolbarItemType::print:
                        case orion::ToolbarItemType::notes:
                        case orion::ToolbarItemType::share:
                        case orion::ToolbarItemType::toggle_reader:
                        case orion::ToolbarItemType::kagi_privacy_pass:
                        case orion::ToolbarItemType::new_window:
                        case orion::ToolbarItemType::new_private_window: {
                            ToolbarItemDetail detail = toolbar_item_get_toolbar_item_details(item);
                            orion::gtk::menu_append_action_with_target(
                                section,
                                detail.translated_name.c_str(),
                                detail.action.c_str(),
                                detail.target.get()
                            );
                            continue;
                        }
                        case orion::ToolbarItemType::programmable_button: {
                            GMenu* submenu = g_menu_new();
                            defer([submenu]{ g_object_unref(submenu); });
                            std::string label = orion::gtk::transform_to_unique_menu_name(
                                "programmable_button", _("Programmable Button"), &menu_names
                            );

                            GMenuItem* submenu_item = g_menu_item_new_submenu(label.c_str(), G_MENU_MODEL(submenu));
                            g_menu_item_set_attribute_value(submenu_item, "id", g_variant_new_int64(dto.id.value));
                            g_menu_append_item(section, submenu_item);
                            g_object_unref(submenu_item);

                            programmable_button_ids.push_back(PROGRAMMABLE_BUTTON_ID(dto.id));

                            {
                                GMenu* subsection = g_menu_new();
                                defer([subsection]{ g_object_unref(subsection); });
                                g_menu_append_section(submenu, NULL, G_MENU_MODEL(subsection));

                                orion::gtk::menu_append_action_with_int64_target(subsection, _("Activate"), "win.programmable-button-action", dto.id.value);
                                orion::gtk::menu_append_action_with_int64_target(subsection, _("Edit…"), "win.edit-programmable-button", dto.id.value);
                            }
                            {
                                GMenu* subsection = g_menu_new();
                                defer([subsection]{ g_object_unref(subsection); });
                                g_menu_append_section(submenu, NULL, G_MENU_MODEL(subsection));

                                orion::gtk::menu_append_action_with_int64_target(subsection, _("Import Button from URL…"), "win.import-button-from-url", dto.id.value);
                                orion::gtk::menu_append_action_with_int64_target(subsection, _("Import Button from File…"), "win.import-button-from-file", dto.id.value);
                                orion::gtk::menu_append_action_with_int64_target(subsection, _("Export to File…"), "win.export-button-to-file", dto.id.value);
                            }
                            continue;
                        }
                    }
                    //will be skipped if the continues on the switch statement run, acting as a default statement
                    orion::log_warning(
                        "header_bar_create_overflow_menu: skipping unsupported overflow menu item type: %s",
                        orion::toolbar_item_type_to_string(dto.item_type).c_str()
                    );
                }

                orion::gtk::WeakRefWrapper section_weak(section);
                orion::GetProgrammableButtonsQuery query{};
                query.ids = std::move(programmable_button_ids);
                global_core_api->get_programmable_buttons(query, [section_weak](orion::GetProgrammableButtonsReply reply){
                    VERIFY_LOG(reply.err);

                    auto menu_strong = section_weak.lock();
                    if(!menu_strong) return;

                    GMenu* menu = G_MENU(menu_strong.get());
                    GMenuModel* model = G_MENU_MODEL(menu);
                    gint n_items = g_menu_model_get_n_items(model);
                    for(gint i = 0; i<n_items; i++)
                    {
                        GVariant* id_var = g_menu_model_get_item_attribute_value(model, i, "id", G_VARIANT_TYPE_INT64);
                        if(id_var)
                        {
                            orion::ProgrammableButtonId id(g_variant_get_int64(id_var));
                            auto it = std::find_if(reply.items.cbegin(), reply.items.cend(), [id](orion::ProgrammableButtonDto dto){ return id == dto.id.value(); });

                            // Replace the menu item with the name of Programmable Button, but only if the button
                            // has a non-empty name
                            if(it != reply.items.cend() && !it->title.empty())
                            {
                                GMenuModel* submenu = g_menu_model_get_item_link(model, i, "submenu");
                                defer([submenu]{ g_object_unref(submenu); });
                                GMenuItem* item = g_menu_item_new_submenu(it->title.c_str(), submenu);
                                defer([item]{ g_object_unref(item); });

                                g_menu_remove(menu, i);
                                g_menu_insert_item(menu, i, item);
                            }
                        }
                    }
                });
            }
            
            // Fixed "Customize Menu" section (always present)
            {
                GMenu* section = g_menu_new();
                defer([section]{ g_object_unref(section); });
                g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

                g_menu_append(section, _("Customize Menu…"), "win.customize-menu");
            }
        });
    });

    return G_MENU_MODEL(menu);
}

static void
header_bar_overflow_menu_popover_show(GtkWidget* widget, gpointer user_data)
{
    GtkPopoverMenu* popover_menu = GTK_POPOVER_MENU(widget);
    HeaderBar* header_bar = APP_HEADER_BAR(user_data);
    
    GMenuModel* main_menu = header_bar_create_overflow_menu(header_bar);
    gtk_popover_menu_set_menu_model(popover_menu, main_menu);
}

static void
header_bar_setup_overflow_menu(HeaderBar* self)
{
    // We have to set some dummy menu model, then we replace it in the "show" signal handler
    GMenu* dummy = g_menu_new();
    defer([dummy]{ g_object_unref(dummy); });

    GtkWidget* popover_menu = gtk_popover_menu_new_from_model_full(G_MENU_MODEL(dummy), GTK_POPOVER_MENU_SLIDING);
    gtk_menu_button_set_popover(self->overflow_menu_button, popover_menu);
    
    // "activate" signal (on GtkMenuButton) does not work here, it is not emitted when the button is clicked
    g_signal_connect(popover_menu, "show", G_CALLBACK(header_bar_overflow_menu_popover_show), self);
}

static void
set_widget_toolbar_item_id(GtkWidget* widget, orion::ToolbarItemId id)
{
    int64_t* value = g_new(int64_t, 1);
    *value = id.value;

    g_object_set_data_full(
        G_OBJECT(widget),
        "toolbar-item-id",
        value,
        g_free);
}

static void
set_widget_toolbar_item_type(GtkWidget* widget, orion::ToolbarItemType item_type)
{
    int64_t* value = g_new(int64_t, 1);
    *value = static_cast<int64_t>(item_type);

    g_object_set_data_full(
        G_OBJECT(widget),
        "toolbar-item-type",
        value,
        g_free);
}

static orion::ToolbarItemId
get_widget_toolbar_item_id(GtkWidget* widget)
{
    int64_t* value = (int64_t*)g_object_get_data(G_OBJECT(widget), "toolbar-item-id");
    if (!value) {
        return orion::ToolbarItemId{0};
    }
    return orion::ToolbarItemId{*value};
}

static orion::ToolbarItemType
get_widget_toolbar_item_type(GtkWidget* widget)
{
    int64_t* value = (int64_t*)g_object_get_data(G_OBJECT(widget), "toolbar-item-type");
    if (!value) {
        return orion::ToolbarItemType::not_set;
    }
    return orion::ToolbarItemType{*value};
}

static void
setup_context_menu_dynamic(
    HeaderBar* self,
    orion::ToolbarItemType item_type,
    orion::ToolbarItemId item_id,
    GtkPopoverMenu** popover_out,
    std::function<void(GMenu*, orion::ToolbarItemType, orion::ToolbarItemId)> menu_builder)
{
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("setup_context_menu_dynamic: could not find root main window");
        return;
    }

    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });
    menu_builder(menu, item_type, item_id);
    if (*popover_out) {
        // Clean the previous popover
        gtk_widget_unparent(GTK_WIDGET(*popover_out));
    }
    GtkWidget* popover = gtk_popover_menu_new_from_model_full(G_MENU_MODEL(menu), GTK_POPOVER_MENU_SLIDING);
    gtk_widget_set_parent(popover, GTK_WIDGET(win));
    *popover_out = GTK_POPOVER_MENU(popover);
}

static void
header_bar_setup_context_menu_for_toolbar_item(
    HeaderBar* self, orion::ToolbarItemType item_type, orion::ToolbarItemId item_id)
{
    switch (item_type) {
        case orion::ToolbarItemType::not_set:
            orion::log_warning(
                "header_bar_setup_context_menu_for_toolbar_item: unsupported toolbar item type %lld", item_type);
            break;
        case orion::ToolbarItemType::bookmark:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->add_page_to_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::new_tab:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->new_tab_button_popover_menu, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::downloads:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->downloads_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::privacy:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->privacy_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::page_tweaks:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->page_tweaks_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::website_settings:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->website_settings_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::history:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->history_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::focus_mode:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->focus_mode_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::share:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->share_button_context_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::tab_overview:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->tab_overview_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::back_forward:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->back_button_popover, create_toolbar_button_context_menu);
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->forward_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::flexible_space:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->flexible_space_popover, create_empty_space_context_menu);
            break;
        case orion::ToolbarItemType::programmable_button:
            // not needed, programmable button has its own context menu
            break;
        case orion::ToolbarItemType::summarize_page:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->summarize_page_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::home:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->home_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::start_page:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->start_page_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::notes:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->notes_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::new_window:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->new_window_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::new_private_window:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->new_private_window_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::zoom:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->zoom_box_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::overflow_menu:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->overflow_menu_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::print:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->print_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::web_inspector:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->web_inspector_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::extensions:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->manage_extensions_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::toggle_reader:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->show_reader_mode_button_popover, create_toolbar_button_context_menu);
            break;
        case orion::ToolbarItemType::kagi_privacy_pass:
            setup_context_menu_dynamic(
                self, item_type, item_id, &self->kagi_privacy_pass_button_popover, create_toolbar_button_context_menu);
            break;
        // TODO: more items
        default: break; // TODO: delete
    }
    orion::log_verbose(
        "header_bar_setup_context_menu_for_toolbar_item: toolbar item type %lld", item_type);
}

static void
on_flexible_space_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_flexible_space_secondary_released: could not find root main window");
        return;
    }

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(click));

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->flexible_space_popover, widget);
}

static void
header_bar_setup_dynamic_toolbar_item_widget(HeaderBar* self, GtkWidget* widget, const orion::ToolbarItemDto& dto)
{
    set_widget_toolbar_item_id(GTK_WIDGET(widget), dto.id);
    set_widget_toolbar_item_type(GTK_WIDGET(widget), dto.item_type);
    header_bar_setup_context_menu_for_toolbar_item(self, dto.item_type, dto.id);
    orion::gtk::register_right_click(GTK_WIDGET(self), widget, on_flexible_space_secondary_released);
}

static GtkWidget*
header_bar_create_flexible_space_widget(HeaderBar* self, const orion::ToolbarItemDto& dto)
{
    GtkWidget* widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(GTK_WIDGET(widget), TRUE);
    header_bar_setup_dynamic_toolbar_item_widget(self, widget, dto);
    return widget;
}

static GtkWidget*
header_bar_create_programmable_button_widget(HeaderBar* self, const orion::ToolbarItemDto& dto)
{
    orion::ProgrammableButtonId button_id(dto.id.value);
    GtkWidget* widget = GTK_WIDGET(programmable_button_new(button_id));
    // Programmable button has its own context menu, so we do not need to set it up here
    set_widget_toolbar_item_id(GTK_WIDGET(widget), dto.id);
    set_widget_toolbar_item_type(GTK_WIDGET(widget), dto.item_type);
    return widget;
}

static void
destroy_dynamic_widgets_in_box(GtkBox* box)
{
    GtkWidget* next_sibling = NULL;

    for (GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(box));
         child != NULL;
         child = next_sibling)
    {
        // We have to capture the next sibling before removing the child, because removing the child potentially
        // invalidates the child pointer
        next_sibling = gtk_widget_get_next_sibling(child);
        orion::ToolbarItemType item_type = get_widget_toolbar_item_type(child);
        if (item_type == orion::ToolbarItemType::flexible_space ||
            item_type == orion::ToolbarItemType::programmable_button)
        {
            gtk_box_remove(box, child);
        }
    }
}

static void
header_bar_process_toolbar_items(HeaderBar* self, bool is_compact_tab_layout, const orion::GetToolbarItemsReply& reply)
{
    orion::log_verbose("%s: processing %zu toolbar items", __func__, reply.items.size());
    // True iff at least 1 flexible space item is present
    bool contains_flexible_space = false;

    for (const orion::ToolbarItemDto& dto : reply.items) {
        if (dto.item_type == orion::ToolbarItemType::flexible_space) {
            contains_flexible_space = true;
        }
    }

    std::map<orion::ToolbarItemId, GtkBox*> toolbar_item_id_to_target_box_map;
    std::map<orion::ToolbarItemId, bool> toolbar_item_id_to_before_address_entry_map;
    bool before_address_entry = true;

    for (const orion::ToolbarItemDto& dto : reply.items) {
        if (dto.item_type == orion::ToolbarItemType::address_and_search) {
            before_address_entry = false;
            if (!is_compact_tab_layout && contains_flexible_space) {
                toolbar_item_id_to_target_box_map[dto.id] = self->middle_box;
            } else {
                toolbar_item_id_to_target_box_map[dto.id] = self->start_box;
            }
        } else if (before_address_entry) {
            toolbar_item_id_to_target_box_map[dto.id] = self->start_box;
        } else {
            if (!is_compact_tab_layout && contains_flexible_space) {
                toolbar_item_id_to_target_box_map[dto.id] = self->end_box;
            } else {
                toolbar_item_id_to_target_box_map[dto.id] = self->start_box;
            }
        }
        toolbar_item_id_to_before_address_entry_map[dto.id] = before_address_entry;
    }

    std::map<orion::ToolbarItemType, GtkWidget*> item_type_to_widget_map{
        {orion::ToolbarItemType::sidebar, GTK_WIDGET(self->sidebar_button_box)},
        {orion::ToolbarItemType::address_and_search, GTK_WIDGET(self->address_entry_overlay)},
        {orion::ToolbarItemType::tab_groups, GTK_WIDGET(self->tab_groups_box)},
        {orion::ToolbarItemType::back_forward, GTK_WIDGET(self->back_forward_box)},
        {orion::ToolbarItemType::privacy, GTK_WIDGET(self->privacy_box)},
        {orion::ToolbarItemType::page_tweaks, GTK_WIDGET(self->page_tweaks_box)},
        {orion::ToolbarItemType::website_settings, GTK_WIDGET(self->website_settings_box)},
        {orion::ToolbarItemType::bookmark, GTK_WIDGET(self->add_page_to_box)},
        {orion::ToolbarItemType::new_tab, GTK_WIDGET(self->new_tab_box)},
        {orion::ToolbarItemType::downloads, GTK_WIDGET(self->downloads_box)},
        {orion::ToolbarItemType::share, GTK_WIDGET(self->share_box)},
        {orion::ToolbarItemType::tab_overview, GTK_WIDGET(self->tab_overview_box)},
        {orion::ToolbarItemType::overflow_menu, GTK_WIDGET(self->overflow_menu_box)},
        {orion::ToolbarItemType::history, GTK_WIDGET(self->history_box)},
        {orion::ToolbarItemType::focus_mode, GTK_WIDGET(self->focus_mode_box)},
        {orion::ToolbarItemType::home, GTK_WIDGET(self->home_box)},
        {orion::ToolbarItemType::summarize_page, GTK_WIDGET(self->summarize_page_box)},
        {orion::ToolbarItemType::start_page, GTK_WIDGET(self->start_page_box)},
        {orion::ToolbarItemType::notes, GTK_WIDGET(self->notes_box)},
        {orion::ToolbarItemType::new_window, GTK_WIDGET(self->new_window_box)},
        {orion::ToolbarItemType::new_private_window, GTK_WIDGET(self->new_private_window_box)},
        {orion::ToolbarItemType::zoom, GTK_WIDGET(self->zoom_box)},
        {orion::ToolbarItemType::print, GTK_WIDGET(self->print_box)},
        {orion::ToolbarItemType::web_inspector, GTK_WIDGET(self->web_inspector_box)},
        {orion::ToolbarItemType::extensions, GTK_WIDGET(self->manage_extensions_box)},
        {orion::ToolbarItemType::toggle_reader, GTK_WIDGET(self->show_reader_mode_box)},
        {orion::ToolbarItemType::kagi_privacy_pass, GTK_WIDGET(self->kagi_privacy_pass_box)},
    };

    // Destroy all existing flexible space and programmable button widgets that were created dynamically in each box
    destroy_dynamic_widgets_in_box(self->start_box);
    destroy_dynamic_widgets_in_box(self->middle_box);
    destroy_dynamic_widgets_in_box(self->end_box);
    destroy_dynamic_widgets_in_box(self->available_box);

    // Move all toolbar item widgets to available box
    orion::gtk::move_all_children_from_box_to_box(self->start_box, self->available_box);
    orion::gtk::move_all_children_from_box_to_box(self->middle_box, self->available_box);
    orion::gtk::move_all_children_from_box_to_box(self->end_box, self->available_box);

    GtkWidget* last_flexible_space = NULL;
    GtkWidget* overflow_menu_widget = NULL;

    for (const orion::ToolbarItemDto& dto : reply.items) {                
        GtkBox* target_box = toolbar_item_id_to_target_box_map[dto.id];

        if (!target_box) {
            orion::log_warning(
                "header_bar_sync_toolbar_items_in_toolbar: no target box found for toolbar item type %lld",
                dto.item_type);
            continue;
        }

        if (dto.item_type == orion::ToolbarItemType::flexible_space) {
            // Ignore flexible spaces in Compact Tabs layout (Mac does something similar)
            if (!is_compact_tab_layout) {
                // Flexible space is special and it can be added to the toolbar multiple times
                GtkWidget* widget = header_bar_create_flexible_space_widget(self, dto);
                gtk_box_append(target_box, widget);
                last_flexible_space = widget;
            }
        } else if (dto.item_type == orion::ToolbarItemType::programmable_button) {
            // Programmable buttons are special and they can be added to the toolbar multiple times
            GtkWidget* widget = header_bar_create_programmable_button_widget(self, dto);
            gtk_box_append(target_box, widget);
        } else {
            auto it = item_type_to_widget_map.find(dto.item_type);
            if (it == item_type_to_widget_map.end()) {
                orion::log_warning(
                    "header_bar_sync_toolbar_items_in_toolbar: unsupported toolbar item type %lld (%s)",
                    dto.item_type, orion::toolbar_item_type_to_string(dto.item_type).c_str());
                continue;
            }

            GtkWidget* widget = GTK_WIDGET(it->second);

            if (dto.item_type == orion::ToolbarItemType::overflow_menu) {
                overflow_menu_widget = widget;
            }

            ToolbarItem* toolbar_item = toolbar_item_new_from_dto(dto);
            defer([toolbar_item]{ g_object_unref(toolbar_item); });
            ToolbarItemDetail detail = toolbar_item_get_toolbar_item_details(toolbar_item);
            // Detail can be used to set action-name, icon-name, and other properties on the widget if needed

            set_widget_toolbar_item_id(widget, dto.id);
            set_widget_toolbar_item_type(widget, dto.item_type);
            header_bar_setup_context_menu_for_toolbar_item(self, dto.item_type, dto.id);

            // Move the item from available box to target box
            orion::gtk::move_child_from_box_to_box(self->available_box, widget, target_box);

            if (dto.item_type == orion::ToolbarItemType::address_and_search && is_compact_tab_layout) {
                // In Compact Tabs layout, the Address Entry widget should always be tightly followed by 
                // the tab bar and then New Tab button
                orion::gtk::move_child_from_box_to_box(
                    self->available_box, GTK_WIDGET(self->compact_tab_bar_box), target_box);
                // Toolbar item type of compact tab bar box is not needed to be set
                orion::gtk::move_child_from_box_to_box(
                    self->available_box, GTK_WIDGET(self->new_tab_box), target_box);
                set_widget_toolbar_item_type(GTK_WIDGET(self->new_tab_box), orion::ToolbarItemType::new_tab);
                header_bar_setup_context_menu_for_toolbar_item(self, dto.item_type, dto.id);
            }
        }
    }

    // Extension action buttons are populated dynamically and are not configurable toolbar items yet.
    // Moving all toolbar children to available_box above also moves their container, so put it back in
    // the visible box selected for items following the address entry.
    // By default, place the extensions after the last flexible space, or before the overflow menu widget if
    // no flexible space is present, or at the end of the toolbar if neither is present.
    if (last_flexible_space) {
        orion::log_verbose("%s: extension_box_n_children=%d",
            __func__, orion::gtk::get_children_count(GTK_WIDGET(self->extension_box)));
        orion::gtk::move_child_from_box_to_box_after_sibling(
            self->available_box,
            GTK_WIDGET(self->extension_box),
            GTK_BOX(gtk_widget_get_parent(last_flexible_space)),
            last_flexible_space);
    } else if (overflow_menu_widget) {
        orion::gtk::move_child_from_box_to_box_before_sibling(
            self->available_box,
            GTK_WIDGET(self->extension_box),
            GTK_BOX(gtk_widget_get_parent(overflow_menu_widget)),
            overflow_menu_widget);
    } else {
        GtkBox* extension_target_box = contains_flexible_space && !is_compact_tab_layout
            ? self->end_box
            : self->start_box;
        orion::gtk::move_child_from_box_to_box(
            self->available_box,
            GTK_WIDGET(self->extension_box),
            extension_target_box);
    }

    gtk_widget_set_visible(GTK_WIDGET(self->middle_box), contains_flexible_space && !is_compact_tab_layout);
    gtk_widget_set_visible(GTK_WIDGET(self->end_box), contains_flexible_space && !is_compact_tab_layout);
}

static void
header_bar_sync_toolbar_items_in_toolbar(HeaderBar* self)
{
    orion::log_verbose("header_bar_sync_toolbar_items_in_toolbar");
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_warning("header_bar_sync_toolbar_items_in_toolbar: could not find root main window");
        return;
    }
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(profile_id, orion::settings::tabs::tab_layout,
    [self_weak,profile_id](orion::GetSettingReply<orion::settings::TabLayout> reply){
        VERIFY_LOG(reply.err);

        bool is_compact_tab_layout = reply.value == orion::settings::TabLayout::compact;

        orion::GetToolbarItemsQuery query{
            .profile_id = profile_id,
            .exclude_in_overflow_menu = true,
            .is_compact_tab_layout = is_compact_tab_layout,
        };

        global_core_api->get_toolbar_items(query,
        [self_weak,is_compact_tab_layout](orion::GetToolbarItemsReply reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            HeaderBar* self = APP_HEADER_BAR(self_strong.get());

            header_bar_process_toolbar_items(self, is_compact_tab_layout, reply);
        });
    });
}

static void
header_bar_register_toolbar_items_observers(HeaderBar* self)
{
    orion::gtk::WeakRefWrapper self_weak(self);

    self->cpp_wrapper.toolbar_items_change_observer = global_core_api->toolbar_items_change().add_observer(
        [self_weak](const orion::ToolbarItemsChange& change){
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            HeaderBar* self = APP_HEADER_BAR(self_strong.get());

            header_bar_sync_toolbar_items_in_toolbar(self);
        });
}

static void
tab_groups_box_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("tab_groups_box_destroy_cb");
}

static void
window_list_button_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("window_list_button_destroy_cb");
}

static void
toolbar_box_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("toolbar_box_destroy_cb");
}

static void
start_box_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("start_box_destroy_cb");
}

static void
available_box_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("available_box_destroy_cb");
}

static void
back_forward_box_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("back_forward_box_destroy_cb");
}

static void
window_title_label_destroy_cb(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("window_title_label_destroy_cb");
}

static void
on_downloads_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_downloads_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->downloads_button_popover,
        GTK_WIDGET(header_bar->downloads_button));
}

static void
on_overflow_menu_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_overflow_menu_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->overflow_menu_button_popover,
        GTK_WIDGET(header_bar->overflow_menu_button));
}

static void
on_add_page_to_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_add_page_to_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->add_page_to_button_popover,
        GTK_WIDGET(header_bar->add_page_to_button));
}

static void
on_privacy_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_privacy_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->privacy_button_popover,
        GTK_WIDGET(header_bar->privacy_button));
}

static void
on_page_tweaks_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_page_tweaks_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->page_tweaks_button_popover,
        GTK_WIDGET(header_bar->page_tweaks_button));
}

static void
on_website_settings_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_website_settings_button_secondary_released: could not find root main window");
        return;
    }
    
	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->website_settings_button_popover,
        GTK_WIDGET(header_bar->website_settings_button));
}

static void
on_history_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_history_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->history_button_popover,
        GTK_WIDGET(header_bar->history_button));
}

static void
on_focus_mode_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_focus_mode_button_secondary_released: could not find root main window");
        return;
    }

	orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->focus_mode_button_popover,
        GTK_WIDGET(header_bar->focus_mode_button));
}

static void
on_share_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_share_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->share_button_context_popover,
        GTK_WIDGET(header_bar->share_button));
}

static void
on_tab_overview_button_secondary_released(GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_tab_overview_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->tab_overview_button_popover,
        GTK_WIDGET(header_bar->tab_overview_button));
}

static detached_task
fill_forward_menu_model(OrionWindow* win, GMenu* menu)
{
    g_object_ref(win);
    defer([win]{ g_object_unref(win); });
    //we need this window to stay alive, to provide a favicon database

    GValue val = G_VALUE_INIT;
    g_value_init(&val, APP_TYPE_BACK_FORWARD_LIST);
    g_object_get_property(G_OBJECT(win), "forward-list", &val);
    defer([val_ptr = &val]{ g_value_unset(val_ptr); });

    //this is memory managed by the GValue
    auto* forward_list = static_cast<orion::BackForwardList*>(g_value_get_boxed(&val));

    //back_list is not necessary for properly freeing, thus we can iterate using it
    //also keeping track of relative index to use for the navigate action itself
    for(int64_t i = 1; forward_list; i++, forward_list = forward_list->next) //comma operator to increment index and also iterate forward_list from the same expression
    {
        const orion::BackForwardListEntry& entry = *(static_cast<orion::BackForwardListEntry*>(forward_list->data));
        GMenuItem* item = g_menu_item_new(entry.title.empty() ? entry.uri.c_str() : entry.title.c_str(), NULL);
        defer([item]{ g_object_unref(item); });

        GIcon* icon = co_await orion::gtk::get_icon_for_uri(win, orion::Uri::parse_strict(entry.uri.c_str()));
        if(icon) g_menu_item_set_attribute_value(item, "icon", g_icon_serialize(icon));

        g_menu_item_set_action_and_target_value(item, "win.go-back-forward-relative-index", orion::gtk::VariantWrapper(int64_t(i)).get());
        g_menu_append_item(menu, item);
    }
}

static detached_task
fill_back_menu_model(OrionWindow* win, GMenu* menu)
{
    g_object_ref(win);
    defer([win]{ g_object_unref(win); });
    //we need this window to stay alive, to provide a favicon database

    GValue val = G_VALUE_INIT;
    g_value_init(&val, APP_TYPE_BACK_FORWARD_LIST);
    g_object_get_property(G_OBJECT(win), "back-list", &val);
    defer([val_ptr = &val]{ g_value_unset(val_ptr); });

    //this is memory managed by the GValue
    auto* back_list = static_cast<orion::BackForwardList*>(g_value_get_boxed(&val));

    //back_list is not necessary for properly freeing, thus we can iterate using it
    //also keeping track of relative index to use for the navigate action itself
    for(int64_t i = 1; back_list; i++, back_list = back_list->next) //comma operator to increment index and also iterate back_list from the same expression
    {
        const orion::BackForwardListEntry& entry = *(static_cast<orion::BackForwardListEntry*>(back_list->data));
        GMenuItem* item = g_menu_item_new(entry.title.empty() ? entry.uri.c_str() : entry.title.c_str(), NULL);
        defer([item]{ g_object_unref(item); });

        GIcon* icon = co_await orion::gtk::get_icon_for_uri(win, orion::Uri::parse_strict(entry.uri.c_str()));
        if(icon) g_menu_item_set_attribute_value(item, "icon", g_icon_serialize(icon));

        g_menu_item_set_action_and_target_value(item, "win.go-back-forward-relative-index", orion::gtk::VariantWrapper(int64_t(-i)).get());
        g_menu_append_item(menu, item);
    }
}

static void
reload_back_list(OrionWindow* win, gpointer unused, GtkPopoverMenu* popover)
{
    GMenu* menu = g_menu_new();

    fill_back_menu_model(win, menu);

    gtk_popover_menu_set_menu_model(popover, G_MENU_MODEL(menu));
}

static void
reload_forward_list(OrionWindow* win, gpointer unused, GtkPopoverMenu* popover)
{
    GMenu* menu = g_menu_new();

    fill_forward_menu_model(win, menu);

    gtk_popover_menu_set_menu_model(popover, G_MENU_MODEL(menu));
}

static void
clear_popover(GtkPopoverMenu* popover, GObject* sig_source)
{
    g_signal_handlers_disconnect_by_data(sig_source, popover);
    auto remover = [](gpointer user_data){
        gtk_widget_unparent(GTK_WIDGET(user_data));
    };
    g_idle_add_once(remover, popover);
}

static void
on_back_button_secondary_released(GtkButton* button, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    GtkWidget* popover_parent = GTK_WIDGET(button);

    GtkWidget* popover_widget = gtk_popover_menu_new_from_model(NULL);
    gtk_widget_set_parent(popover_widget, popover_parent);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(popover_parent));
    if (!win) {
        orion::log_warning("on_back_button_secondary_released: could not find root main window");
        return;
    }


    reload_back_list(win, NULL, GTK_POPOVER_MENU(popover_widget));
    GtkPopover* popover = GTK_POPOVER(popover_widget);
    g_signal_connect_object(win, "notify::back-list", G_CALLBACK(reload_back_list), popover, G_CONNECT_DEFAULT);
    g_signal_connect_object(popover, "closed", G_CALLBACK(clear_popover), win, G_CONNECT_DEFAULT);

    gtk_popover_popup(popover);
}

static void
on_forward_button_secondary_released(GtkButton* button, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    GtkWidget* popover_parent = GTK_WIDGET(button);

    GtkWidget* popover_widget = gtk_popover_menu_new_from_model(NULL);
    gtk_widget_set_parent(popover_widget, popover_parent);

    OrionWindow* win = ORION_WINDOW(gtk_widget_get_root(popover_parent));
    if (!win) {
        orion::log_warning("on_back_button_secondary_released: could not find root main window");
        return;
    }


    reload_forward_list(win, NULL, GTK_POPOVER_MENU(popover_widget));
    GtkPopover* popover = GTK_POPOVER(popover_widget);
    g_signal_connect_object(win, "notify::forward-list", G_CALLBACK(reload_forward_list), popover, G_CONNECT_DEFAULT);
    g_signal_connect_object(popover, "closed", G_CALLBACK(clear_popover), win, G_CONNECT_DEFAULT);

    gtk_popover_popup(popover);
}

static void
claim_sequence(GtkGesture* gesture)
{
    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
on_summarize_page_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_summarize_page_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->summarize_page_button_popover,
        GTK_WIDGET(header_bar->summarize_page_button));
}

static void
on_home_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_home_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->home_button_popover,
        GTK_WIDGET(header_bar->home_button));
}

static void
on_start_page_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_start_page_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->start_page_button_popover,
        GTK_WIDGET(header_bar->start_page_button));
}

static void
on_notes_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_notes_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->notes_button_popover,
        GTK_WIDGET(header_bar->notes_button));
}

static void
on_new_window_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_new_window_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->new_window_button_popover,
        GTK_WIDGET(header_bar->new_window_button));
}

static void
on_new_private_window_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_new_private_window_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->new_private_window_button_popover,
        GTK_WIDGET(header_bar->new_private_window_button));
}

static void
on_print_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_print_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->print_button_popover,
        GTK_WIDGET(header_bar->print_button));
}

static void
on_web_inspector_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_web_inspector_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->web_inspector_button_popover,
        GTK_WIDGET(header_bar->web_inspector_button));
}

static void
on_manage_extensions_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_manage_extensions_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->manage_extensions_button_popover,
        GTK_WIDGET(header_bar->manage_extensions_button));
}

static void
on_show_reader_mode_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_show_reader_mode_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->show_reader_mode_button_popover,
        GTK_WIDGET(header_bar->show_reader_mode_button));
}

static void
on_kagi_privacy_pass_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_kagi_privacy_pass_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->kagi_privacy_pass_button_popover,
        GTK_WIDGET(header_bar->kagi_privacy_pass_button));
}

static void
on_zoom_button_secondary_released(
    GtkWidget* parent, gint n_press, gdouble x, gdouble y, GtkGestureClick* click)
{
    HeaderBar* header_bar = APP_HEADER_BAR(parent);
    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(header_bar));
    if (!win) {
        orion::log_warning("on_zoom_button_secondary_released: could not find root main window");
        return;
    }

    orion::gtk::button_released_handler(
        GTK_WIDGET(win), click, x, y, header_bar->zoom_box_popover,
        GTK_WIDGET(header_bar->zoom_in_button));
}

static void
header_bar_register_right_clicks(HeaderBar* self)
{
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->add_page_to_box),
        on_add_page_to_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->downloads_box),
        on_downloads_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->overflow_menu_box),
        on_overflow_menu_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->privacy_box),
        on_privacy_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->page_tweaks_box),
        on_page_tweaks_button_secondary_released);
	orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->website_settings_box),
        on_website_settings_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->history_box),
        on_history_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->focus_mode_box),
        on_focus_mode_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->share_box),
        on_share_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->tab_overview_box),
        on_tab_overview_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->summarize_page_box),
        on_summarize_page_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->home_box),
        on_home_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->start_page_box),
        on_start_page_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->notes_box),
        on_notes_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->new_window_box),
        on_new_window_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->new_private_window_box),
        on_new_private_window_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->zoom_box),
        on_zoom_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->print_box),
        on_print_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->web_inspector_box),
        on_web_inspector_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->manage_extensions_box),
        on_manage_extensions_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->show_reader_mode_box),
        on_show_reader_mode_button_secondary_released);
    orion::gtk::register_right_click(
        GTK_WIDGET(self), GTK_WIDGET(self->kagi_privacy_pass_box),
        on_kagi_privacy_pass_button_secondary_released);
}

static void
header_bar_unparent_toolbar_button_popovers(HeaderBar* self)
{
    g_clear_pointer(&self->new_tab_button_popover_menu, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->downloads_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->overflow_menu_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->add_page_to_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->privacy_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->page_tweaks_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->website_settings_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->history_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->focus_mode_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->share_button_context_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->tab_overview_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->back_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->forward_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->flexible_space_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->summarize_page_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->home_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->start_page_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->notes_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->new_window_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->new_private_window_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->zoom_box_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->print_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->web_inspector_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->manage_extensions_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->show_reader_mode_button_popover, orion::gtk::widget_unparent_wrapper);
    g_clear_pointer(&self->kagi_privacy_pass_button_popover, orion::gtk::widget_unparent_wrapper);
}

static void
on_header_bar_unmap(GtkWidget* widget, gpointer user_data)
{
    HeaderBar* self = APP_HEADER_BAR(widget);
    
    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.tab_layout_setting_observer);
    global_core_api->toolbar_items_change().clear_observer(&self->cpp_wrapper.toolbar_items_change_observer);
}

static void
on_header_bar_map(HeaderBar* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::gtk::WeakRefWrapper self_weak(self);

    global_core_api->get_setting(main_window_get_current_profile_id(win), orion::settings::tabs::tab_layout,
        [self_weak](orion::GetSettingReply<orion::settings::TabLayout> reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            HeaderBar* self = APP_HEADER_BAR(self_strong.get());

            header_bar_sync_toolbar_items_in_toolbar(self);
            gtk_widget_set_visible(
                GTK_WIDGET(self->compact_tab_bar), reply.value == orion::settings::TabLayout::compact);
        });

    self->cpp_wrapper.tab_layout_setting_observer = global_core_api->add_single_setting_change_observer(
        orion::settings::tabs::tab_layout,
        [self_weak](orion::SingleSettingChange<orion::settings::TabLayout> change){            
            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            HeaderBar* self = APP_HEADER_BAR(self_strong.get());

            header_bar_sync_toolbar_items_in_toolbar(self);
            gtk_widget_set_visible(
                GTK_WIDGET(self->compact_tab_bar), change.new_value == orion::settings::TabLayout::compact);
            update_toolbar_item_label_visibility(self);
        });

    header_bar_sync_toolbar_items_in_toolbar(APP_HEADER_BAR(self));
    header_bar_register_toolbar_items_observers(self);
}

static void
remove_header_bar_observers(HeaderBar* self)
{
    std::vector<GtkWidget*> descendants;
    orion::gtk::get_descendants(GTK_WIDGET(self->start_box), &descendants);
    orion::gtk::get_descendants(GTK_WIDGET(self->middle_box), &descendants);
    orion::gtk::get_descendants(GTK_WIDGET(self->end_box), &descendants);
    orion::gtk::get_descendants(GTK_WIDGET(self->available_box), &descendants);

    for (GtkWidget* widget : descendants) {
        gpointer ptr = g_object_get_data(G_OBJECT(widget), "window-observer-id");
        if (ptr) {
            g_object_set_data(G_OBJECT(widget), "window-observer-id", NULL);
            orion::log_verbose(
                "remove_header_bar_observers: removed observer: key=window-observer-id type=%s ptr=%p",
                G_OBJECT_TYPE_NAME(widget), widget);
        }
    }
}

static void
header_bar_realize(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(header_bar_parent_class)->realize(widget); // chain up
}

static gboolean
window_controls_start_should_be_visible(HeaderBar* self, gboolean show_sidebar, gboolean controls_empty)
{
    return !show_sidebar && !controls_empty;
}

static void
header_bar_root(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(header_bar_parent_class)->root(widget); // important: always call up

    HeaderBar* self = APP_HEADER_BAR(widget);

    MainWindow* win = orion::gtk::get_root_main_window(GTK_WIDGET(self));
    if (!win) {
        orion::log_debug("header_bar_root: could not find root main window");
        return;
    }

    GtkWidget* overlay_split_view = gtk_widget_get_ancestor(widget, ADW_TYPE_OVERLAY_SPLIT_VIEW);
    if (overlay_split_view) {
        GtkExpression* visibility_params[] = {
            gtk_property_expression_new(
                ADW_TYPE_OVERLAY_SPLIT_VIEW,
                gtk_object_expression_new(G_OBJECT(overlay_split_view)),
                "show-sidebar"
            ),
            gtk_property_expression_new(
                GTK_TYPE_WINDOW_CONTROLS,
                gtk_object_expression_new(G_OBJECT(self->window_controls_start)),
                "empty"
            ),
        };
        GtkExpression* visibility_expression = gtk_cclosure_expression_new(
            G_TYPE_BOOLEAN,
            NULL,
            G_N_ELEMENTS(visibility_params),
            visibility_params,
            G_CALLBACK(window_controls_start_should_be_visible),
            NULL,
            NULL
        );
        gtk_expression_bind(
            visibility_expression,
            self->window_controls_start,
            "visible",
            self
        );
    } else {
        orion::log_warning("header_bar_root: could not find overlay split view");
    }

    g_signal_connect(win, "notify::profile-id", G_CALLBACK(on_profile_id_notify), self);
    header_bar_reload_profile(self);
    
    GActionMap* map = G_ACTION_MAP(gtk_widget_get_root(widget));
    GAction* action = g_action_map_lookup_action(map, "tab-layout-setting");
    g_object_bind_property_full(action, "state", self, "show-compact-tab-bar", G_BINDING_DEFAULT,
        [](GBinding* binding, const GValue* from, GValue* to, gpointer user_data){
            GVariant* variant = g_value_get_variant(from);
            std::string state = g_variant_get_string(variant, NULL);
            g_value_set_boolean(to, state == "compact");
            return 1;
        }, NULL, NULL, NULL);

    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper self_weak(self);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);

    {
        auto handler = [win_weak,self_weak](bool new_value){
            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            HeaderBar* self = APP_HEADER_BAR(self_strong.get());
            
            GAction* action = g_action_map_lookup_action(G_ACTION_MAP(win), "use-mini-toolbar");
            g_action_change_state(action, g_variant_new_boolean(new_value));

            header_bar_set_mini_toolbar_enabled(self, new_value);

            auto add_or_remove_func = new_value
                ? gtk_widget_add_css_class
                : gtk_widget_remove_css_class;

            add_or_remove_func(GTK_WIDGET(self->window_controls_start), "mini-toolbar-window-controls");
            add_or_remove_func(GTK_WIDGET(self->window_controls_end), "mini-toolbar-window-controls");
            add_or_remove_func(GTK_WIDGET(self), "mini-toolbar-header-bar-containers");
            add_or_remove_func(GTK_WIDGET(self->window_handle), "mini-toolbar-header-bar-containers");
            add_or_remove_func(GTK_WIDGET(self->header_bar_box), "mini-toolbar-header-bar-box");

            Sidebar* sidebar = main_window_get_sidebar(win);
            sidebar_set_mini_toolbar_enabled(sidebar, new_value);

            BookmarksBar* bookmarks_bar = main_window_get_bookmarks_bar(win);
            bookmarks_bar_set_mini_toolbar_enabled(bookmarks_bar, new_value);
        };

        global_core_api->get_setting(profile_id, orion::settings::tabs::use_mini_toolbar,
            [handler](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                handler(reply.value);
            });

        self->cpp_wrapper.use_mini_toolbar_observer = global_core_api->add_single_setting_change_observer(
            orion::settings::tabs::use_mini_toolbar,
            [handler,profile_id](orion::SingleSettingChange<bool> change){
                if (change.profile_id != profile_id) return;
                handler(change.new_value);
            });
    }

    GAction* toolbar_item_style_action = g_action_map_lookup_action(G_ACTION_MAP(win), "toolbar-item-style");
    GAction* toggle_sidebar_action = g_action_map_lookup_action(G_ACTION_MAP(win), "toggle-sidebar");
    g_signal_connect(toolbar_item_style_action, "notify::state", G_CALLBACK(update_toolbar_item_label_visibility_cb), self);
    g_signal_connect(toggle_sidebar_action, "notify::state", G_CALLBACK(update_toolbar_item_label_visibility_cb), self);
    g_signal_connect(self->tab_groups_box, "destroy", G_CALLBACK(tab_groups_box_destroy_cb), NULL);
    g_signal_connect(self->window_list_button, "destroy", G_CALLBACK(window_list_button_destroy_cb), NULL);
    g_signal_connect(self->toolbar_box, "destroy", G_CALLBACK(toolbar_box_destroy_cb), NULL);
    g_signal_connect(self->start_box, "destroy", G_CALLBACK(start_box_destroy_cb), NULL);
    g_signal_connect(self->available_box, "destroy", G_CALLBACK(available_box_destroy_cb), NULL);
    g_signal_connect(self->back_forward_box, "destroy", G_CALLBACK(back_forward_box_destroy_cb), NULL);
    g_signal_connect(self->window_title_label, "destroy", G_CALLBACK(window_title_label_destroy_cb), NULL);

    // MaxWidthContainer test (does work similar to AdwClamp):
    // MaxWidthContainer* max_width_container = APP_MAX_WIDTH_CONTAINER(max_width_container_new(600));
    // gtk_center_box_set_center_widget(self->toolbar_box, GTK_WIDGET(max_width_container));
    // max_width_container_set_child(max_width_container, GTK_WIDGET(self->center_widget));
    // gtk_widget_set_hexpand(GTK_WIDGET(max_width_container), FALSE);
    // gtk_widget_set_halign(GTK_WIDGET(max_width_container), GTK_ALIGN_CENTER);

    gtk_widget_set_parent(GTK_WIDGET(self->new_tab_button_popover_menu), GTK_WIDGET(self));

    header_bar_register_right_clicks(self);
}

static void
header_bar_dispose(GObject* gobject)
{
    HeaderBar* self = APP_HEADER_BAR(gobject);

    g_clear_handle_id(&self->popup_blocked_message_timeout_id, g_source_remove);

    g_clear_pointer(&self->reader_enabled_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->is_readerable_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->tab_audio_unmuted_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->tab_audio_visible_watch, gtk_expression_watch_unwatch);

    for(auto& extension_button : self->cpp_wrapper.extension_buttons) {
        gtk_box_remove(self->extension_box, GTK_WIDGET(extension_button.widget.get()));
    }
    self->cpp_wrapper.extension_buttons.clear();

    if (self->extension_popover) {
        gtk_widget_unparent(GTK_WIDGET(self->extension_popover));
    }

    header_bar_unparent_toolbar_button_popovers(self);

    remove_header_bar_observers(self); // temporary solution to avoid leaked window observers

    // Removal of toolbar item widgets from boxes is not necessary
    orion::gtk::remove_all_children_from_box(self->start_box);
    orion::gtk::remove_all_children_from_box(self->middle_box);
    orion::gtk::remove_all_children_from_box(self->end_box);
    orion::gtk::remove_all_children_from_box(self->available_box);

    // Note: gtk_widget_dispose_template can be called here because we derive HeaderBar from GtkBox.
    // If we would derive from AdwBin instead, then gtk_widget_dispose_template would not work properly
    // because of gtk_widget_unparent internal call in Adwaita.
    // Nevertheless, calling this might cause a warning because of GtkWindowHandle special widget.
    gtk_widget_dispose_template(GTK_WIDGET(gobject), APP_HEADER_BAR_TYPE);

    G_OBJECT_CLASS(header_bar_parent_class)->dispose(gobject);
}

static void
header_bar_finalize(GObject* gobject)
{
    HeaderBar* self = APP_HEADER_BAR(gobject);

    global_core_api->single_setting_change().clear_observer(&self->cpp_wrapper.use_mini_toolbar_observer);

    std::destroy_at(&self->cpp_wrapper);

    G_OBJECT_CLASS(header_bar_parent_class)->finalize(gobject);
}

static void
header_bar_init(HeaderBar* self)
{
    std::construct_at(&self->cpp_wrapper);

    g_type_ensure(APP_TYPE_READER_MODE_SETTINGS_POPOVER);
    g_type_ensure(downloads_dialog_get_type());
    g_type_ensure(APP_DOWNLOADS_BUTTON_CONTENT_TYPE);

    gtk_widget_init_template(GTK_WIDGET(self));

    GtkGesture* popup_blocked_message_click = gtk_gesture_click_new();
    gtk_event_controller_set_propagation_phase(
        GTK_EVENT_CONTROLLER(popup_blocked_message_click), GTK_PHASE_CAPTURE);
    g_signal_connect(
        popup_blocked_message_click, "pressed",
        G_CALLBACK(on_address_entry_overlay_pressed), self);
    gtk_widget_add_controller(
        GTK_WIDGET(self->address_entry_overlay),
        GTK_EVENT_CONTROLLER(popup_blocked_message_click));

    {
        GtkExpression* root_expression = gtk_property_expression_new(header_bar_get_type(), NULL, "root");
        GtkExpression* tab_view_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "tab-view");
        GtkExpression* reader_enabled_expression = gtk_property_expression_new(
            ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_expression), "reader-mode-enabled");

        self->reader_enabled_watch = gtk_expression_watch(reader_enabled_expression, self, GtkExpressionNotify(reader_enabled_notify), self, NULL);
        gtk_expression_unref(reader_enabled_expression);

        GtkExpression* tab_audio_unmuted_expression = gtk_property_expression_new(
            ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_expression), "any-tab-playing-audio-unmuted");

        self->tab_audio_unmuted_watch = gtk_expression_watch(tab_audio_unmuted_expression, self, GtkExpressionNotify(tab_audio_unmuted_notify), self, NULL);

        GtkExpression* tab_audio_visible_expression = gtk_property_expression_new(
            ORION_WEBVIEW_TAB_VIEW_TYPE, gtk_expression_ref(tab_view_expression), "any-tab-playing-audio");

        self->tab_audio_visible_watch = gtk_expression_watch(tab_audio_visible_expression, self, GtkExpressionNotify(tab_audio_visible_notify), self, NULL);

        GtkExpression* is_readerable_expression = gtk_property_expression_new(
            ORION_WEBVIEW_TAB_VIEW_TYPE, tab_view_expression, "is-readerable");

        self->is_readerable_watch = gtk_expression_watch(is_readerable_expression, self, GtkExpressionNotify(is_readerable_notify), self, NULL);
        gtk_expression_unref(is_readerable_expression);
    }

    header_bar_setup_overflow_menu(self);
}

static GtkAlign
get_header_bar_center_alignment(gboolean show_compact_tab_bar)
{
    return show_compact_tab_bar?GTK_ALIGN_FILL:GTK_ALIGN_CENTER;
}

static void
header_bar_class_init(HeaderBarClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = header_bar_dispose;
    object_class->finalize = header_bar_finalize;

    object_class->set_property = header_bar_set_property;
    object_class->get_property = header_bar_get_property;

    obj_properties[PROP_SHOW_COMPACT_TAB_BAR] = g_param_spec_boolean("show-compact-tab-bar",
        "show-compact-tab-bar",
        "Whether compact tab bar should be visible",
        false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_FOCUS_MODE_COLLAPSED] = g_param_spec_boolean("focus-mode-collapsed",
        "Focus mode button collapsed",
        "Whether focus mode button is collapsed into overflow menu",
        false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_SHARE_COLLAPSED] = g_param_spec_boolean("share-collapsed",
        "Share button collapsed",
        "Whether share button is collapsed into overflow menu",
        false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_TAB_OVERVIEW_COLLAPSED] = g_param_spec_boolean("tab-overview-collapsed",
        "Tab overview button collapsed",
        "Whether tab overview button is collapsed into overflow menu",
        false,
        GParamFlags(G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);

    

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->realize = header_bar_realize;
    widget_class->root = header_bar_root;
    gtk_widget_class_set_css_name(widget_class, "headerbar");
    gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/main_window/header/header_bar.ui");

    gtk_widget_class_bind_template_child(widget_class, HeaderBar, window_controls_start);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, window_controls_end);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, header_bar_box);
    // Note: keep in mind that GtkWindowHandle is a special widget that does not play nicely with
    // gtk_widget_dispose_template call in dispose.
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, window_handle);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, window_list_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, sidebar_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, back_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, forward_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, sidebar_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, back_forward_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, privacy_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, page_tweaks_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, website_settings_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, profiles_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, address_focus_mode_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, reader_mode_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_audio_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, rss_feeds_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, certificates_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, address_entry);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, reload_page_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, add_page_to_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_tab_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, history_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, compact_tab_bar);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, profiles_button_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, page_zoom_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, popup_blocked_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, popup_blocked_message);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, menu_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, focus_mode_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, share_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_overview_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, downloads_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, overflow_menu_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, more_toolbar_items_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, summarize_page_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, notes_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_window_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_private_window_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, print_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, web_inspector_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, manage_extensions_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, show_reader_mode_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, kagi_privacy_pass_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, zoom_in_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, zoom_out_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, home_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, start_page_button);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, privacy_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, page_tweaks_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, website_settings_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, add_page_to_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_tab_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, history_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, focus_mode_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, share_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_overview_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, downloads_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, overflow_menu_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_groups_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, home_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, summarize_page_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, notes_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_window_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_private_window_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, print_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, web_inspector_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, manage_extensions_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, show_reader_mode_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, kagi_privacy_pass_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, zoom_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, start_page_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, extension_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, toolbar_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_tab_button_popover_menu);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, start_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, middle_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, end_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, sidebar_button_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, address_entry_overlay);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_groups_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, back_forward_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, privacy_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, page_tweaks_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, website_settings_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, add_page_to_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_tab_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, downloads_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, share_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, tab_overview_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, overflow_menu_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, static_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, available_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, history_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, focus_mode_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, home_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, summarize_page_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, notes_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_window_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, new_private_window_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, print_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, web_inspector_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, manage_extensions_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, show_reader_mode_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, kagi_privacy_pass_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, zoom_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, start_page_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, window_title_label);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, compact_tab_bar_box);
    gtk_widget_class_bind_template_child(widget_class, HeaderBar, back_forward_buttons_box);

    gtk_widget_class_bind_template_callback(widget_class, on_header_bar_map);
    gtk_widget_class_bind_template_callback(widget_class, on_header_bar_unmap);
    gtk_widget_class_bind_template_callback(widget_class, get_header_bar_center_alignment);
    gtk_widget_class_bind_template_callback(widget_class, on_sidebar_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, bind_object_with_label_to_window);
    gtk_widget_class_bind_template_callback(widget_class, bind_window_icon);
    gtk_widget_class_bind_template_callback(widget_class, lookup_window_list_button_style_action);
    gtk_widget_class_bind_template_callback(widget_class, window_list_button_style_setting_to_title_visibility);
    gtk_widget_class_bind_template_callback(widget_class, window_list_button_style_setting_to_icon_visibility);
    gtk_widget_class_bind_template_callback(widget_class, toolbar_item_label_visibility);
    gtk_widget_class_bind_template_callback(widget_class, on_page_zoom_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_popup_blocked_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reader_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_tab_audio_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_focus_mode_clicked);
    gtk_widget_class_bind_template_callback(widget_class, downloads_button_content_notify_click);
    gtk_widget_class_bind_template_callback(widget_class, get_certificates_button_icon);
    gtk_widget_class_bind_template_callback(widget_class, tls_status_to_certificates_button_visibility);
    gtk_widget_class_bind_template_callback(widget_class, on_back_button_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, on_forward_button_secondary_released);
    gtk_widget_class_bind_template_callback(widget_class, claim_sequence
    );
}
