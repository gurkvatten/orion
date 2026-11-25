#include <core/webextensions/web_view.h>

#include <cassert>
#include <chrono>
#include <deque>

#include <core/webkit_bridge/API/bridge.h>
#include <common/src/dispatch.h>
#include <common/src/log.h>
#include <common/src/string_util.h>
#include <src/net_http_client.h>

namespace WebExtensions {

namespace {

static bool
same_security_origin(const WebExtensionRequestSecurityOrigin& security_origin)
{
    return !security_origin.request_host.empty()
        && security_origin.request_host == security_origin.web_view_host;
}

static std::optional<orion::Uri>
decode_proxy_uri(const orion::Uri& proxy_uri)
{
    std::deque<std::string> path_parts = orion::str::split(proxy_uri.path(), '/');
    if(path_parts.size() < 3) {
        return std::nullopt;
    }
    const std::string scheme = path_parts[1];
    if(scheme != "https" && scheme != "http") {
        return std::nullopt;
    }
    const std::string authority = path_parts[2];
    for(int i = 0; i < 3; ++i) {
        path_parts.pop_front();
    }
    const std::string path = "/" + orion::str::join(path_parts, '/');
    const std::string query = proxy_uri.query().empty() ? "" : ("?" + proxy_uri.query());
    return orion::Uri::parse_strict((scheme + "://" + authority + path + query).c_str());
}

static std::string
header_value(const WebExtensionHttpHeaders& headers, const std::string& name)
{
    for(const auto& header : headers) {
        if(orion::str::equals_case_insensitive(header.first, name)) {
            return header.second;
        }
    }
    return "";
}

static const char*
reason_phrase_for_status(int status_code)
{
    switch(status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 202: return "Accepted";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "";
    }
}

static WebExtensionHttpHeaders
proxy_response_headers(const WebExtensionHttpHeaders& headers, const std::string& origin)
{
    WebExtensionHttpHeaders result;
    result.reserve(headers.size() + (origin.empty() ? 0 : 2));
    for(const auto& header : headers) {
        if(orion::str::equals_case_insensitive(header.first, "Access-Control-Allow-Origin")
            || orion::str::equals_case_insensitive(header.first, "Access-Control-Allow-Credentials")) {
            continue;
        }
        result.push_back(header);
    }
    if(!origin.empty()) {
        result.emplace_back("Access-Control-Allow-Origin", origin);
        result.emplace_back("Access-Control-Allow-Credentials", "true");
    }
    return result;
}

static WebExtensionSchemeResponse
scheme_response(WebExtensionRequestStatus status, int http_status_code, std::string reason_phrase)
{
    WebExtensionSchemeResponse response;
    response.status = status;
    response.http_status_code = http_status_code;
    response.reason_phrase = std::move(reason_phrase);
    return response;
}

struct ProxyRequestState {
    std::shared_ptr<orion::net::HttpClient> http_client;
    WebExtensionSchemeResponseCallback completion;
    WebExtensionSchemeResponse result;
    std::string origin;
    std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
    bool completion_dispatched = false;
};

static void
dispatch_proxy_request_completion(const std::shared_ptr<ProxyRequestState>& state)
{
    if(state->completion_dispatched) {
        return;
    }
    state->completion_dispatched = true;
    state->result.headers = proxy_response_headers(state->result.headers, state->origin);

    WebExtensionSchemeResponseCallback completion = std::move(state->completion);
    WebExtensionSchemeResponse result = std::move(state->result);
    orion::dispatch(
        orion::on_main_thread(),
        [completion = std::move(completion), result = std::move(result)]() mutable {
            completion(std::move(result));
        }
    );
}

static void
schedule_proxy_request_processing(const std::shared_ptr<ProxyRequestState>& state)
{
    orion::dispatch(orion::on_network_thread(), [state]() {
        if(state->completion_dispatched) {
            return;
        }

        const auto elapsed = std::chrono::steady_clock::now() - state->started_at;
        if(elapsed >= std::chrono::milliseconds(orion::net::HTTP_CLIENT_DEFAULT_TIMEOUT_MS)) {
            dispatch_proxy_request_completion(state);
            return;
        }

        const bool has_pending_work = state->http_client->process();
        if(state->completion_dispatched) {
            return;
        }
        if(has_pending_work) {
            schedule_proxy_request_processing(state);
            return;
        }

        dispatch_proxy_request_completion(state);
    });
}

static void
execute_proxy_request_with_http_client(
    const orion::Uri& target_uri,
    const WebExtensionProxyRequest& proxy_request,
    WebExtensionSchemeResponseCallback completion
)
{
    const std::string target_url = target_uri.uri();
    orion::dispatch(
        orion::on_network_thread(),
        [target_url, proxy_request, completion = std::move(completion)]() mutable {
            auto state = std::make_shared<ProxyRequestState>();
            state->http_client = orion::net::HttpClient::create(orion::Uri::parse_strict(target_url.c_str()));
            state->completion = std::move(completion);
            state->result = scheme_response(WebExtensionRequestStatus::failed, 504, "Gateway Timeout");
            state->result.error_message = "Timed out loading proxied extension resource";
            state->origin = header_value(proxy_request.headers, "Origin");

            orion::net::HttpClient::RequestDto request {
                .method = proxy_request.method.empty() ? std::string(orion::net::METHOD_GET) : proxy_request.method,
                .raw_url = target_url,
                .body = {
                    .content_type = proxy_request.body.content_type,
                    .content = proxy_request.body.content,
                    .total_content_length = proxy_request.body.total_content_length.value_or(orion::net::HTTP_CONTENT_LENGTH_UNDEFINED),
                },
                .headers = proxy_request.headers,
                .follow_redirects = true,
            };
            if(request.body.content_type.empty()) {
                request.body.content_type = header_value(proxy_request.headers, "Content-Type");
            }

            state->http_client->execute(
                std::move(request),
                [state_weak = std::weak_ptr(state)](orion::net::HttpClient::ResponseDto response) {
                    auto state = state_weak.lock();
                    if(!state || state->completion_dispatched) {
                        return;
                    }

                    switch(response.state) {
                        case orion::net::HttpClient::ResponseState::headers:
                            if(!response.headers.empty()) {
                                state->result.headers = response.headers;
                                state->result.content_type = header_value(response.headers, "Content-Type");
                            }
                            break;
                        case orion::net::HttpClient::ResponseState::data:
                            if(!response.body.content.empty()) {
                                state->result.body.append(response.body.content);
                            }
                            break;
                        case orion::net::HttpClient::ResponseState::done:
                            if(response.error) {
                                state->result = scheme_response(WebExtensionRequestStatus::failed, 502, "Bad Gateway");
                                state->result.error_message = response.error.message();
                            } else {
                                state->result.status = WebExtensionRequestStatus::ok;
                                state->result.error_message.clear();
                                state->result.http_status_code = response.status_code;
                                state->result.reason_phrase = reason_phrase_for_status(response.status_code);
                            }
                            dispatch_proxy_request_completion(state);
                            break;
                        default:
                            break;
                    }
                }
            );

            if(!state->completion_dispatched) {
                schedule_proxy_request_processing(state);
            }
        }
    );
}

} // namespace

} // namespace WebExtensions

