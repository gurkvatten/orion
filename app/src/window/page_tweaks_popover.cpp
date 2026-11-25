#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>

#include <core/settings.h>

#include <common/src/defer.h>

#include "app.h"

#include "page_tweaks_popover.h"

#include "util/action_util.h"
#include "util/settings_util.h"
#include "util/css_util.h"

#include "window/main_window/main_window.h"

namespace {
    struct CppWrapper {
        orion::ObserverId settings_observer;
    };
}

// TODO: rename this to PageTweaksDialog
struct _PageTweaksPopover
{
	GtkPopover parent;

    GtkButton* light_mode_toggle_button;
    GtkButton* dark_mode_toggle_button;

    GtkDropDown* font_drop_down;

    GtkSwitch* disable_sticky_switch;

    GtkLabel* erase_elements_label;

    GtkExpressionWatch* dark_mode_enabled_watch;
    GtkExpressionWatch* font_family_watch;
    GtkExpressionWatch* disable_sticky_watch;

    GtkExpressionWatch* uri_watch;

    CppWrapper wrapper;
};

//TODO: probably remove
// struct ZoomLevel {
//         double percentage;
//         orion::settings::PageZoom value;
//     };

// static constexpr std::array<ZoomLevel, int(enchantum::max<orion::settings::PageZoom>)> zoom_levels = {{
//         {50.0, orion::settings::PageZoom::zoom_50},   {75.0, orion::settings::PageZoom::zoom_75},
//         {80.0, orion::settings::PageZoom::zoom_80},   {85.0, orion::settings::PageZoom::zoom_85},
//         {90.0, orion::settings::PageZoom::zoom_90},   {95.0, orion::settings::PageZoom::zoom_95},
//         {100.0, orion::settings::PageZoom::zoom_100}, {115.0, orion::settings::PageZoom::zoom_115},
//         {125.0, orion::settings::PageZoom::zoom_125}, {150.0, orion::settings::PageZoom::zoom_150},
//         {175.0, orion::settings::PageZoom::zoom_175}, {200.0, orion::settings::PageZoom::zoom_200},
//         {250.0, orion::settings::PageZoom::zoom_250}, {300.0, orion::settings::PageZoom::zoom_300}
//     }};

G_DEFINE_TYPE(PageTweaksPopover, page_tweaks_popover, ADW_TYPE_DIALOG)

static void on_font_drop_down_notify_selected(GtkDropDown* drop_down, GParamSpec* pspec, PageTweaksPopover* self);
static gboolean on_disable_sticky_switch_state_set(GtkSwitch* sw, GParamSpec* pspec, PageTweaksPopover* self);

AdwDialog*
page_tweaks_popover_new()
{
	return ADW_DIALOG(g_object_new(PAGE_TWEAKS_POPOVER_TYPE, NULL));
}

static void
enable_page_tweaks_for_site(orion::ProfileId id, std::string domain)
{
    auto setting = WEBSITE_SETTING(enable_page_tweaks, domain);

    orion::DeleteSettingForWebsiteQuery query {
        .profile_id = id,
        .website = domain,
        .key_name = setting.key_name(),
    };
    global_core_api->delete_setting_for_website(query, [](orion::Error err){ VERIFY_LOG(err); });
}

static void
dark_mode_enabled_notify(gpointer data)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(data);

    GValue val = G_VALUE_INIT;
    if(gtk_expression_watch_evaluate(self->dark_mode_enabled_watch, &val))
    {
        if(g_value_get_boolean(&val))
        {
            gtk_widget_unset_state_flags(GTK_WIDGET(self->light_mode_toggle_button), GTK_STATE_FLAG_CHECKED);
            gtk_widget_set_state_flags(GTK_WIDGET(self->dark_mode_toggle_button), GTK_STATE_FLAG_CHECKED, FALSE);
        }
        else
        {
            gtk_widget_unset_state_flags(GTK_WIDGET(self->dark_mode_toggle_button), GTK_STATE_FLAG_CHECKED);
            gtk_widget_set_state_flags(GTK_WIDGET(self->light_mode_toggle_button), GTK_STATE_FLAG_CHECKED, FALSE);
        }
    }

    g_value_unset(&val);
}

