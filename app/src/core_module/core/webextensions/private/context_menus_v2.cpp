#include <core/webextensions/private/context_menus_v2.h>

#include <core/webextensions/extension.h>
#include <core/webextensions/runtime_impl.h>

#include <algorithm>
#include <set>
#include <type_traits>
#include <utility>

namespace WebExtensions {
namespace {

using MenuId = WebExtensionContextMenuId;

ContextMenusV2Impl*
context_menus(orion::bridge::ProfileWorld& profile_world)
{
    return static_cast<ContextMenusV2Impl*>(
        static_cast<WebExtensionsRuntimeImpl*>(profile_world.runtime()));
}

std::optional<MenuId>
menu_id(const rfl::Generic& value)
{
    if(const auto* integer = std::get_if<int64_t>(&value.get())) {
        return *integer;
    }
    if(const auto* string = std::get_if<std::string>(&value.get())) {
        return *string;
    }
    return std::nullopt;
}

rfl::Generic
generic_id(const MenuId& value)
{
    if(const auto* integer = std::get_if<int64_t>(&value)) {
        return rfl::Generic(*integer);
    }
    return rfl::Generic(std::get<std::string>(value));
}

std::string
menu_item_key(const MenuId& id)
{
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<T, int64_t>) {
            return std::string("n:") + std::to_string(value);
        } else {
            return std::string("s:") + value;
        }
    }, id);
}

std::map<std::string, std::string>
string_map(const rfl::Generic* value)
{
    std::map<std::string, std::string> result;
    if(!value) {
        return result;
    }
    const auto* object = std::get_if<rfl::Generic::Object>(&value->get());
    if(!object) {
        return result;
    }
    for(const auto& [key, item] : *object) {
        if(const auto* string = std::get_if<std::string>(&item.get())) {
            result.insert_or_assign(key, *string);
        }
    }
    return result;
}

WebExtensionContextMenuItem
make_item(const MenuId& id, const v2::ContextMenuCreateProperties& properties)
{
    WebExtensionContextMenuItem item;
    item.id = id;
    item.type = properties.type ? *properties.type : "normal";
    item.title = properties.title ? *properties.title : "";
    item.checked = properties.checked ? *properties.checked : false;
    item.contexts = properties.contexts
        ? *properties.contexts
        : std::vector<std::string>{"page"};
    item.visible = properties.visible ? *properties.visible : true;
    item.enabled = properties.enabled ? *properties.enabled : true;
    item.document_url_patterns = properties.documentUrlPatterns
        ? *properties.documentUrlPatterns
        : std::vector<std::string>{};
    item.target_url_patterns = properties.targetUrlPatterns
        ? *properties.targetUrlPatterns
        : std::vector<std::string>{};
    item.icons = string_map(properties.icons);
    item.view_types = properties.viewTypes
        ? *properties.viewTypes
        : std::vector<std::string>{};
    if(properties.command) {
        item.command = *properties.command;
    }
    if(properties.parentId) {
        item.parent_id = menu_id(*properties.parentId);
    }
    return item;
}

void
apply_update(
    WebExtensionContextMenuItem& item,
    const v2::ContextMenuUpdateProperties& properties)
{
    if(properties.type) item.type = *properties.type;
    if(properties.title) item.title = *properties.title;
    if(properties.checked) item.checked = *properties.checked;
    if(properties.contexts) item.contexts = *properties.contexts;
    if(properties.visible) item.visible = *properties.visible;
    if(properties.enabled) item.enabled = *properties.enabled;
    if(properties.documentUrlPatterns)
        item.document_url_patterns = *properties.documentUrlPatterns;
    if(properties.targetUrlPatterns)
        item.target_url_patterns = *properties.targetUrlPatterns;
    if(properties.icons) item.icons = string_map(properties.icons);
    if(properties.viewTypes) item.view_types = *properties.viewTypes;
    if(properties.parentId) item.parent_id = menu_id(*properties.parentId);
}

std::optional<std::string>
validate_item(
    const auto& items,
    const std::string& key,
    const WebExtensionContextMenuItem& item)
{
    if(item.type != "separator" && item.title.empty()) {
        return "A context menu item requires a title unless its type is separator";
    }
    if(item.contexts.empty()) {
        return "Context menu contexts must not be empty";
    }
    if(!item.parent_id) {
        return std::nullopt;
    }

    std::string parent_key = menu_item_key(*item.parent_id);
    if(parent_key == key) {
        return "A context menu item cannot be its own parent";
    }
    std::set<std::string> visited {key};
    while(true) {
        if(!visited.insert(parent_key).second) {
            return "A context menu item cannot be a child of one of its descendants";
        }
        const auto parent = items.find(parent_key);
        if(parent == items.end()) {
            return "Cannot find the parent context menu item";
        }
        if(!parent->second.item.parent_id) {
            break;
        }
        parent_key = menu_item_key(*parent->second.item.parent_id);
    }
    return std::nullopt;
}

