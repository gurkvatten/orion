#define ENABLE_VERBOSE_LOGS 0

#include <gtk/gtk.h>
#include <cstring>
#include <list>

#include <common/src/log.h>
#include <common/src/uri.h>
#include <core/settings.h>
#include <common/src/defer.h>
#include <common/src/uri.h>
#include <common/src/time_util.h>

#include "app.h"

#include "util/ref_util.h"
#include "util/menu_util.h"
#include "util/tab_context_menu.h"
#include "util/action_util.h"

#include "data_item/back_forward_list.h"

#include "orion_webview_tab_view.h"
#include "orion_webview_tab_content.h"
#include "orion_webview_tab_container.h"

#include "window/main_window/main_window.h"
#include "window/main_menu/main_menu.h"

namespace {
    struct CppWrapper {
        std::optional<orion::ObserverId> tabs_change_observer;
        orion::ObserverId windows_change_observer;
        std::vector<UNIQUE_PTR(GBinding, g_binding_unbind)> page_property_bindings;
        std::string title;
        orion::Uri uri;
        orion::gtk::WeakRefWrapper prev_selected_page_child_weak = orion::gtk::WeakRefWrapper(NULL);
        orion::TabId id_to_select = orion::TabId(0);
    };
}

struct _OrionWebviewTabView
{
	AdwBin parent;

    //the actual tab view, here as a child because tab view is not derivable
    AdwTabView* tab_view;

    GtkButton* back_button;
    GtkButton* forward_button;
    GtkButton* reload_button;


    const char* title;

    //actions to go forward and backwards
    
    gulong notify_window_id_handler_id;

    //signals that next tab moved into/in this tab view should be moved along with its children
    bool move_children_to_parent = false;
    bool group_next_tab_with_existing = false;
    bool is_tab_group = false;
    bool is_readerable = false;
    bool reader_mode_enabled = false;

    //aggregate audio state across every tab in this view, kept live via per-container notify listeners
    bool any_tab_playing_audio = false;
    bool any_tab_playing_audio_unmuted = false;

    CppWrapper cpp_data;
};

enum {
    PROP_0,
    PROP_IS_TAB_GROUP,
    PROP_TITLE,
    PROP_URI,
    PROP_IS_READERABLE,
    PROP_READER_MODE_ENABLED,
    PROP_CAN_GO_BACK,
    PROP_CAN_GO_FORWARD,
    PROP_ANY_TAB_PLAYING_AUDIO,
    PROP_ANY_TAB_PLAYING_AUDIO_UNMUTED,
    PROP_TLS_STATUS,
    PROP_BACK_LIST,
    PROP_FORWARD_LIST,
    N_PROPS,
};

static GParamSpec* obj_properties[N_PROPS];

// We derive from AdwBin because AdwTabView is final and cannot be derived from.
G_DEFINE_TYPE(OrionWebviewTabView, orion_webview_tab_view, ADW_TYPE_BIN)

// Predeclarations of private methods
static void
sync_selected_page_to_backend(AdwTabView* view, GParamSpec* pspec, gpointer user_data);

//region Externally usable methods

AdwTabPage*
orion_webview_tab_view_get_selected_page(OrionWebviewTabView* view)
{
    return adw_tab_view_get_selected_page(ORION_WEBVIEW_TAB_VIEW(view)->tab_view);
}

AdwTabPage*
orion_webview_tab_view_get_page(OrionWebviewTabView* view, GtkWidget* content)
{
    return orion_webview_tab_view_get_page_with_tab_id(view, orion_webview_tab_content_get_id(ORION_WEBVIEW_TAB_CONTENT(content)));
}

int
orion_webview_tab_view_get_n_pages(OrionWebviewTabView* view)
{
    return adw_tab_view_get_n_pages(ORION_WEBVIEW_TAB_VIEW(view)->tab_view);
}

AdwTabView*
orion_webview_tab_view_get_actual_view(OrionWebviewTabView* view)
{
    return (ORION_WEBVIEW_TAB_VIEW(view)->tab_view);
}

void
orion_webview_tab_view_set_selected_page(OrionWebviewTabView* view, AdwTabPage* page)
{
    AdwTabView* tab_view = ORION_WEBVIEW_TAB_VIEW(view)->tab_view;
    adw_tab_view_set_selected_page(tab_view, page);
}

void
orion_webview_tab_view_set_transfer_next_tab_with_children(OrionWebviewTabView* view, bool val)
{
    view->move_children_to_parent = val;
}

static void
orion_webview_tab_view_set_selected_page_silent(OrionWebviewTabView* view, AdwTabPage* page)
{
    AdwTabView* tab_view = ORION_WEBVIEW_TAB_VIEW(view)->tab_view;

    g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)sync_selected_page_to_backend, NULL);
    adw_tab_view_set_selected_page(tab_view, page);
    g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)sync_selected_page_to_backend, NULL);
}

// convenience method to get page by tab id
static AdwTabPage*
orion_adw_tab_view_get_page_with_tab_id(AdwTabView* tab_view, orion::TabId tab_id)
{
    int n_pages = adw_tab_view_get_n_pages(tab_view);
    for (int i = 0; i < n_pages; i++) {
        AdwTabPage* page = adw_tab_view_get_nth_page(tab_view, i);
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        if (container && orion_webview_tab_container_get_id(container) == tab_id) {
            return page;
        }
    }
    return NULL;
}

AdwTabPage*
orion_webview_tab_view_get_page_with_tab_id(OrionWebviewTabView* view, orion::TabId tab_id)
{
    AdwTabView* tab_view = ORION_WEBVIEW_TAB_VIEW(view)->tab_view;
    return orion_adw_tab_view_get_page_with_tab_id(tab_view, tab_id);
}

void
orion_webview_tab_view_select_next_created_tab_with_id(OrionWebviewTabView* view, orion::TabId id)
{
    view->cpp_data.id_to_select = id;
}

void
orion_webview_tab_view_transfer_page(
    OrionWebviewTabView* view, AdwTabPage* page, int pos, AdwTabView* original_view,
    bool transfer_with_children, bool group_with_existing)
{

    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);
    GtkRoot* target_root = gtk_widget_get_root(GTK_WIDGET(view));
    if (!target_root) {
        orion::log_warning("orion_webview_tab_view_transfer_page: target view has no root widget");
        return;
    }
    MainWindow* target_win = MAIN_WINDOW(target_root);
    orion::WindowId target_window_id = main_window_get_window_id(target_win);

    orion::log_verbose("orion_webview_tab_view_transfer_page: pos=%d tab_id=%lld target_window_id=%lld"
        " transfer_with_children=%d group_with_existing=%d",
        pos, tab_id.value, target_window_id.value, transfer_with_children, group_with_existing
    );

    orion::CanMoveTabQuery query{
        .tab_id = tab_id,
        .target_window_id = target_window_id,
        .target_position = pos,
        .group_with_existing = group_with_existing,
    };

    orion::gtk::WeakRefWrapper view_weak(view);
    orion::gtk::WeakRefWrapper page_weak(page);
    orion::gtk::WeakRefWrapper original_view_weak(original_view);

    global_core_api->can_move_tab(query,
        [view_weak,page_weak,original_view_weak,pos,tab_id,target_window_id,group_with_existing,transfer_with_children]
        (orion::CanMoveTabReply reply){
            VERIFY_LOG(reply.err);

            auto view_strong = view_weak.lock();
            if (!view_strong) return;
            OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

            auto page_strong = page_weak.lock();
            if (!page_strong) return;
            AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

            auto original_view_strong = original_view_weak.lock();
            if (!original_view_strong) return;
            AdwTabView* original_view = ADW_TAB_VIEW(original_view_strong.get());

            orion::log_verbose("orion_webview_tab_view_transfer_page: tab_id=%lld target_window_id=%lld can_move_tab=%d",
                tab_id.value, target_window_id.value, reply.can_move
            );

            if (!reply.can_move) {
                return;
            }


            view->move_children_to_parent = transfer_with_children;
            view->group_next_tab_with_existing = group_with_existing;
            //if group with existing is true, pos is the position of the parent so we need to calculate
            //whether to transfer to pos or pos+1
            int tmp_pos = pos;
            if(group_with_existing)
            {
                if((original_view != orion_webview_tab_view_get_actual_view(view)) ||
                (adw_tab_view_get_page_position(original_view, page)>pos))
                {
                    tmp_pos++;
                }
            }
            
            orion::log_verbose("orion_webview_tab_view_transfer_page: tab_id=%lld target_window_id=%lld to_pos=%d",
                tab_id.value, target_window_id.value, tmp_pos
            );
            adw_tab_view_transfer_page(original_view, page, view->tab_view, tmp_pos);
        });
}

OrionWebviewTabContent*
orion_webview_tab_view_get_current_tab_content(OrionWebviewTabView* self)
{
    AdwTabPage* page = adw_tab_view_get_selected_page(ORION_WEBVIEW_TAB_VIEW(self)->tab_view);
    if(page == NULL)
    {
        return NULL;
    }
    else
    {
        return orion_webview_tab_container_get_content(ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page)));
    }
}

orion::TlsStatus
orion_webview_tab_view_get_tls_status(OrionWebviewTabView* self)
{
    AdwTabPage* page = adw_tab_view_get_selected_page(ORION_WEBVIEW_TAB_VIEW(self)->tab_view);
    if(page == NULL)
    {
        return orion::TlsStatus::none;
    }
    else
    {
        return orion_webview_tab_container_get_tls_status(ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page)));
    }
}

orion::BackForwardList*
orion_webview_tab_view_get_back_list(OrionWebviewTabView* self)
{
    AdwTabPage* page = adw_tab_view_get_selected_page(ORION_WEBVIEW_TAB_VIEW(self)->tab_view);
    if(page == NULL)
    {
        return NULL;
    }
    else
    {
        return orion_webview_tab_container_get_back_list(ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page)));
    }
}

