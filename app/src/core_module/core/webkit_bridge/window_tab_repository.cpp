#include <core/webkit_bridge/API/bridge.h>
#include <core/core_api.h>
#include <core/dto/tab_dto.h>
#include <core/dto/window_dto.h>
#include <core/settings.h>
#include <core/webkit_bridge/sequence_order.h>

#include <common/src/coroutine.h>
#include <common/src/log.h>

#include <algorithm>
#include <format>
#include <utility>

namespace orion::bridge {

namespace {

std::string repository_log_context(ProfileId profile_id, bool is_private)
{
    return std::format("[window-tab-repo profile={} world={}]", profile_id.value, is_private ? "private" : "normal");
}

template<typename... Args>
void trace_repository(const std::string& context, std::format_string<Args...> format, Args&&... args)
{
#ifndef NDEBUG
    log_debug("%s %s", context.c_str(), std::format(format, std::forward<Args>(args)...).c_str());
#endif
}

} // namespace

WindowTabModelRepository::WindowTabModelRepository(ProfileWorld& parent)
    : m_parent(parent.weak_from_this())
{
}

void
WindowTabModelRepository::hydrate() {
    if(m_hydration_state == HydrationState::querying
        || m_hydration_state == HydrationState::applying
        || m_hydration_state == HydrationState::hydrated) return;
    m_hydration_state = HydrationState::querying;

    [](std::weak_ptr<ProfileWorld> parent) -> detached_task {
        if(auto world = parent.lock()) {
            auto& repository = world->model_repository();
            const auto context = repository_log_context(world->profile_id(), world->private_mode() == PrivateMode::yes);
            auto bridge = world->profile_bridge().lock();
            if(!bridge) {
                repository.m_hydration_state = HydrationState::failed;
                co_return;
            }
            auto& core_api = bridge->core_api();
            auto setting_reply = co_await async(
                [&core_api, profile_id = bridge->profile_id()]
                (Cb<GetSettingReply<settings::AppOpensWith>> cb) {
                    core_api.get_setting(
                        profile_id, settings::general::app_opens_with, std::move(cb));
                });
            if(setting_reply.err) {
                LOG_ERROR(setting_reply.err);
                repository.m_hydration_state = HydrationState::failed;
                co_return;
            }
            repository.m_app_opens_with = setting_reply.value;

            auto reply = co_await async(
                invoke(&core_api, &CoreApi::get_windows_by_query), bridge->profile_id()
            );
            if(reply.err) {
                LOG_ERROR(reply.err);
                repository.m_hydration_state = HydrationState::failed;
                co_return;
            }

            std::stable_sort(reply.windows.begin(), reply.windows.end(),
                [](const WindowDto& left, const WindowDto& right) {
                    return left.seq_num < right.seq_num;
                });

            // Remember the selected logical window for later presentation.
            // Hydration itself never creates a native shell.
            WindowId fallback_window_id{};
            const bool no_windows = reply.windows.empty();
            auto selected = co_await async(
                invoke(&core_api, &CoreApi::get_selected_window_id), bridge->profile_id()
            );
            if(selected.err) {
                LOG_ERROR(selected.err);
                repository.m_hydration_state = HydrationState::failed;
                co_return;
            }
            repository.m_selected_window_id = selected.window_exists
                ? selected.window_id : WindowId{};
            if(!no_windows && !std::ranges::any_of(reply.windows, &WindowDto::has_ui_window)) {
                auto window = selected.window_exists
                    ? std::ranges::find(reply.windows, selected.window_id, &WindowDto::id)
                    : reply.windows.end();
                fallback_window_id = window != reply.windows.end()
                    ? window->id : reply.windows.front().id;
            }

            struct HydratedWindow {
                WindowDto window;
                std::vector<TabDto> tabs;
            };
            std::vector<HydratedWindow> hydrated_windows;
            hydrated_windows.reserve(reply.windows.size());

            // Hydrate only this world's model and ordering.
            // Finish every database read before exposing a partial live model.
            for(WindowDto& window : reply.windows) {
                if(window.is_private != (world->private_mode() == PrivateMode::yes)) {
                    continue;
                }
                auto tabs_reply = co_await async(
                    invoke(&core_api, &CoreApi::get_tabs_by_query),
                    GetTabIdsQuery{.window_id = window.id}
                );
                if(tabs_reply.err) {
                    LOG_ERROR(tabs_reply.err);
                    repository.m_hydration_state = HydrationState::failed;
                    co_return;
                }
                std::stable_sort(tabs_reply.tabs.begin(), tabs_reply.tabs.end(),
                    [](const TabDto& left, const TabDto& right) {
                        return left.seq_num < right.seq_num;
                    });
                hydrated_windows.push_back({
                    .window = std::move(window),
                    .tabs = std::move(tabs_reply.tabs),
                });
            }

            // Shared pinned tabs belong to the normal world. Its hidden window
            // occupies a key there, but must not constrain the private world.
            if(world->private_mode() == PrivateMode::no) {
                auto shared = co_await async(invoke(&core_api, &CoreApi::get_shared_pinned_tabs_window_id),
                    bridge->profile_id());
                if(shared.err) {
                    LOG_ERROR(shared.err);
                    repository.m_hydration_state = HydrationState::failed;
                    co_return;
                }
                if(shared.window_id.value) {
                    auto hidden = co_await async(invoke(&core_api, &CoreApi::get_windows),
                        std::vector<WindowId>{shared.window_id});
                    if(hidden.err) {
                        LOG_ERROR(hidden.err);
                        repository.m_hydration_state = HydrationState::failed;
                        co_return;
                    }
                    for(const auto& window : hidden.windows)
                        repository.m_reserved_window_keys.insert(window.seq_num);
                }
            }

            repository.m_hydration_state = HydrationState::applying;
            for(const HydratedWindow& hydrated : hydrated_windows) {
                const auto& window = hydrated.window;
                const bool has_ui_window = window.has_ui_window || window.id == fallback_window_id;
                WindowLiveId window_live_id{};
                world->windows().create_window(orion::bridge::CreateWindow{
                    .focus_mode = window.is_focus_mode ? FocusMode::yes : FocusMode::no,
                    .is_tab_group = window.is_tab_group,
                    .dont_create_host = !has_ui_window,
                    .name = window.name,
                    .icon_name = window.icon_name,
                    .window_id = window.id,
                    .selected_tab_id = window.selected_tab_id,
                    .keyboard_shortcut = window.keyboard_shortcut,
                    .is_suspended = window.is_suspended,
                }, &window_live_id);
                if(!window_live_id.value) continue;
                trace_repository(context, "hydrate create window db={} live={} seq_num={:.17g} has_ui={} tab_group={} focus={} suspended={}",
                    window.id.value, window_live_id.value, window.seq_num, has_ui_window, window.is_tab_group, window.is_focus_mode, window.is_suspended);

                auto& state = repository.m_state;
                state.windows[window.id] = window;
                for(const TabDto& tab : hydrated.tabs) {
                    TabLiveId tab_live_id{};
                    world->tabs().create_tab(orion::bridge::CreateTab{
                        .window_live_id = window_live_id,
                        .name = tab.name,
                        .icon_name = tab.icon_name,
                        .is_pinned = tab.is_pinned,
                        .is_suspended = tab.is_suspended,
                        .uri = tab.uri,
                        .session_state = tab.session_state,
                        .parent_tab_id = tab.parent_tab_id,
                        .is_collapsed = tab.is_collapsed,
                        .back_forward_list_index = tab.back_forward_list_index,
                        .webkit_back_forward_list_index = tab.webkit_back_forward_list_index,
                        .merged_back_forward_list_length = tab.merged_back_forward_list_length,
                        .snapshot_image = tab.snapshot_image,
                        .tab_id = tab.id,
                    }, &tab_live_id);
                    if(tab_live_id.value) {
                        state.tabs[tab.id] = tab;
                        trace_repository(context, "hydrate create tab db={} live={} window_db={} window_live={} index={} seq_num={:.17g} pinned={} suspended={}",
                            tab.id.value, tab_live_id.value, window.id.value, window_live_id.value,
                            tab.index, tab.seq_num, tab.is_pinned, tab.is_suspended);
                    }
                }
            }

            // ModelState contains only restored records. UI requests made during
            // the reads are still new records at the first checkpoint.
            // Establish the hydrated state as the first committed baseline.
            // Repository checkpointing is intentionally suppressed in applying state.
            bridge->commit(*world);
            repository.m_hydration_state = HydrationState::hydrated;

            // Bootstrap only the normal world of an empty profile. Presentation
            // remains a separate, explicit request.
            if(no_windows && world->private_mode() == PrivateMode::no) {
                bool model_is_empty = false;
                {
                    auto lock = world->live_model().begin();
                    auto snapshot = world->live_model().snapshot(lock);
                    model_is_empty = snapshot && snapshot->windows.empty();
                }
                if(model_is_empty) {
                    world->tabs().create_tab(CreateTab{
                        .uri = "orion://newtab/",
                        .auto_create_window = true,
                    });
                }
            }
            if(repository.m_presentation_requested) repository.present();
            // Requests may have arrived while reads were in flight, including
            // when this world had no saved windows to schedule a later commit.
            // Unchanged restored records produce no database writes; fallback
            // activation and a new empty tab are persisted by this checkpoint.
            bridge->schedule_commit(*world);
        }
    }(m_parent);
}

void
WindowTabModelRepository::present()
{
    m_presentation_requested = true;
    if(m_hydration_state != HydrationState::hydrated) return;
    auto world = m_parent.lock();
    if(!world) return;

    const auto opens_with = m_app_opens_with;
    auto remove_tab_groups = [&] {
        std::vector<WindowLiveId> live_ids;
        for(auto live_id : world->windows().all()) {
            if(world->windows().to_live_window(live_id).is_tab_group) {
                live_ids.push_back(live_id);
            }
        }
        if(!live_ids.empty()) world->windows().remove(std::move(live_ids));
    };
    if(world->private_mode() == PrivateMode::no) {
        auto bridge = world->profile_bridge().lock();
        if(opens_with == settings::AppOpensWith::new_private_window) {
            if(!world->main_windows().all().empty()) {
                m_presentation_requested = false;
                return;
            }
            remove_tab_groups();
            m_presentation_requested = false;
            if(bridge) {
                if(auto private_world = bridge->private_world().lock()) {
                    private_world->present();
                }
            }
            return;
        }
        if(opens_with == settings::AppOpensWith::all_windows_from_last_session
            && bridge) {
            if(auto private_world = bridge->private_world().lock()) {
                private_world->present();
            }
        }
    }

    const bool present_fresh_window =
        ((opens_with == settings::AppOpensWith::new_window
                || opens_with == settings::AppOpensWith::not_set)
            && world->private_mode() == PrivateMode::no)
        || (opens_with == settings::AppOpensWith::new_private_window
            && world->private_mode() == PrivateMode::yes);
    if(present_fresh_window) {
        if(!world->main_windows().all().empty()) {
            m_presentation_requested = false;
            return;
        }
        remove_tab_groups();
        world->tabs().create_tab({
            .uri = "orion://newtab/",
            .auto_create_window = true,
            .auto_create_main_window = true,
        });
        m_presentation_requested = false;
        return;
    }

    std::vector<WindowLiveId> to_present;
    bool has_main_window = false;
    bool has_logical_window = false;
    {
        // Copy the selection while holding the model lock, then release it
        // before mutating the model to create the native presentation.
        auto lock = world->live_model().begin();
        auto snapshot = world->live_model().snapshot(lock);
        if(!snapshot) return;
        has_main_window = !snapshot->main_windows.empty();
        has_logical_window = !snapshot->windows.empty();
        for(const auto& window : snapshot->windows) {
            auto record = m_state.windows.find(window.window_id);
            if(record != m_state.windows.end() && record->second.has_ui_window) {
                to_present.push_back(window.id);
            }
        }
        if(to_present.empty() && m_selected_window_id.value) {
            auto selected = std::ranges::find(snapshot->windows, m_selected_window_id,
                &LiveWindow::window_id);
            if(selected != snapshot->windows.end()) to_present.push_back(selected->id);
        }
        if(to_present.empty() && !snapshot->windows.empty()) {
            to_present.push_back(snapshot->windows.front().id);
        }
    }

    if(has_main_window) {
        m_presentation_requested = false;
        return;
    }
    if(to_present.empty()) {
        if(!has_logical_window && world->private_mode() == PrivateMode::no) {
            m_presentation_requested = false;
            world->tabs().create_tab({
                .uri = "orion://newtab/",
                .auto_create_main_window = true,
            });
        }
        return;
    }

    m_presentation_requested = false;
    for(WindowLiveId window_live_id : to_present) {
        world->main_windows().create_main_window({.window_live_id = window_live_id});
    }
}

bool
WindowTabModelRepository::can_checkpoint() const
{
    return m_hydration_state == HydrationState::applying
        || m_hydration_state == HydrationState::hydrated;
}

bool
WindowTabModelRepository::preserve_windows_on_quit() const
{
    return m_app_opens_with == settings::AppOpensWith::all_windows_from_last_session
        || m_app_opens_with
            == settings::AppOpensWith::all_non_private_windows_from_last_session;
}

namespace {

auto window_properties(const WindowDto& w)
{
    return std::tie(w.name, w.icon_name, w.keyboard_shortcut, w.is_tab_group, w.has_ui_window,
        w.is_focus_mode, w.is_private, w.is_suspended, w.selected_tab_id);
}

auto tab_properties(const auto& t)
{
    return std::tie(t.name, t.icon_name, t.is_pinned, t.is_suspended, t.uri, t.session_state,
        t.parent_tab_id, t.is_collapsed, t.back_forward_list_index,
        t.webkit_back_forward_list_index, t.merged_back_forward_list_length, t.snapshot_image);
}

} // namespace

WindowTabCheckpoint
WindowTabModelRepository::reconcile_window_tab_state(ProfileId profile_id,
    const LiveWindowTabModelSnapshot& snapshot, ModelState& state,
    const std::set<double>& reserved)
{
    WindowTabCheckpoint batch{.profile_id = profile_id};
    // A repository handles one world. The old state supplies context when the
    // incoming snapshot removes its last window.
    const bool is_private = !snapshot.windows.empty()
        ? snapshot.windows.front().private_mode == PrivateMode::yes
        : !state.windows.empty() && state.windows.begin()->second.is_private;
    const auto context = repository_log_context(profile_id, is_private);
    std::vector<const LiveWindow*> ordered_windows;
    std::vector<const LiveTab*> ordered_tabs;
    for(const auto& window : snapshot.windows) ordered_windows.push_back(&window);
    for(const auto& tab : snapshot.tabs) ordered_tabs.push_back(&tab);
    std::ranges::stable_sort(ordered_windows, {}, &LiveWindow::index);
    std::ranges::stable_sort(ordered_tabs, [](const LiveTab* a, const LiveTab* b) {
        return std::tie(a->window_live_id, a->index) < std::tie(b->window_live_id, b->index);
    });

    std::vector<std::optional<double>> previous;
    for(const auto* w : ordered_windows) {
        auto old = state.windows.find(w->window_id);
        previous.push_back(old == state.windows.end() ? std::nullopt : std::optional(old->second.seq_num));
    }
    auto window_keys = reconcile_sequence_order(previous, reserved);
    std::map<WindowLiveId, WindowId> window_ids;
    std::set<WindowId> current_windows;
    std::set<TabId> current_tabs;
    for(size_t i = 0; i < ordered_windows.size(); ++i) {
        const auto& w = *ordered_windows[i];
        window_ids[w.id] = w.window_id;
        current_windows.insert(w.window_id);
        WindowDto record{
            .id = w.window_id, .profile_id = profile_id, .name = w.name, .icon_name = w.icon_name,
            .keyboard_shortcut = w.keyboard_shortcut,
            .is_suspended = w.is_suspended,
            .seq_num = window_keys[i], .is_tab_group = w.is_tab_group,
            .selected_tab_id = w.selected_tab_id, .has_ui_window = w.has_host,
            .is_focus_mode = w.focus_mode == FocusMode::yes, .is_private = w.private_mode == PrivateMode::yes,
        };
        auto old = state.windows.find(w.window_id);
        const bool create = old == state.windows.end();
        const bool properties = create || window_properties(record) != window_properties(old->second);
        const bool order = create || old->second.seq_num != record.seq_num;
        if(create || properties || order) {
            trace_repository(context, "checkpoint {} window live={} db={} index={} seq_num={}->{:.17g} properties={} order={} has_ui={} tab_group={} focus={} suspended={}",
                create ? "create" : "update", w.id.value, w.window_id.value, w.index,
                create ? "none" : std::format("{:.17g}", old->second.seq_num), record.seq_num,
                properties, order, record.has_ui_window, record.is_tab_group, record.is_focus_mode, record.is_suspended);
            batch.windows.push_back({
                .record = record, .create = create, .properties = properties, .order = order,
            });
        }
        state.windows[w.window_id] = std::move(record);
    }

    // Diff callback indices describe insertions. Sequence synthesis uses final
    // sibling positions, including shifts caused by creating/removing peers.
    for(size_t start = 0; start < ordered_tabs.size();) {
        const auto window_live_id = ordered_tabs[start]->window_live_id;
        const auto window_id = window_ids.at(window_live_id);
        size_t end = start;
        previous.clear();
        while(end < ordered_tabs.size() && ordered_tabs[end]->window_live_id == window_live_id) {
            const auto tab_id = ordered_tabs[end]->tab_id;
            auto old = state.tabs.find(tab_id);
            previous.push_back(old != state.tabs.end() && old->second.window_id == window_id
                ? std::optional(old->second.seq_num) : std::nullopt);
            ++end;
        }
        auto keys = reconcile_sequence_order(previous);
        for(size_t i = start; i < end; ++i) {
            const auto& t = *ordered_tabs[i];
            const auto key = keys[i - start];
            current_tabs.insert(t.tab_id);
            TabDto record{
                .id = t.tab_id, .window_id = window_id, .name = t.name, .icon_name = t.icon_name,
                .is_pinned = t.is_pinned, .is_suspended = t.is_suspended, .seq_num = key,
                .uri = t.uri, .session_state = t.session_state, .index = t.index,
                .parent_tab_id = t.parent_tab_id, .is_collapsed = t.is_collapsed,
                .back_forward_list_index = t.back_forward_list_index,
                .webkit_back_forward_list_index = t.webkit_back_forward_list_index,
                .merged_back_forward_list_length = t.merged_back_forward_list_length,
                .snapshot_image = t.snapshot_image,
            };
            auto old = state.tabs.find(t.tab_id);
            const bool create = old == state.tabs.end();
            const bool properties = create || tab_properties(record) != tab_properties(old->second);
            const bool order = create || old->second.window_id != window_id || old->second.seq_num != key;
            if(create || properties || order) {
                trace_repository(context, "checkpoint {} tab live={} db={} window_db={}->{} index={}->{} seq_num={}->{:.17g} properties={} order={} pinned={} suspended={}",
                    create ? "create" : "update", t.id.value, t.tab_id.value,
                    create ? 0 : old->second.window_id.value, window_id.value,
                    create ? -1 : old->second.index, record.index,
                    create ? "none" : std::format("{:.17g}", old->second.seq_num), record.seq_num,
                    properties, order, record.is_pinned, record.is_suspended);
                batch.tabs.push_back({
                    .record = record, .create = create, .properties = properties, .order = order,
                });
            }
            state.tabs[t.tab_id] = std::move(record);
        }
        start = end;
    }
    std::erase_if(state.windows, [&](const auto& item) {
        if(current_windows.contains(item.first)) return false;
        trace_repository(context, "checkpoint delete window db={} seq_num={:.17g}", item.first.value, item.second.seq_num);
        batch.deleted_windows.push_back(item.first);
        return true;
    });
    std::erase_if(state.tabs, [&](const auto& item) {
        if(current_tabs.contains(item.first)) return false;
        trace_repository(context, "checkpoint delete tab db={} window_db={} index={} seq_num={:.17g}",
            item.first.value, item.second.window_id.value, item.second.index, item.second.seq_num);
        batch.deleted_tabs.push_back(item.first);
        return true;
    });
    return batch;
}

void
WindowTabModelRepository::checkpoint(const LiveWindowTabModelSnapshot& snapshot)
{
    if(m_hydration_state != HydrationState::hydrated) return;
    auto world = m_parent.lock();
    if(!world) return;
    auto batch = reconcile_window_tab_state(world->profile_id(), snapshot,
        m_state, m_reserved_window_keys);
    if(batch.empty()) return;
    // State has already advanced, before the host adapter runs. No pending
    // snapshots, acknowledgements, rollback, or retry queue in the repository.
    world->core_api().db_apply_window_tab_checkpoint(std::move(batch), [](Error err) {
        if(err) LOG_ERROR(err);
    });
}

} // namespace orion::bridge