rfl::Generic
make_tab(const orion::TabDto& source)
{
    rfl::Generic::Object tab;
    tab["id"] = source.id.value;
    tab["windowId"] = source.window_id.value;
    tab["url"] = source.uri;
    tab["title"] = source.name;
    tab["index"] = source.index;
    tab["active"] = true;
    tab["highlighted"] = true;
    tab["selected"] = true;
    tab["pinned"] = source.is_pinned;
    tab["discarded"] = source.is_suspended;
    tab["incognito"] = false;
    tab["status"] = std::string("complete");
    return rfl::Generic(std::move(tab));
}

} // namespace

std::string
ContextMenusV2Impl::script_source_for_extension(WebExtension*)
{
    return R"v2contextmenus(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};
    const callbackMaps = new Map();
    const listening = new Set();
    let nextContextMenuId = -Math.floor(Math.random() * 0x10000000000000) - 1;
    const key = (id) => `${typeof id}:${String(id)}`;

    const apiObject = (api) => api === "menus"
        ? window.browser?.menus
        : (window.browser?.contextMenus || window.chrome?.contextMenus);
    const callbacksFor = (api) => {
        if(!callbackMaps.has(api)) callbackMaps.set(api, new Map());
        return callbackMaps.get(api);
    };
    const ensureClickListener = (api) => {
        if(listening.has(api)) return;
        const object = apiObject(api);
        if(!object?.onClicked) return;
        object.onClicked.addListener((info, tab) => {
            const callback = callbacksFor(api).get(key(info.menuItemId));
            if(typeof callback === "function") callback(info, tab);
        });
        listening.add(api);
    };
    const create = (api) => (context) => {
        const properties = context.args.createProperties || {};
        const id = properties.id === undefined ? nextContextMenuId-- : properties.id;
        const callback = context.args.callback;
        context.invokeNative("__create", {id, createProperties: properties}).then(
            () => {
                if(typeof properties.onclick === "function") {
                    callbacksFor(api).set(key(id), properties.onclick);
                    ensureClickListener(api);
                }
                if(typeof callback === "function") callback();
            },
            (error) => {
                if(typeof callback === "function") {
                    window.__bridge_v2__.callbackWithError(callback, error);
                    return;
                }
                console.error("WebExtensions v2 operation failed", error);
            }
        ).catch((error) => console.error("WebExtensions v2 operation failed", error));
        return id;
    };
    const update = (api) => (context) => {
        const id = context.args.id;
        const properties = context.args.updateProperties || {};
        return context.invokeNative("__update", context.args).then(() => {
            if(Object.prototype.hasOwnProperty.call(properties, "onclick")) {
                if(typeof properties.onclick === "function") {
                    callbacksFor(api).set(key(id), properties.onclick);
                    ensureClickListener(api);
                } else {
                    callbacksFor(api).delete(key(id));
                }
            }
        });
    };
    const remove = (api) => (context) => context.invokeNative("__remove", context.args)
        .then(() => callbacksFor(api).delete(key(context.args.menuItemId)));
    const removeAll = (api) => (context) => context.invokeNative("__removeAll", context.args)
        .then(() => callbacksFor(api).clear());

    implementations["ContextMenus.ACTION_MENU_TOP_LEVEL_LIMIT"] = () => 6;
    implementations["ContextMenus.create"] = create("contextMenus");
    implementations["ContextMenus.update"] = update("contextMenus");
    implementations["ContextMenus.remove"] = remove("contextMenus");
    implementations["ContextMenus.removeAll"] = removeAll("contextMenus");
    implementations["Menus.ACTION_MENU_TOP_LEVEL_LIMIT"] = () => 6;
    implementations["Menus.create"] = create("menus");
    implementations["Menus.update"] = update("menus");
    implementations["Menus.remove"] = remove("menus");
    implementations["Menus.removeAll"] = removeAll("menus");
    // The application does not currently expose WebKit's context-menu target
    // token mapping to the isolated world.
    implementations["Menus.getTargetElement"] = () => null;
})();
)v2contextmenus";
}

