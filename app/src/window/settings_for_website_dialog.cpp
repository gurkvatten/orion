#include <gdk/gdk.h>
#include <common/src/log.h>
#include <core/core_api.h>
#include <core/settings.h>

#include "app.h"
#include "settings_for_website_dialog.h"
#include "window/main_window/main_window.h"
#include "util/text_util.h"
#include "util/compatibility_mode.h"

namespace {
    struct CppWrapper {
        std::string website;
        std::string default_page_zoom{orion::settings::websites::page_zoom_default_setting_for_all_websites.default_value_as_string()};
        std::string default_user_agent{orion::settings::websites::user_agent_default_setting_for_all_websites.default_value_as_string()};
        std::string default_auto_play{orion::settings::websites::autoplay_default_setting_for_all_websites.default_value_as_string()};
        std::string default_picture_in_picture{orion::settings::websites::picture_in_picture_default_setting_for_all_websites.default_value_as_string()};
        std::string default_pop_up_windows{orion::settings::websites::popup_windows_default_setting_for_all_websites.default_value_as_string()};
        std::string default_ssl_certificate_check{orion::settings::websites::ssl_certificate_validation_setting_for_all_websites.default_value_as_string()};
        std::string default_camera{orion::settings::websites::camera_default_setting_for_all_websites.default_value_as_string()};
        std::string default_microphone{orion::settings::websites::microphone_default_setting_for_all_websites.default_value_as_string()};
        std::string default_screen_sharing{orion::settings::websites::screen_sharing_default_setting_for_all_websites.default_value_as_string()};
        std::string default_location{orion::settings::websites::location_default_setting_for_all_websites.default_value_as_string()};
        std::string default_notifications{orion::settings::websites::notifications_default_setting_for_all_websites.default_value_as_string()};
        std::string default_downloads{orion::settings::websites::downloads_default_setting_for_all_websites.default_value_as_string()};
        std::string default_external_apps{orion::settings::websites::external_apps_default_setting_for_all_websites.default_value_as_string()};
        std::string default_page_tweaks{orion::settings::marshal_value(true)}; // this cannot be user-configured
    };
}

// TODO: rename this to SettingsForWebsiteDialog
struct _SettingsForWebsitePopover
{
	AdwDialog parent;

	GtkPopover* compatibility_help_popover;
    CppWrapper wrapper;
    GtkLabel* website_label;
    GtkButton* reset_button;
    GtkCheckButton* enable_content_blockers_button;
    GtkCheckButton* enable_intelligent_tracking_prevention_button;
    GtkCheckButton* block_web_fonts_button;
    GtkCheckButton* enable_javascript_button;
    GtkCheckButton* enable_cookies_button;
    GtkCheckButton* enable_reader_mode_button;
    GtkCheckButton* enable_page_tweaks_button;
    GtkDropDown* page_zoom_drop_down;
    GtkDropDown* user_agent_drop_down;
    GtkDropDown* auto_play_combo_box;
    GtkDropDown* picture_in_picture_combo_box;
    GtkDropDown* pop_up_windows_combo_box;
    GtkDropDown* ssl_certificate_check_combo_box;
    GtkDropDown* camera_combo_box;
    GtkDropDown* microphone_combo_box;
    GtkDropDown* screen_sharing_combo_box;
    GtkDropDown* location_combo_box;
    GtkDropDown* notifications_combo_box;
    GtkDropDown* downloads_combo_box;
    GtkDropDown* external_apps_combo_box;
    GtkSwitch* compatibility_mode_switch;
};

G_DEFINE_TYPE(SettingsForWebsitePopover, settings_for_website_popover, ADW_TYPE_DIALOG)

namespace {
    constexpr const char* programmatic_setting_change_key = "orion-programmatic-setting-change";

    struct WidgetDefaultValue {
        GtkWidget* widget{};
        std::string default_value{};
        std::string type{};
    };

    static bool
    should_ignore_setting_change(GtkWidget* widget)
    {
        return g_object_get_data(G_OBJECT(widget), programmatic_setting_change_key) != nullptr;
    }

    static void
    set_drop_down_selected_programmatically(GtkDropDown* dropdown, guint selected)
    {
        g_object_set_data(G_OBJECT(dropdown), programmatic_setting_change_key, GINT_TO_POINTER(TRUE));
        gtk_drop_down_set_selected(dropdown, selected);
        g_object_set_data(G_OBJECT(dropdown), programmatic_setting_change_key, nullptr);
    }