orion::BackForwardList*
orion_webview_tab_view_get_forward_list(OrionWebviewTabView* self)
{
    AdwTabPage* page = adw_tab_view_get_selected_page(ORION_WEBVIEW_TAB_VIEW(self)->tab_view);
    if(page == NULL)
    {
        return NULL;
    }
    else
    {
        return orion_webview_tab_container_get_forward_list(ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page)));
    }
}

//endregion



//region Navigation methods

gboolean
orion_webview_tab_view_can_go_back(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        return orion_webview_tab_content_can_go_back(current_page);
    }
    else
    {
        return false;
    }
}

gboolean
orion_webview_tab_view_can_go_forward(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        return orion_webview_tab_content_can_go_forward(current_page);
    }
    else
    {
        return false;
    }
}

void
orion_webview_tab_view_go_back(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_go_back(current_page);
    }
}

void
orion_webview_tab_view_go_forward(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_go_forward(current_page);
    }
}

void
orion_webview_tab_view_go_back_forward_relative_index(OrionWebviewTabView* view, int64_t i)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_navigate_to_relative_bf_list_index(current_page, i);
    }
}

void
orion_webview_tab_view_reload(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_reload(current_page);
    }
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
void
orion_webview_tab_view_show_web_inspector(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_show_web_inspector(current_page);
    }
}

void
orion_webview_tab_view_toggle_web_inspector(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_toggle_web_inspector(current_page);
    }
}
#endif

void
orion_webview_tab_view_toggle_reader_mode(OrionWebviewTabView* view){
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
    if(!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    const auto setting = WEBSITE_SETTING(enable_reader_mode, uri.authority());

    orion::SetSettingQuery query{
            .profile_id = profile_id,
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .default_value = setting.default_value_as_string()
    };
    if(view->reader_mode_enabled)
    {
        query.value = orion::settings::marshal_value(orion::settings::ReaderModeWebsiteSetting::off);
    }
    else
    {
        query.value = orion::settings::marshal_value(orion::settings::ReaderModeWebsiteSetting::on);
    }
    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
        orion::log_verbose("set reader mode setting to value: %s", reply.new_value.c_str());
    });
}

void
orion_webview_tab_view_exit_reader_mode(OrionWebviewTabView* view){
    if(!view->reader_mode_enabled) return;
    
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
    if(!win) return;
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::Uri uri = orion_webview_tab_view_get_uri(view);

    const auto setting = WEBSITE_SETTING(enable_reader_mode, uri.authority());

    orion::SetSettingQuery query{
            .profile_id = profile_id,
            .key = setting.key(),
            .realm = setting.realm(),
            .key_name = setting.key_name(),
            .type = setting.type(),
            .value = orion::settings::marshal_value(orion::settings::ReaderModeWebsiteSetting::off),
            .default_value = setting.default_value_as_string()
    };
    global_core_api->set_setting_raw(query, [](orion::SetSettingReply<std::string> reply){
        VERIFY_LOG(reply.err);
        orion::log_verbose("set reader mode setting to value: %s", reply.new_value.c_str());
    });
}

//endregion



//region Getters

orion::Uri
orion_webview_tab_view_get_uri(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        return orion_webview_tab_content_get_uri(current_page);
    }
    else
    {
        return orion::Uri::parse_strict("");
    }
}

orion::Uri
orion_webview_tab_view_get_visual_uri(OrionWebviewTabView* view)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion::Uri uri = orion_webview_tab_content_get_uri(current_page);

        //masks 
        if (orion_webview_tab_content_can_go_back(current_page) ||
            uri.uri() != "orion://newtab/")
        {
            return uri;
        }
        else
        {
            return orion::Uri::parse_strict("");
        }
    }
    else
    {
        return orion::Uri::parse_strict("");
    }
}

const char*
orion_webview_tab_view_get_title(OrionWebviewTabView* view)
{
    return ORION_WEBVIEW_TAB_VIEW(view)->cpp_data.title.c_str();
}

gboolean
orion_webview_tab_view_get_is_readerable(OrionWebviewTabView* view)
{
    return view->is_readerable;
}

//endregion



//region Fake callbacks used to update ui

void
orion_webview_tab_view_navigation_notify(OrionWebviewTabView* view)
{
    //TODO: handle this using the backend instead
    //TODO: possibly warn instead of just return
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_VIEW(view));

    //reload button state
    if(view->back_button!=NULL) gtk_widget_set_sensitive(GTK_WIDGET(view->back_button), orion_webview_tab_view_can_go_back(view));
    if(view->forward_button!=NULL) gtk_widget_set_sensitive(GTK_WIDGET(view->forward_button), orion_webview_tab_view_can_go_forward(view));

    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_CAN_GO_BACK]);
    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_CAN_GO_FORWARD]);
}

//endregion



//region G_CALLBACKs


static void
on_page_selected(AdwTabView* self, GParamSpec* pspec, gpointer user_data)
{
    orion::log_verbose("orion_webview_tabview: on_page_selected");
    AdwTabPage* page = adw_tab_view_get_selected_page(self);
    if (!page) {
        orion::log_warning("on_page_selected: no page selected");
        return;
    }

    GtkWidget* child = adw_tab_page_get_child(page);
    //orion_webview_tab_view_uri_changed(ORION_WEBVIEW_TAB_VIEW(user_data), ADW_BIN(page_content), orion_webview_tab_content_get_uri(page_content));
    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(user_data);
    auto& property_bindings = view->cpp_data.page_property_bindings;
    property_bindings.clear();

    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "tab-title", view, "title", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "uri", view, "uri", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "tls-status", view, "tls-status", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "is-readerable", view, "is-readerable", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "reader-mode-enabled", view, "reader-mode-enabled", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "back-list", view, "back-list", G_BINDING_SYNC_CREATE)));
    property_bindings.emplace_back(g_object_ref(g_object_bind_property(child, "forward-list", view, "forward-list", G_BINDING_SYNC_CREATE)));

    orion::gtk::GObjectPtr prev_selected_child_strong = view->cpp_data.prev_selected_page_child_weak.lock();
    if(prev_selected_child_strong)
    {
        g_signal_handlers_disconnect_by_data(prev_selected_child_strong.get(), view);
    }
    orion_webview_tab_view_navigation_notify(ORION_WEBVIEW_TAB_VIEW(user_data));
    g_signal_connect_object(child, "notify::can-go-back", G_CALLBACK(orion_webview_tab_view_navigation_notify), view, G_CONNECT_SWAPPED);
    g_signal_connect_object(child, "notify::can-go-forward", G_CALLBACK(orion_webview_tab_view_navigation_notify), view, G_CONNECT_SWAPPED);
    view->cpp_data.prev_selected_page_child_weak.reset(child);

    OrionWebviewTabContent* content = orion_webview_tab_view_get_current_tab_content(view);
    if (content) {
        bool edit_text_on_page = orion_webview_tab_content_get_edit_text_on_page(content);
        GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(view));
        if (root && MAIN_IS_WINDOW(root)) {
            MainWindow* win = MAIN_WINDOW(root);
            // Reflect the edit_text_on_page state in the action map so that main menu Tools -> Edit Text on Page
            // is updated accordingly
            orion::gtk::set_boolean_action_state(G_ACTION_MAP(win), "edit-text-on-page", edit_text_on_page);
        }
    }
}

static void
recompute_audio_aggregate(OrionWebviewTabView* view)
{
    bool any_playing = false;
    bool any_playing_unmuted = false;
    int n_pages = adw_tab_view_get_n_pages(view->tab_view);
    for(int i = 0; i < n_pages; i++)
    {
        AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, i);
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
        if(orion_webview_tab_container_get_is_playing_audio(container))
        {
            any_playing = true;
            if(!orion_webview_tab_container_get_is_muted(container))
            {
                any_playing_unmuted = true;
            }
        }
    }
    view->any_tab_playing_audio = any_playing;
    view->any_tab_playing_audio_unmuted = any_playing_unmuted;
    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_ANY_TAB_PLAYING_AUDIO]);
    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_ANY_TAB_PLAYING_AUDIO_UNMUTED]);
}

static void
on_audio_aggregate_container_notify(OrionWebviewTabContainer* container, GParamSpec* pspec, gpointer user_data)
{
    // user_data is the AdwTabView widget, not the OrionWebviewTabView, on purpose -- see comment
    // in on_audio_aggregate_page_attached() below.
    AdwTabView* tab_view = ADW_TAB_VIEW(user_data);
    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(gtk_widget_get_ancestor(GTK_WIDGET(tab_view), ORION_WEBVIEW_TAB_VIEW_TYPE));
    if(view) recompute_audio_aggregate(view);
}

static void
on_audio_aggregate_page_attached(AdwTabView* self, AdwTabPage* page, gint position, gpointer user_data)
{
    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(user_data);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    // Tie these listeners to `self` (the AdwTabView widget), not `view`: on_page_selected() calls
    // g_signal_handlers_disconnect_by_data(prev_selected_child, view) whenever the selected tab
    // changes, which would otherwise silently rip these out for every tab as soon as it stops
    // being the selected one.
    g_signal_connect_object(container, "notify::is-playing-audio", G_CALLBACK(on_audio_aggregate_container_notify), self, G_CONNECT_DEFAULT);
    g_signal_connect_object(container, "notify::is-muted", G_CALLBACK(on_audio_aggregate_container_notify), self, G_CONNECT_DEFAULT);
    recompute_audio_aggregate(view);
}

static void
on_audio_aggregate_page_detached(AdwTabView* self, AdwTabPage* page, gint position, gpointer user_data)
{
    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(user_data);
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    g_signal_handlers_disconnect_by_data(container, self);
    recompute_audio_aggregate(view);
}

static void
on_back_button_clicked(GtkButton* button, gpointer user_data)
{
    orion_webview_tab_view_go_back(ORION_WEBVIEW_TAB_VIEW(user_data));
}

