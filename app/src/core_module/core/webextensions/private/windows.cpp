#include <core/webextensions/private/windows.h>
#include <core/webextensions/runtime_impl.h>

namespace WebExtensions {

std::string
WindowsImpl::script_source_for_extension(WebExtension*)
{
    return R"v2windows(
(function(){
    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};

    implementations["Windows.WINDOW_ID_NONE"] = () => -1;
    implementations["Windows.WINDOW_ID_CURRENT"] = () => -2;
})();
)v2windows";
}

void
WindowsImpl::set_profile_world(
    std::weak_ptr<orion::bridge::ProfileWorld> profile_world)
{
    m_profile_world = profile_world;
}

static v2::WindowsWindowStorage
make_window(const int64_t window_id, const std::optional<int64_t> tab_id)
{
    std::vector<rfl::Generic> tabs;
    if(tab_id) {
        rfl::Generic::Object tab;
        tab.insert(std::make_pair(std::string("id"), rfl::Generic(*tab_id)));
        tab.insert(std::make_pair(std::string("windowId"), rfl::Generic(window_id)));
        tab.insert(std::make_pair(std::string("active"), rfl::Generic(true)));
        tab.insert(std::make_pair(std::string("highlighted"), rfl::Generic(true)));
        tab.insert(std::make_pair(std::string("incognito"), rfl::Generic(false)));
        tabs.push_back(rfl::Generic(std::move(tab)));
    }
    return {
        .id = window_id,
        .focused = true,
        .top = std::nullopt,
        .left = std::nullopt,
        .width = std::nullopt,
        .height = std::nullopt,
        .tabs = std::move(tabs),
        .incognito = false,
        .type = "normal",
        .state = "normal",
        .alwaysOnTop = false,
        .sessionId = std::nullopt,
        .title = std::nullopt,
    };
}

void
WindowsImpl::getCurrent(
    v2::Promise<v2::WindowsWindow> promise,
    const v2::WindowsQueryOptions*)
{
    auto profile_world = m_profile_world.lock();
    if(!profile_world) {
        promise.reject("Browser window information is unavailable");
        return;
    }

    auto web_view_bridge = promise.webview().lock();
    const std::optional<int64_t> window_id = web_view_bridge
        ? web_view_bridge->web_view_window_id()
        : std::nullopt;
    if(!window_id) {
        auto* runtime = profile_world->runtime();
        if(!runtime) {
            promise.reject("Extension runtime is unavailable");
            return;
        }
        profile_world->core_api().get_selected_window_id(
            runtime->profile_id(),
            [promise](orion::GetSelectedWindowIdReply reply) {
                const int64_t selected_window_id =
                    reply.window_exists ? reply.window_id.value : 0;
                const auto window = make_window(selected_window_id, std::nullopt);
                promise.resolve(v2::from_storage(window));
            });
        return;
    }
    const std::optional<int64_t> tab_id =
        web_view_bridge->web_view_active_tab_id();
    const auto window = make_window(*window_id, tab_id);
    promise.resolve(v2::from_storage(window));
}

} // namespace WebExtensions