    static void
    set_check_button_active_programmatically(GtkCheckButton* button, bool active)
    {
        g_object_set_data(G_OBJECT(button), programmatic_setting_change_key, GINT_TO_POINTER(TRUE));
        gtk_check_button_set_active(button, active);
        g_object_set_data(G_OBJECT(button), programmatic_setting_change_key, nullptr);
    }

    static void
    set_switch_active_programmatically(GtkSwitch* sw, bool active)
    {
        g_object_set_data(G_OBJECT(sw), programmatic_setting_change_key, GINT_TO_POINTER(TRUE));
        gtk_switch_set_active(sw, active);
        g_object_set_data(G_OBJECT(sw), programmatic_setting_change_key, nullptr);
    }
}

AdwDialog*
settings_for_website_popover_new(const std::string& website)
{
    SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(
        g_object_new(SETTINGS_FOR_WEBSITE_POPOVER_TYPE, NULL)
    );
    dialog->wrapper.website = website;
    gtk_label_set_text(dialog->website_label, orion::text::translatef(_("Settings for %1$s:"), website.c_str()).c_str());
    return ADW_DIALOG(dialog);
}

static void
on_compatibility_help_clicked(GtkButton* button, gpointer user_data)
{
	auto win = SETTINGS_FOR_WEBSITE_POPOVER(user_data);

	graphene_point_t zero{};
	graphene_point_t pos{};
	if (!gtk_widget_compute_point(GTK_WIDGET(button), GTK_WIDGET(win), &zero, &pos)) {
		orion::log_warning("on_compatibility_help_clicked: failed to compute point");
	}

	// Point the popover arrow to the button's bottom (vertically) center (horizontally)
	GdkRectangle rect{
		.x = int(pos.x + gtk_widget_get_width(GTK_WIDGET(button)) / 2),
		.y = int(pos.y + gtk_widget_get_height(GTK_WIDGET(button))),
		.width = 1,
		.height = 1,
	};

	gtk_popover_set_pointing_to(win->compatibility_help_popover, &rect);
	gtk_popover_set_has_arrow(win->compatibility_help_popover, TRUE);
	gtk_popover_popup(win->compatibility_help_popover);
}

template <typename DefaultSetting, typename SpecificSetting>
static void
on_enum_drop_down_map(
    GtkDropDown* self,
    gpointer user_data,
    const DefaultSetting& default_setting,
    const SpecificSetting& specific_setting,
    std::string CppWrapper::* default_value_member)
{
    using Value = typename DefaultSetting::value_type;

    orion::gtk::WeakRefWrapper self_weak(self);
    orion::gtk::WeakRefWrapper dialog_weak(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data);
    if (!dialog) return;
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning("on_enum_drop_down_map: website is empty, cannot get setting '%s'",
            specific_setting.key_name().c_str());
        return;
    }

    global_core_api->get_setting_or_default(profile_id, specific_setting, default_setting,
        [self_weak, dialog_weak, default_value_member](orion::GetSettingReply<Value> reply){
            VERIFY_LOG(reply.err);

            auto self_strong = self_weak.lock();
            if (!self_strong) return;
            auto dialog_strong = dialog_weak.lock();
            if (!dialog_strong) return;

            auto* dialog = SETTINGS_FOR_WEBSITE_POPOVER(dialog_strong.get());
            dialog->wrapper.*default_value_member = orion::settings::marshal_value(reply.default_value);
            set_drop_down_selected_programmatically(
                GTK_DROP_DOWN(self_strong.get()),
                static_cast<guint>(reply.value) - 1
            );
        }
    );
}

template <typename DefaultSetting, typename SpecificSetting>
static void
on_enum_drop_down_changed(
    GtkDropDown* self,
    gpointer user_data,
    const DefaultSetting& default_setting,
    const SpecificSetting& specific_setting,
    std::string CppWrapper::* default_value_member)
{
    using Value = typename DefaultSetting::value_type;

    SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data);
    if (!dialog) return;
    if (should_ignore_setting_change(GTK_WIDGET(self))) return;

    Value value = static_cast<Value>(gtk_drop_down_get_selected(self) + 1);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning("on_enum_drop_down_changed: website is empty, cannot set setting '%s'",
            specific_setting.key_name().c_str());
        return;
    }

    orion::SetSettingQuery query{
        .profile_id = profile_id,
        .key = specific_setting.key(),
        .realm = specific_setting.realm(),
        .key_name = specific_setting.key_name(),
        .type = specific_setting.type(),
        .value = orion::settings::marshal_value(value),
    };

    global_core_api->set_setting_raw(query,
        [](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
        }
    );
}