static void
font_family_notify(gpointer data)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(data);

    GValue val = G_VALUE_INIT;
    //g_value_init(&val, G_TYPE_BOOLEAN);
    if(gtk_expression_watch_evaluate(self->font_family_watch, &val))
    {
        GtkStringList* list = GTK_STRING_LIST(gtk_drop_down_get_model(self->font_drop_down));
        const char* value = g_value_get_string(&val); //should never actually equal our default, but keeping that here just in case
        
        if(0 == g_strcmp0(value, gtk_string_list_get_string(list, gtk_drop_down_get_selected(self->font_drop_down))))
        {
            //noop, constant selection
        }
        else if(!value || 0 == g_strcmp0(value, gtk_string_list_get_string(list, 0)))
        {
            orion::log_warning("font_family_notify(): got default, should not happen");

            g_signal_handlers_block_matched(self->font_drop_down, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
                NULL, (gpointer)on_font_drop_down_notify_selected, self); 

            gtk_drop_down_set_selected(self->font_drop_down, 0);

            g_signal_handlers_unblock_matched(self->font_drop_down, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
                NULL, (gpointer)on_font_drop_down_notify_selected, self); 
        }
        else
        {
            guint n_items = g_list_model_get_n_items(G_LIST_MODEL(list));
            if(n_items == 1)
            {
                orion::log_warning("font_family_notify(): font list includes no actual elements");
            }
            else
            {
                //binary search based on g_utf8_collate, will fail silently if unsuccessful for now
                uint low = 1;
                uint high = n_items-1;

                while(low<=high)
                {
                    uint mid = low + (high-low)/2;

                    const char* mid_string = gtk_string_list_get_string(list, mid);

                    int cmp = g_utf8_collate(mid_string, value);

                    if(cmp == 0)
                    {
                        g_signal_handlers_block_matched(self->font_drop_down, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
                            NULL, (gpointer)on_font_drop_down_notify_selected, self); 

                        gtk_drop_down_set_selected(self->font_drop_down, mid);

                        g_signal_handlers_unblock_matched(self->font_drop_down, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
                            NULL, (gpointer)on_font_drop_down_notify_selected, self); 
                        break;
                    }
                    else if(cmp < 0)
                    {
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid - 1;
                    }
                }
            }
        }
        
    }

    g_value_unset(&val);
}

static void
disable_sticky_enabled_notify(gpointer data)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(data);

    g_signal_handlers_block_matched(self->disable_sticky_switch, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
        NULL, (gpointer)on_disable_sticky_switch_state_set, self); 
    GValue val = G_VALUE_INIT;
    if(gtk_expression_watch_evaluate(self->disable_sticky_watch, &val))
    {
        gboolean enabled = g_value_get_boolean(&val);
        gtk_switch_set_active(self->disable_sticky_switch, enabled);
        gtk_switch_set_state(self->disable_sticky_switch, enabled);
    }
    g_signal_handlers_unblock_matched(self->disable_sticky_switch, GSignalMatchType(G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC), 0, 0,
        NULL, (gpointer)on_disable_sticky_switch_state_set, self); 

    g_value_unset(&val);
}



static gboolean
get_uri_is_in_filter(PageTweaksPopover* self, const gchar* uri_c_str, const orion::DomainFilter* filter)
{
    if(!uri_c_str) return FALSE;
    if(!filter) return FALSE;

    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);

    if (uri.known_scheme() != orion::Uri::Scheme::http &&
        uri.known_scheme() != orion::Uri::Scheme::https)
    {
        return FALSE;
    }
        
    return filter->is_match_or_subdomain(uri.authority());
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static gchar*
get_font_family(PageTweaksPopover* self, const gchar* uri_c_str, const orion::TypeToWebKitUserStyleSheetMap<std::string>* font_family_map)
{
    if(!uri_c_str) return FALSE;
    if(!font_family_map) return FALSE;

    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);

    if (uri.known_scheme() != orion::Uri::Scheme::http &&
        uri.known_scheme() != orion::Uri::Scheme::https)
    {
        return FALSE;
    }

    for(auto& pair : *font_family_map)
    {
        const auto& filter_for_font_family = pair.second.first;
        if(filter_for_font_family.is_match_or_subdomain(uri.authority()))
        {
            return g_strdup(pair.first.c_str());
        }
    }
        
    return NULL;
}
#else
static gchar*
get_font_family(PageTweaksPopover* self, const gchar* uri_c_str, const void* font_family_map)
{
    return NULL;
}
#endif


