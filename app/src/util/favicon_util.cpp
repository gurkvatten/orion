#include "favicon_util.h"
#include "custom_uri_util.h"
#include "coro_util.h"

#include "window/main_window/main_window.h"

#include <common/src/defer.h>

namespace orion::gtk {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

static void
icon_callback(GObject* source, GAsyncResult* result, gpointer user_data)
{
    WebKitFaviconDatabase* db = WEBKIT_FAVICON_DATABASE(source);
    GtkImage* image = GTK_IMAGE(user_data);

    GError* err = NULL;
    GdkTexture* icon = webkit_favicon_database_get_favicon_finish(db, result, &err);
    if(!err)
    {
        gtk_image_set_from_gicon(image, G_ICON(icon));
    }
    else
    {
        log_verbose("favicon_util: %s", err->message);
        g_error_free(err);
        gtk_image_set_from_icon_name(image, "globe-alt2-symbolic");
    }

    g_object_unref(image);
}

#endif

void
load_gtk_image_favicon_from_uri(OrionWindow* win, GtkImage* image, const gchar* uri)
{
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    if (g_strcmp0(uri, "orion://newtab/") == 0)
    {
        gtk_image_set_from_icon_name(image, "compass-symbolic");
    }
    else if(strlen(uri))
    {
        WebExtensionWebViewFactory& factory = orion_window_get_webview_factory(win);
        WebKitNetworkSession* session = (WebKitNetworkSession*)factory.network_session();
        WebKitFaviconDatabase* db = webkit_website_data_manager_get_favicon_database(webkit_network_session_get_website_data_manager(session));
        g_object_ref(image);
        
        webkit_favicon_database_get_favicon(db, uri, NULL, icon_callback, image);
    }
    else
    {
        gtk_image_set_from_icon_name(image, "globe-alt2-symbolic");
    }

#else

    gtk_image_set_from_icon_name(image, "orion-logo");

#endif
}

async_task<GIcon*>
get_icon_for_uri(OrionWindow* win, const orion::Uri& uri)
{
    if(uri.known_scheme() == orion::Uri::Scheme::orion)
    {
        co_return custom_uri_get_corresponding_icon(uri.uri());
    }
    else
    {
        WebExtensionWebViewFactory& factory = orion_window_get_webview_factory(win);
#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
        WebKitNetworkSession* session = (WebKitNetworkSession*)factory.network_session();
        WebKitFaviconDatabase* db = webkit_website_data_manager_get_favicon_database(webkit_network_session_get_website_data_manager(session));

        GlibResult res = co_await orion::gtk::await<webkit_favicon_database_get_favicon, webkit_favicon_database_get_favicon_finish>(
            db,
            uri.uri().c_str(),
            nullptr
        );

        if(!orion::gtk::log_result_error(res))
        {
            co_return G_ICON(res.value());
        }
        else
        {
            co_return NULL;
        }
#else
        co_return NULL; // Fallback if WebKit is not enabled
#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
    }
}


} // namespace orion::gtk