#define DEFINE_ENUM_DROP_DOWN_CALLBACKS(name, default_setting, specific_setting_name, default_value_member) \
    static void \
    on_##name##_map(GtkDropDown* self, gpointer user_data) \
    { \
        auto* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data); \
        if (!dialog) return; \
        std::string website = dialog->wrapper.website; \
        on_enum_drop_down_map(self, user_data, default_setting, WEBSITE_SETTING(specific_setting_name, website), default_value_member); \
    } \
    static void \
    on_##name##_changed(GtkDropDown* self, gpointer user_data) \
    { \
        auto* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data); \
        if (!dialog) return; \
        std::string website = dialog->wrapper.website; \
        on_enum_drop_down_changed(self, user_data, default_setting, WEBSITE_SETTING(specific_setting_name, website), default_value_member); \
    } \
    static void \
    on_##name##_notify_selected(GtkDropDown* self, GParamSpec* pspec, gpointer user_data) \
    { \
        on_##name##_changed(self, user_data); \
    }

DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    page_zoom_drop_down,
    orion::settings::websites::page_zoom_default_setting_for_all_websites,
    page_zoom,
    &CppWrapper::default_page_zoom)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    user_agent_drop_down,
    orion::settings::websites::user_agent_default_setting_for_all_websites,
    user_agent,
    &CppWrapper::default_user_agent)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    auto_play_combo_box,
    orion::settings::websites::autoplay_default_setting_for_all_websites,
    autoplay_default_setting,
    &CppWrapper::default_auto_play)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    picture_in_picture_combo_box,
    orion::settings::websites::picture_in_picture_default_setting_for_all_websites,
    picture_in_picture_default_setting,
    &CppWrapper::default_picture_in_picture)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    pop_up_windows_combo_box,
    orion::settings::websites::popup_windows_default_setting_for_all_websites,
    popup_windows_default_setting,
    &CppWrapper::default_pop_up_windows)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    ssl_certificate_check_combo_box,
    orion::settings::websites::ssl_certificate_validation_setting_for_all_websites,
    ssl_certificate_validation_setting,
    &CppWrapper::default_ssl_certificate_check)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    camera_combo_box,
    orion::settings::websites::camera_default_setting_for_all_websites,
    camera_default_setting,
    &CppWrapper::default_camera)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    microphone_combo_box,
    orion::settings::websites::microphone_default_setting_for_all_websites,
    microphone_default_setting,
    &CppWrapper::default_microphone)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    screen_sharing_combo_box,
    orion::settings::websites::screen_sharing_default_setting_for_all_websites,
    screen_sharing_default_setting,
    &CppWrapper::default_screen_sharing)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    location_combo_box,
    orion::settings::websites::location_default_setting_for_all_websites,
    location_default_setting,
    &CppWrapper::default_location)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    notifications_combo_box,
    orion::settings::websites::notifications_default_setting_for_all_websites,
    notifications_default_setting,
    &CppWrapper::default_notifications)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    downloads_combo_box,
    orion::settings::websites::downloads_default_setting_for_all_websites,
    downloads_default_setting,
    &CppWrapper::default_downloads)
DEFINE_ENUM_DROP_DOWN_CALLBACKS(
    external_apps_combo_box,
    orion::settings::websites::external_apps_default_setting_for_all_websites,
    external_apps_default_setting,
    &CppWrapper::default_external_apps)

#undef DEFINE_ENUM_DROP_DOWN_CALLBACKS

