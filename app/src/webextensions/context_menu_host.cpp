#include "webextensions/context_menu_host.h"

#include <algorithm>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT
#include <webkit/webkit.h>
#endif

namespace orion {

#if defined(ENABLE_WEBKIT) && ENABLE_WEBKIT

namespace {

using ContextMenuClick = WebExtensions::WebExtensionContextMenuClick;
using ContextMenuId = WebExtensions::WebExtensionContextMenuId;
using ContextMenuHitTest = WebExtensions::WebExtensionContextMenuHitTest;
using ContextMenuItem = WebExtensions::WebExtensionContextMenuItem;
using ContextMenuOverride = WebExtensions::WebExtensionContextMenuOverride;
using ContextMenuScope = WebExtensions::WebExtensionContextMenuScope;
using ContextMenuShown = WebExtensions::WebExtensionContextMenuShown;

struct ContextMenuScopeKey {
    std::string extension_id;

    auto operator<=>(const ContextMenuScopeKey&) const = default;
};

ContextMenuScopeKey
scope_key(const ContextMenuScope& scope)
{
    return {scope.extension_id};
}

std::string
menu_id_key(const ContextMenuId& id)
{
    return std::visit([](const auto& value) {
        using Value = std::decay_t<decltype(value)>;
        if constexpr(std::is_same_v<Value, int64_t>) {
            return std::string("n:") + std::to_string(value);
        } else {
            return std::string("s:") + value;
        }
    }, id);
}

bool
contains(const std::vector<std::string>& values, const std::string& value)
{
    return std::find(values.begin(), values.end(), value) != values.end();
}

bool
matches_any_pattern(const std::vector<std::string>& patterns, const std::string& url)
{
    if(patterns.empty()) return true;
    if(url.empty()) return false;
    for(const auto& pattern_string : patterns) {
        GError* error = nullptr;
        WebKitWebExtensionMatchPattern* pattern =
            webkit_web_extension_match_pattern_new_with_string(
                pattern_string.c_str(), &error);
        if(error) g_error_free(error);
        if(!pattern) continue;
        const bool matches = webkit_web_extension_match_pattern_matches_url(
            pattern, url.c_str(), WEBKIT_WEB_EXTENSION_MATCH_PATTERN_OPTIONS_NONE);
        webkit_web_extension_match_pattern_unref(pattern);
        if(matches) return true;
    }
    return false;
}

struct ContextMenuInvocation {
    std::vector<std::string> contexts;
    std::optional<int64_t> tab_id;
    std::optional<std::string> view_type;
    std::optional<std::string> media_type;
    std::optional<std::string> link_text;
    std::optional<std::string> link_url;
    std::optional<std::string> src_url;
    std::optional<std::string> page_url;
    std::optional<std::string> frame_url;
    std::optional<int64_t> frame_id;
    std::optional<std::string> selection_text;
    bool editable = false;
    std::optional<std::string> bookmark_id;
    std::optional<int64_t> target_element_id;
    std::vector<std::string> modifiers;
    std::optional<int64_t> button;
};

ContextMenuClick
make_click(const ContextMenuInvocation& invocation, const ContextMenuId& id)
{
    ContextMenuClick click;
    click.menu_item_id = id;
    click.tab_id = invocation.tab_id;
    click.view_type = invocation.view_type;
    click.media_type = invocation.media_type;
    click.link_text = invocation.link_text;
    click.link_url = invocation.link_url;
    click.src_url = invocation.src_url;
    click.page_url = invocation.page_url;
    click.frame_url = invocation.frame_url;
    click.frame_id = invocation.frame_id;
    click.selection_text = invocation.selection_text;
    click.editable = invocation.editable;
    click.bookmark_id = invocation.bookmark_id;
    click.target_element_id = invocation.target_element_id;
    click.modifiers = invocation.modifiers;
    click.button = invocation.button;
    return click;
}

ContextMenuShown
make_shown(
    const ContextMenuInvocation& invocation,
    std::vector<ContextMenuId> menu_item_ids)
{
    ContextMenuShown shown;
    shown.menu_item_ids = std::move(menu_item_ids);
    shown.contexts = invocation.contexts;
    shown.tab_id = invocation.tab_id;
    shown.view_type = invocation.view_type;
    shown.media_type = invocation.media_type;
    shown.link_text = invocation.link_text;
    shown.link_url = invocation.link_url;
    shown.src_url = invocation.src_url;
    shown.page_url = invocation.page_url;
    shown.frame_url = invocation.frame_url;
    shown.selection_text = invocation.selection_text;
    shown.editable = invocation.editable;
    shown.target_element_id = invocation.target_element_id;
    return shown;
}

bool
item_matches(const ContextMenuItem& item, const ContextMenuInvocation& invocation)
{
    if(!item.visible) return false;
    bool context_matches = contains(item.contexts, "all");
    for(const auto& context : invocation.contexts) {
        context_matches = context_matches || contains(item.contexts, context);
    }
    if(!context_matches) return false;
    if(!item.view_types.empty()
        && (!invocation.view_type || !contains(item.view_types, *invocation.view_type))) {
        return false;
    }
    if(!matches_any_pattern(
        item.document_url_patterns, invocation.page_url.value_or(""))) {
        return false;
    }
    if(item.target_url_patterns.empty()) return true;
    return (invocation.link_url
            && matches_any_pattern(item.target_url_patterns, *invocation.link_url))
        || (invocation.src_url
            && matches_any_pattern(item.target_url_patterns, *invocation.src_url));
}

std::string
projected_title(const ContextMenuItem& item, const ContextMenuScope& scope,
    const ContextMenuInvocation& invocation)
{
    std::string title = item.title.empty() ? scope.extension_name : item.title;
    const std::string selection = invocation.selection_text.value_or("");
    for(size_t position = 0; (position = title.find("%s", position)) != std::string::npos;) {
        title.replace(position, 2, selection);
        position += selection.size();
    }
    return title;
}

struct ContextMenuModel {
    ContextMenuScope scope;
    std::vector<ContextMenuItem> items;
    uint64_t order = 0;
};

struct ProjectedMenuNode {
    const ContextMenuItem* item = nullptr;
    std::vector<ProjectedMenuNode> children;
};

std::optional<ProjectedMenuNode>
project_node(const ContextMenuModel& model, size_t index,
    const ContextMenuInvocation& invocation, size_t depth)
{
    if(depth > model.items.size()) return std::nullopt;
    const ContextMenuItem& item = model.items[index];
    ProjectedMenuNode node {.item = &item, .children = {}};
    const std::string parent_key = menu_id_key(item.id);
    for(size_t child_index = 0; child_index < model.items.size(); ++child_index) {
        const auto& child = model.items[child_index];
        if(!child.parent_id || menu_id_key(*child.parent_id) != parent_key) continue;
        if(auto projected = project_node(model, child_index, invocation, depth + 1)) {
            node.children.push_back(std::move(*projected));
        }
    }
    const bool has_child_action = std::any_of(
        node.children.begin(), node.children.end(), [](const auto& child) {
            return child.item->type != "separator";
        });
    if(item_matches(item, invocation) || has_child_action) return node;
    return std::nullopt;
}

std::vector<ProjectedMenuNode>
project_roots(const ContextMenuModel& model, const ContextMenuInvocation& invocation)
{
    std::vector<ProjectedMenuNode> roots;
    for(size_t index = 0; index < model.items.size(); ++index) {
        if(model.items[index].parent_id) continue;
        if(auto projected = project_node(model, index, invocation, 0)) {
            roots.push_back(std::move(*projected));
        }
    }
    return roots;
}

struct ContextMenuActionData {
    std::weak_ptr<bridge::ContextMenuBridge> bridge;
    ContextMenuScope scope;
    ContextMenuClick click;
};

void
context_menu_action_activated(GSimpleAction*, GVariant*, ContextMenuActionData* data)
{
    if(auto bridge = data->bridge.lock()) {
        bridge->context_menu_clicked(data->scope, data->click);
    }
}

void
destroy_context_menu_action_data(gpointer data, GClosure*)
{
    delete static_cast<ContextMenuActionData*>(data);
}

WebKitContextMenuItem*
make_action_item(bridge::ContextMenuBridge& bridge,
    const ContextMenuModel& model, const ContextMenuItem& item,
    const ContextMenuInvocation& invocation)
{
    static uint64_t next_action_id = 1;
    const std::string action_name =
        "orion-extension-context-menu-" + std::to_string(next_action_id++);
    GSimpleAction* action = nullptr;
    if(item.type == "checkbox" || item.type == "radio") {
        action = g_simple_action_new_stateful(
            action_name.c_str(), nullptr, g_variant_new_boolean(item.checked));
    } else {
        action = g_simple_action_new(action_name.c_str(), nullptr);
    }
    g_simple_action_set_enabled(action, item.enabled);
    auto* action_data = new ContextMenuActionData {
        .bridge = bridge.weak_from_this(),
        .scope = model.scope,
        .click = make_click(invocation, item.id),
    };
    g_signal_connect_data(
        action, "activate", G_CALLBACK(context_menu_action_activated), action_data,
        destroy_context_menu_action_data, G_CONNECT_DEFAULT);
    const std::string title = projected_title(item, model.scope, invocation);
    WebKitContextMenuItem* menu_item = webkit_context_menu_item_new_from_gaction(
        G_ACTION(action), title.c_str(), nullptr);
    g_object_unref(action);
    return menu_item;
}

size_t
append_projected_nodes(WebKitContextMenu* destination,
    const std::vector<ProjectedMenuNode>& nodes, const ContextMenuModel& model,
    const ContextMenuInvocation& invocation, std::vector<ContextMenuId>& displayed_ids,
    bridge::ContextMenuBridge& bridge)
{
    std::optional<ContextMenuId> pending_separator;
    size_t appended_actions = 0;
    for(const auto& node : nodes) {
        const ContextMenuItem& item = *node.item;
        if(item.type == "separator") {
            if(appended_actions > 0) pending_separator = item.id;
            continue;
        }

        WebKitContextMenuItem* menu_item = nullptr;
        std::vector<ContextMenuId> child_ids;
        if(!node.children.empty()) {
            WebKitContextMenu* submenu = webkit_context_menu_new();
            const size_t child_count = append_projected_nodes(
                submenu, node.children, model, invocation, child_ids, bridge);
            if(child_count > 0) {
                menu_item = make_action_item(bridge, model, item, invocation);
                webkit_context_menu_item_set_submenu(menu_item, submenu);
            }
            g_object_unref(submenu);
        } else if(item_matches(item, invocation)) {
            menu_item = make_action_item(bridge, model, item, invocation);
        }
        if(!menu_item) continue;
        if(pending_separator) {
            WebKitContextMenuItem* separator = webkit_context_menu_item_new_separator();
            webkit_context_menu_append(destination, separator);
            displayed_ids.push_back(std::move(*pending_separator));
            pending_separator.reset();
        }
        webkit_context_menu_append(destination, menu_item);
        displayed_ids.push_back(item.id);
        displayed_ids.insert(displayed_ids.end(), child_ids.begin(), child_ids.end());
        ++appended_actions;
    }
    return appended_actions;
}

struct ActiveContextMenu {
    ~ActiveContextMenu()
    {
        for(auto* item : default_items) g_object_unref(item);
        if(menu) g_object_unref(menu);
    }

