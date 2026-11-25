#include <core/webkit_bridge/API/bridge.h>

#include <cassert>
#include <utility>

namespace orion::bridge {

namespace {

std::string callback_key(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    return scope.extension_id;
}

}

ContextMenuBridge::ContextMenuBridge() = default;

ContextMenuBridge::~ContextMenuBridge()
{
    m_host = nullptr;
}

HostAttachment
ContextMenuBridge::attach(ContextMenuHost& host)
{
    assert(!m_host || m_host == &host);
    m_host = &host;
    std::weak_ptr<ContextMenuBridge> weak_bridge = weak_from_this();
    return HostAttachment([weak_bridge, host = &host] {
        if(auto bridge = weak_bridge.lock()) bridge->detach(*host);
    });
}

void
ContextMenuBridge::detach(ContextMenuHost& host)
{
    if(m_host == &host) m_host = nullptr;
}

void
ContextMenuBridge::update_from_core(
    const WebExtensions::WebExtensionContextMenuScope& scope,
    const std::vector<WebExtensions::WebExtensionContextMenuItem>& items,
    WebExtensions::WebExtensionContextMenuCallbacks callbacks)
{
    m_callbacks[callback_key(scope)] = std::move(callbacks);
    if(m_host) {
        m_host->web_extension_context_menus_update(scope, items);
    }
}

void
ContextMenuBridge::clear_from_core(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    if(m_host) m_host->web_extension_context_menus_clear(scope);
    m_callbacks.erase(callback_key(scope));
}

void
ContextMenuBridge::refresh_from_core(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    if(m_host) m_host->web_extension_context_menus_refresh(scope);
}

void
ContextMenuBridge::override_from_core(
    WebViewBridge& web_view,
    const WebExtensions::WebExtensionContextMenuOverride& options)
{
    if(m_host) {
        m_host->web_extension_context_menus_override(web_view, options);
    }
}

void
ContextMenuBridge::context_menu_clicked(
    const WebExtensions::WebExtensionContextMenuScope& scope,
    WebExtensions::WebExtensionContextMenuClick click)
{
    if(!m_host) return;
    if(auto it = m_callbacks.find(callback_key(scope));
        it != m_callbacks.end() && it->second.clicked) {
        it->second.clicked(std::move(click));
    }
}

void
ContextMenuBridge::context_menu_shown(
    const WebExtensions::WebExtensionContextMenuScope& scope,
    WebExtensions::WebExtensionContextMenuShown shown)
{
    if(!m_host) return;
    if(auto it = m_callbacks.find(callback_key(scope));
        it != m_callbacks.end() && it->second.shown) {
        it->second.shown(std::move(shown));
    }
}

void
ContextMenuBridge::context_menu_hidden(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    if(!m_host) return;
    if(auto it = m_callbacks.find(callback_key(scope));
        it != m_callbacks.end() && it->second.hidden) {
        it->second.hidden();
    }
}

} // namespace orion::bridge
