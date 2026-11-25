#pragma once

#include <core/webkit_bridge/API/bridge.h>

#include <memory>

class OpaqueContextMenu;

namespace orion {

class GtkContextMenuHost final : public bridge::ContextMenuHost {
public:
    explicit GtkContextMenuHost(
        bridge::ContextMenuBridge& bridge);
    ~GtkContextMenuHost();

    GtkContextMenuHost(const GtkContextMenuHost&) = delete;
    GtkContextMenuHost& operator=(const GtkContextMenuHost&) = delete;

    void web_extension_context_menus_update(
        const WebExtensions::WebExtensionContextMenuScope& scope,
        const std::vector<WebExtensions::WebExtensionContextMenuItem>& items) override;
    void web_extension_context_menus_clear(
        const WebExtensions::WebExtensionContextMenuScope& scope) override;
    void web_extension_context_menus_refresh(
        const WebExtensions::WebExtensionContextMenuScope& scope) override;
    void web_extension_context_menus_override(
        bridge::WebViewBridge& webview,
        const WebExtensions::WebExtensionContextMenuOverride& options) override;
    bool populate_context_menu(
        bridge::WebViewBridge& webview,
        OpaqueContextMenu* menu,
        const WebExtensions::WebExtensionContextMenuHitTest& hit_test);
    void context_menu_dismissed(bridge::WebViewBridge& webview);

private:
    bridge::HostAttachment m_attachment;
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace orion