    void release_projection()
    {
        for(auto* item : default_items) g_object_unref(item);
        default_items.clear();
        if(menu) {
            g_object_unref(menu);
            menu = nullptr;
        }
    }

    WebKitContextMenu* menu = nullptr;
    std::vector<WebKitContextMenuItem*> default_items;
    ContextMenuInvocation invocation;
    std::optional<ContextMenuOverride> override_options;
    std::set<ContextMenuScopeKey> shown_scopes;
    std::vector<ContextMenuScope> hidden_scopes;
};

} // namespace

class GtkContextMenuHost::Impl {
public:
    explicit Impl(bridge::ContextMenuBridge& bridge)
        : m_bridge(bridge.weak_from_this())
    {
    }

    void update(const ContextMenuScope& scope,
        const std::vector<ContextMenuItem>& items)
    {
        const ContextMenuScopeKey key = scope_key(scope);
        if(auto model = m_models.find(key); model != m_models.end()) {
            const uint64_t order = model->second.order;
            model->second = {scope, items, order};
        } else {
            m_models.emplace(key,
                ContextMenuModel {scope, items, m_next_model_order++});
        }
    }

    void clear(const ContextMenuScope& scope)
    {
        m_models.erase(scope_key(scope));
    }

    void refresh(const ContextMenuScope& scope)
    {
        // update() already stores the latest snapshot. WebKitGTK copies the
        // public model immediately after the context-menu signal returns, so
        // refresh applies to the next projection.
        (void)scope;
    }

