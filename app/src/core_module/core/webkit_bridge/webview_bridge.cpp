#include <core/webkit_bridge/API/bridge.h>

#include <cassert>

namespace orion::bridge {

WebViewBridge::WebViewBridge(ProfileWorld& parent)
    : m_parent(parent.weak_from_this())
{
}

WebViewBridge::~WebViewBridge()
{
    m_parent.reset();
    m_host = nullptr;
}

HostAttachment
WebViewBridge::attach(WebViewHost& host)
{
    assert(!m_host || m_host == &host);
    m_host = &host;
    std::weak_ptr<WebViewBridge> weak_bridge = weak_from_this();
    return HostAttachment([weak_bridge, host = &host] {
        if(auto bridge = weak_bridge.lock()) bridge->detach(*host);
    });
}

void
WebViewBridge::detach(WebViewHost& host)
{
    if(m_host != &host) return;
    m_host = nullptr;
    if(auto parent = m_parent.lock()) {
        parent->remove_web_view_bridge(*this);
    }
}

void
WebViewBridge::web_view_call(
    const std::string& body, const std::string& argument0_json,
    const std::string& script_world) const
{
    if(m_host) {
        m_host->web_view_call(body, argument0_json, script_world);
    }
}

void
WebViewBridge::web_view_add_script(
    const std::vector<std::string>& matches,
    const std::vector<std::string>& exclude_matches,
    const std::string& script, const std::string& script_world,
    WebViewScriptInjectionTime injection_time,
    WebViewScriptFrames frames) const
{
    if(m_host) {
        m_host->web_view_add_script(
            matches, exclude_matches, script, script_world,
            injection_time, frames);
    }
}

bool
WebViewBridge::web_view_register_script_message_handler(
    const std::string& name, WebViewHost::WebViewMessageHandler handler,
    void* context, void(*destroy_context)(void*),
    const std::string& script_world) const
{
    if(m_host) {
        return m_host->web_view_register_script_message_handler(
            name, handler, context, destroy_context, script_world);
    }
    if(destroy_context) destroy_context(context);
    return false;
}

std::string
WebViewBridge::web_view_security_origin_get_host() const
{
    return m_host ? m_host->web_view_security_origin_get_host() : std::string();
}

std::string
WebViewBridge::web_view_uri() const
{
    return m_host ? m_host->web_view_uri() : std::string();
}

std::optional<int64_t>
WebViewBridge::web_view_tab_id() const
{
    return m_host ? m_host->web_view_tab_id() : std::nullopt;
}

std::optional<int64_t>
WebViewBridge::web_view_window_id() const
{
    return m_host ? m_host->web_view_window_id() : std::nullopt;
}

std::optional<int64_t>
WebViewBridge::web_view_active_tab_id() const
{
    return m_host ? m_host->web_view_active_tab_id() : std::nullopt;
}

} // namespace orion::bridge
