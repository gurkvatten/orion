#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include "error_page_helper.h"
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include "../util/html_injector.h"
#include "../util/text_util.h"
#include "../util/file_util.h"
#include "../main_window/main_window.h"
#include <adwaita.h>
#include <common/src/defer.h>

ErrorPageBuilder::ErrorPageBuilder(WebKitWebView* webView)
    : webView(webView)
{
}

ErrorPageBuilder&
ErrorPageBuilder::set_title(std::string title)
{
    this->title = std::move(title);
    return *this;
}

ErrorPageBuilder&
ErrorPageBuilder::set_description(std::string description)
{
    this->description = std::move(description);
    return *this;
}

ErrorPageBuilder&
ErrorPageBuilder::set_url(std::string url)
{
    this->url = std::move(url);
    return *this;
}

ErrorPageBuilder&
ErrorPageBuilder::hide_adblocker_button(bool hide)
{
    this->hide_adblocker_btn = hide;
    return *this;
}

void
ErrorPageBuilder::load()
{
    gchar* html = orion::load_text_from_resource(GRESOURCE_PREFIX "/internalHTML/loading.html");
    defer([html]{ g_free(html); });

    orion::HtmlInjector injector;
    injector.set("Title", title)
            .set("Description", description)
            .set("URL", url)
            .set("reloadWithoutCB-style", hide_adblocker_btn ? "style=\"display:none\"" : "")
            .set("appkit-accent-color", get_accent_color_hex(GTK_WIDGET(webView)));

    std::string result = injector.process(html);
    webkit_web_view_load_alternate_html(webView, result.c_str(), url.c_str(), url.c_str());
}

std::string
ErrorPageBuilder::get_accent_color_hex(GtkWidget* widget) const
{
    AdwStyleManager* styleManager = adw_style_manager_get_default();
    AdwAccentColor accent = adw_style_manager_get_accent_color(styleManager);
    GdkRGBA color;
    adw_accent_color_to_rgba(accent, &color);
    return rgba_to_hex(color);
}

std::string
ErrorPageBuilder::rgba_to_hex(const GdkRGBA& rgba) const
{
    guint r = (guint)(rgba.red * 255);
    guint g = (guint)(rgba.green * 255);
    guint b = (guint)(rgba.blue * 255);
    char* hex = g_strdup_printf("#%02x%02x%02x", r, g, b);
    std::string result(hex);
    g_free(hex);
    return result;
}

#endif