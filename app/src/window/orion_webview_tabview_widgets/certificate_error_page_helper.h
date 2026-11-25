#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#include <string>

//TODO Could be portable if we get rid of the webview and moved to core?
class CertificateErrorPageBuilder {
public:
    explicit CertificateErrorPageBuilder(WebKitWebView* web_view);

    CertificateErrorPageBuilder& set_url(std::string url);
    CertificateErrorPageBuilder& set_warning(std::string warning);
    CertificateErrorPageBuilder& allow_bypass(bool allowed = true);
    void load();

private:
    WebKitWebView* web_view;
    std::string url;
    std::string warning;
    bool can_skip = false;
};
#endif