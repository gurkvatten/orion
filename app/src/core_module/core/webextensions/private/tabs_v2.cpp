#include <core/webextensions/private/tabs_v2.h>
#include <core/webextensions/runtime_impl.h>

namespace WebExtensions {

namespace {

rfl::Generic
make_tab(const orion::TabDto& source, bool active)
{
    rfl::Generic::Object tab;
    tab.insert(std::make_pair(std::string("id"), rfl::Generic(source.id.value)));
    tab.insert(std::make_pair(std::string("windowId"), rfl::Generic(source.window_id.value)));
    tab.insert(std::make_pair(std::string("url"), rfl::Generic(source.uri)));
    tab.insert(std::make_pair(std::string("title"), rfl::Generic(source.name)));
    tab.insert(std::make_pair(std::string("index"), rfl::Generic(source.index)));
    tab.insert(std::make_pair(std::string("active"), rfl::Generic(active)));
    tab.insert(std::make_pair(std::string("highlighted"), rfl::Generic(active)));
    tab.insert(std::make_pair(std::string("selected"), rfl::Generic(active)));
    tab.insert(std::make_pair(std::string("pinned"), rfl::Generic(source.is_pinned)));
    tab.insert(std::make_pair(std::string("discarded"), rfl::Generic(source.is_suspended)));
    tab.insert(std::make_pair(std::string("incognito"), rfl::Generic(false)));
    tab.insert(std::make_pair(std::string("status"), rfl::Generic(std::string("complete"))));
    return rfl::Generic(std::move(tab));
}

void
resolve_active_tab(
    orion::CoreApi* api, v2::Promise<std::vector<rfl::Generic>> promise,
    const orion::TabId tab_id)
{
    if(!tab_id.value) {
        promise.resolve(std::vector<rfl::Generic>{});
        return;
    }
    api->get_tabs(
        {tab_id},
        [promise](orion::GetTabsReply reply) {
            if(reply.tabs.empty()) {
                promise.resolve(std::vector<rfl::Generic>{});
                return;
            }
            promise.resolve(std::vector<rfl::Generic>{make_tab(reply.tabs.front(), true)});
        });
}

} // namespace

void
TabsV2Impl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

void
TabsV2Impl::getCurrent(v2::Promise<rfl::Generic> promise)
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) {
        promise.resolve();
        return;
    }
    auto web_view_bridge = promise.webview().lock();
    if(!web_view_bridge) {
        promise.resolve();
        return;
    }
    const std::optional<int64_t> tab_id =
        web_view_bridge->web_view_tab_id();
    if(!tab_id) {
        promise.resolve();
        return;
    }

    rfl::Generic::Object tab;
    tab.insert(std::make_pair(std::string("id"), rfl::Generic(*tab_id)));
    tab.insert(std::make_pair(
        std::string("url"),
        rfl::Generic(web_view_bridge->web_view_uri())));
    promise.resolve(rfl::Generic(std::move(tab)));
}

void
TabsV2Impl::query(
    v2::Promise<std::vector<rfl::Generic>> promise,
    const v2::TabsQueryInfo&)
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) {
        promise.resolve(std::vector<rfl::Generic>{});
        return;
    }

    auto web_view_bridge = promise.webview().lock();
    const std::optional<int64_t> active_tab_id = web_view_bridge
        ? web_view_bridge->web_view_active_tab_id()
        : std::nullopt;
    if(active_tab_id) {
        resolve_active_tab(&profile_world->core_api(), promise, orion::TabId(*active_tab_id));
        return;
    }

    auto* runtime = profile_world->runtime();
    if(!runtime) {
        promise.resolve(std::vector<rfl::Generic>{});
        return;
    }
    orion::CoreApi* api = &profile_world->core_api();
    api->get_selected_window_id(
        runtime->profile_id(),
        [api, promise](orion::GetSelectedWindowIdReply selected_reply) {
            if(!selected_reply.window_exists) {
                promise.resolve(std::vector<rfl::Generic>{});
                return;
            }
            api->get_windows(
                {selected_reply.window_id},
                [api, promise](orion::GetWindowsReply windows_reply) {
                    if(windows_reply.windows.empty()) {
                        promise.resolve(std::vector<rfl::Generic>{});
                        return;
                    }
                    resolve_active_tab(
                        api, promise, windows_reply.windows.front().selected_tab_id);
                });
        });
}

} // namespace WebExtensions
