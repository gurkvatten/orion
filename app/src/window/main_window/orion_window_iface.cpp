#include "orion_window_iface.h"


G_DEFINE_INTERFACE(OrionWindow, orion_window, GTK_TYPE_WINDOW)

static void
orion_window_default_init(OrionWindowInterface *iface)
{
    g_object_interface_install_property(
        iface,
        g_param_spec_int64(
            "window-id",
            NULL,
            NULL,
            G_MININT64,
            G_MAXINT64,
            0,                            
            GParamFlags(G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_int64(
            "profile-id",
            NULL,
            NULL,
            G_MININT64,
            G_MAXINT64,
            0,                            
            GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_object(
            "profile-data-holder",
            NULL,
            NULL,
            APP_TYPE_PROFILE_DATA_HOLDER,
            GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_pointer(
            "web-view-factory",
            NULL,
            NULL,
            GParamFlags(G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_int64(
            "private-mode",
            "Whether this window is in private mode or no",
            "An extra argument passed at construction",
            G_MININT64,
            G_MAXINT64,
            (gint64)orion::PrivateMode::no,
            GParamFlags(G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    //this should only be utilised for visual purposes, check orion_webview_tab_view.cpp:orion_webview_tab_view_get_visual_uri() for main window's masking
    g_object_interface_install_property(
        iface,
        g_param_spec_string(
            "uri",
            NULL,
            NULL,
            "",
            GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_int64(
            "tls-status",
            NULL,
            NULL,
            G_MININT64,
            G_MAXINT64,
            (gint64)orion::TlsStatus::none,
            GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_boxed(
            "back-list",
            NULL,
            NULL,
            APP_TYPE_BACK_FORWARD_LIST,
            GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
    g_object_interface_install_property(
        iface,
        g_param_spec_boxed(
            "forward-list",
            NULL,
            NULL,
            APP_TYPE_BACK_FORWARD_LIST,
            GParamFlags(G_PARAM_EXPLICIT_NOTIFY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)
        )
    );
}

#define DEFINE_GETTER(prop_name_kebab, prop_name, PropType, VALUE_TYPE, value_type) \
PropType \
orion_window_get_##prop_name(OrionWindow* self) \
{ \
    GValue val = G_VALUE_INIT; \
    g_value_init(&val, VALUE_TYPE); \
    g_object_get_property(G_OBJECT(self), #prop_name_kebab, &val); \
    PropType res = (PropType)(g_value_get_##value_type(&val)); \
    g_value_unset(&val); \
    return res; \
}

DEFINE_GETTER(window-id, window_id, orion::WindowId, G_TYPE_INT64, int64);
DEFINE_GETTER(profile-id, profile_id, orion::ProfileId, G_TYPE_INT64, int64);
DEFINE_GETTER(profile-data-holder, profile_data_holder, ProfileDataHolder*, G_TYPE_OBJECT, object);
DEFINE_GETTER(private-mode, private_mode, orion::PrivateMode, G_TYPE_INT64, int64);
DEFINE_GETTER(tls-status, tls_status, orion::TlsStatus, G_TYPE_INT64, int64);

WebExtensions::WebExtensionWebViewFactory&
orion_window_get_webview_factory(OrionWindow* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_POINTER);
    g_object_get_property(G_OBJECT(self), "web-view-factory", &val);
    WebExtensions::WebExtensionWebViewFactory* res = (WebExtensions::WebExtensionWebViewFactory*)(g_value_get_pointer(&val));
    g_value_unset(&val);
    return *res;
}

std::string
orion_window_get_uri(OrionWindow* self)
{
    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_STRING);
    g_object_get_property(G_OBJECT(self), "uri", &val);
    const char* c_str = g_value_get_string(&val);
    std::string res = c_str?c_str:"";
    g_value_unset(&val);
    return res;
}

orion::BackForwardList*
orion_window_get_back_list(OrionWindow* self)
{
    orion::BackForwardList* back_list = NULL;
    //internally runs g_value_dup_boxed
    g_object_get(self, "back-list", &back_list, NULL);
    return back_list;
}

orion::BackForwardList*
orion_window_get_forward_list(OrionWindow* self)
{
    orion::BackForwardList* forward_list = NULL;
    //internally runs g_value_dup_boxed
    g_object_get(self, "forward-list", &forward_list, NULL);
    return forward_list;
}
