#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <common/src/defer.h>

#include "certificate_error_page_helper.h"
#include "window/main_window/main_window.h"
#include "util/html_injector.h"
#include "util/text_util.h"
#include "util/file_util.h"
#include "error_page_helper.h"

CertificateErrorPageBuilder::CertificateErrorPageBuilder(WebKitWebView* web_view)
    : web_view(web_view)
{
}

CertificateErrorPageBuilder&
CertificateErrorPageBuilder::set_url(std::string url)
{
    this->url = std::move(url);
    return *this;
}

CertificateErrorPageBuilder&
CertificateErrorPageBuilder::set_warning(std::string warning)
{
    this->warning = std::move(warning);
    return *this;
}

CertificateErrorPageBuilder&
CertificateErrorPageBuilder::allow_bypass(bool allowed)
{
    this->can_skip = allowed;
    return *this;
}

void
CertificateErrorPageBuilder::load()
{
    gchar* html = orion::load_text_from_resource(GRESOURCE_PREFIX "/internalHTML/certificate_error.html");
    defer([html] { g_free(html); });

    std::string button_text;
    if (can_skip) {
        std::string first = R"verbatim(<a class="accent-button" href="javascript:CertificateWarningHandler.skipCertificateWarning()"><div>)verbatim";
        std::string second = R"(</div></a>)";
        button_text = orion::text::translatef("%1$s Visit Insecure %2$s", first.c_str(), second.c_str());
    }

    orion::HtmlInjector injector;
    injector.set("TitleText", _("This connection is not private"))
            .set("WarningText", warning)
            .set("SkipButton", button_text);

    std::string result = injector.process(html);
    webkit_web_view_load_alternate_html(
        web_view,
        result.c_str(),
        url.c_str(), // failing_uri
        url.c_str()); // base_uri: potential security issue here, use about:blank for example instead
}
#endif