static void
on_forward_button_clicked(GtkButton* button, gpointer user_data)
{
    orion_webview_tab_view_go_forward(ORION_WEBVIEW_TAB_VIEW(user_data));
}

static void
on_reload_button_clicked(GtkButton* button, gpointer user_data)
{
    orion_webview_tab_view_reload(ORION_WEBVIEW_TAB_VIEW(user_data));
}

static void
sync_selected_page_to_backend(AdwTabView* view, GParamSpec* pspec, gpointer user_data)
{
    GtkRoot* root = gtk_widget_get_root(GTK_WIDGET(view));
    if (!MAIN_IS_WINDOW(root)) return;

    MainWindow* win = MAIN_WINDOW(root);
    AdwTabPage* page = adw_tab_view_get_selected_page(view);
    if (!page) return;
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::WindowId window_id = main_window_get_window_id(win);

    orion::gtk::WeakRefWrapper win_weak(G_OBJECT(win));
    orion::gtk::WeakRefWrapper container_weak(container);

    global_core_api->get_windows({window_id}, [win_weak,window_id,container_weak](orion::GetWindowsReply reply){
        VERIFY_LOG(reply.err);
        if (reply.windows.empty()) return;
        const orion::WindowDto& window = reply.windows[0];
        orion::TabId old_tab_id = window.selected_tab_id;

        auto win_strong = win_weak.lock();
        if (!win_strong) return;
        MainWindow* win = MAIN_WINDOW(win_strong.get());

        auto container_strong = container_weak.lock();
        if (!container_strong) return;
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(container_strong.get());

        // Mark current time as last active time of the old/previously selected tab
        OrionWebviewTabContainer* old_container = main_window_get_orion_webview_tab_container_by_tab_id(
            win, old_tab_id
        );
        if (old_container) {
            orion_webview_tab_container_set_last_active_time(old_container, orion::tm::unix_time_utc64_now());
        }
        
        orion::UpdateWindowQuery query{};
        query.id = window_id;
        query.selected_tab_id = orion_webview_tab_container_get_id(container);
        query.update_selected_tab_id = true;

        global_core_api->update_window(query, [win_weak,tab_id=query.selected_tab_id](orion::Error err){
            VERIFY_LOG(err);

            global_core_api->get_tabs({tab_id}, [win_weak,tab_id](orion::GetTabsReply reply){
                VERIFY_LOG(reply.err);
                if (reply.tabs.empty()) return;
                orion::TabDto tab = reply.tabs[0];
                
                // unsuspend the tab if it is suspended
                if (tab.is_suspended) {
                    orion::UpdateTabQuery query{
                        .id = tab.id,
                        .is_suspended = false,
                        .update_is_suspended = true,
                    };
                    global_core_api->update_tab(query, [win_weak,tab_id=tab.id](orion::Error err){
                        VERIFY_LOG(err);
                        auto win_ptr = win_weak.lock();
                        if (!win_ptr) return;
                        MainWindow* win = MAIN_WINDOW(win_ptr.get());
                        main_window_reload_tab_by_tab_id(win, tab_id);
                    });
                }
            });
        });
    });
}

static gboolean
on_tab_view_close_page(AdwTabView* self, AdwTabPage* page, gpointer user_data)
{
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::WindowId window_id = main_window_get_window_id(win);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    int n_pages = adw_tab_view_get_n_pages(self);

    gboolean is_last_page = n_pages == 1;

    if(is_last_page)
    {
        orion::gtk::WeakRefWrapper self_weak(G_OBJECT(self));
        orion::gtk::WeakRefWrapper page_weak(G_OBJECT(page));

        global_core_api->get_setting(profile_id,
            orion::settings::tabs::close_window_with_last_tab, 
            [self_weak,page_weak](orion::GetSettingReply<bool> reply){
                VERIFY_LOG(reply.err);
                bool close_window_with_last_tab = reply.value;

                auto page_strong = page_weak.lock();
                if (!page_strong) return;
                AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());
                OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
                if(!content) return;
                orion_webview_tab_content_close(content,
                    [close_window_with_last_tab,self_weak,page_weak](orion::Error err){
                        // on error, adw_tab_view_close_page_finish below will not be called,
                        // so the tab will remain open
                        VERIFY_LOG(err);

                        auto self_strong = self_weak.lock();
                        if (!self_strong) return;
                        AdwTabView* self = ADW_TAB_VIEW(self_strong.get());

                        auto page_strong = page_weak.lock();
                        if (!page_strong) return;
                        AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

                        OrionWebviewTabView* orion_tab_view = ORION_WEBVIEW_TAB_VIEW(
                            gtk_widget_get_ancestor(GTK_WIDGET(self), ORION_WEBVIEW_TAB_VIEW_TYPE)
                        );

                        if (!close_window_with_last_tab)
                        {
                            orion_webview_tab_view_new_tab(orion_tab_view);
                        }
                        adw_tab_view_close_page_finish(self, page, true);
                        if (close_window_with_last_tab)
                        {
                            GtkRoot* win = gtk_widget_get_root(GTK_WIDGET(self));
                            if(win) gtk_window_close(GTK_WINDOW(win));
                        }
                    });
            });
    }
    else
    {
        orion::gtk::WeakRefWrapper self_weak(self);
        orion::gtk::WeakRefWrapper page_weak(page);

        global_core_api->get_windows({window_id}, [profile_id,self_weak,page_weak](orion::GetWindowsReply reply){
            VERIFY_LOG(reply.err);

            if (reply.windows.empty()) {
                orion::log_warning("orion_webview_tab_view: on_tab_view_close_page: window not found");
                return;
            }
            orion::WindowDto window = reply.windows.at(0);

            global_core_api->get_setting(profile_id,
                orion::settings::tabs::return_to_last_active_tab_when_closing_current_tab, 
                [self_weak,page_weak,window](orion::GetSettingReply<bool> reply){
                    VERIFY_LOG(reply.err);

                    bool return_to_last_active_tab = reply.value;

                    auto page_strong = page_weak.lock();
                    if (!page_strong) return;
                    AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                    OrionWebviewTabContent* content = orion_webview_tab_container_get_content(container);
                    if(!content) return;
                    orion_webview_tab_content_close(content,
                        std::move([self_weak,page_weak,return_to_last_active_tab,window]
                        (orion::Error err){
                            // on error, adw_tab_view_close_page_finish below will not be called,
                            // so the tab will remain open
                            VERIFY_LOG(err);

                            auto self_strong = self_weak.lock();
                            if (!self_strong) return;
                            AdwTabView* self = ADW_TAB_VIEW(self_strong.get());

                            auto page_strong = page_weak.lock();
                            if (!page_strong) return;
                            AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

                            if (return_to_last_active_tab) {
                                AdwTabPage* prev_page = orion_adw_tab_view_get_page_with_tab_id(
                                    self, window.prev_selected_tab_id
                                );
                                if (prev_page) {
                                    adw_tab_view_set_selected_page(self, prev_page);
                                }
                            } else {
                                // if return_to_last_active_tab_when_closing_current setting is false, then default
                                // Adwaita behavior of selecting next page after close is fine
                            }
                            adw_tab_view_close_page_finish(self, page, true);
                        }));
                });
        });
    }


    return GDK_EVENT_STOP;
}

static void
on_tab_page_pinned_changed(AdwTabPage* page, GParamSpec* pspec, gpointer user_data)
{
    orion::log_verbose("orion_webview_tabview: on_tab_page_pinned_changed");
}

static void
on_tab_view_page_attached(AdwTabView* self, AdwTabPage* page, gint position, gpointer user_data)
{
    //TODO: check whether tab page is a child of self
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(user_data);
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::WindowId window_id = main_window_get_window_id(win);

    // double seq_num;
    bool is_pinned = adw_tab_page_get_pinned(page);
    int n_pinned_pages = adw_tab_view_get_n_pinned_pages(self);
    int n_pages = adw_tab_view_get_n_pages(self);
    int n_unpinned_pages = n_pages-n_pinned_pages;

    orion::TabId tab_after{};
    orion::TabId parent_id{};
    int position_in_unpinned = position-n_pinned_pages;
    if((!is_pinned) && n_unpinned_pages>1)
    {
        if(position_in_unpinned==0)
        {
            AdwTabPage* next_page = adw_tab_view_get_nth_page(self, position+1);
            OrionWebviewTabContainer* next_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(next_page));
            tab_after = orion_webview_tab_container_get_id(next_container);
            parent_id = 0;
        }
        else if(position_in_unpinned == n_unpinned_pages-1)
        {
            if(view->group_next_tab_with_existing)
            {
                AdwTabPage* prev_page = adw_tab_view_get_nth_page(self, position-1);
                OrionWebviewTabContainer* prev_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(prev_page));
                parent_id = orion_webview_tab_container_get_id(prev_container);
            }
            else
            {
                parent_id = 0;
            }
        }
        else
        {
            AdwTabPage* prev_page = adw_tab_view_get_nth_page(self, position-1);
            OrionWebviewTabContainer* prev_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(prev_page));
            AdwTabPage* next_page = adw_tab_view_get_nth_page(self, position+1);
            OrionWebviewTabContainer* next_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(next_page));
            tab_after = orion_webview_tab_container_get_id(next_container);
            if(view->group_next_tab_with_existing)
            {
                parent_id = orion_webview_tab_container_get_id(prev_container); // TODO: check if this is correct
            }
            else
            {
                parent_id = orion_webview_tab_container_get_parent_id(next_container);
            }
        }
    }
    else
    {
        //TODO: validate that a next page actually exists
        parent_id = 0;
        if(position + 1 < n_pages)
        {
            AdwTabPage* next_page = adw_tab_view_get_nth_page(self, position+1);
            OrionWebviewTabContainer* next_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(next_page));
            tab_after = orion_webview_tab_container_get_id(next_container);
        }
        else
        {
            tab_after = 0;
        }
    }
    if(!container) return;
    orion_webview_tab_container_move_to_window(container, window_id, tab_after, parent_id, view->move_children_to_parent);
    view->group_next_tab_with_existing = false;
    view->move_children_to_parent = false;
}