template <typename E>
static void
on_enum_setting_map(GtkWidget* widget, SettingsForWebsitePopover* dialog, std::string key_name, E enabled_value)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    if (!SETTINGS_FOR_WEBSITE_IS_POPOVER(dialog)) return;
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_setting_map: cannot get website setting '%s' because website is empty",
            key_name.c_str()
        );
        return;
    }

    orion::gtk::WeakRefWrapper widget_weak(widget);

    global_core_api->get_setting_for_website_or_default({
        .profile_id = profile_id,
        .website = website,
        .key_name = key_name,
    }, [widget_weak, enabled_value](orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);

        auto widget_strong = widget_weak.lock();
        if (!widget_strong) return;
        GtkWidget* widget = GTK_WIDGET(widget_strong.get());

        if (GTK_IS_CHECK_BUTTON(widget)) {
            E value{};
            VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &value));

            set_check_button_active_programmatically(GTK_CHECK_BUTTON(widget), value == enabled_value);
        } else {
            orion::log_warning(
                "on_setting_map: widget is not a check button, cannot set value for website setting '%s'",
                reply.value.c_str()
            );
            return;
        }
    });
}

static void
on_setting_map(GtkWidget* widget, SettingsForWebsitePopover* dialog, std::string key_name)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(widget)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    if (!SETTINGS_FOR_WEBSITE_IS_POPOVER(dialog)) return;
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_setting_map: cannot get website setting '%s' because website is empty",
            key_name.c_str()
        );
        return;
    }

    orion::gtk::WeakRefWrapper widget_weak(widget);

    global_core_api->get_setting_for_website_or_default({
        .profile_id = profile_id,
        .website = website,
        .key_name = key_name,
    }, [widget_weak](orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);

        auto widget_strong = widget_weak.lock();
        if (!widget_strong) return;
        GtkWidget* widget = GTK_WIDGET(widget_strong.get());

        if (GTK_IS_CHECK_BUTTON(widget)) {
            bool is_active{};
            VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &is_active));

            set_check_button_active_programmatically(GTK_CHECK_BUTTON(widget), is_active);
        } else {
            orion::log_warning(
                "on_setting_map: widget is not a check button, cannot set value for website setting '%s'",
                reply.value.c_str()
            );
            return;
        }
    });
}

template<typename E>
static void
on_setting_toggled_enum(GtkCheckButton* self, SettingsForWebsitePopover* dialog, std::string key_name, E default_value, E disabled_value, E enabled_value)
{
    if (!dialog) return;
    if (should_ignore_setting_change(GTK_WIDGET(self))) return;

    bool is_active = gtk_check_button_get_active(self);
    
    orion::log_debug(
        "on_setting_toggled_enum: key_name=%s is_active=%s",
        key_name.c_str(),
        is_active ? "true" : "false"
    );

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_setting_toggled_bool: cannot get website setting '%s' because website is empty",
            key_name.c_str()
        );
        return;
    }

    orion::gtk::WeakRefWrapper win_weak(GTK_WIDGET(win));


    // Always set the value explicitly in backend, do not short-circuit here even if is_active == current_value.
    // This is because we need to explicitly record user-initiated setting change in the backend so that it can
    // override the default value for all websites.

    orion::SetSettingQuery query = global_core_api->create_set_setting_query_for_website_integer({
        .profile_id = profile_id,
        .key_name = key_name,
        .value = int64_t(is_active?enabled_value:disabled_value),
        .default_value = int64_t(default_value),
        .website = website,
    });

    global_core_api->set_setting_raw(query, [win_weak](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
    });
}

static void
on_setting_toggled_bool(GtkCheckButton* self, SettingsForWebsitePopover* dialog, std::string key_name, bool default_value)
{
    if (!dialog) return;
    if (should_ignore_setting_change(GTK_WIDGET(self))) return;

    bool is_active = gtk_check_button_get_active(self);
    
    orion::log_debug(
        "on_setting_toggled_bool: key_name=%s default_value=%s is_active=%s",
        key_name.c_str(),
        default_value ? "true" : "false",
        is_active ? "true" : "false"
    );

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_setting_toggled_bool: cannot get website setting '%s' because website is empty",
            key_name.c_str()
        );
        return;
    }

    orion::gtk::WeakRefWrapper win_weak(GTK_WIDGET(win));

    orion::GetSettingQuery query = global_core_api->create_get_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = key_name,
        .default_value = default_value,
        .website = website,
    });

    global_core_api->get_setting_raw(query,
    [is_active,profile_id,website,win_weak,key_name,default_value](orion::GetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);

        bool current_value{};
        VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &current_value));

        // Always set the value explicitly in backend, do not short-circuit here even if is_active == current_value.
        // This is because we need to explicitly record user-initiated setting change in the backend so that it can
        // override the default value for all websites.

        orion::SetSettingQuery query = global_core_api->create_set_setting_query_for_website_bool({
            .profile_id = profile_id,
            .key_name = key_name,
            .value = is_active,
            .default_value = default_value,
            .website = website,
        });

        global_core_api->set_setting_raw(query, [win_weak](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
        });
    });
}

