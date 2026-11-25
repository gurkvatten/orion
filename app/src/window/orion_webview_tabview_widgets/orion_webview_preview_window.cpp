
#include "app.h"

#include <math.h>

#include <common/src/defer.h>

#include "orion_webview_preview_window.h"

#include "window/main_window/main_window.h"
#include "window/main_window/orion_window_iface.h"

#include "data_item/back_forward_list.h"

#include "util/enum_util.h"

namespace {
    struct CppWrapper {
        orion::ProfileId profile_id;
        orion::WindowId window_id;
        std::string uri;
        orion::PrivateMode private_mode;
    };
}

struct _OrionWebviewPreviewWindow
{
	AdwApplicationWindow parent;

    AdwToolbarView* toolbar_view;
    GtkLabel* title_label;
    AdwHeaderBar* header;

    MainWindow* source_win;
    ProfileDataHolder* profile_data_holder;
    
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    WebKitWebView* webview;
#endif
    CppWrapper cpp_data;
};

typedef enum {
    PROP_0,

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
} PropId;

static GParamSpec* obj_properties[N_PROPS] = { NULL, };


static void
orion_webview_preview_window_orion_window_iface_init(OrionWindowInterface* iface) {} //noop, iface provides only properties which are overriden in class_init
static void orion_webview_preview_window_action_map_iface_init(GActionMapInterface* iface);
static void orion_webview_preview_window_action_group_iface_init(GActionGroupInterface* iface);

G_DEFINE_TYPE_WITH_CODE(OrionWebviewPreviewWindow, orion_webview_preview_window, ADW_TYPE_APPLICATION_WINDOW,
    G_IMPLEMENT_INTERFACE(ORION_TYPE_WINDOW, orion_webview_preview_window_orion_window_iface_init)
    G_IMPLEMENT_INTERFACE(G_TYPE_ACTION_MAP, orion_webview_preview_window_action_map_iface_init)
    G_IMPLEMENT_INTERFACE(G_TYPE_ACTION_GROUP, orion_webview_preview_window_action_group_iface_init)
)

static void notify_tls_status(GObject* self){ g_object_notify_by_pspec(self, obj_properties[PROP_TLS_STATUS]); }
static void move_webview_to_tab(OrionWebviewPreviewWindow* self);

static void
win_weak_notify(gpointer data, GObject* destroyed_object)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(data);

    self->source_win = NULL;

    gtk_widget_insert_action_group(GTK_WIDGET(self), "win", NULL);
    gtk_window_close(GTK_WINDOW(self));
}

static gchar*
format_webview_label(gpointer instance, const gchar* title, const gchar* uri, gpointer user_data)
{
    return g_strdup_printf("%s — %s", title, uri);
}


#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static gboolean
on_webview_decide_policy(WebKitWebView* webview, WebKitPolicyDecision* decision, WebKitPolicyDecisionType type, OrionWebviewPreviewWindow* self)
{
    // if(type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION)
    // {
    //     WebKitNavigationAction* action = webkit_navigation_policy_decision_get_navigation_action(WEBKIT_NAVIGATION_POLICY_DECISION(decision));
    //     if(webkit_navigation_action_is_user_gesture(action))
    //     {
    //         move_webview_to_tab(self);
    //     }
    // }
    return FALSE;
}
#endif

template <PropId val>
static void notify_prop_id(OrionWebviewPreviewWindow* self)
{
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[val]);
}

static void
action_added(OrionWebviewPreviewWindow* self, gchar* action_name)
{
    g_signal_emit_by_name(self, "action-added", action_name);
}

static void
action_enabled_changed(OrionWebviewPreviewWindow* self, gchar* action_name, gboolean enabled)
{
    g_signal_emit_by_name(self, "action-enabled-changed", action_name, enabled);
}

static void
action_removed(OrionWebviewPreviewWindow* self, gchar* action_name)
{
    g_signal_emit_by_name(self, "action-removed", action_name);
}

static void
action_state_changed(OrionWebviewPreviewWindow* self, gchar* action_name, GVariant* value)
{
    g_signal_emit_by_name(self, "action-state-changed", action_name, value);
}

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
static void
on_webview_go_back(WebKitWebView* view, OrionWebviewPreviewWindow* self)
{
    webkit_web_view_go_back(view);
}

static void
on_webview_go_forward(WebKitWebView* view, OrionWebviewPreviewWindow* self)
{
    webkit_web_view_go_forward(view);
}