namespace orion::bridge {

using namespace WebExtensions;

struct WebViewFactoryBridge::State {
    using WebExtensionMap =
        std::map<ExtensionId, std::weak_ptr<WebExtension>>;

    bool initialized = false;
    std::optional<WebExtensionMap> background_extensions;
    std::shared_ptr<WebExtensionsRuntime::OnExtensionsUpdateDelegate>
        extensions_update_delegate;
};

WebViewFactoryBridge::WebViewFactoryBridge(
    ProfileWorld& parent)
    : m_parent(parent.weak_from_this())
{
}

WebViewFactoryBridge::~WebViewFactoryBridge()
{
    cleanup();
}

HostAttachment
WebViewFactoryBridge::attach(WebViewFactoryHost& host)
{
    assert(!m_host && "Only one factory attachment is allowed");
    if(m_host) return {};
    m_host = &host;
    m_state = std::make_shared<State>();
    return HostAttachment([weak_bridge = weak_from_this(),
                           weak_state = std::weak_ptr<State>(m_state)] {
        auto bridge = weak_bridge.lock();
        auto state = weak_state.lock();
        if(bridge && state && bridge->m_state == state) bridge->detach();
    });
}

void
WebViewFactoryBridge::detach()
{
    cleanup();
    if(auto world = m_parent.lock()) world->cleanup();
}

void
WebViewFactoryBridge::cleanup()
{
    m_host = nullptr;
    m_state.reset();
}

void
WebViewFactoryBridge::initialize()
{
    assert(m_host && m_state);
    if(!m_host || !m_state || m_state->initialized) return;
    auto state = m_state;
    auto world = m_parent.lock();
    auto* runtime = world ? world->runtime() : nullptr;
    assert(runtime);
    if(!runtime) return;

    m_host->initialize_platform();
    if(m_state != state) return;
    state->initialized = true;
    auto extensions_update = [weak_bridge = weak_from_this(),
                              weak_state = std::weak_ptr<State>(state)](WebExtensionList) {
        auto bridge = weak_bridge.lock();
        auto state = weak_state.lock();
        if(bridge && state && bridge->m_state == state) {
            bridge->update_background_pages();
        }
    };
    state->extensions_update_delegate =
        std::make_shared<WebExtensionsRuntime::OnExtensionsUpdateDelegate>(
            std::move(extensions_update));
    runtime->on_extensions_update += state->extensions_update_delegate;
    if(runtime->is_ready()) update_background_pages();
}

bool
WebViewFactoryBridge::is_ready() const
{
    auto world = m_parent.lock();
    auto* runtime = world ? world->runtime() : nullptr;
    return m_host && m_state && m_state->initialized
        && runtime && runtime->is_ready();
}

void
WebViewFactoryBridge::when_ready(std::function<void()> reply)
{
    auto world = m_parent.lock();
    auto* runtime = world ? world->runtime() : nullptr;
    if(!runtime || !m_host || !m_state) return;
    runtime->when_ready([weak_bridge = weak_from_this(),
                         weak_state = std::weak_ptr<State>(m_state),
                         reply = std::move(reply)] {
        auto bridge = weak_bridge.lock();
        auto state = weak_state.lock();
        if(bridge && state && bridge->m_state == state && bridge->is_ready()) {
            reply();
        }
    });
}

void
WebViewFactoryBridge::register_schemes() const
{
    auto world = m_parent.lock();
    if(!world) return;
    if(!m_host || !m_state) return;
    WebExtensionSchemeRequestHandler extension_resource_handler =
        [weak_bridge=weak_from_this(), weak_state=std::weak_ptr<State>(m_state)](const WebExtensionSchemeRequest& request, WebExtensionSchemeResponseCallback completion) {
            auto factory = weak_bridge.lock();
            auto state = weak_state.lock();
            if(!factory || !state || factory->m_state != state) {
                completion(WebExtensions::scheme_response(
                    WebExtensionRequestStatus::failed, 503, "Service Unavailable"));
                return;
            }
            completion(factory->extension_resource_for_request({
                .uri = request.uri,
                .security_origin = request.security_origin,
            }));
        };

    m_host->register_scheme(
        {
            .scheme = "chrome-extension",
            .secure = true,
        },
        extension_resource_handler
    );
    m_host->register_scheme(
        {
            .scheme = "moz-extension",
            .secure = true,
        },
        std::move(extension_resource_handler)
    );
    m_host->register_scheme(
        {
            .scheme = "extension-proxy",
            .secure = true,
            .cors_enabled = true,
        },
        [weak_bridge=weak_from_this(), weak_state=std::weak_ptr<State>(m_state)](const WebExtensionSchemeRequest& request, WebExtensionSchemeResponseCallback completion) {
            auto factory = weak_bridge.lock();
            auto state = weak_state.lock();
            if(!factory || !state || factory->m_state != state) {
                completion(WebExtensions::scheme_response(
                    WebExtensionRequestStatus::failed, 503, "Service Unavailable"));
                return;
            }
            factory->extension_proxy_for_request(
                {
                    .uri = request.uri,
                    .security_origin = request.security_origin,
                    .method = request.method,
                    .headers = request.headers,
                    .body = request.body,
                },
                std::move(completion)
            );
        }
    );
}

std::weak_ptr<WebViewBridge>
WebViewFactoryBridge::create_web_view_bridge()
{
    auto world = m_parent.lock();
    return world && m_host && m_state && m_state->initialized
        ? world->create_web_view_bridge()
                 : std::weak_ptr<WebViewBridge>();
}

void
WebViewFactoryBridge::configure_web_view(WebViewBridge& web_view)
{
    auto world = m_parent.lock();
    if(auto* runtime = world && m_host ? world->runtime() : nullptr) {
        runtime->configure_web_view(web_view);
    }
}

std::optional<std::string>
WebViewFactoryBridge::document_uri(
    ExtensionId extension_id, WebExtensionDocumentKind kind) const
{
    auto world = m_parent.lock();
    auto* runtime = world ? world->runtime() : nullptr;
    return runtime && m_host
        ? runtime->document_uri(extension_id, kind) : std::nullopt;
}

WebExtensionSchemeResponse
WebViewFactoryBridge::extension_resource_for_request(
    const WebExtensionResourceRequest& request) const
{
    if(!WebExtensions::same_security_origin(request.security_origin)) {
        log_warning("WebExtensionWebView> Attempted to access extension outside security origin");
        return WebExtensions::scheme_response(
            WebExtensionRequestStatus::forbidden, 403, "Forbidden");
    }

    Uri uri = Uri::parse_strict(request.uri.c_str());
    log_debug("WebExtensionWebView> Intercepted request: %s", uri.to_string().c_str());

    auto extension = extension_for_uri(uri);
    if(!extension) {
        return WebExtensions::scheme_response(
            WebExtensionRequestStatus::not_found, 404, "Not Found");
    }
    if(extension->resources().scheme() != uri.scheme()) {
        log_debug(
            "WebExtensionWebView> Extension scheme is '%s' but trying to access with '%s'",
            extension->resources().scheme().c_str(), uri.scheme().c_str());
        return WebExtensions::scheme_response(
            WebExtensionRequestStatus::not_found, 404, "Not Found");
    }

    auto resource = extension->resources().data_for_path(uri.path());
    if(!resource) {
        log_debug("WebExtensionWebView> Resource not found");
        return WebExtensions::scheme_response(
            WebExtensionRequestStatus::not_found, 404, "Not Found");
    }

    auto response = WebExtensions::scheme_response(
        WebExtensionRequestStatus::ok, 200, "OK");
    response.body = std::string(
        reinterpret_cast<const char*>(resource->data), resource->datalen);
    return response;
}

void
WebViewFactoryBridge::extension_proxy_for_request(
    const WebExtensionProxyRequest& request,
    WebExtensionSchemeResponseCallback completion) const
{
    if(!WebExtensions::same_security_origin(request.security_origin)) {
        log_warning("WebExtensionProxy> Attempted to access extension outside security origin");
        completion(WebExtensions::scheme_response(
            WebExtensionRequestStatus::forbidden, 403, "Forbidden"));
        return;
    }

    Uri proxy_uri = Uri::parse_strict(request.uri.c_str());
    log_debug("WebExtensionProxy> Intercepted request: %s", proxy_uri.to_string().c_str());

    if(!extension_for_uri(proxy_uri)) {
        completion(WebExtensions::scheme_response(
            WebExtensionRequestStatus::not_found, 404, "Not Found"));
        return;
    }

    std::optional<Uri> target_uri = WebExtensions::decode_proxy_uri(proxy_uri);
    if(!target_uri) {
        completion(WebExtensions::scheme_response(
            WebExtensionRequestStatus::forbidden, 403, "Forbidden"));
        return;
    }
    log_debug(
        "WebExtensionProxy> Redirected: %s -> %s",
        proxy_uri.to_string().c_str(), target_uri->to_string().c_str());

    WebExtensions::execute_proxy_request_with_http_client(
        *target_uri, request, std::move(completion));
}

void
WebViewFactoryBridge::update_background_pages()
{
    auto world = m_parent.lock();
    if(!world || !m_host || !m_state) return;
    auto* runtime = world->runtime();
    if(!runtime) return;

    State::WebExtensionMap extensions;
    for(const auto& weak_extension
        : runtime->get_extensions().value_or(WebExtensionList())) {
        auto extension = weak_extension.lock();
        if(extension) extensions[extension->extension_id()] = weak_extension;
    }

    WebExtensionBackgroundPagesChange change;
    if(m_state->background_extensions) {
        for(const auto& [extension_id, old_weak_extension]
            : *m_state->background_extensions) {
            auto old_extension = old_weak_extension.lock();
            auto current = extensions.find(extension_id);
            auto new_extension = current == extensions.end()
                ? nullptr
                : current->second.lock();
            if(current == extensions.end()
                || old_extension.get() != new_extension.get()) {
                change.deleted.push_back(extension_id);
            }
        }
    }
    for(const auto& [extension_id, new_weak_extension] : extensions) {
        auto new_extension = new_weak_extension.lock();
        if(!new_extension) continue;
        auto previous = m_state->background_extensions
            ? m_state->background_extensions->find(extension_id)
            : State::WebExtensionMap::const_iterator();
        auto old_extension = !m_state->background_extensions
            || previous == m_state->background_extensions->end()
            ? nullptr
            : previous->second.lock();
        auto uri = new_extension->background_page_uri();
        if(old_extension.get() != new_extension.get() && uri) {
            change.created.push_back({
                .extension_id = extension_id,
                .uri = std::move(*uri),
            });
        }
    }
    m_state->background_extensions = std::move(extensions);

    if(!change.created.empty() || !change.deleted.empty()) {
        m_host->update_background_pages(change);
    }
}

std::shared_ptr<WebExtension>
WebViewFactoryBridge::extension_for_uri(const Uri& uri) const
{
    auto world = m_parent.lock();
    auto* runtime = world ? world->runtime() : nullptr;
    if(!runtime || !runtime->is_ready()) {
        log_warning("WebExtensionWebView> Attempted to access extension resource before initialization");
        return nullptr;
    }

    auto extension = runtime->extension_for_id(ExtensionId(uri.authority()));
    if(!extension) {
        log_debug("WebExtensionWebView> No extension with id '%s'", uri.authority().c_str());
    }
    return extension;
}

} // namespace orion::bridge