static void
page_tweaks_popover_init(PageTweaksPopover* win)
{
    std::construct_at(&win->wrapper);

	gtk_widget_init_template(GTK_WIDGET(win));

    gtk_drop_down_set_model(win->font_drop_down, G_LIST_MODEL(orion_app_get_default_fonts_string_list()));
    
    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");

    GtkExpression* tab_view_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, gtk_expression_ref(root_expression), "tab-view");
    GtkExpression* uri_expression = gtk_property_expression_new(orion_webview_tab_view_get_type(), tab_view_expression, "uri");

    GtkExpression* profile_data_holder_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "profile-data-holder");
    GtkExpression* dark_mode_list_expression = gtk_property_expression_new(APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), 
        "page-tweaks-dark-mode-filter");
    GtkExpression* font_family_map_expression = gtk_property_expression_new(APP_TYPE_PROFILE_DATA_HOLDER, gtk_expression_ref(profile_data_holder_expression), 
        "page-tweaks-font-family-map");
    GtkExpression* disable_sticky_list_expression = gtk_property_expression_new(APP_TYPE_PROFILE_DATA_HOLDER, profile_data_holder_expression, 
        "page-tweaks-disable-sticky-filter");

    const int n_params = 2;
    GtkExpression* dark_mode_params[n_params] = {gtk_expression_ref(uri_expression), dark_mode_list_expression};
    GtkExpression* dark_mode_enabled_expression = gtk_cclosure_expression_new(
        G_TYPE_BOOLEAN,
        NULL,
        n_params,
        dark_mode_params,
        G_CALLBACK(get_uri_is_in_filter),
        NULL,
        NULL
    );
    win->dark_mode_enabled_watch = gtk_expression_watch(dark_mode_enabled_expression, win, dark_mode_enabled_notify, win, NULL);
    gtk_expression_unref(dark_mode_enabled_expression);

    GtkExpression* font_family_params[n_params] = {gtk_expression_ref(uri_expression), font_family_map_expression};
    GtkExpression* font_family_expression = gtk_cclosure_expression_new(
        G_TYPE_STRING,
        NULL,
        n_params,
        font_family_params,
        G_CALLBACK(get_font_family),
        NULL,
        NULL
    );
    win->font_family_watch = gtk_expression_watch(font_family_expression, win, font_family_notify, win, NULL);
    gtk_expression_unref(font_family_expression);

    GtkExpression* disable_sticky_params[n_params] = {gtk_expression_ref(uri_expression), disable_sticky_list_expression};
    GtkExpression* disable_sticky_expression = gtk_cclosure_expression_new(
        G_TYPE_BOOLEAN,
        NULL,
        n_params,
        disable_sticky_params,
        G_CALLBACK(get_uri_is_in_filter),
        NULL,
        NULL
    );
    win->disable_sticky_watch = gtk_expression_watch(disable_sticky_expression, win, disable_sticky_enabled_notify, win, NULL);
    gtk_expression_unref(disable_sticky_expression);

}

static void
on_erase_clicked(GtkButton* button, PageTweaksPopover* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    EraseElementsBar* bar = main_window_get_erase_elements_bar(win);
    erase_elements_bar_start(bar);

    adw_dialog_close(ADW_DIALOG(self));

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);
    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
}