static void
on_back_forward_list_changed(WebKitBackForwardList* bf_list, WebKitBackForwardListItem* added, gpointer removed, OrionWebviewPreviewWindow* self)
{
    g_object_set(self->webview,
        "can-go-back", webkit_back_forward_list_get_back_item(bf_list)?TRUE:FALSE,
        "can-go-forward", webkit_back_forward_list_get_forward_item(bf_list)?TRUE:FALSE,
        NULL);

    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_BACK_LIST]);
    g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_FORWARD_LIST]);
}

static void
on_uri_notify(gpointer data)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(data);

    if(self->webview)
    {
        const gchar* uri_c_str = webkit_web_view_get_uri(self->webview);

        self->cpp_data.uri = uri_c_str?uri_c_str:"";
        g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_URI]);
    }
}
#endif

OrionWebviewPreviewWindow*
orion_webview_preview_window_new(const std::string& uri, const GdkRectangle* pointing_to, MainWindow* win)
{
	OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(g_object_new(
        ORION_WEBVIEW_PREVIEW_WINDOW_TYPE,
        "uri",          uri.c_str(),
        "profile-id",   main_window_get_current_profile_id(win).value,
        "private-mode", (gint64)main_window_get_private_mode(win),
        NULL));

    gtk_window_set_transient_for(GTK_WINDOW(self), GTK_WINDOW(win));
    g_object_bind_property(win, "window-id", self, "window-id", G_BINDING_SYNC_CREATE);

    self->source_win = win;

    g_object_weak_ref(G_OBJECT(win), win_weak_notify, self);

    g_signal_connect_object(win, "primary-pressed", G_CALLBACK(gtk_window_close), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(win, "action-added", G_CALLBACK(action_added), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(win, "action-enabled-changed", G_CALLBACK(action_enabled_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(win, "action-removed", G_CALLBACK(action_removed), self, G_CONNECT_SWAPPED);
    g_signal_connect_object(win, "action-state-changed", G_CALLBACK(action_state_changed), self, G_CONNECT_SWAPPED);
    g_signal_connect(win, "notify::profile-id", G_CALLBACK(notify_prop_id<PROP_PROFILE_ID>), self);
    g_signal_connect(win, "notify::profile-data-holder", G_CALLBACK(notify_prop_id<PROP_PROFILE_DATA_HOLDER>), self);

    //relic of a failed attempt at a draggable and resizable popover, maybe one day
    //gtk_popover_set_pointing_to(GTK_POPOVER(self), pointing_to);

    //duplicating all actions, could be optimsied to only dupe necessary

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    orion::gtk::WeakRefWrapper self_weak(self);
    auto& factory = main_window_get_current_webview_factory(win);

    factory.web_view_for_content([self_weak](WebViewRef webview){
        auto self_strong = self_weak.lock();
        if(!self_strong)
        {
            return;
        }

        OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(self_strong.get());
        if(self->webview)
        {
            g_assert_not_reached();
        }
        else
        {
            self->webview = WEBKIT_WEB_VIEW(g_object_ref(webview.get()));
            adw_toolbar_view_set_content(self->toolbar_view, GTK_WIDGET(self->webview));
            orion_webkit_webview_set_page_tweaks_enabled(ORION_WEBKIT_WEBVIEW(self->webview), TRUE);

            GtkExpression* webview_expression = gtk_object_expression_new(G_OBJECT(self->webview));

            GtkExpression* uri_expression = gtk_property_expression_new(WEBKIT_TYPE_WEB_VIEW, webview_expression, "uri");
            //watch doesn't consume the expression, only bind does
            gtk_expression_watch(uri_expression, self, on_uri_notify, self, NULL);

            //reffing object expression to recycle for 2 propery expressions
            GtkExpression* params[2];
            params[0] = gtk_property_expression_new(WEBKIT_TYPE_WEB_VIEW, gtk_expression_ref(webview_expression), "title");
            params[1] = uri_expression;

            GtkExpression *closure_expr = gtk_cclosure_expression_new(G_TYPE_STRING, NULL, 2, params, G_CALLBACK (format_webview_label), NULL, NULL);
            gtk_expression_bind (closure_expr, self->title_label, "label", self->title_label);

            

            g_signal_connect_object(self->webview, "decide-policy", G_CALLBACK(on_webview_decide_policy), self, G_CONNECT_DEFAULT);
            g_signal_connect_object(self->webview, "go-back", G_CALLBACK(on_webview_go_back), self, G_CONNECT_DEFAULT);
            g_signal_connect_object(self->webview, "go-forward", G_CALLBACK(on_webview_go_forward), self, G_CONNECT_DEFAULT);
            g_signal_connect_object(webkit_web_view_get_back_forward_list(self->webview), "changed", G_CALLBACK(on_back_forward_list_changed), self, G_CONNECT_DEFAULT);
            
            g_signal_connect_object(self->webview, "notify::tls-status", G_CALLBACK(notify_tls_status), self, G_CONNECT_SWAPPED);

            webkit_web_view_load_uri(self->webview, self->cpp_data.uri.c_str());
        }
    });
#endif
    return self;
}

static void
orion_webview_preview_window_map(GtkWidget* widget)
{
    //OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(widget);
    GTK_WIDGET_CLASS(orion_webview_preview_window_parent_class)->map(widget);
}

static void
orion_webview_preview_window_init(OrionWebviewPreviewWindow* win)
{
    std::construct_at(&win->cpp_data);
    gtk_widget_init_template(GTK_WIDGET(win));
}

static void
orion_webview_preview_window_dispose(GObject* object)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(object);
    gtk_widget_dispose_template(GTK_WIDGET(self), ORION_WEBVIEW_PREVIEW_WINDOW_TYPE);
    if(self->source_win)
    {
        g_signal_handlers_disconnect_by_data(self->source_win, self);
        g_object_weak_unref(G_OBJECT(self->source_win), win_weak_notify, self);
        self->source_win = NULL;
    }
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    g_clear_object(&self->webview);
#endif
    if(self->profile_data_holder)
    {
        orion_app_unref_profile_data_holder(self->cpp_data.profile_id);
        self->profile_data_holder = NULL;
    }
    G_OBJECT_CLASS(orion_webview_preview_window_parent_class)->dispose(object);
}

static void
orion_webview_preview_window_finalize(GObject* object)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(object);
    std::destroy_at(&self->cpp_data);
    G_OBJECT_CLASS(orion_webview_preview_window_parent_class)->finalize(object);
}

static void
move_webview_to_tab(OrionWebviewPreviewWindow* self)
{
    if(!self->source_win) return; //should never be the case, we close ourselves on the window weak ref dying

    orion::CreateTabQuery query{};
    query.window_id = main_window_get_window_id(self->source_win); 
    
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if(!self->webview) return;

    g_signal_handlers_disconnect_by_data(webkit_web_view_get_back_forward_list(self->webview), self);
    g_signal_handlers_disconnect_by_data(self->webview, self);

    orion::gtk::StrongRefWrapper webview_strong(g_steal_pointer(&self->webview));
    orion::gtk::WeakRefWrapper window_weak(self->source_win);

    global_core_api->create_tab(query, [webview_strong = std::move(webview_strong), window_weak](orion::CreateTabReply reply){
        VERIFY_LOG(reply.err);

        auto window_strong = window_weak.lock();
        if(window_strong)
        {
            MainWindow* win = MAIN_WINDOW(window_strong.get());

            //NOTE: avoids raciness because this callback is always fired before change notifications
            gtk_widget_unparent(GTK_WIDGET(webview_strong.get()));
            OrionWebviewTabContent* content = orion_webview_tab_content_new_from_webview(reply.id, ORION_WEBKIT_WEBVIEW(webview_strong.get()));

            OrionApp* app = ORION_APP(g_application_get_default());
            orion_app_add_shared_pinned_tab(app, content);
            main_window_autoselect_next_created_tab_with_id(win, reply.id);
        }
    });
#else
    query.uri = "orion://newtab/";
    global_core_api->create_tab(query, [](orion::CreateTabReply reply){
        VERIFY_LOG(reply.err);
    });
#endif
    gtk_window_close(GTK_WINDOW(self));
}




static void
orion_webview_preview_window_set_property(GObject* object, guint property_id, const GValue* value, GParamSpec* pspec)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(object);

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);

    switch (property_id)
    {
        case PROP_WINDOW_ID:
            self->cpp_data.window_id = orion::WindowId(g_value_get_int64(value));
            break;
        case PROP_PROFILE_ID:
            self->cpp_data.profile_id = orion::ProfileId(g_value_get_int64(value));
            if(self->cpp_data.profile_id == orion::ProfileId(0))
            {
                orion::log_error("created preview window with profile id not set");
            }
            else
            {
                self->profile_data_holder = orion_app_ref_profile_data_holder(self->cpp_data.profile_id);
                g_object_notify_by_pspec(G_OBJECT(self), obj_properties[PROP_PROFILE_DATA_HOLDER]);
            }
            break;
        case PROP_PRIVATE_MODE:
            self->cpp_data.private_mode = orion::PrivateMode(g_value_get_int64(value));
            break;
        case PROP_URI:
            {
                const gchar* uri_c_str = g_value_get_string(value);

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                if(self->webview)
                {
                    webkit_web_view_load_uri(self->webview, uri_c_str?uri_c_str:"");
                }
                else
                {
                    self->cpp_data.uri = uri_c_str?uri_c_str:"";
                    g_object_notify_by_pspec(object, obj_properties[PROP_URI]);
                }
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
orion_webview_preview_window_get_property(GObject* object, guint property_id, GValue* value, GParamSpec* pspec)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(object);

    auto wrapped_ref = std::make_shared<orion::gtk::WeakRefWrapper>(self);

    switch (property_id)
    {
        case PROP_WINDOW_ID:
            g_value_set_int64(value, self->cpp_data.window_id.value);
            break;
        case PROP_PROFILE_ID:
            g_value_set_int64(value, self->cpp_data.profile_id.value);
            break;
        case PROP_PROFILE_DATA_HOLDER:
            g_value_set_object(value, self->profile_data_holder);
            break;
        case PROP_WEB_VIEW_FACTORY:
            if(self->source_win) g_value_set_pointer(value, &(main_window_get_current_webview_factory(self->source_win)));
            break;
        case PROP_PRIVATE_MODE:
            g_value_set_int64(value, (gint64)self->cpp_data.private_mode);
            break;
        case PROP_URI:
            g_value_set_string(value, self->cpp_data.uri.c_str());
            break;
        case PROP_TLS_STATUS:
            {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                if(self->webview)
                {
                    g_object_get_property(G_OBJECT(self->webview), "tls-status", value);
                }
                else
                {
                    g_value_set_int64(value, gint64(orion::TlsStatus::none));
                }
#else           
                g_value_set_int64(value, gint64(orion::TlsStatus::none));        
#endif
            }
            break;
        case PROP_BACK_LIST:
            {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                GList* list = NULL;
                if(self->webview)
                {
                    WebKitBackForwardList* bf_list = webkit_web_view_get_back_forward_list(self->webview);
                    GList* wk_back_list = webkit_back_forward_list_get_back_list(bf_list);
                    defer([wk_back_list]{ g_list_free(wk_back_list); }); //captures pointer to start, meaning we can now modify wk_back_list

                    GList* result_tail = list;
                    for(; wk_back_list; wk_back_list = wk_back_list->next)
                    {
                        result_tail = g_list_append(result_tail, new orion::BackForwardListEntry{
                            .uri = webkit_back_forward_list_item_get_uri(WEBKIT_BACK_FORWARD_LIST_ITEM(wk_back_list->data)),
                            .title = webkit_back_forward_list_item_get_title(WEBKIT_BACK_FORWARD_LIST_ITEM(wk_back_list->data))
                        });
                    }
                }
                g_value_take_boxed(value, list);
#else           
                g_value_set_boxed(value, NULL);        
#endif
            }
            break;
        case PROP_FORWARD_LIST:
            {
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
                GList* list = NULL;
                if(self->webview)
                {
                    WebKitBackForwardList* bf_list = webkit_web_view_get_back_forward_list(self->webview);
                    GList* wk_forward_list = webkit_back_forward_list_get_forward_list(bf_list);
                    defer([wk_forward_list]{ g_list_free(wk_forward_list); }); //captures pointer to start, meaning we can now modify wk_forward_list

                    for(; wk_forward_list; wk_forward_list = wk_forward_list->next)
                    {
                        list = g_list_prepend(list, new orion::BackForwardListEntry{
                            .uri = webkit_back_forward_list_item_get_uri(WEBKIT_BACK_FORWARD_LIST_ITEM(wk_forward_list->data)),
                            .title = webkit_back_forward_list_item_get_title(WEBKIT_BACK_FORWARD_LIST_ITEM(wk_forward_list->data))
                        });
                    }
                }
                g_value_take_boxed(value, list);
#else           
                g_value_set_boxed(value, NULL);        
#endif
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
            break;
    }
}

static void
add_action(GActionMap* map, GAction* action)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(map);
    
    if(self->source_win) g_action_map_add_action(G_ACTION_MAP(self->source_win), action);
}

static GAction*
lookup_action(GActionMap* map, const gchar* name)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(map);
    
    if(self->source_win) return g_action_map_lookup_action(G_ACTION_MAP(self->source_win), name);
    else return NULL;
}

static void
remove_action(GActionMap* map, const gchar* name)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(map);
    
    if(self->source_win) g_action_map_remove_action(G_ACTION_MAP(self->source_win), name);
}