static void
on_tab_view_page_reordered_after_create_window(AdwTabView* self, AdwTabPage* page, gint position, gpointer user_data)
{
    MainWindow* this_win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
    orion::ProfileId profile_id = main_window_get_current_profile_id(this_win);

    // The newly created window should never be private in this case
    orion::CreateWindowQuery query{
        .profile_id = profile_id,
        .icon_name = "window-flat-symbolic",
        .is_tab_group = true,
    };

    auto view_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);
    auto page_ref = std::make_shared<orion::gtk::WeakRefWrapper>(page);
    global_core_api->create_window(query,[view_ref, page_ref, profile_id](orion::CreateWindowReply reply){
        VERIFY_LOG(reply.err);
        AdwTabView* old_view = ADW_TAB_VIEW(view_ref->get());
        AdwTabPage* page = ADW_TAB_PAGE(page_ref->get());
        if(old_view && page)
        {
            MainWindow* new_win = main_window_new(
                g_application_get_default(), profile_id, reply.id, orion::PrivateMode::no); // never private

            OrionWebviewTabView* orion_webview_tab_view = main_window_get_webview_tab_view(new_win);
            gtk_window_present(GTK_WINDOW(new_win));
            orion_webview_tab_view_transfer_page(orion_webview_tab_view, page, 0, old_view, false, false);
        }


        if(old_view) g_object_unref(old_view);
        if(page) g_object_unref(page);
    });
    g_signal_handlers_unblock_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);
    g_signal_handlers_disconnect_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)on_tab_view_page_reordered_after_create_window, NULL);
}

static AdwTabView*
on_tab_view_create_window(AdwTabView* self, gpointer user_data)
{
    g_signal_connect(self, "page-attached", G_CALLBACK(on_tab_view_page_reordered_after_create_window), NULL);

    g_signal_handlers_block_matched(self, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);
    return(self);
}

//endregion



//region Constructor and setup methods

void
orion_webview_tab_view_set_back_button(OrionWebviewTabView* view, GtkButton* button)
{
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_VIEW(view));
    if(view->back_button)
    {
        g_signal_handlers_disconnect_by_data(view->back_button, view);
    }
    view->back_button = GTK_BUTTON(button);
    g_signal_connect_object(view->back_button, "clicked", G_CALLBACK(on_back_button_clicked), view, G_CONNECT_DEFAULT);
}

void
orion_webview_tab_view_set_forward_button(OrionWebviewTabView* view, GtkButton* button)
{
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_VIEW(view));
    if(view->forward_button)
    {
        g_signal_handlers_disconnect_by_data(view->forward_button, view);
    }
    view->forward_button = GTK_BUTTON(button);
    g_signal_connect_object(view->forward_button, "clicked", G_CALLBACK(on_forward_button_clicked), view, G_CONNECT_DEFAULT);
}

void
orion_webview_tab_view_set_reload_button(OrionWebviewTabView* view, GtkButton* button)
{
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_VIEW(view));
    if(view->reload_button)
    {
        g_signal_handlers_disconnect_by_data(view->reload_button, view);
    }
    view->reload_button = GTK_BUTTON(button);
    g_signal_connect_object(view->reload_button, "clicked", G_CALLBACK(on_reload_button_clicked), view, G_CONNECT_DEFAULT);
}

static AdwTabPage*
orion_webview_tab_view_insert_page(OrionWebviewTabView* view, const orion::TabDto& tab)
{
    orion::log_verbose("orion_webview_tab_view_insert_page: seq_num=%f", tab.seq_num);
    int len = adw_tab_view_get_n_pages(view->tab_view);
    //pos ends up being set to the first page after where this one should be inserted
    int pos = 0;
    for(; pos<len; pos++)
    {
        AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, pos);
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

        bool page_is_pinned = adw_tab_page_get_pinned(page);
        if(orion_webview_tab_container_get_seq_num(container)>tab.seq_num && (page_is_pinned == tab.is_pinned))
        {
            break;
        }
        if(tab.is_pinned && !page_is_pinned)
        {
            break;
        }
    }
    
    OrionWebviewTabContainer* container = orion_webview_tab_container_new(tab.id, tab.seq_num);
    orion_webview_tab_container_set_group_expanded(container, !tab.is_collapsed);
    
    OrionWebviewTabContent* content;
    OrionApp* app = ORION_APP(g_application_get_default());
    content = orion_app_get_shared_tab(app, tab.id);
    if(!content)
    {
        orion::gtk::WeakRefWrapper container_weak(container);
        global_core_api->get_tab_back_forward_list_entries(tab.id, [tab, container_weak](orion::GetTabBackForwardListReply reply){
            VERIFY_LOG(reply.err);

            auto container_strong = container_weak.lock();
            if(!container_strong) return;
            OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(container_strong.get());

            OrionApp* app = ORION_APP(g_application_get_default());
            OrionWebviewTabContent* content = orion_app_get_shared_tab(app, tab.id);

            bool window_is_active = gtk_window_is_active(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(container))));
            if(content)
            {
                if(window_is_active)
                {
                    orion_webview_tab_container_set_child_content(container, content);
                }
            }
            else
            {
                content = g_object_ref_sink(orion_webview_tab_content_new_from_dto(tab, reply.entries));
                defer([content]{ g_object_unref(content); });
                orion_app_add_shared_pinned_tab(app, content);
                if(window_is_active)
                {
                    orion_webview_tab_container_set_child_content(container, content);
                }
            }
        });
    }
    else
    {
        if(gtk_window_is_active(GTK_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)))))
        {
            orion_webview_tab_container_set_child_content(container, content);
        }
    }

    AdwTabPage* new_page;
    if(!tab.is_pinned)
    {
        new_page = adw_tab_view_insert(ORION_WEBVIEW_TAB_VIEW(view)->tab_view, GTK_WIDGET(container), pos);
    }
    else
    {
        new_page = adw_tab_view_insert_pinned(ORION_WEBVIEW_TAB_VIEW(view)->tab_view, GTK_WIDGET(container), pos);
    }
    g_object_bind_property(container, "icon", new_page, "icon", G_BINDING_SYNC_CREATE);
    g_object_bind_property(container, "tab-title", new_page, "title", G_BINDING_SYNC_CREATE);
    g_object_bind_property(container, "is-loading", new_page, "loading", G_BINDING_SYNC_CREATE);
    return new_page;
}

static void
orion_webview_tab_view_reorder_page(OrionWebviewTabView* view, AdwTabPage* page, double seq_num)
{
    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);

    orion::log_verbose("orion_webview_tab_view_reorder_page: tab_id=%lld seq_num=%f->%f",
        tab_id.value, orion_webview_tab_container_get_seq_num(container), seq_num
    );

    // Note: executing this code below and calling adw_tab_view_reorder_page can be
    // the cause of erroneous tab ordering when moving parent tab (a tab with at least
    // one child tab in Nested Tabs mode) into a tab that immediately precedes it.
    // It seems that calling adw_tab_view_reorder_page is not needed anymore.

    int old_pos = adw_tab_view_get_page_position(view->tab_view, page);
    int len = adw_tab_view_get_n_pages(view->tab_view);
    //pos ends up being set to the first page after where this one should be inserted
    int pos = 0;
    bool is_pinned = adw_tab_page_get_pinned(page);
    for(; pos<len; pos++)
    {
        AdwTabPage* nth_page = adw_tab_view_get_nth_page(view->tab_view, pos);
        if(nth_page != page)
        {
            bool other_page_is_pinned = adw_tab_page_get_pinned(nth_page);
            if(!!other_page_is_pinned == !!is_pinned)
            {
                OrionWebviewTabContainer* nth_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(nth_page));
                if(orion_webview_tab_container_get_seq_num(nth_container)>seq_num)
                {
                    break;
                }
            }
            if(is_pinned && !other_page_is_pinned)
            {
                break;
            }
        }
    }
    //if pos is higher than this page's original position, we should insert into pos-1 as new position
    adw_tab_view_reorder_page(view->tab_view, page, pos<old_pos?pos:pos-1);
    orion_webview_tab_container_set_content_seq_num(container, seq_num, UpdateBackend::no);
    orion::log_verbose("orion_webview_tab_view_reorder_page: attempted reordering page with id %ld to pos %i", orion_webview_tab_container_get_id(container).value, pos<old_pos?pos:pos-1);
}