void
ContextMenusV2Impl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

WebExtensionContextMenuScope
ContextMenusV2Impl::scope_for(const std::string& extension_id) const
{
    WebExtensionContextMenuScope scope;
    scope.extension_id = extension_id;
    scope.extension_name = extension_id;
    auto profile_world = m_profile_world.lock();
    if(!profile_world) return scope;
    auto* runtime = profile_world->runtime();
    if(!runtime) return scope;
    for(const auto& weak_extension : runtime->get_extensions().value_or(WebExtensionList())) {
        if(const auto extension = weak_extension.lock();
            extension && extension->extension_id().value == extension_id) {
            scope.extension_name = extension->default_title().value_or(extension_id);
            break;
        }
    }
    return scope;
}

void
ContextMenusV2Impl::publish(const std::string& extension_id)
{
    auto world = m_profile_world.lock();
    if(!world) return;
    std::vector<const StoredItem*> ordered;
    if(const auto extension = m_items.find(extension_id); extension != m_items.end()) {
        for(const auto& [key, item] : extension->second) ordered.push_back(&item);
    }
    std::sort(ordered.begin(), ordered.end(), [](const auto* left, const auto* right) {
        return left->order < right->order;
    });
    std::vector<WebExtensionContextMenuItem> items;
    items.reserve(ordered.size());
    for(const auto* item : ordered) items.push_back(item->item);

    auto profile_world = m_profile_world;
    WebExtensionContextMenuCallbacks callbacks;
    callbacks.clicked = [profile_world, extension_id](WebExtensionContextMenuClick click) {
        auto world = profile_world.lock();
        auto* impl = world ? context_menus(*world) : nullptr;
        if(impl) impl->handle_click(extension_id, std::move(click));
    };
    callbacks.shown = [profile_world, extension_id](WebExtensionContextMenuShown shown) {
        auto world = profile_world.lock();
        auto* impl = world ? context_menus(*world) : nullptr;
        if(impl) impl->handle_shown(extension_id, std::move(shown));
    };
    callbacks.hidden = [profile_world, extension_id]() {
        auto world = profile_world.lock();
        auto* impl = world ? context_menus(*world) : nullptr;
        if(impl) impl->handle_hidden(extension_id);
    };
    if(auto bridge = world->context_menu_bridge().lock()) {
        bridge->update_from_core(
            scope_for(extension_id), items, std::move(callbacks));
    }
}

void
ContextMenusV2Impl::__create(
    v2::Promise<void> promise,
    const rfl::Generic& id_value,
    const v2::ContextMenuCreateProperties& create_properties)
{
    const auto id = menu_id(id_value);
    if(!id) {
        promise.reject("Context menu IDs must be strings or integers");
        return;
    }
    auto& items = m_items[promise.extension_id()];
    const std::string key = menu_item_key(*id);
    if(items.contains(key)) {
        promise.reject("Cannot create two context menu items with the same ID");
        return;
    }
    auto item = make_item(*id, create_properties);
    if(const auto error = validate_item(items, key, item)) {
        promise.reject(*error);
        return;
    }
    if(item.type == "radio" && item.checked) {
        for(auto& [candidate_key, candidate] : items) {
            if(candidate.item.type == "radio"
                && candidate.item.parent_id == item.parent_id) {
                candidate.item.checked = false;
            }
        }
    }
    items.emplace(key, StoredItem{std::move(item), m_next_order++});
    publish(promise.extension_id());
    promise.resolve();
}

void
ContextMenusV2Impl::__update(
    v2::Promise<void> promise,
    const rfl::Generic& id_value,
    const v2::ContextMenuUpdateProperties& update_properties)
{
    const auto id = menu_id(id_value);
    if(!id) {
        promise.reject("Context menu IDs must be strings or integers");
        return;
    }
    auto extension = m_items.find(promise.extension_id());
    const std::string key = menu_item_key(*id);
    if(extension == m_items.end() || !extension->second.contains(key)) {
        promise.reject("Cannot find a context menu item with the requested ID");
        return;
    }
    auto updated = extension->second.at(key).item;
    apply_update(updated, update_properties);
    if(const auto error = validate_item(extension->second, key, updated)) {
        promise.reject(*error);
        return;
    }
    if(updated.type == "radio" && updated.checked) {
        for(auto& [candidate_key, candidate] : extension->second) {
            if(candidate_key != key && candidate.item.type == "radio"
                && candidate.item.parent_id == updated.parent_id) {
                candidate.item.checked = false;
            }
        }
    }
    extension->second.at(key).item = std::move(updated);
    publish(promise.extension_id());
    promise.resolve();
}