    void set_override(bridge::WebViewBridge& webview, const ContextMenuOverride& options)
    {
        m_pending_overrides.insert_or_assign(webview.weak_from_this(), options);
    }

    bool populate(bridge::WebViewBridge& webview, OpaqueContextMenu* opaque_menu,
        const ContextMenuHitTest& hit_test, std::optional<int64_t> tab_id)
    {
        auto* menu = reinterpret_cast<WebKitContextMenu*>(opaque_menu);
        dismiss(webview);

        auto session = std::make_unique<ActiveContextMenu>();
        session->menu = WEBKIT_CONTEXT_MENU(g_object_ref(menu));
        for(GList* item = webkit_context_menu_get_items(menu); item; item = item->next) {
            session->default_items.push_back(
                WEBKIT_CONTEXT_MENU_ITEM(g_object_ref(item->data)));
        }
        if(auto pending = m_pending_overrides.find(webview.weak_from_this());
            pending != m_pending_overrides.end()) {
            session->override_options = std::move(pending->second);
            m_pending_overrides.erase(pending);
        }
        session->invocation = invocation_for(hit_test, tab_id, session->override_options);

        ActiveContextMenu& active = *session;
        m_active_menus.insert_or_assign(webview.weak_from_this(), std::move(session));
        rebuild(active, true);
        const bool suppress = webkit_context_menu_get_n_items(menu) == 0;
        active.release_projection();
        if(active.shown_scopes.empty()) {
            m_active_menus.erase(webview.weak_from_this());
        }
        return suppress;
    }