void
orion_webview_tab_view_new_tab_with_uri(
    OrionWebviewTabView* view, const char* uri, const OrionWebviewTabViewNewTabOptions& options)
{
    if (!uri) {
        orion::log_warning("orion_webview_tab_view_new_tab_with_uri: uri is null");
        return;
    }

    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
    orion::WindowId window_id = main_window_get_window_id(win);
    orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    orion::TabId current_tab_id = main_window_get_current_tab_id(win);
    orion::gtk::WeakRefWrapper win_weak(win);
    orion::gtk::WeakRefWrapper view_weak(view);

    std::string uri_str = uri; // need to wrap this in std::string for the lambda capture

    GAction* open_new_tab_next_to_current_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "open-new-tab-next-to-current-tab");
    GVariant_autoptr open_new_tab_next_to_current_state = g_action_get_state(open_new_tab_next_to_current_setting);
    GAction* show_nested_vertical_tabs_setting = g_action_map_lookup_action(G_ACTION_MAP(win), "show-nested-tabs");
    GVariant_autoptr show_nested_vertical_tabs_state = g_action_get_state(show_nested_vertical_tabs_setting);
    bool open_new_tab_next_to_current_tab = g_variant_get_boolean(open_new_tab_next_to_current_state);
    bool show_nested_vertical_tabs = g_variant_get_boolean(show_nested_vertical_tabs_state);

    orion::CreateTabQuery query{
        .window_id = window_id,
        .name = "",
        .icon_name = "",
        .is_pinned = false,
        .is_suspended = false,
        .uri = uri_str,
    };

    AdwTabPage* selected_page = adw_tab_view_get_selected_page(view->tab_view);
    OrionWebviewTabContainer* selected_container = NULL;
    if(selected_page)
    {
        selected_container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(selected_page));
    }

    bool force_open_next_to_current = options.force_open_next_to_current;

    if (open_new_tab_next_to_current_tab || force_open_next_to_current) {
        if(show_nested_vertical_tabs)
        {
            query.parent_tab_id = orion_webview_tab_container_get_parent_id(selected_container);
            if(selected_container)
            {
                orion::TabId selected_tab_id = orion_webview_tab_container_get_id(selected_container);
                //needs to possibly be moved after the selected container's children
                int n_pages = adw_tab_view_get_n_pages(view->tab_view);
                int selected_pos = adw_tab_view_get_page_position(view->tab_view, selected_page);

                int i = selected_pos + 1; //will hold 1+last child position (or 1 + selected_pos if no children exist) after this for loop

                for(std::vector<orion::TabId> parent_ids_stack{selected_tab_id}; i<n_pages; i++)
                {
                    AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, i);
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));

                    orion::TabId parent_id = orion_webview_tab_container_get_parent_id(container);
                    auto it = std::find(parent_ids_stack.begin(), parent_ids_stack.end(), parent_id);
                    if(it != parent_ids_stack.end())
                    {
                        parent_ids_stack.erase(it+1, parent_ids_stack.end());

                        parent_ids_stack.push_back(orion_webview_tab_container_get_id(container));
                    }
                    else
                    {
                        break;
                    }
                }

                //i is guaranteed to be >0, <= n_pages, thus page before always exists
                AdwTabPage* page_before = adw_tab_view_get_nth_page(view->tab_view, i-1);
                OrionWebviewTabContainer* container_before = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page_before));
                double seq_num_before = orion_webview_tab_container_get_seq_num(container_before);
                if(i<n_pages)
                {
                    AdwTabPage* page_after = adw_tab_view_get_nth_page(view->tab_view, i);
                    OrionWebviewTabContainer* container_after = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page_after));
                    double seq_num_after = orion_webview_tab_container_get_seq_num(container_after);
                    query.seq_num = (seq_num_before + seq_num_after)/2;
                }
                else
                {
                    query.seq_num = seq_num_before+1;
                }
            }
        }
        else
        {
            int n_pages = adw_tab_view_get_n_pages(view->tab_view);
            if(n_pages == 0)
            {
                query.seq_num = 0;
            }
            else
            {
                if(!selected_page)
                {
                    orion::log_warning("orion_webview_tab_view_new_tab_with_uri(): tab view has no tab selected despite having tabs, something is wrong");
                    AdwTabPage* last_page = adw_tab_view_get_nth_page(view->tab_view, n_pages-1);
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(last_page));
                    query.seq_num = orion_webview_tab_container_get_seq_num(container)+1;
                }
                int selected_pos = adw_tab_view_get_page_position(view->tab_view, selected_page);
                if(selected_pos == n_pages-1)
                {
                    AdwTabPage* last_page = adw_tab_view_get_nth_page(view->tab_view, n_pages-1);
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(last_page));
                    query.seq_num = orion_webview_tab_container_get_seq_num(container)+1;
                }
                else
                {
                    AdwTabPage* page_after = adw_tab_view_get_nth_page(view->tab_view, selected_pos+1);
                    OrionWebviewTabContainer* container_after = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page_after));
                    double seq_num_before = orion_webview_tab_container_get_seq_num(selected_container);
                    double seq_num_after = orion_webview_tab_container_get_seq_num(container_after);
                    query.seq_num = (seq_num_before + seq_num_after) / 2;
                }
            }
        }
    }

    bool focus_address_entry = options.focus_address_entry;
    bool autoselect = options.autoselect;
    bool ignore_when_opening_setting = options.ignore_when_opening_setting;

    if(!ignore_when_opening_setting && win)
    {
        autoselect = orion::gtk::get_boolean_action_state(G_ACTION_MAP(win), "when-opening-link-in-new-tab-make-it-active");
    }
    focus_address_entry = autoselect?focus_address_entry:false;   

    global_core_api->create_tab(query, [win_weak,focus_address_entry,view_weak,autoselect](orion::CreateTabReply reply){
        VERIFY_LOG(reply.err);

        if (focus_address_entry) {
            // We must wait some time before trying to focus and select the entry.
            // Too short delay will cuase the selection not to work correctly.
            global_core_api->async_after(std::chrono::milliseconds(50), [win_weak]{        
                auto win_strong = win_weak.lock();
                if (!win_strong) return;
                MainWindow* win = MAIN_WINDOW(win_strong.get());

                g_action_group_activate_action(G_ACTION_GROUP(win), "select-address-entry", NULL);
            });
        }
        if(autoselect)
        {
            auto view_strong = view_weak.lock();
            if(view_strong)
            {
                orion_webview_tab_view_select_next_created_tab_with_id(ORION_WEBVIEW_TAB_VIEW(view_strong.get()), reply.id);
            }
        }
    });
}

void
orion_webview_tab_view_new_tab(OrionWebviewTabView* view)
{
    // Always focus new tab (Start Page) after opening it, ignoring
    // when_opening_link_in_new_tab_make_it_active user setting
    orion_webview_tab_view_new_tab_with_uri(view, "orion://newtab/", {
        .focus_address_entry = true,
        .autoselect = true,
        .ignore_when_opening_setting = true,
    });
}

void
orion_webview_tab_view_load_uri(OrionWebviewTabView* view, orion::Uri uri)
{
    OrionWebviewTabContent* current_page = orion_webview_tab_view_get_current_tab_content(view);
    if(current_page!=NULL)
    {
        orion_webview_tab_content_load_uri(current_page, uri);
    }
}

static void
orion_webview_tab_view_load_tab_from_dto(OrionWebviewTabView* view, const orion::TabDto& dto, bool set_selected)
{
    orion::log_verbose("orion_webview_tab_view_load_tab_from_dto: id=%d seq_num=%f is_pinned=%d",
        dto.id, dto.seq_num, dto.is_pinned
    );
    g_return_if_fail(ORION_IS_WEBVIEW_TAB_VIEW(view));


    g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);
    AdwTabPage* new_page = orion_webview_tab_view_insert_page(view, dto);
    g_signal_connect(new_page, "notify::pinned", G_CALLBACK(on_tab_page_pinned_changed), view);
    g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);
    if(set_selected) adw_tab_view_set_selected_page(view->tab_view, new_page);
    if (dto.is_pinned) 
    {
        orion::log_verbose("orion_webview_tab_view_load_tab_from_dto: tab_id=%lld is_pinned=%d", dto.id, dto.is_pinned);
    }
}

void
orion_webview_tab_view_load_bookmark(OrionWebviewTabView* view, orion::BookmarkId id)
{
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(view);
    orion::GetBookmarksQuery query{
        .ids = {id},
    };
    global_core_api->get_bookmarks(query, [wrapped_ref](orion::GetBookmarksReply reply){
        VERIFY_LOG(reply.err);
        OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(wrapped_ref->get());
        if(view)
        {
            for(orion::BookmarkDto& bookmark : reply.bookmarks)
            {
                orion_webview_tab_view_new_tab_with_uri(view, bookmark.address.c_str());
            }
            g_object_unref(view);
        }
    });
}

void
orion_webview_tab_view_load_bookmark_folder(OrionWebviewTabView* view, orion::BookmarkFolderId id)
{
    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(view);
    orion::ProfileId profile_id = main_window_get_current_profile_id(MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view))));
    global_core_api->get_bookmark_folder_ids(profile_id, [wrapped_ref, id, profile_id](orion::GetBookmarkFolderIdsReply reply){
        VERIFY_LOG(reply.err);
        global_core_api->get_bookmark_folders(reply.ids, [wrapped_ref, id, profile_id](orion::GetBookmarkFoldersReply reply)
        {
            VERIFY_LOG(reply.err);
            auto folders = std::make_shared<std::vector<orion::BookmarkFolderDto>>(reply.folders);
            orion::GetBookmarkIdsQuery query{};
            query.profile_id = profile_id;
            global_core_api->get_bookmark_ids(query, [wrapped_ref, id, folders](orion::GetBookmarkIdsReply reply)
            {
                VERIFY_LOG(reply.err);
                orion::GetBookmarksQuery query{
                    .ids = reply.ids,
                };
                global_core_api->get_bookmarks(query, [wrapped_ref, id, folders](orion::GetBookmarksReply reply)
                {
                    VERIFY_LOG(reply.err);
                    auto& bookmarks = reply.bookmarks;
                    OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(wrapped_ref->get());
                    if(view)
                    {
                        std::vector<orion::BookmarkFolderId> bookmark_folders_to_open = {id};
                        bool flag = true;
                        while(flag)
                        {
                            flag = false;
                            for(auto it = folders->begin(); it!=folders->end(); ++it)
                            {
                                std::optional<orion::BookmarkFolderId> parent_folder_id = it->parent_folder_id;
                                auto search_it = std::find_if(bookmark_folders_to_open.begin(), bookmark_folders_to_open.end(),
                                    [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                                if(search_it!=bookmark_folders_to_open.end())
                                {
                                    flag = true;
                                    bookmark_folders_to_open.push_back(it->id);
                                    it = --folders->erase(it);
                                }
                            }
                        }

                        std::vector<orion::BookmarkDto*> bookmarks_to_open;

                        for(orion::BookmarkDto& bookmark : bookmarks)
                        {
                            auto parent_folder_id = bookmark.parent_folder_id;
                            auto search_it = std::find_if(bookmark_folders_to_open.begin(), bookmark_folders_to_open.end(),
                                [parent_folder_id](orion::BookmarkFolderId a){return a==parent_folder_id;});
                            if(search_it!=bookmark_folders_to_open.end())
                            {
                                bookmarks_to_open.push_back(&bookmark);
                            }
                        }
                        


                        for(auto bookmark : bookmarks_to_open)
                        {
                            orion_webview_tab_view_new_tab_with_uri(view, bookmark->address.c_str());
                        }
                        g_object_unref(view);
                    }
                });
            });
        });
    });
}

static void
on_tab_view_setup_menu(AdwTabView* self, AdwTabPage* page, gpointer user_data)
{
    GMenu* menu = G_MENU(adw_tab_view_get_menu_model(self));
    g_menu_remove_all(menu);

    orion::log_verbose("orion_webview_tabview: on_tab_view_setup_menu: self=%p page=%p menu=%p",
        self, page, menu
    );

    if (page == NULL) {
        // this means that the context menu is being closed
        return;
    }

    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
    orion::TabId tab_id = orion_webview_tab_container_get_id(container);

    orion::GetOtherTabIdsQuery other_tab_ids_query{
        .exclude_tab_id = tab_id,
    };

    orion::gtk::WeakRefWrapper self_weak(self);
    orion::gtk::WeakRefWrapper page_weak(page);

    global_core_api->get_other_tab_ids(other_tab_ids_query, [self_weak,page_weak](orion::GetOtherTabIdsReply reply){
        VERIFY_LOG(reply.err);

        auto self_strong = self_weak.lock();
        if (!self_strong) return;
        AdwTabView* self = ADW_TAB_VIEW(self_strong.get());

        auto page_strong = page_weak.lock();
        if (!page_strong) return;
        AdwTabPage* page = ADW_TAB_PAGE(page_strong.get());

        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(self)));
        GMenu* menu = G_MENU(adw_tab_view_get_menu_model(self));

        orion::gtk::fill_tab_context_menu(menu, win, self, page, reply, orion::gtk::CreateTabContextMenuType::main_tabbar);
    });
}