static void
on_enable_content_blockers_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_content_blockers.key_name()
    );
}

static void
on_enable_content_blockers_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_content_blockers.key_name(),
        true
    );
}

static void
on_enable_intelligent_tracking_prevention_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_intelligent_tracking_prevention.key_name()
    );
}

static void
on_enable_intelligent_tracking_prevention_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_intelligent_tracking_prevention.key_name(),
        true
    );
}

static void
on_block_web_fonts_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::block_web_fonts.key_name()
    );
}

static void
on_block_web_fonts_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::block_web_fonts.key_name(),
        false
    );
}

static void
on_enable_javascript_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_javascript.key_name()
    );
}

static void
on_enable_javascript_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_javascript.key_name(),
        true
    );
}

static void
on_enable_cookies_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_cookies.key_name()
    );
}

static void
on_enable_cookies_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_cookies.key_name(),
        true
    );
}

static void
on_enable_reader_mode_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_enum_setting_map<orion::settings::ReaderModeWebsiteSetting>(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_reader_mode.key_name(),
        orion::settings::ReaderModeWebsiteSetting::on
    );
}

static void
on_enable_reader_mode_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_enum<orion::settings::ReaderModeWebsiteSetting>(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_reader_mode.key_name(),
        orion::settings::ReaderModeWebsiteSetting::not_set,
        orion::settings::ReaderModeWebsiteSetting::off,
        orion::settings::ReaderModeWebsiteSetting::on
    );
}

static void
on_enable_page_tweaks_button_map(GtkCheckButton* self, gpointer user_data)
{
    on_setting_map(
        GTK_WIDGET(self),
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_page_tweaks.key_name()
    );
}

static void
on_enable_page_tweaks_button_toggled(GtkCheckButton* self, gpointer user_data)
{
    on_setting_toggled_bool(
        self,
        SETTINGS_FOR_WEBSITE_POPOVER(user_data),
        orion::settings::website::specific::enable_page_tweaks.key_name(),
        true
    );
}

static void
on_compatibility_mode_switch_map(GtkSwitch* self, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data);
    if (!dialog) return;
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_compatibility_mode_switch_map: cannot get website setting because website is empty"
        );
        return;
    }

    orion::gtk::WeakRefWrapper self_weak(self);
    global_core_api->get_setting(
        profile_id,
        orion::settings::websites::compatibility_mode_default_setting_for_all_websites,
    [self_weak,profile_id,website]
    (orion::GetSettingReply<orion::settings::CompatibilityModeWebsiteSetting> default_reply) {
        VERIFY_LOG(default_reply.err);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;

        bool default_value = default_reply.value == orion::settings::CompatibilityModeWebsiteSetting::on;
        orion::GetSettingQuery query = global_core_api->create_get_setting_query_for_website_bool({
            .profile_id = profile_id,
            .key_name = orion::settings::website::specific::enable_compatibility_mode.key_name(),
            .default_value = default_value,
            .website = website,
        });

        global_core_api->get_setting_raw(query, [self_weak](orion::GetSettingReply<std::string> reply) {
            VERIFY_LOG(reply.err);

            bool is_active{};
            VERIFY_LOG(orion::settings::unmarshal_value(reply.value, &is_active));

            auto self_again = self_weak.lock();
            if (!self_again) return;

            set_switch_active_programmatically(GTK_SWITCH(self_again.get()), is_active);
        });
    });
}