static void
orion_webview_preview_window_action_map_iface_init(GActionMapInterface* iface)
{
    iface->add_action = add_action;
    iface->lookup_action = lookup_action;
    iface->remove_action = remove_action;
}

static gchar**
list_actions(GActionGroup* group)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(group);
    
    if(self->source_win)
    {
        return g_action_group_list_actions(G_ACTION_GROUP(self->source_win));
    }
    else
    {
        return g_new0(gchar*, 1);
    }
}

static gboolean
query_action(
    GActionGroup* group,
    const gchar* name,
    gboolean* enabled,
    const GVariantType** parameter_type,
    const GVariantType** state_type,
    GVariant** state_hint,
    GVariant** state)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(group);
    
    if(self->source_win)
    {
        return g_action_group_query_action(G_ACTION_GROUP(self->source_win), name, enabled, parameter_type, state_type, state_hint, state);
    }
    else
    {
        return FALSE;
    }
}

static void
activate_action(
    GActionGroup* group,
    const gchar* action_name,
    GVariant* parameter)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(group);
    
    if(self->source_win)
    {
        g_action_group_activate_action(G_ACTION_GROUP(self->source_win), action_name, parameter);
    }
    else
    {
        return;
    }
}

static void
change_action_state(
    GActionGroup* group,
    const gchar* action_name,
    GVariant* value)
{
    OrionWebviewPreviewWindow* self = ORION_WEBVIEW_PREVIEW_WINDOW(group);
    
    if(self->source_win)
    {
        g_action_group_change_action_state(G_ACTION_GROUP(self->source_win), action_name, value);
    }
    else
    {
        return;
    }
}