void
ContextMenusV2Impl::__remove(
    v2::Promise<void> promise, const rfl::Generic& menu_item_id)
{
    const auto id = menu_id(menu_item_id);
    if(!id) {
        promise.reject("Context menu IDs must be strings or integers");
        return;
    }
    auto extension = m_items.find(promise.extension_id());
    const std::string key = menu_item_key(*id);
    if(extension == m_items.end() || !extension->second.contains(key)) {
        promise.reject("Cannot find a context menu item with the requested ID");
        return;
    }

    std::set<std::string> removed {key};
    bool changed = true;
    while(changed) {
        changed = false;
        for(const auto& [candidate_key, candidate] : extension->second) {
            if(candidate.item.parent_id
                && removed.contains(menu_item_key(*candidate.item.parent_id))
                && removed.insert(candidate_key).second) {
                changed = true;
            }
        }
    }
    for(const auto& removed_key : removed) extension->second.erase(removed_key);
    if(extension->second.empty()) m_items.erase(extension);
    publish(promise.extension_id());
    promise.resolve();
}

void
ContextMenusV2Impl::__removeAll(v2::Promise<void> promise)
{
    m_items.erase(promise.extension_id());
    auto profile_world = m_profile_world.lock();
    if(profile_world) {
        if(auto bridge = profile_world->context_menu_bridge().lock()) {
            bridge->clear_from_core(scope_for(promise.extension_id()));
        }
    }
    promise.resolve();
}

void
ContextMenusV2Impl::overrideContext(
    v2::Promise<void> promise,
    const v2::ContextMenuOverrideOptions& options)
{
    if(auto profile_world = m_profile_world.lock()) {
        WebExtensionContextMenuOverride bridge_options;
        bridge_options.extension_id = promise.extension_id();
        bridge_options.show_defaults = options.showDefaults
            ? *options.showDefaults
            : false;
        if(options.context) bridge_options.context = *options.context;
        if(options.bookmarkId) bridge_options.bookmark_id = *options.bookmarkId;
        if(options.tabId) bridge_options.tab_id = *options.tabId;
        auto web_view_bridge = promise.webview().lock();
        if(auto bridge = profile_world->context_menu_bridge().lock();
            bridge && web_view_bridge) {
            bridge->override_from_core(*web_view_bridge, bridge_options);
        }
    }
    promise.resolve();
}

void
ContextMenusV2Impl::refresh(v2::Promise<void> promise)
{
    auto profile_world = m_profile_world.lock();
    if(profile_world) {
        if(auto bridge = profile_world->context_menu_bridge().lock()) {
            bridge->refresh_from_core(scope_for(promise.extension_id()));
        }
    }
    promise.resolve();
}

void
ContextMenusV2Impl::handle_click(
    const std::string& extension_id, WebExtensionContextMenuClick click)
{
    auto extension = m_items.find(extension_id);
    const std::string key = menu_item_key(click.menu_item_id);
    if(extension == m_items.end()) return;
    auto item = extension->second.find(key);
    if(item == extension->second.end() || !item->second.item.visible
        || !item->second.item.enabled) return;

    std::optional<bool> was_checked;
    std::optional<bool> checked;
    if(item->second.item.type == "checkbox") {
        was_checked = item->second.item.checked;
        item->second.item.checked = !item->second.item.checked;
        checked = item->second.item.checked;
        publish(extension_id);
    } else if(item->second.item.type == "radio") {
        was_checked = item->second.item.checked;
        for(auto& [candidate_key, candidate] : extension->second) {
            if(candidate.item.type == "radio"
                && candidate.item.parent_id == item->second.item.parent_id) {
                candidate.item.checked = candidate_key == key;
            }
        }
        checked = true;
        publish(extension_id);
    }

    v2::ContextMenuOnClickDataStorage storage;
    storage.menuItemId = generic_id(item->second.item.id);
    if(item->second.item.parent_id)
        storage.parentMenuItemId = generic_id(*item->second.item.parent_id);
    storage.mediaType = std::move(click.media_type);
    storage.linkUrl = std::move(click.link_url);
    storage.srcUrl = std::move(click.src_url);
    storage.pageUrl = std::move(click.page_url);
    storage.frameUrl = std::move(click.frame_url);
    if(click.frame_id) storage.frameId = static_cast<int>(*click.frame_id);
    storage.selectionText = std::move(click.selection_text);
    storage.editable = click.editable;
    storage.wasChecked = was_checked;
    storage.checked = checked;
    storage.viewType = std::move(click.view_type);
    storage.linkText = std::move(click.link_text);
    storage.bookmarkId = std::move(click.bookmark_id);
    storage.modifiers = std::move(click.modifiers);
    if(click.button) storage.button = static_cast<int>(*click.button);
    if(click.target_element_id)
        storage.targetElementId = static_cast<int>(*click.target_element_id);

    auto notify = [profile_world = m_profile_world, extension_id,
                   storage = std::move(storage)](
        const rfl::Generic& tab) mutable {
        auto world = profile_world.lock();
        auto* impl = world ? context_menus(*world) : nullptr;
        if(!impl) return;
        const auto info = v2::from_storage(storage);
        static_cast<v2::ContextMenus&>(*impl).onClicked.notify_extension(
            extension_id, info, tab);
        static_cast<v2::Menus&>(*impl).onClicked.notify_extension(
            extension_id, info, tab);
    };
    auto profile_world = m_profile_world.lock();
    if(!profile_world || !click.tab_id) {
        notify(rfl::Generic(rfl::Generic::Null));
        return;
    }
    profile_world->core_api().get_tabs(
        {orion::TabId(*click.tab_id)},
        [notify = std::move(notify)](orion::GetTabsReply reply) mutable {
            notify(reply.tabs.empty() ? rfl::Generic(rfl::Generic::Null)
                                      : make_tab(reply.tabs.front()));
        });
}