static gboolean
on_compatibility_mode_switch_state_set(GtkSwitch* self, gboolean state, gpointer user_data)
{
    if (should_ignore_setting_change(GTK_WIDGET(self))) return FALSE;

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if (!win) return FALSE;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(user_data);
    if (!dialog) return FALSE;
    std::string website = dialog->wrapper.website;
    if (website.empty()) {
        orion::log_warning(
            "on_compatibility_mode_switch_state_set: cannot set website setting because website is empty"
        );
        return FALSE;
    }

    bool new_state = !!state;
    orion::gtk::WeakRefWrapper dialog_weak(dialog);
    orion::gtk::WeakRefWrapper win_weak(win);

    orion::SetSettingQuery query = global_core_api->create_set_setting_query_for_website_bool({
        .profile_id = profile_id,
        .key_name = orion::settings::website::specific::enable_compatibility_mode.key_name(),
        .value = new_state,
        .default_value = false,
        .website = website,
    });

    global_core_api->set_setting_raw(query,
    [new_state,dialog_weak,profile_id,website,win_weak](orion::SetSettingReply<std::string> reply) {
        VERIFY_LOG(reply.err);

        auto dialog_strong = dialog_weak.lock();
        if (!dialog_strong) return;
        SettingsForWebsitePopover* dialog = SETTINGS_FOR_WEBSITE_POPOVER(dialog_strong.get());

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());
        
        guint user_agent_idx = static_cast<guint>(new_state
            ? orion::settings::UserAgentWebsiteSetting::firefox
            : orion::settings::UserAgentWebsiteSetting::automatic)
            - 1;

        set_check_button_active_programmatically(dialog->enable_content_blockers_button, !new_state);
        set_check_button_active_programmatically(dialog->enable_intelligent_tracking_prevention_button, !new_state);
        // always set to false when compatibility mode is changed
        set_check_button_active_programmatically(dialog->block_web_fonts_button, false);
        // always set to true when compatibility mode is changed
        set_check_button_active_programmatically(dialog->enable_cookies_button, true);
        // always set to false when compatibility mode is changed
        set_check_button_active_programmatically(dialog->enable_reader_mode_button, false);
        set_check_button_active_programmatically(dialog->enable_page_tweaks_button, !new_state);
        set_drop_down_selected_programmatically(dialog->user_agent_drop_down, user_agent_idx);
        
        orion::set_enable_compatibility_mode(win, website, new_state);
    });

    return FALSE;
}