static void
on_reset_clicked(GtkButton* button, PageTweaksPopover* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;
    
    EraseElementsBar* bar = main_window_get_erase_elements_bar(win);
    if(erase_elements_bar_get_is_running(bar))
    {
        erase_elements_bar_reset_selectors(bar);
    }
    else
    {
        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
        if(!view) return;

        orion::Uri uri = orion_webview_tab_view_get_uri(view);

        orion::DeleteSettingForWebsiteQuery query{};
        query.profile_id = main_window_get_current_profile_id(win);
        query.key_name = orion::settings::website::specific::page_tweaks_element_blocker_list.key_name();
        query.website = uri.authority();
        global_core_api->delete_setting_for_website(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
        //NOTE: I believe it is a better user experience not to reenable all page tweaks for this site forcefully from here
        //enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
    }
}

static void
on_light_mode_clicked(GtkButton* button, PageTweaksPopover* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    orion::DeleteSettingForWebsiteQuery query{};
    query.profile_id = main_window_get_current_profile_id(win);
    query.key_name = orion::settings::website::specific::page_tweaks_force_dark_mode.key_name();
    query.website = uri.authority();
    global_core_api->delete_setting_for_website(query, [](orion::Error err){
        VERIFY_LOG(err);
    });
    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
}

static void
on_dark_mode_clicked(GtkButton* button, PageTweaksPopover* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    orion::CreateSetSettingQueryForWebsiteBoolArg args{};
    args.profile_id = main_window_get_current_profile_id(win);
    args.key_name = orion::settings::website::specific::page_tweaks_force_dark_mode.key_name();
    args.website = uri.authority();
    args.value = true;
    args.default_value = orion::settings::website::specific::page_tweaks_force_dark_mode.default_value();
    orion::SetSettingQuery query = global_core_api->create_set_setting_query_for_website_bool(args);
    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
    });
    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
}

static void
increment_font_size(MainWindow* win, int64_t by)
{
    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;
    

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    auto setting = WEBSITE_SETTING(page_tweaks_font_size, uri.authority());
    orion::IncrementAndClampSettingQuery query{
        .profile_id = main_window_get_current_profile_id(win),
        .key = setting.key(),
        .realm = setting.realm(),
        .key_name = setting.key_name(),
        .by = by,
        .lower = 1,
        .upper = int64_t(enchantum::max<orion::settings::PageZoom>),
        .unset_value = int64_t(orion::settings::PageZoom::not_set),
        .starting_value = int64_t(orion::settings::PageZoom::zoom_100),
        .delete_if_eq_starting = true,
        .default_value = int64_t(setting.default_value())
    };
    global_core_api->increment_and_clamp_setting(query, [authority = uri.authority()](orion::SetSettingReply<int64_t> reply){
        VERIFY_LOG(reply.err);

        orion::log_verbose("new page tweaks font size value for website %s: %s", authority.c_str(), enchantum::to_string(orion::settings::PageZoom(reply.new_value)).data());
    });

    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
}

static void
on_decrease_font_size_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    if(!win) return;

    increment_font_size(win, -1);
}

static void
on_increase_font_size_clicked(GtkButton* button, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(button)));
    if(!win) return;

    increment_font_size(win, 1);
}

static void
on_font_drop_down_notify_selected(GtkDropDown* drop_down, GParamSpec* pspec, PageTweaksPopover* self)
{
    GtkStringObject* string_object = GTK_STRING_OBJECT(gtk_drop_down_get_selected_item(drop_down));
    if(!string_object) return;
    const gchar* string = gtk_string_object_get_string(string_object);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    if(0 == g_strcmp0(string, DEFAULT_FONT_FAMILY_NAME))
    {
        orion::DeleteSettingForWebsiteQuery query {
            .profile_id = main_window_get_current_profile_id(win),
            .website = uri.authority(),
            .key_name = orion::settings::website::specific::page_tweaks_font_family.key_name()
        };
        global_core_api->delete_setting_for_website(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
    }
    else
    {
        auto setting = WEBSITE_SETTING(page_tweaks_font_family, uri.authority());
        orion::SetSettingQuery query {
            .profile_id = main_window_get_current_profile_id(win),
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = "text",
            .value = gtk_string_object_get_string(string_object),
            .default_value = setting.default_value()
        };
        global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
        });
    }
    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
}


static gboolean
on_disable_sticky_switch_state_set(GtkSwitch* sw, GParamSpec* pspec, PageTweaksPopover* self)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return FALSE;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return FALSE;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    gboolean active = gtk_switch_get_active(sw);

    if(active)
    {
        auto setting = WEBSITE_SETTING(page_tweaks_disable_sticky, uri.authority());
        orion::SetSettingQuery query {
            .profile_id = main_window_get_current_profile_id(win),
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = "boolean",
            .value = orion::settings::marshal_value(bool(active)),
            .default_value = orion::settings::marshal_value(setting.default_value())
        };
        global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
            VERIFY_LOG(reply.err);
        });
    }
    else
    {
        orion::DeleteSettingForWebsiteQuery query{
            .profile_id = main_window_get_current_profile_id(win),
            .website = uri.authority(),
            .key_name = orion::settings::website::specific::page_tweaks_disable_sticky.key_name()
        };
        global_core_api->delete_setting_for_website(query, [](orion::Error err){
            VERIFY_LOG(err);
        });
    }
    enable_page_tweaks_for_site(main_window_get_current_profile_id(win), uri.authority());
    return TRUE;
}

