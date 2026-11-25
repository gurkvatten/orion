#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include <core/core_api.h>

namespace WebExtensions {

class WebExtensionsRuntimeImpl;

class ContextMenusV2Impl : public v2::ContextMenus, public v2::Menus {
public:
    static std::string script_source_for_extension(WebExtension* extension);

    void set_profile_world(std::weak_ptr<orion::bridge::ProfileWorld> profile_world);

    void __create(
        v2::Promise<void> promise,
        const rfl::Generic& id,
        const v2::ContextMenuCreateProperties& create_properties) override;
    void __update(
        v2::Promise<void> promise,
        const rfl::Generic& id,
        const v2::ContextMenuUpdateProperties& update_properties) override;
    void __remove(v2::Promise<void> promise, const rfl::Generic& menu_item_id) override;
    void __removeAll(v2::Promise<void> promise) override;
    void overrideContext(
        v2::Promise<void> promise,
        const v2::ContextMenuOverrideOptions& context_options) override;
    void refresh(v2::Promise<void> promise) override;

    void clear_extension(const std::string& extension_id);
    void clear_all();

private:
    struct StoredItem {
        WebExtensionContextMenuItem item;
        uint64_t order = 0;
    };

    using ExtensionItems = std::map<std::string, StoredItem>;

    WebExtensionContextMenuScope scope_for(const std::string& extension_id) const;
    void publish(const std::string& extension_id);
    void handle_click(
        const std::string& extension_id, WebExtensionContextMenuClick click);
    void handle_shown(
        const std::string& extension_id, WebExtensionContextMenuShown shown);
    void handle_hidden(const std::string& extension_id);

    std::weak_ptr<orion::bridge::ProfileWorld> m_profile_world;
    uint64_t m_next_order = 1;
    std::map<std::string, ExtensionItems> m_items;
};

} // namespace WebExtensions