static void
on_reset_button_clicked(GtkButton* button, gpointer user_data)
{
    SettingsForWebsitePopover* self = SETTINGS_FOR_WEBSITE_POPOVER(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    if (!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    std::string website = self->wrapper.website;
    if (website.empty()) return;

    orion::gtk::WeakRefWrapper win_weak(GTK_WIDGET(win));
    orion::gtk::WeakRefWrapper self_weak(GTK_WIDGET(self));

    global_core_api->reset_website_settings({
        .profile_id = profile_id,
        .website = website,
    }, [win_weak,self_weak](orion::Error err) {
        VERIFY_LOG(err);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        SettingsForWebsitePopover* self = SETTINGS_FOR_WEBSITE_POPOVER(self_strong.get());

        on_enable_content_blockers_button_map(self->enable_content_blockers_button, self);
        on_enable_intelligent_tracking_prevention_button_map(self->enable_intelligent_tracking_prevention_button, self);
        on_block_web_fonts_button_map(self->block_web_fonts_button, self);
        on_enable_javascript_button_map(self->enable_javascript_button, self);
        on_enable_cookies_button_map(self->enable_cookies_button, self);
        on_enable_reader_mode_button_map(self->enable_reader_mode_button, self);
        on_enable_page_tweaks_button_map(self->enable_page_tweaks_button, self);
        on_compatibility_mode_switch_map(self->compatibility_mode_switch, self);
        on_page_zoom_drop_down_map(self->page_zoom_drop_down, self);
        on_user_agent_drop_down_map(self->user_agent_drop_down, self);
        on_auto_play_combo_box_map(self->auto_play_combo_box, self);
        on_picture_in_picture_combo_box_map(self->picture_in_picture_combo_box, self);
        on_pop_up_windows_combo_box_map(self->pop_up_windows_combo_box, self);
        on_ssl_certificate_check_combo_box_map(self->ssl_certificate_check_combo_box, self);
        on_camera_combo_box_map(self->camera_combo_box, self);
        on_microphone_combo_box_map(self->microphone_combo_box, self);
        on_screen_sharing_combo_box_map(self->screen_sharing_combo_box, self);
        on_location_combo_box_map(self->location_combo_box, self);
        on_notifications_combo_box_map(self->notifications_combo_box, self);
        on_downloads_combo_box_map(self->downloads_combo_box, self);
        on_external_apps_combo_box_map(self->external_apps_combo_box, self);

        global_core_api->async_after(std::chrono::milliseconds(100), [win_weak]{
            auto win_strong = win_weak.lock();
            if (!win_strong) return;
            MainWindow* win = MAIN_WINDOW(win_strong.get());

            main_window_reload_current_tab_bypass_cache(win);
        });
    });
}

// Closes the dialog if user clicks outside of it
static void
on_dialog_clicked(GtkWidget* box, gint n_press, gdouble x, gdouble y, GtkEventController* gesture)
{
    orion::log_verbose("clicked dialog");
    GtkWidget* dialog = gtk_event_controller_get_widget(gesture);
    graphene_point_t dialog_point{.x = (float)x, .y = (float)y}, box_point{};

    if(!gtk_widget_compute_point(dialog, box,  &dialog_point, &box_point))
    {
        orion::log_error("on_dialog_clicked: failed to compute point");
        return;
    }
    if(!gtk_widget_contains(box, box_point.x, box_point.y))
    {
        adw_dialog_close(ADW_DIALOG(dialog));
        gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);
    }
}

static void
on_destroy(SettingsForWebsitePopover* win)
{
	gtk_widget_unparent(GTK_WIDGET(win->compatibility_help_popover));
}

static void
settings_for_website_finalize(GObject* object)
{
    SettingsForWebsitePopover* self = SETTINGS_FOR_WEBSITE_POPOVER(object);
    
    std::destroy_at(&self->wrapper);

    G_OBJECT_CLASS(settings_for_website_popover_parent_class)->finalize(object);
}

static void
settings_for_website_popover_init(SettingsForWebsitePopover* self)
{
    std::construct_at(&self->wrapper);

	gtk_widget_init_template(GTK_WIDGET(self));
	
	gtk_widget_set_parent(GTK_WIDGET(self->compatibility_help_popover), GTK_WIDGET(self));

	g_signal_connect(self, "destroy", G_CALLBACK(on_destroy), NULL);
}

static void
settings_for_website_popover_class_init(SettingsForWebsitePopoverClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = settings_for_website_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/settings_for_website_dialog.ui");

	gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, compatibility_help_popover);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, website_label);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, reset_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_content_blockers_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_intelligent_tracking_prevention_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, block_web_fonts_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_javascript_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_cookies_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, page_zoom_drop_down);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, user_agent_drop_down);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, auto_play_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, picture_in_picture_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, pop_up_windows_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, ssl_certificate_check_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, camera_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, microphone_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, screen_sharing_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, location_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, notifications_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, downloads_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, external_apps_combo_box);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, compatibility_mode_switch);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_reader_mode_button);
    gtk_widget_class_bind_template_child(widget_class, SettingsForWebsitePopover, enable_page_tweaks_button);

	gtk_widget_class_bind_template_callback(widget_class, on_compatibility_help_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_page_zoom_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_page_zoom_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_user_agent_drop_down_map);
    gtk_widget_class_bind_template_callback(widget_class, on_user_agent_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_play_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_auto_play_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_picture_in_picture_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_picture_in_picture_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_pop_up_windows_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_pop_up_windows_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_ssl_certificate_check_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_ssl_certificate_check_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_camera_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_camera_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_microphone_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_microphone_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_screen_sharing_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_screen_sharing_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_location_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_location_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_notifications_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_notifications_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_downloads_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_downloads_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_external_apps_combo_box_map);
    gtk_widget_class_bind_template_callback(widget_class, on_external_apps_combo_box_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_content_blockers_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_content_blockers_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_intelligent_tracking_prevention_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_intelligent_tracking_prevention_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_block_web_fonts_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_block_web_fonts_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_javascript_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_javascript_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_cookies_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_cookies_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_reader_mode_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_reader_mode_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_page_tweaks_button_map);
    gtk_widget_class_bind_template_callback(widget_class, on_enable_page_tweaks_button_toggled);
    gtk_widget_class_bind_template_callback(widget_class, on_dialog_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reset_button_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_compatibility_mode_switch_map);
    gtk_widget_class_bind_template_callback(widget_class, on_compatibility_mode_switch_state_set);
}