static void
orion_webview_tab_view_force_close_page_silent(OrionWebviewTabView* view, AdwTabPage* page)
{
    if(gtk_widget_get_native(GTK_WIDGET(view->tab_view)))
    {
        g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_close_page, NULL);
        adw_tab_view_set_page_pinned(view->tab_view, page, false);
        adw_tab_view_close_page(view->tab_view, page);
        g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_close_page, NULL);
    }
}

void
orion_webview_tab_view_dispose_of_pinned(OrionWebviewTabView* view)
{
    int n_pinned = adw_tab_view_get_n_pinned_pages(view->tab_view);
    for(int i = 0; i<n_pinned; i++)
    {
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(adw_tab_view_get_nth_page(view->tab_view, i)));
        orion_webview_tab_container_set_child_content(container, NULL);
    }
}

static void
orion_webview_tab_view_handle_tab_change(OrionWebviewTabView* view, const orion::TabsChange& change, std::vector<orion::WindowId> window_ids)
{
    // orion::log_verbose("orion_webview_tab_view_handle_tab_change: window_id=%d created=%zu deleted=%zu updated=%zu",
    //     window_id.value, change.created.size(), change.deleted.size(), change.updated.size()
    // );
    MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
    orion::gtk::WeakRefWrapper view_weak(view);

    auto change_handler = [change=std::move(change),window_ids,view_weak](){

        auto view_strong = view_weak.lock();
        if (!view_strong) return;
        OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

        for(const orion::TabDto& tab_created : change.created)
        {
            if(std::find(window_ids.begin(), window_ids.end(), tab_created.window_id) != window_ids.end())
            {
                bool to_be_force_selected = tab_created.id == view->cpp_data.id_to_select;
                bool set_selected = to_be_force_selected;
                // if (tab_created.uri == "orion://newtab/") {
                //     set_selected = true;
                // }
                orion_webview_tab_view_load_tab_from_dto(view, tab_created, set_selected);
                if(to_be_force_selected)
                {
                    view->cpp_data.id_to_select = orion::TabId(0);
                }
            }
        }
        for(orion::TabId tab_deleted : change.deleted)
        {
            int n_pages = adw_tab_view_get_n_pages(view->tab_view);
            for(int i = 0; i<n_pages; i++)
            {
                AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, i);
                OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                if (container)
                {
                    orion::TabId tab_id = orion_webview_tab_container_get_id(container);
                    if(tab_id == tab_deleted)
                    {
                        orion_webview_tab_view_force_close_page_silent(view, page);
                        break;
                    }
                }
            }
        }
        for(const orion::TabDto& tab_updated : change.updated)
        {
            //finds associated tab, reorders, sets seq_num

            if(std::find(window_ids.begin(), window_ids.end(), tab_updated.window_id) != window_ids.end())
            {
                int n_pages = adw_tab_view_get_n_pages(view->tab_view);
                bool tab_exists = false;
                for(int i = 0; i<n_pages; i++)
                {
                    AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, i);
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                    if(orion_webview_tab_container_get_id(container) == tab_updated.id)
                    {
                        g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);

                        // first change is_pinned and only then reorder (change position) to avoid issues with
                        // adwaita tab page indexing
                        if (tab_updated.is_pinned != adw_tab_page_get_pinned(page)) {
                            orion::log_verbose("orion_webview_tab_view_handle_tab_change: tab_id=%lld is_pinned=%d->%d",
                                tab_updated.id, adw_tab_page_get_pinned(page), tab_updated.is_pinned
                            );
                            adw_tab_view_set_page_pinned(view->tab_view, page, tab_updated.is_pinned);
                            orion_webview_tab_view_reorder_page(view, page, tab_updated.seq_num);
                        }

                        //if (tab_updated.seq_num != orion_webview_tab_content_get_seq_num(content)) {
                            orion::log_verbose("orion_webview_tab_view_handle_tab_change: tab_id=%lld seq_num=%f->%f",
                                tab_updated.id, orion_webview_tab_container_get_seq_num(container), tab_updated.seq_num
                            );
                            orion_webview_tab_view_reorder_page(view, page, tab_updated.seq_num);
                        //}

                        if (tab_updated.is_suspended != orion_webview_tab_container_get_is_suspended(container)) {
                            orion::log_verbose("orion_webview_tab_view_handle_tab_change: tab_id=%lld is_suspended=%d->%d",
                                tab_updated.id, orion_webview_tab_container_get_is_suspended(container), tab_updated.is_suspended
                            );
                            orion_webview_tab_container_set_content_is_suspended(container, tab_updated.is_suspended);
                        }
                        
                        if (tab_updated.parent_tab_id != orion_webview_tab_container_get_parent_id(container)) {
                            orion::log_verbose("orion_webview_tab_view_handle_tab_change: tab_id=%lld parent_id=%lld->%lld",
                                tab_updated.id, orion_webview_tab_container_get_parent_id(container), tab_updated.parent_tab_id
                            );
                            orion_webview_tab_container_set_parent_id(container, tab_updated.parent_tab_id);
                        }
                        g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (void*)on_tab_view_page_attached, NULL);
                        tab_exists = true;
                        break;
                    }
                }
                if(!tab_exists)
                {
                    orion_webview_tab_view_load_tab_from_dto(view, tab_updated, false);
                }
            }
            else
            {
                int n_pages = adw_tab_view_get_n_pages(view->tab_view);
                for(int i = 0; i<n_pages; i++)
                {
                    AdwTabPage* page = adw_tab_view_get_nth_page(view->tab_view, i);
                    OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(page));
                    if(orion_webview_tab_container_get_id(container) == tab_updated.id)
                    {
                        orion_webview_tab_view_force_close_page_silent(view, page);
                        break;
                    }
                }
            }
        }
    };

        
    // if(!win)
    // {
    //     orion::log_debug("orion_webview_tab_view_handle_tab_change(): tab view not yet rooted for change with %i created, %i deleted, %i updated", change.created.size(), change.deleted.size(), change.updated.size());
    //     change_handler(false);
    // }
    // else
    // {
    //     orion::ProfileId profile_id = main_window_get_current_profile_id(win);
    //     global_core_api->get_setting(
    //         profile_id,
    //         orion::settings::tabs::when_opening_link_in_new_tab_make_it_active,
    //         [change_handler = std::move(change_handler)](orion::GetSettingReply<bool> reply)
    //         {
    //             VERIFY_LOG(reply.err);
    //             change_handler(reply.value);
    //         });
    // }
    change_handler();
}

static void
orion_webview_tab_view_init_from_tabs(OrionWebviewTabView* view, const std::vector<const std::vector<orion::TabDto>*>& tabs, orion::TabId selected_tab)
{
    std::vector<orion::TabId> existing_ids{};
    int n_pages = adw_tab_view_get_n_pages(view->tab_view);
    for(int i = 0; i<n_pages; i++)
    {
        OrionWebviewTabContainer* container = ORION_WEBVIEW_TAB_CONTAINER(adw_tab_page_get_child(adw_tab_view_get_nth_page(view->tab_view, i)));
        existing_ids.push_back(orion_webview_tab_container_get_id(container));
    }
    g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)on_tab_view_page_attached, NULL);
    g_signal_handlers_block_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)sync_selected_page_to_backend, NULL);
    for(const std::vector<orion::TabDto>* val : tabs)
    {
        for(const orion::TabDto& tab : *val)
        {
            auto it = std::find(existing_ids.cbegin(), existing_ids.cend(), tab.id);
            if(it == existing_ids.cend())
            {
                orion_webview_tab_view_load_tab_from_dto(view, tab, tab.id == selected_tab);
                existing_ids.push_back(tab.id);
            }
            else
            {
                orion::log_error("received duplicate tab when initialising tab view from backend");
            }
        }
    }
    g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)on_tab_view_page_attached, NULL);
    g_signal_handlers_unblock_matched(view->tab_view, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, (gpointer)sync_selected_page_to_backend, NULL);
}

