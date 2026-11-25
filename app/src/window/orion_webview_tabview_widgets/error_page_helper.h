#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <string>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <adwaita.h>

//TODO Could be portable if we get rid of the webview and moved to core?
class ErrorPageBuilder {
public:
    explicit ErrorPageBuilder(WebKitWebView* webView);

    ErrorPageBuilder& set_title(std::string title);
    ErrorPageBuilder& set_description(std::string description);
    ErrorPageBuilder& set_url(std::string url);
    ErrorPageBuilder& hide_adblocker_button(bool hide = true);

    void load();

private:
    WebKitWebView* webView;
    std::string title;
    std::string description;
    std::string url;
    bool hide_adblocker_btn = true;

    std::string get_accent_color_hex(GtkWidget* widget) const;
    std::string rgba_to_hex(const GdkRGBA& rgba) const;
};

#endif