static void
orion_webview_preview_window_action_group_iface_init(GActionGroupInterface* iface)
{
    iface->list_actions = list_actions;
    iface->query_action = query_action;
    iface->activate_action = activate_action;
    iface->change_action_state = change_action_state;
}

static gboolean
on_window_key_pressed(GtkEventControllerKey* controller, guint keyval, guint keycode, GdkModifierType state, OrionWebviewPreviewWindow* self)
{
    if(keyval == GDK_KEY_Escape)
    {
        gtk_window_close(GTK_WINDOW(self));
        return TRUE;
    }
    return FALSE;
}

static void
on_notify_default_width(GtkWindow* self_window, GParamSpec* pspec, OrionWebviewPreviewWindow* self)
{
    int width = 0;
    gtk_window_get_default_size(self_window, &width, NULL);
    global_core_api->set_setting(self->cpp_data.profile_id, orion::settings::appearance::web_preview_width, width,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        });
}

static void
on_notify_default_height(GtkWindow* self_window, GParamSpec* pspec, OrionWebviewPreviewWindow* self)
{
    int height = 0;
    gtk_window_get_default_size(self_window, NULL, &height);
    global_core_api->set_setting(self->cpp_data.profile_id, orion::settings::appearance::web_preview_height, height,
        [](orion::SetSettingReply<int64_t> reply){
            VERIFY_LOG(reply.err);
        });
}