static void
orion_webview_tab_view_set_uri(OrionWebviewTabView* view, orion::Uri uri)
{
    //view->cpp_data->uri = orion::Uri::parse_strict("https://datatracker.ietf.org/doc/html/rfc3986#appendix-B");
    view->cpp_data.uri = uri;
    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_URI]);
}

static void
orion_webview_tab_view_set_title(OrionWebviewTabView* view, std::string title)
{
    view->cpp_data.title = title;
    g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_TITLE]);
}

static void
orion_webview_tab_view_set_is_tab_group(OrionWebviewTabView* view, bool is_tab_group)
{
    if(view->is_tab_group != is_tab_group)
    {
        view->is_tab_group = is_tab_group;
        g_object_notify_by_pspec(G_OBJECT(view), obj_properties[PROP_IS_TAB_GROUP]);

        MainWindow* win = MAIN_WINDOW(gtk_widget_get_root(GTK_WIDGET(view)));
        if(!win) return;

        int n_pinned = adw_tab_view_get_n_pinned_pages(view->tab_view);
        //drops all now invalidated pinned tabs
        for(int i = 0; i<n_pinned; i++)
        {
            orion_webview_tab_view_force_close_page_silent(view, adw_tab_view_get_nth_page(view->tab_view, 0));
        }

        if(is_tab_group)
        {
            //inits itself with shared pinned tabs
            orion::gtk::WeakRefWrapper view_weak(view);
            orion::WindowId window_id = main_window_get_window_id(win);
            orion::ProfileId profile_id = main_window_get_current_profile_id(win);

            global_core_api->get_windows({window_id}, [view_weak,profile_id](orion::GetWindowsReply reply){
                VERIFY_LOG(reply.err);

                if (reply.windows.size()<1) return;
                const orion::WindowDto& window = reply.windows.at(0);

                global_core_api->get_shared_pinned_tabs_window_id(profile_id,
                    [view_weak,window](orion::GetSharedPinnedTabsWindowIdReply reply){

                    VERIFY_LOG(reply.err);

                    orion::GetTabIdsQuery query{};
                    query.window_id = reply.window_id;
                    global_core_api->get_tab_ids(query, [view_weak,window](orion::GetTabIdsReply reply){
                        VERIFY_LOG(reply.err);

                        global_core_api->get_tabs(reply.ids, [view_weak,window](orion::GetTabsReply reply){
                            VERIFY_LOG(reply.err);

                            std::vector<orion::TabDto>& shared_pinned_tabs = reply.tabs;
                            if (window.is_private) {
                                // In private windows, we do not want to show shared pinned tabs at all, ignore them
                                shared_pinned_tabs.clear();
                            }
                            
                            auto view_strong = view_weak.lock();
                            if(!view_strong) return;
                            OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());
                            orion_webview_tab_view_init_from_tabs(view, {&shared_pinned_tabs}, orion::TabId(0));
                        });
                    });
                });
            });
        }
    }
}

void
orion_webview_tab_view_load_from_backend(
    OrionWebviewTabView* view, orion::WindowId window_id, orion::ProfileId profile_id, DoneCallback done_cb,
    void* user_data)
{
    orion::log_verbose("orion_webview_tab_view_load_from_backend: window_id=%d", window_id);
    orion::gtk::WeakRefWrapper view_weak(view);
    global_core_api->get_shared_pinned_tabs_window_id(
        profile_id,
        [window_id,view_weak,done_cb=std::move(done_cb),user_data](orion::GetSharedPinnedTabsWindowIdReply reply){

        VERIFY_LOG(reply.err);
        auto pinned_tabs_window_id = reply.window_id;

        global_core_api->get_windows(
            {window_id},
            [pinned_tabs_window_id,window_id,view_weak,done_cb=std::move(done_cb),user_data]
            (orion::GetWindowsReply reply){

            VERIFY_LOG(reply.err);

            auto view_strong = view_weak.lock();
            if (!view_strong) return;
            OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

            if(reply.windows.size()<1) return;
            const orion::WindowDto& window = reply.windows.at(0);
            orion_webview_tab_view_set_is_tab_group(view, window.is_tab_group);
            orion::TabId selected_tab = window.selected_tab_id;
            orion::GetTabIdsQuery query{};
            query.window_id = window_id;

            global_core_api->get_tabs_by_query(
                query,
                [view_weak,selected_tab,pinned_tabs_window_id,window_id,done_cb=std::move(done_cb),user_data,
                 window]
                (orion::GetTabsReply reply){

                VERIFY_LOG(reply.err);

                for(auto& tab : reply.tabs)
                {
                    orion::log_verbose("orion_webview_tab_view_load_from_backend cb: tab_id=%li", tab.id.value);
                }

                auto view_strong = view_weak.lock();
                if (!view_strong) return;
                OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

                if(view->is_tab_group)
                {
                    // This window is Unsaved Tab Group.
                    orion::GetTabIdsQuery query{};
                    query.window_id = pinned_tabs_window_id;
                    std::vector<orion::TabDto> tabs = reply.tabs;
                    global_core_api->get_tabs_by_query(
                        query,
                        [view_weak,selected_tab,pinned_tabs_window_id,window_id,tabs=std::move(tabs),
                         done_cb=std::move(done_cb),user_data,window]
                        (orion::GetTabsReply reply){

                        VERIFY_LOG(reply.err);

                        std::vector<orion::TabDto>& shared_pinned_tabs = reply.tabs;
                        
                        for(auto& t : shared_pinned_tabs)
                        {
                            orion::log_verbose("shared pinned tab obtained with id: %li", t.id.value);
                        }

                        if (window.is_private) {
                            // In private windows, we do not want to show shared pinned tabs at all, ignore them
                            shared_pinned_tabs.clear();
                        }

                        auto view_strong = view_weak.lock();
                        if (!view_strong) return;
                        OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

                        // If selected_tab points to a pinned tab, fall back to the first non-pinned tab
                        orion::TabId effective_selected_tab = selected_tab;
                        bool selected_is_pinned = false;
                        for (const auto& t : shared_pinned_tabs) {
                            if (t.id == selected_tab && t.is_pinned) {
                                selected_is_pinned = true;
                                break;
                            }
                        }
                        if (selected_is_pinned || selected_tab.value == 0) {
                            // Pick the first non-pinned tab from the window's own tabs
                            if (!tabs.empty()) {
                                effective_selected_tab = tabs.front().id;
                            }
                        }

                        orion_webview_tab_view_init_from_tabs(
                            view, {&tabs, &shared_pinned_tabs}, effective_selected_tab
                        );
                        if(view->cpp_data.tabs_change_observer.has_value())
                        {
                            global_core_api->tabs_change().remove_observer(
                                view->cpp_data.tabs_change_observer.value()
                            );
                        }
                        view->cpp_data.tabs_change_observer = global_core_api->tabs_change().add_observer(
                            [view_weak,pinned_tabs_window_id,window_id,window](const orion::TabsChange& change){

                            //TODO: handle deleted, created and seq num updated tabs
                            auto view_strong = view_weak.lock();
                            if (!view_strong) return;
                            OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

                            std::vector<orion::WindowId> window_ids = {window_id};
                            if(view->is_tab_group && !window.is_private) {
                                window_ids.push_back(pinned_tabs_window_id);
                            }

                            orion_webview_tab_view_handle_tab_change(view, change, window_ids);
                        });
                        if (done_cb) {
                            done_cb(user_data);
                        }
                    });
                }
                else
                {
                    // This is window is Saved Window (Saved Tab Group).
                    // Do not select the first non-pinned tab (in similar way it is done in Unsaved Tab
                    // Group window), instead, always select the tab which is marked as the selected one.

                    orion_webview_tab_view_init_from_tabs(view, {&(reply.tabs)}, selected_tab);
                    if(view->cpp_data.tabs_change_observer.has_value())
                    {
                        global_core_api->tabs_change().remove_observer(
                            view->cpp_data.tabs_change_observer.value()
                        );
                    }
                    view->cpp_data.tabs_change_observer = global_core_api->tabs_change().add_observer(
                        [view_weak,pinned_tabs_window_id,window_id](const orion::TabsChange& change){

                        //TODO: handle deleted, created and seq num updated tabs
                        auto view_strong = view_weak.lock();
                        if (!view_strong) return;
                        OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());
                        
                        std::vector<orion::WindowId> window_ids = {window_id};
                        if(view->is_tab_group) window_ids.push_back(pinned_tabs_window_id);

                        orion_webview_tab_view_handle_tab_change(view, change, window_ids);
                    });

                    if (done_cb) {
                        done_cb(user_data);
                    }
                }
            });

            if(view->cpp_data.windows_change_observer.value)
            {
                global_core_api->windows_change().remove_observer(view->cpp_data.windows_change_observer);
            }
            view->cpp_data.windows_change_observer = global_core_api->windows_change().add_observer(
                [window_id,view_weak](const orion::WindowsChange& change){

                auto view_strong = view_weak.lock();
                if (!view_strong) return;
                OrionWebviewTabView* view = ORION_WEBVIEW_TAB_VIEW(view_strong.get());

                for (const orion::WindowDto& window : change.updated) {
                    if (window_id != window.id) continue;
                    
                    if (change.updated_selected_tab_id) {
                        orion::log_verbose(
                            "orion_webview_tab_view_map: window_id=%lld selected_tab_id changed to %lld",
                            window.id.value, window.selected_tab_id.value
                        );

                        AdwTabPage* page = orion_webview_tab_view_get_page_with_tab_id(
                            view, window.selected_tab_id
                        );
                        if (!page) {
                            orion::log_verbose(
                                "orion_webview_tab_view_map: window_id=%lld selected_tab_id=%lld not found",
                                window.id.value, window.selected_tab_id.value
                            );
                            return;
                        }
                        orion_webview_tab_view_set_selected_page_silent(view, page);
                    }
                    if(change.updated_is_tab_group)
                    {
                        orion_webview_tab_view_set_is_tab_group(view, change.updated.at(0).is_tab_group);
                    }
                }
            });
        });
    });
}