static void
notify_uri(gpointer data)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(data);

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    if(!win) return;

    OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
    if(!view) return;

    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    orion::gtk::WeakRefWrapper self_weak(self);
    //there technically is a race condition here between this get and the change observer I think, minor one and only visual

    orion::GetSettingForWebsiteOrDefaultQuery query {
        .profile_id = main_window_get_current_profile_id(win),
        .website = uri.authority(),
        .key_name = orion::settings::website::specific::page_tweaks_element_blocker_list.key_name()
    };
    global_core_api->get_setting_for_website_or_default(query, [self_weak](orion::GetSettingForWebsiteOrDefaultReply reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(self_strong.get());

        unsigned long count = orion::count_css_selectors(reply.value);
        if(count > 0)
        {
            char* n_erase_elements_label = g_strdup_printf(
                ngettext("%lu element erased", "%lu  elements erased", count),
                count
            );
            defer([n_erase_elements_label]{ g_free(n_erase_elements_label); });

            gtk_label_set_label(self->erase_elements_label, n_erase_elements_label);
        }
        else
        {
            gtk_label_set_label(self->erase_elements_label, _("Erase elements"));
        }
    });
}

static void
page_tweaks_popover_map(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(page_tweaks_popover_parent_class)->map(widget);

    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(widget);

    GtkExpression* root_expression = gtk_property_expression_new(GTK_TYPE_WIDGET, NULL, "root");
    GtkExpression* tab_view_expression = gtk_property_expression_new(MAIN_WINDOW_TYPE, root_expression, "tab-view");
    GtkExpression* uri_expression = gtk_property_expression_new(ORION_WEBVIEW_TAB_VIEW_TYPE, tab_view_expression, "uri");
    self->uri_watch = gtk_expression_watch(uri_expression, self, notify_uri, self, NULL);
    gtk_expression_unref(uri_expression);

    notify_uri(self);

    orion::gtk::WeakRefWrapper self_weak(self);
    self->wrapper.settings_observer = global_core_api->settings_change().add_observer([self_weak](const orion::SettingsChange& change){
        auto self_strong = self_weak.lock();
        if(!self_strong) return;
        PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(self_strong.get());

        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        if(!win) return;

        OrionWebviewTabView* view = main_window_get_webview_tab_view(win);
        if(!view) return;

        orion::Uri uri = orion_webview_tab_view_get_uri(view);
        std::string current_site = uri.authority();

        for(const auto& updated : change.updated)
        {
            std::string website;
            if (!orion::parse_website_specific_setting_key(updated.key, &website, nullptr)) continue;

            if (website == current_site &&
                updated.key_name == orion::settings::website::specific::page_tweaks_element_blocker_list.key_name())
            {
                unsigned long count = orion::count_css_selectors(updated.new_value);
                if(count > 0)
                {
                    char* n_erase_elements_label = g_strdup_printf(
                        ngettext("%lu element erased", "%lu  elements erased", count),
                        count
                    );
                    defer([n_erase_elements_label]{ g_free(n_erase_elements_label); });

                    gtk_label_set_label(self->erase_elements_label, n_erase_elements_label);
                }
                else
                {
                    gtk_label_set_label(self->erase_elements_label, _("Erase elements"));
                }
            }
        }
    });
}

