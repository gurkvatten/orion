#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <common/src/uri.h>
#include <core/webextensions/extension.h>
#include <core/webextensions/repository.h>
#include <core/webextensions/runtime.h>

namespace orion::bridge {
class ProfileBridge;
class ProfileWorld;
class WebViewHost;
}

namespace WebExtensions {

struct WebExtensionSchemeRegistration {
    std::string scheme;
    bool secure = false;
    bool cors_enabled = false;
};

enum class WebExtensionRequestStatus {
    ok,
    forbidden,
    not_found,
    failed
};

struct WebExtensionRequestSecurityOrigin {
    std::string request_host;
    std::string web_view_host;
};

using WebExtensionHttpHeaders = std::vector<std::pair<std::string, std::string>>;

struct WebExtensionRequestBody {
    std::string content_type;
    std::string content;
    std::optional<size_t> total_content_length;
};

struct WebExtensionSchemeRequest {
    std::string uri;
    WebExtensionRequestSecurityOrigin security_origin;
    std::string method;
    WebExtensionHttpHeaders headers;
    WebExtensionRequestBody body;
};

struct WebExtensionSchemeResponse {
    WebExtensionRequestStatus status = WebExtensionRequestStatus::not_found;
    int http_status_code = 200;
    std::string reason_phrase;
    std::string content_type;
    WebExtensionHttpHeaders headers;
    std::string body;
    std::string error_message;
};

struct WebExtensionResourceRequest {
    std::string uri;
    WebExtensionRequestSecurityOrigin security_origin;
};

struct WebExtensionBackgroundPage {
    orion::ExtensionId extension_id;
    std::string uri;
};

struct WebExtensionBackgroundPagesChange {
    std::vector<WebExtensionBackgroundPage> created;
    std::vector<orion::ExtensionId> deleted;
};

struct WebExtensionProxyRequest {
    std::string uri;
    WebExtensionRequestSecurityOrigin security_origin;
    std::string method;
    WebExtensionHttpHeaders headers;
    WebExtensionRequestBody body;
};

const char* extension_proxy_script_source();

} // namespace WebExtensions

using WebExtensions::WebExtensionRequestStatus;
using WebExtensions::WebExtensionHttpHeaders;
using WebExtensions::WebExtensionProxyRequest;
using WebExtensions::WebExtensionBackgroundPage;
using WebExtensions::WebExtensionBackgroundPagesChange;
using WebExtensions::WebExtensionResourceRequest;
using WebExtensions::WebExtensionRequestBody;
using WebExtensions::WebExtensionSchemeRegistration;
using WebExtensions::WebExtensionSchemeRequest;
using WebExtensions::WebExtensionSchemeRequestHandler;
using WebExtensions::WebExtensionSchemeResponse;
using WebExtensions::WebExtensionSchemeResponseCallback;