// called when the widget is hidden (by calling gtk_widget_set_visibility for example)
static void
orion_webview_tab_view_unmap(GtkWidget* widget, gpointer user_data)
{
    orion::log_verbose("orion_webview_tabview: orion_webview_tab_view_unmap: widget=%p", widget);
    // do not disconnect from other signals or observers here, we still want to get notified
    // even when the widget is hidden
}

OrionWebviewTabView*
orion_webview_tab_view_new()
{
    OrionWebviewTabView* tab_view = ORION_WEBVIEW_TAB_VIEW(g_object_new(ORION_WEBVIEW_TAB_VIEW_TYPE, NULL));
    //gtk_widget_set_name(GTK_WIDGET(tab_view), "tab-view"); // should not be needed
    gtk_widget_set_hexpand(GTK_WIDGET(tab_view), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(tab_view), TRUE);
    gtk_widget_set_halign(GTK_WIDGET(tab_view), GTK_ALIGN_FILL);
    return tab_view;
}

static void
orion_webview_tab_view_finalize(GObject* object)
{
    OrionWebviewTabView* self = ORION_WEBVIEW_TAB_VIEW(object);

    std::destroy_at(&self->cpp_data);
    orion::log_verbose("orion_webview_tab_view_finalize: self=%p", self);

    G_OBJECT_CLASS(orion_webview_tab_view_parent_class)->finalize(object);
}

static void
orion_webview_tab_view_dispose(GObject* object)
{
    orion::log_verbose("orion_webview_tab_view_dispose: object=%p", object);

    OrionWebviewTabView* self = ORION_WEBVIEW_TAB_VIEW(object);
    if(self->cpp_data.tabs_change_observer.has_value())
    {
        global_core_api->tabs_change().remove_observer(self->cpp_data.tabs_change_observer.value());
        self->cpp_data.tabs_change_observer.reset();
    }

    global_core_api->windows_change().clear_observer(&self->cpp_data.windows_change_observer);
    
    if (self->back_button)
    {
        g_signal_handlers_disconnect_by_data(self->back_button, self);
        self->back_button = NULL;
    }
    if (self->reload_button)
    {
        g_signal_handlers_disconnect_by_data(self->reload_button, self);
        self->reload_button = NULL;
    }
    if (self->forward_button)
    {
        g_signal_handlers_disconnect_by_data(self->forward_button, self);
        self->forward_button = NULL;
    }
    G_OBJECT_CLASS(orion_webview_tab_view_parent_class)->dispose(object);
}

static void
orion_webview_tab_view_init(OrionWebviewTabView* self)
{
    std::construct_at(&self->cpp_data);
    
    g_type_ensure(ORION_WEBVIEW_TAB_CONTENT_TYPE);

    self->tab_view = adw_tab_view_new();
    adw_bin_set_child(ADW_BIN(self), GTK_WIDGET(self->tab_view));
    adw_tab_view_set_shortcuts(self->tab_view, ADW_TAB_VIEW_SHORTCUT_NONE);

    g_signal_connect_data(self->tab_view, "notify::selected-page", G_CALLBACK(on_page_selected), self, NULL, G_CONNECT_AFTER);  
    g_signal_connect_data(self->tab_view, "notify::selected-page", G_CALLBACK(sync_selected_page_to_backend), self, NULL, G_CONNECT_AFTER); 
    g_signal_connect(self->tab_view, "close-page", G_CALLBACK(on_tab_view_close_page), self);
    g_signal_connect(self->tab_view, "create-window", G_CALLBACK(on_tab_view_create_window), self);
    g_signal_connect_after(self->tab_view, "page-attached", G_CALLBACK(on_tab_view_page_attached), self);
    g_signal_connect_after(self->tab_view, "page-attached", G_CALLBACK(on_audio_aggregate_page_attached), self);
    g_signal_connect(self->tab_view, "page-detached", G_CALLBACK(on_audio_aggregate_page_detached), self);
    g_signal_connect(self, "unmap", G_CALLBACK(orion_webview_tab_view_unmap), NULL);
    g_signal_connect(self->tab_view, "setup-menu", G_CALLBACK(on_tab_view_setup_menu), self);

    GMenu* menu = g_menu_new();
    defer([menu]{ g_object_unref(menu); });
    adw_tab_view_set_menu_model(self->tab_view, G_MENU_MODEL(menu));
}
static void
orion_webview_tab_view_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec)
{
    OrionWebviewTabView* self = ORION_WEBVIEW_TAB_VIEW(object);

    switch (prop_id) {
        case PROP_IS_TAB_GROUP:
            orion_webview_tab_view_set_is_tab_group(self, g_value_get_boolean(value));
            break;
        case PROP_TITLE:
        {
            std::string title;
            const char* string = g_value_get_string(value);
            if(string) title = string;
            orion_webview_tab_view_set_title(self, title);
            break;
        }
        case PROP_URI:
        {
            const gchar* text = g_value_get_string(value);
            std::string text_str = text ? text : "";

            orion_webview_tab_view_set_uri(self, orion::Uri::parse_strict(text_str.c_str()));
            orion::log_verbose("orion_webview_tab_view_set_property: uri=%s", text_str.c_str());
            break;
        }
        case PROP_IS_READERABLE:
            self->is_readerable = g_value_get_boolean(value);
            g_object_notify_by_pspec(object, pspec);
            break;
        case PROP_READER_MODE_ENABLED:
            self->reader_mode_enabled = g_value_get_boolean(value);
            //orion::log_debug("reader enabled: %s", self->reader_mode_enabled?"true":"false");
            g_object_notify_by_pspec(object, pspec);
            break;
        case PROP_TLS_STATUS:
            break;
        case PROP_BACK_LIST:
            break;
        case PROP_FORWARD_LIST:
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
orion_webview_tab_view_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec)
{
    OrionWebviewTabView* self = ORION_WEBVIEW_TAB_VIEW(object);

    switch (prop_id) {
        case PROP_IS_TAB_GROUP:
            g_value_set_boolean(value, self->is_tab_group);
            break;
        case PROP_TITLE:
            g_value_set_string(value, self->cpp_data.title.c_str());
            break;
        case PROP_URI:
        {
            const char* return_val = self->cpp_data.uri.uri().c_str();
            if(return_val) g_value_set_string(value, return_val);
            else g_value_set_string(value, "");
            break;
        }
        case PROP_IS_READERABLE:
            g_value_set_boolean(value, self->is_readerable);
            break;
        case PROP_READER_MODE_ENABLED:
            g_value_set_boolean(value, self->reader_mode_enabled);
            break;
        case PROP_CAN_GO_BACK:
            g_value_set_boolean(value, orion_webview_tab_view_can_go_back(self));
            break;
        case PROP_CAN_GO_FORWARD:
            g_value_set_boolean(value, orion_webview_tab_view_can_go_forward(self));
            break;
        case PROP_ANY_TAB_PLAYING_AUDIO:
            g_value_set_boolean(value, self->any_tab_playing_audio);
            break;
        case PROP_ANY_TAB_PLAYING_AUDIO_UNMUTED:
            g_value_set_boolean(value, self->any_tab_playing_audio_unmuted);
            break;
        case PROP_TLS_STATUS:
            g_value_set_int64(value, gint64(orion_webview_tab_view_get_tls_status(self)));
            break;
        case PROP_BACK_LIST:
            g_value_take_boxed(value, orion_webview_tab_view_get_back_list(self));
            break;
        case PROP_FORWARD_LIST:
            g_value_take_boxed(value, orion_webview_tab_view_get_forward_list(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
orion_webview_tab_view_class_init(OrionWebviewTabViewClass* klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = orion_webview_tab_view_finalize;
    object_class->dispose = orion_webview_tab_view_dispose;
    object_class->set_property = orion_webview_tab_view_set_property;
    object_class->get_property = orion_webview_tab_view_get_property;
    obj_properties[PROP_IS_TAB_GROUP] = 
        g_param_spec_boolean(
            "is-tab-group",
            NULL,
            NULL,
            true,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_TITLE] = 
        g_param_spec_string(
            "title",
            NULL,
            NULL,
            "",
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_URI] = 
        g_param_spec_string(
            "uri",
            NULL,
            NULL,
            "",
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_IS_READERABLE] = 
        g_param_spec_boolean(
            "is-readerable",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_READER_MODE_ENABLED] = 
        g_param_spec_boolean(
            "reader-mode-enabled",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_CAN_GO_BACK] = 
        g_param_spec_boolean(
            "can-go-back",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_CAN_GO_FORWARD] =
        g_param_spec_boolean(
            "can-go-forward",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_ANY_TAB_PLAYING_AUDIO] =
        g_param_spec_boolean(
            "any-tab-playing-audio",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_ANY_TAB_PLAYING_AUDIO_UNMUTED] =
        g_param_spec_boolean(
            "any-tab-playing-audio-unmuted",
            NULL,
            NULL,
            FALSE,
            GParamFlags(G_PARAM_STATIC_STRINGS | G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY));
    obj_properties[PROP_TLS_STATUS] =
        g_param_spec_int64(
            "tls-status",
            NULL,
            NULL,
            INT64_MIN,
            INT64_MAX,
            gint64(orion::TlsStatus::none),
            GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_BACK_LIST] = 
        g_param_spec_boxed(
            "back-list",
            NULL,
            NULL,
            APP_TYPE_BACK_FORWARD_LIST,
            GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
    obj_properties[PROP_FORWARD_LIST] =
        g_param_spec_boxed("forward-list",
            NULL,
            NULL,
            APP_TYPE_BACK_FORWARD_LIST,
            GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_properties(object_class, N_PROPS, obj_properties);
}

//endregion
