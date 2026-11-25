#pragma once

#include "app.h"


#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

#include <webkit/webkit.h>

#define ORION_WEBKIT_WEBVIEW_TYPE (orion_webkit_webview_get_type ())

G_DECLARE_FINAL_TYPE(OrionWebkitWebview, orion_webkit_webview, ORION, WEBKIT_WEBVIEW, WebKitWebView)

void orion_webkit_webview_ignore_next_download(OrionWebkitWebview* webview);
GTlsCertificate* orion_webkit_webview_get_last_tls_certificate(OrionWebkitWebview* webview);
GTlsCertificateFlags orion_webkit_webview_get_last_tls_errors(OrionWebkitWebview* webview);
const char* orion_webkit_webview_get_last_tls_uri(OrionWebkitWebview* webview);
std::string orion_webkit_webview_get_current_website(OrionWebkitWebview* webview);
void orion_webkit_webview_leave_webkit_fullscreen(OrionWebkitWebview* self);

GAction* orion_webkit_webview_get_go_back_action(OrionWebkitWebview* webview);
GAction* orion_webkit_webview_get_go_forward_action(OrionWebkitWebview* webview);

void orion_webkit_webview_set_page_tweaks_enabled(OrionWebkitWebview* self, gboolean enabled);
void orion_webkit_webview_set_preview_erased(OrionWebkitWebview* self, std::string_view selectors, std::string_view host, gboolean value);
void orion_webkit_webview_update_preview_erased_selectors(OrionWebkitWebview* self, std::string_view selectors);

#endif // defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