static void
orion_webview_preview_window_class_init(OrionWebviewPreviewWindowClass* klass)
{
    GtkWidgetClass* widget_class = GTK_WIDGET_CLASS(klass);

    widget_class->map = orion_webview_preview_window_map;

	gtk_widget_class_set_template_from_resource(widget_class, GRESOURCE_PREFIX "/src/window/orion_webview_tabview_widgets/orion_webview_preview_window.ui");
    
    gtk_widget_class_bind_template_child(widget_class, OrionWebviewPreviewWindow, toolbar_view);
    gtk_widget_class_bind_template_child(widget_class, OrionWebviewPreviewWindow, title_label);
    gtk_widget_class_bind_template_child(widget_class, OrionWebviewPreviewWindow, header);
    gtk_widget_class_bind_template_callback(widget_class, move_webview_to_tab);
    gtk_widget_class_bind_template_callback(widget_class, on_window_key_pressed);
    gtk_widget_class_bind_template_callback(widget_class, on_notify_default_width);
    gtk_widget_class_bind_template_callback(widget_class, on_notify_default_height);

    GObjectClass* object_class = G_OBJECT_CLASS(klass);

    object_class->dispose = orion_webview_preview_window_dispose;
    object_class->finalize = orion_webview_preview_window_finalize;
    object_class->get_property = orion_webview_preview_window_get_property;
    object_class->set_property = orion_webview_preview_window_set_property;

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
}