static void
page_tweaks_popover_unmap(GtkWidget* widget)
{
    GTK_WIDGET_CLASS(page_tweaks_popover_parent_class)->unmap(widget);

    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(widget);
    g_clear_pointer(&self->uri_watch, gtk_expression_watch_unwatch);
    global_core_api->settings_change().clear_observer(&self->wrapper.settings_observer);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static gchar*
get_font_size_percentage_string(PageTweaksPopover* popover, const gchar* uri_c_str, const orion::TypeToWebKitUserStyleSheetMap<orion::settings::PageZoom>* font_size_map)
{
    if(!uri_c_str) return NULL;
    if(!font_size_map) return NULL;

    orion::Uri uri = orion::Uri::parse_strict(uri_c_str);

    orion::settings::PageZoom zoom = orion::settings::PageZoom::not_set;

    if (uri.known_scheme() != orion::Uri::Scheme::http &&
        uri.known_scheme() != orion::Uri::Scheme::https)
    {
        return NULL;
    }

    for(auto& map_pair: *font_size_map)
    {
        const auto& filter_for_font_size = map_pair.second.first;
        if(filter_for_font_size.is_match_or_subdomain(uri.authority()))
        {
            zoom = map_pair.first;
            break;
        }
    }

#define CASE(percentage) \
        case orion::settings::PageZoom::zoom_##percentage: \
            return g_strdup(#percentage "%")
    switch(zoom)
    {
        case orion::settings::PageZoom::not_set:
        case orion::settings::PageZoom::zoom_100:
            return NULL;
        CASE(50);
        CASE(75);
        CASE(80);
        CASE(85);
        CASE(90);
        CASE(95);
        CASE(115);
        CASE(125);
        CASE(150);
        CASE(175);
        CASE(200);
        CASE(250);
        CASE(300);
    }
#undef CASE

    return NULL;
}
#else
static gchar*
get_font_size_percentage_string(PageTweaksPopover* popover, const gchar* uri_c_str, const gpointer nothing)
{
    return NULL;
}
#endif

static void
page_tweaks_popover_dispose(GObject* object)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(object);

    g_clear_pointer(&self->dark_mode_enabled_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->font_family_watch, gtk_expression_watch_unwatch);
    g_clear_pointer(&self->disable_sticky_watch, gtk_expression_watch_unwatch);

    G_OBJECT_CLASS(page_tweaks_popover_parent_class)->dispose(object);
}

static void
page_tweaks_popover_finalize(GObject* object)
{
    PageTweaksPopover* self = PAGE_TWEAKS_POPOVER(object);

    std::destroy_at(&self->wrapper);

    G_OBJECT_CLASS(page_tweaks_popover_parent_class)->finalize(object);
}

static void
page_tweaks_popover_class_init(PageTweaksPopoverClass* klass) //TODO: add emojichooser popover to allow for
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->dispose = page_tweaks_popover_dispose;
    object_class->finalize = page_tweaks_popover_finalize;

    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->map = page_tweaks_popover_map;
    widget_class->unmap = page_tweaks_popover_unmap;
	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/page_tweaks_popover.ui");
    gtk_widget_class_bind_template_child(widget_class, PageTweaksPopover, light_mode_toggle_button);
    gtk_widget_class_bind_template_child(widget_class, PageTweaksPopover, dark_mode_toggle_button);
    gtk_widget_class_bind_template_child(widget_class, PageTweaksPopover, font_drop_down);
    gtk_widget_class_bind_template_child(widget_class, PageTweaksPopover, disable_sticky_switch);
    gtk_widget_class_bind_template_child(widget_class, PageTweaksPopover, erase_elements_label);
    gtk_widget_class_bind_template_callback(widget_class, on_erase_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_reset_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_light_mode_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_dark_mode_clicked);
    // gtk_widget_class_bind_template_callback(widget_class, on_font_size_spin_button_input);
    // gtk_widget_class_bind_template_callback(widget_class, on_font_size_spin_button_output);
    // gtk_widget_class_bind_template_callback(widget_class, on_font_size_spin_button_value_changed);
    gtk_widget_class_bind_template_callback(widget_class, on_decrease_font_size_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_increase_font_size_clicked);
    gtk_widget_class_bind_template_callback(widget_class, on_disable_sticky_switch_state_set);
    gtk_widget_class_bind_template_callback(widget_class, on_font_drop_down_notify_selected);
    gtk_widget_class_bind_template_callback(widget_class, get_font_attributes_for_string_list_item);
    gtk_widget_class_bind_template_callback(widget_class, get_font_label_for_string_list_item);
    gtk_widget_class_bind_template_callback(widget_class, get_font_size_percentage_string);
}