    void dismiss(bridge::WebViewBridge& webview)
    {
        auto active = m_active_menus.find(webview.weak_from_this());
        if(active == m_active_menus.end()) return;
        auto scopes = std::move(active->second->hidden_scopes);
        m_active_menus.erase(active);
        for(const auto& scope : scopes) {
            if(auto bridge = m_bridge.lock()) {
                bridge->context_menu_hidden(scope);
            }
        }
    }

private:
    ContextMenuInvocation invocation_for(const ContextMenuHitTest& hit_test,
        std::optional<int64_t> tab_id,
        const std::optional<ContextMenuOverride>& override_options) const
    {
        ContextMenuInvocation invocation;
        invocation.contexts = hit_test.contexts;
        invocation.tab_id = tab_id;
        invocation.view_type = "tab";
        invocation.media_type = hit_test.media_type;
        invocation.link_text = hit_test.link_text;
        invocation.link_url = hit_test.link_url;
        invocation.src_url = hit_test.src_url;
        invocation.page_url = hit_test.page_url;
        invocation.frame_url = hit_test.frame_url;
        invocation.frame_id = hit_test.frame_id;
        invocation.selection_text = hit_test.selection_text;
        invocation.editable = hit_test.editable;
        invocation.target_element_id = hit_test.target_element_id;
        invocation.modifiers = hit_test.modifiers;
        invocation.button = hit_test.button;
        if(invocation.contexts.empty()) invocation.contexts.push_back("page");

        if(override_options) {
            if(override_options->context) {
                invocation.contexts = {*override_options->context};
            }
            invocation.bookmark_id = override_options->bookmark_id;
            if(override_options->tab_id) invocation.tab_id = override_options->tab_id;
        }
        return invocation;
    }

    bool model_is_selected(
        const ContextMenuModel& model, const ActiveContextMenu& session) const
    {
        return !session.override_options
            || model.scope.extension_id == session.override_options->extension_id;
    }