namespace WebExtensions {

const char*
extension_proxy_script_source()
{
    return
R"{verbatim}(
(function () {
    if (!window.fetch || !window.XMLHttpRequest) return;

    function encodeProxyUrl(url) {
        const u = new URL(url, document.baseURI);
        if(u.protocol !== "https:" && u.protocol !== "http:") {
            return url;
        }
        // extension-proxy://<extension-id>/<scheme>/<host><pathname>?<search>
        return "extension-proxy://" +
            browser.runtime.id +
            "/" + u.protocol.replace(":", "") +
            "/" + u.host +
            u.pathname +
            u.search;
    }

    function decodeProxyUrl(proxyUrl) {
        const u = new URL(proxyUrl);
        const scheme = u.hostname;
        const path = u.pathname;
        const parts = path.split('/');
        const host = parts[1];
        const restPath = "/" + parts.slice(2).join('/');
        const query = u.search || "";
        return scheme + "://" + host + restPath + query;
    }

    async function normalizeBodyToArrayBuffer(body) {
        if (body == null) return null;
        if (body instanceof ArrayBuffer) return body;
        if (ArrayBuffer.isView(body)) return body.buffer;
        if (body instanceof Blob) return body.arrayBuffer();
        if (typeof ReadableStream !== "undefined" && body instanceof ReadableStream) {
            return new Response(body).arrayBuffer();
        }
        if (typeof FormData !== "undefined" && body instanceof FormData) {
            const req = new Request("data:", { method: "POST", body });
            return req.arrayBuffer();
        }
        if (typeof body === "string") {
            return new TextEncoder().encode(body).buffer;
        }
        if (typeof body === "object") {
            const json = JSON.stringify(body);
            return new TextEncoder().encode(json).buffer;
        }
        throw new TypeError("Unsupported request body type for proxy");
    }

    const __fetch = window.fetch.bind(window);
    const __Request = window.Request;

    async function proxyFetch(input, init) {
        const req = (input instanceof __Request)
            ? input
            : new __Request(input, init);

        const url = req.url;
        const proxyUrl = encodeProxyUrl(url);
        const method = req.method || "GET";
        const headers = new Headers(req.headers);

        const proxyInit = {
            method: method,
            headers: headers,
            mode: req.mode,
            credentials: req.credentials,
            cache: req.cache,
            redirect: req.redirect,
            referrer: req.referrer,
            referrerPolicy: req.referrerPolicy,
            integrity: req.integrity,
            keepalive: req.keepalive,
            signal: req.signal
        };

        if (method.toUpperCase() !== "GET" && method.toUpperCase() !== "HEAD") {
            const clone = req.clone();
            const buf = await clone.arrayBuffer();
            if (buf && buf.byteLength > 0) {
                proxyInit.body = buf;
            }
        }

        const resp = await __fetch(proxyUrl, proxyInit);

        try {
            Object.defineProperty(resp, "url", {
                value: url,
                writable: false,
                enumerable: true,
                configurable: false
            });
        } catch {
        }

        return resp;
    }

    proxyFetch.native = __fetch;
    window.fetch = proxyFetch;

    const __XMLHttpRequest = window.XMLHttpRequest;

    function ProxyXHR() {
        this._xhr = new __XMLHttpRequest();

        const xhr = this._xhr;
        const self = this;

        [
            "onload", "onerror", "onabort", "onloadend", "onloadstart",
            "onprogress", "ontimeout", "onreadystatechange"
        ].forEach(function (name) {
            Object.defineProperty(self, name, {
                get() { return xhr[name]; },
                set(v) { xhr[name] = v; }
            });
        });

        ["timeout", "responseType", "withCredentials"].forEach(function (name) {
            Object.defineProperty(self, name, {
                get() { return xhr[name]; },
                set(v) { xhr[name] = v; }
            });
        });

        [
            "readyState", "response", "responseText", "responseURL",
            "responseXML", "status", "statusText", "upload"
        ].forEach(function (name) {
            Object.defineProperty(self, name, {
                get() { return xhr[name]; }
            });
        });
    }

    ProxyXHR.prototype.open = function (method, url, async, user, password) {
        this._method = method;
        this._originalUrl = url;
        const proxyUrl = encodeProxyUrl(url);
        this._xhr.open(method, proxyUrl, async !== false, user, password);
    };

    ProxyXHR.prototype.send = function (body) {
        this._xhr.send(body);
    };

    ProxyXHR.prototype.abort = function () {
        this._xhr.abort();
    };

    ProxyXHR.prototype.setRequestHeader = function (name, value) {
        this._xhr.setRequestHeader(name, value);
    };

    ProxyXHR.prototype.getAllResponseHeaders = function () {
        return this._xhr.getAllResponseHeaders();
    };

    ProxyXHR.prototype.getResponseHeader = function (name) {
        return this._xhr.getResponseHeader(name);
    };

    ProxyXHR.prototype.overrideMimeType = function (mime) {
        this._xhr.overrideMimeType(mime);
    };

    ProxyXHR.prototype.addEventListener = function (type, listener, options) {
        this._xhr.addEventListener(type, listener, options);
    };

    ProxyXHR.prototype.removeEventListener = function (type, listener, options) {
        this._xhr.removeEventListener(type, listener, options);
    };

    ProxyXHR.prototype.dispatchEvent = function (event) {
        return this._xhr.dispatchEvent(event);
    };

    ProxyXHR.native = __XMLHttpRequest;
    window.XMLHttpRequest = ProxyXHR;
})();

){verbatim}";
}

} // namespace WebExtensions