void
ContextMenusV2Impl::handle_shown(
    const std::string& extension_id, WebExtensionContextMenuShown shown)
{
    v2::ContextMenuShownDataStorage storage;
    for(const auto& id : shown.menu_item_ids) storage.menuIds.push_back(generic_id(id));
    storage.contexts = std::move(shown.contexts);
    storage.viewType = std::move(shown.view_type);
    storage.editable = shown.editable;
    storage.mediaType = std::move(shown.media_type);
    storage.linkUrl = std::move(shown.link_url);
    storage.linkText = std::move(shown.link_text);
    storage.srcUrl = std::move(shown.src_url);
    storage.pageUrl = std::move(shown.page_url);
    storage.frameUrl = std::move(shown.frame_url);
    storage.selectionText = std::move(shown.selection_text);
    if(shown.target_element_id)
        storage.targetElementId = static_cast<int>(*shown.target_element_id);

    auto notify = [profile_world = m_profile_world, extension_id,
                   storage = std::move(storage)](
        const rfl::Generic& tab) mutable {
        auto world = profile_world.lock();
        auto* impl = world ? context_menus(*world) : nullptr;
        if(!impl) return;
        const auto info = v2::from_storage(storage);
        static_cast<v2::ContextMenus&>(*impl).onShown.notify_extension(
            extension_id, info, tab);
        static_cast<v2::Menus&>(*impl).onShown.notify_extension(
            extension_id, info, tab);
    };
    auto profile_world = m_profile_world.lock();
    if(!profile_world || !shown.tab_id) {
        notify(rfl::Generic(rfl::Generic::Null));
        return;
    }
    profile_world->core_api().get_tabs(
        {orion::TabId(*shown.tab_id)},
        [notify = std::move(notify)](orion::GetTabsReply reply) mutable {
            notify(reply.tabs.empty() ? rfl::Generic(rfl::Generic::Null)
                                      : make_tab(reply.tabs.front()));
        });
}

void
ContextMenusV2Impl::handle_hidden(const std::string& extension_id)
{
    v2::ContextMenus::onHidden.notify_extension(extension_id);
    v2::Menus::onHidden.notify_extension(extension_id);
}

void
ContextMenusV2Impl::clear_extension(const std::string& extension_id)
{
    m_items.erase(extension_id);
    auto profile_world = m_profile_world.lock();
    if(profile_world) {
        if(auto bridge = profile_world->context_menu_bridge().lock()) {
            bridge->clear_from_core(scope_for(extension_id));
        }
    }
}

void
ContextMenusV2Impl::clear_all()
{
    auto profile_world = m_profile_world.lock();
    if(profile_world) {
        if(auto bridge = profile_world->context_menu_bridge().lock()) {
            for(const auto& [extension_id, items] : m_items) {
                WebExtensionContextMenuScope scope;
                scope.extension_id = extension_id;
                scope.extension_name = extension_id;
                bridge->clear_from_core(scope);
            }
        }
    }
    m_items.clear();
}

} // namespace WebExtensions