    void rebuild(ActiveContextMenu& session, bool notify_shown)
    {
        auto bridge = m_bridge.lock();
        if(!bridge) return;
        webkit_context_menu_remove_all(session.menu);
        const bool show_defaults = !session.override_options
            || session.override_options->show_defaults;
        if(show_defaults) {
            for(auto* item : session.default_items) {
                webkit_context_menu_append(session.menu, item);
            }
        }

        std::vector<const ContextMenuModel*> selected_models;
        for(const auto& [key, model] : m_models) {
            if(model_is_selected(model, session)) selected_models.push_back(&model);
        }
        std::sort(selected_models.begin(), selected_models.end(),
            [](const auto* left, const auto* right) {
                return left->order < right->order;
            });

        struct ShownNotification {
            ContextMenuShown shown;
            ContextMenuScope scope;
            ContextMenuScopeKey key;
        };
        std::vector<ShownNotification> notifications;
        bool appended_extension_items = false;
        for(const ContextMenuModel* model : selected_models) {
            auto roots = project_roots(*model, session.invocation);
            const size_t root_actions = std::count_if(
                roots.begin(), roots.end(), [](const auto& node) {
                    return node.item->type != "separator";
                });
            if(root_actions == 0) continue;

            if(!appended_extension_items
                && webkit_context_menu_get_n_items(session.menu) > 0) {
                webkit_context_menu_append(
                    session.menu, webkit_context_menu_item_new_separator());
            }

            std::vector<ContextMenuId> displayed_ids;
            if(root_actions == 1) {
                append_projected_nodes(
                    session.menu, roots, *model, session.invocation, displayed_ids,
                    *bridge);
            } else {
                WebKitContextMenu* extension_menu = webkit_context_menu_new();
                append_projected_nodes(
                    extension_menu, roots, *model, session.invocation, displayed_ids,
                    *bridge);
                const std::string& label = model->scope.extension_name.empty()
                    ? model->scope.extension_id
                    : model->scope.extension_name;
                webkit_context_menu_append(session.menu,
                    webkit_context_menu_item_new_with_submenu(
                        label.c_str(), extension_menu));
                g_object_unref(extension_menu);
            }
            if(displayed_ids.empty()) continue;
            appended_extension_items = true;
            if(notify_shown) {
                notifications.push_back({
                    make_shown(session.invocation, std::move(displayed_ids)),
                    model->scope,
                    scope_key(model->scope),
                });
            }
        }

        for(auto& notification : notifications) {
            session.shown_scopes.insert(notification.key);
            session.hidden_scopes.push_back(notification.scope);
            if(auto bridge = m_bridge.lock()) {
                bridge->context_menu_shown(
                    notification.scope, std::move(notification.shown));
            }
        }
    }

    std::weak_ptr<bridge::ContextMenuBridge> m_bridge;
    uint64_t m_next_model_order = 1;
    std::map<ContextMenuScopeKey, ContextMenuModel> m_models;
    using WebViewRef = std::weak_ptr<bridge::WebViewBridge>;
    std::map<WebViewRef, ContextMenuOverride, std::owner_less<WebViewRef>> m_pending_overrides;
    std::map<WebViewRef, std::unique_ptr<ActiveContextMenu>, std::owner_less<WebViewRef>> m_active_menus;
};

#else

class GtkContextMenuHost::Impl {
public:
    explicit Impl(bridge::ContextMenuBridge&) {}
    void update(const WebExtensions::WebExtensionContextMenuScope&,
        const std::vector<WebExtensions::WebExtensionContextMenuItem>&) {}
    void clear(const WebExtensions::WebExtensionContextMenuScope&) {}
    void refresh(const WebExtensions::WebExtensionContextMenuScope&) {}
    void set_override(bridge::WebViewBridge&,
        const WebExtensions::WebExtensionContextMenuOverride&) {}
    bool populate(bridge::WebViewBridge&, OpaqueContextMenu*,
        const WebExtensions::WebExtensionContextMenuHitTest&,
        std::optional<int64_t>) { return false; }
    void dismiss(bridge::WebViewBridge&) {}
};

#endif

GtkContextMenuHost::GtkContextMenuHost(
    bridge::ContextMenuBridge& bridge)
    : m_attachment(bridge.attach(*this))
    , m_impl(std::make_unique<Impl>(bridge))
{
}

GtkContextMenuHost::~GtkContextMenuHost() = default;

void
GtkContextMenuHost::web_extension_context_menus_update(
    const WebExtensions::WebExtensionContextMenuScope& scope,
    const std::vector<WebExtensions::WebExtensionContextMenuItem>& items)
{
    m_impl->update(scope, items);
}

void
GtkContextMenuHost::web_extension_context_menus_clear(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    m_impl->clear(scope);
}

void
GtkContextMenuHost::web_extension_context_menus_refresh(
    const WebExtensions::WebExtensionContextMenuScope& scope)
{
    m_impl->refresh(scope);
}

void
GtkContextMenuHost::web_extension_context_menus_override(
    bridge::WebViewBridge& webview,
    const WebExtensions::WebExtensionContextMenuOverride& options)
{
    m_impl->set_override(webview, options);
}

bool
GtkContextMenuHost::populate_context_menu(
    bridge::WebViewBridge& webview, OpaqueContextMenu* menu,
    const WebExtensions::WebExtensionContextMenuHitTest& hit_test)
{
    return m_impl->populate(
        webview, menu, hit_test, webview.web_view_tab_id());
}

void
GtkContextMenuHost::context_menu_dismissed(bridge::WebViewBridge& webview)
{
    m_impl->dismiss(webview);
}

} // namespace orion
