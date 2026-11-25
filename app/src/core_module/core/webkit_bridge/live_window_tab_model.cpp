#include <core/webkit_bridge/API/bridge.h>
#include <core/core_api.h>
#include <core/dto/window_dto.h>

#include <common/src/coroutine.h>
#include <common/src/log.h>

#include <deque>
#include <algorithm>
#include <format>
#include <map>
#include <set>
#include <utility>

namespace orion::bridge {

struct LiveWindowTabModelState {
    std::deque<LiveMainWindow> main_windows;
    std::deque<LiveWindow> windows;
    std::deque<LiveTab> tabs;
};

class LiveWindowTabModel::Impl {
public:
    explicit Impl(const ProfileWorld& world)
        : m_log_context(std::format("[live-model profile={} world={}]", world.profile_id().value,
            world.private_mode() == PrivateMode::yes ? "private" : "normal"))
    {
    }

    template<typename... Args>
    void trace(std::format_string<Args...> format, Args&&... args) const
    {
#ifndef NDEBUG
        log_debug("%s %s", m_log_context.c_str(), std::format(format, std::forward<Args>(args)...).c_str());
#endif
    }

    bool try_lock()
    {
        if(!m_model_is_stable) return false;
        m_model_is_stable = false;
        return true;
    }

    void unlock()
    {
        m_model_is_stable = true;
    }

    bool is_locked() const
    {
        return !m_model_is_stable;
    }

    LiveWindowTabModelState& mutable_model()
    {
        return m_uncommited;
    }

    const LiveWindowTabModelState& mutable_model() const
    {
        return m_uncommited;
    }

    const LiveWindowTabModelState& committed_model() const
    {
        return m_commited;
    }

    void commit()
    {
        m_commited = m_uncommited;
    }

    template<typename Id>
    Id next_id()
    {
        // One sequence for every kind of live node within this world.
        return Id(m_next_id++);
    }

private:
    const std::string m_log_context;
    LiveWindowTabModelState m_commited;
    LiveWindowTabModelState m_uncommited;
    int m_next_id = 1;
    bool m_model_is_stable = true;
};

LiveWindowTabModelLock::LiveWindowTabModelLock(LiveWindowTabModel& model)
    : m_model(&model)
    , m_lock_acquired(model.m_state->try_lock())
{
}

LiveWindowTabModelLock::~LiveWindowTabModelLock()
{
    if(m_lock_acquired) m_model->m_state->unlock();
}

LiveWindowTabModel::LiveWindowTabModel(ProfileWorld& parent) : m_parent(parent.weak_from_this()), m_state(std::make_unique<Impl>(parent))
{
}

LiveWindowTabModel::~LiveWindowTabModel()
{
}

std::vector<MainWindowLiveId>
LiveWindowTabModel::all_main_windows() const
{
    const auto& model = m_state->committed_model();
    std::vector<MainWindowLiveId> ids;
    ids.reserve(model.main_windows.size());
    for(const auto& window : model.main_windows) ids.push_back(window.id);
    return ids;
}

std::vector<WindowLiveId>
LiveWindowTabModel::all_windows() const
{
    const auto& model = m_state->committed_model();
    std::vector<WindowLiveId> ids;
    ids.reserve(model.windows.size());
    for(const auto& window : model.windows) ids.push_back(window.id);
    return ids;
}

std::vector<TabLiveId>
LiveWindowTabModel::all_tabs() const
{
    const auto& model = m_state->committed_model();
    std::vector<TabLiveId> ids;
    ids.reserve(model.tabs.size());
    for(const auto& tab : model.tabs) ids.push_back(tab.id);
    return ids;
}

LiveMainWindow
LiveWindowTabModel::live_main_window(MainWindowLiveId live_id) const
{
    if(m_parent.expired()) return {};
    const auto& windows = std::as_const(*m_state).mutable_model().main_windows;
    auto window = std::ranges::find(windows, live_id, &LiveMainWindow::id);
    return window != windows.end() ? *window : LiveMainWindow{};
}

MainWindowLiveId
LiveWindowTabModel::create_main_window(CreateMainWindow query)
{
    auto world = m_parent.lock();
    if(!world) return {};
    schedule_commit();
    if(m_state->is_locked()) return {};
    auto& model = m_state->mutable_model();
    if(query.window_live_id.value
        && std::ranges::find(model.windows, query.window_live_id, &LiveWindow::id)
            == model.windows.end()) return {};
    auto id = m_state->next_id<MainWindowLiveId>();
    model.main_windows.push_back({.id = id, .private_mode = world->private_mode()});
    activate_window(id, query.window_live_id);
    return id;
}

void
LiveWindowTabModel::activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id)
{
    if(m_parent.expired()) return;
    // Rejected/no-op requests still complete on the next commit.
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& model = m_state->mutable_model();
    auto main = std::ranges::find(model.main_windows, live_id, &LiveMainWindow::id);
    if(main == model.main_windows.end()) return;
    const WindowLiveId previous_window_live_id = main->window_live_id;
    if(window_live_id.value) {
        auto window = std::ranges::find(model.windows, window_live_id, &LiveWindow::id);
        if(window == model.windows.end()) return;
        window->has_host = true;
        window->is_suspended = false;
    }
    for(auto& other : model.main_windows) {
        if(other.window_live_id == window_live_id) other.window_live_id = {};
    }
    main->window_live_id = window_live_id;

    // A logical window without a MainWindow is no longer presented. Keep its
    // data alive (saved windows can be reopened), but persist that it has no UI.
    if(previous_window_live_id.value && previous_window_live_id != window_live_id
        && std::ranges::none_of(model.main_windows,
            [previous_window_live_id](const LiveMainWindow& other) {
                return other.window_live_id == previous_window_live_id;
            })) {
        auto previous = std::ranges::find(
            model.windows, previous_window_live_id, &LiveWindow::id);
        if(previous != model.windows.end()) previous->has_host = false;
    }
}

void
LiveWindowTabModel::remove_main_window(MainWindowLiveId live_id)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& model = m_state->mutable_model();
    auto main = std::ranges::find(model.main_windows, live_id, &LiveMainWindow::id);
    if(main == model.main_windows.end()) return;
    const auto window_live_id = main->window_live_id;
    model.main_windows.erase(main);
    // An empty/transferred shell does not own another shell's logical window.
    if(std::ranges::find(model.main_windows, window_live_id, &LiveMainWindow::window_live_id)
        != model.main_windows.end()) return;
    auto window = std::ranges::find(model.windows, window_live_id, &LiveWindow::id);
    if(window == model.windows.end()) return;
    if(window->is_tab_group) {
        remove_window(window_live_id);
    } else {
        // Saved content survives closing its presentation and can be reactivated.
        window->has_host = false;
    }
}

void
LiveWindowTabModel::quit(bool preserve_windows_on_quit)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;

    auto& model = m_state->mutable_model();
    model.main_windows.clear();
    if(preserve_windows_on_quit) return;

    std::set<WindowLiveId> removed_windows;
    for(const auto& window : model.windows) {
        if(window.is_tab_group) removed_windows.insert(window.id);
    }
    std::erase_if(model.windows, [](const LiveWindow& window) {
        return window.is_tab_group;
    });
    std::erase_if(model.tabs, [&removed_windows](const LiveTab& tab) {
        return removed_windows.contains(tab.window_live_id);
    });
    for(size_t index = 0; index < model.windows.size(); ++index) {
        model.windows[index].index = static_cast<int>(index);
        model.windows[index].has_host = false;
    }
}

WindowLiveId
LiveWindowTabModel::create_window(CreateWindow query)
{
    auto world = m_parent.lock();
    if(!world) return {};
    schedule_commit();
    if(m_state->is_locked()) return {};
    auto& model = m_state->mutable_model();
    if(query.main_window_live_id.value
        && std::ranges::find(model.main_windows, query.main_window_live_id, &LiveMainWindow::id)
            == model.main_windows.end()) return {};
    if(!query.main_window_live_id.value && query.auto_create_main_window) {
        query.main_window_live_id = create_main_window({});
    }
    WindowId window_id = query.window_id.value
        ? query.window_id
        : world->core_api().allocate_window_id();
    if(!window_id.value) return {};
    auto id = m_state->next_id<WindowLiveId>();
    model.windows.push_back(LiveWindow{
        .id = id,
        .private_mode = world->private_mode(),
        .focus_mode = query.focus_mode,
        .is_tab_group = query.is_tab_group,
        .has_host = !query.dont_create_host,
        .name = query.name,
        .icon_name = query.icon_name,
        .index = static_cast<int>(model.windows.size()),
        .window_id = window_id,
        .selected_tab_id = query.selected_tab_id,
        .keyboard_shortcut = std::move(query.keyboard_shortcut),
        .is_suspended = query.is_suspended,
    });
    if(query.main_window_live_id.value) {
        activate_window(query.main_window_live_id, id);
    }
    return id;
}

WindowLiveId
LiveWindowTabModel::duplicate_window(WindowLiveId live_id)
{
    if(m_parent.expired() || m_state->is_locked()) return {};
    const auto& model = std::as_const(*m_state).mutable_model();
    auto source = std::ranges::find(model.windows, live_id, &LiveWindow::id);
    if(source == model.windows.end()) return {};

    std::vector<LiveTab> tabs;
    for(const auto& tab : model.tabs) {
        if(tab.window_live_id == live_id && !tab.is_pinned) tabs.push_back(tab);
    }
    std::ranges::stable_sort(tabs, {}, &LiveTab::index);

    auto duplicate = create_window({
        .is_tab_group = true,
        .icon_name = "window-flat-symbolic",
        .auto_create_main_window = true,
    });
    if(!duplicate.value) return {};

    // Match legacy duplication semantics: pinned tabs and navigation/session
    // history are not copied into the new unsaved group.
    for(const auto& tab : tabs) {
        create_tab({
            .window_live_id = duplicate,
            .name = tab.name,
            .icon_name = tab.icon_name,
            .is_suspended = tab.is_suspended,
            .uri = tab.uri,
        });
    }
    return duplicate;
}

LiveWindow
LiveWindowTabModel::live_window(WindowId window_id) const
{
    if(m_parent.expired()) return {};
    const auto& windows = std::as_const(*m_state).mutable_model().windows;
    auto window = std::ranges::find(windows, window_id, &LiveWindow::window_id);
    return window != windows.end() ? *window : LiveWindow{};
}

LiveWindow
LiveWindowTabModel::live_window(WindowLiveId live_id) const
{
    if(m_parent.expired()) return {};
    const auto& windows = std::as_const(*m_state).mutable_model().windows;
    auto window = std::ranges::find(windows, live_id, &LiveWindow::id);
    return window != windows.end() ? *window : LiveWindow{};
}

LiveTab
LiveWindowTabModel::live_tab(TabId tab_id) const
{
    if(m_parent.expired()) return {};
    const auto& tabs = std::as_const(*m_state).mutable_model().tabs;
    auto tab = std::ranges::find(tabs, tab_id, &LiveTab::tab_id);
    return tab != tabs.end() ? *tab : LiveTab{};
}

LiveTab
LiveWindowTabModel::live_tab(TabLiveId live_id) const
{
    if(m_parent.expired()) return {};
    const auto& tabs = std::as_const(*m_state).mutable_model().tabs;
    auto tab = std::ranges::find(tabs, live_id, &LiveTab::id);
    return tab != tabs.end() ? *tab : LiveTab{};
}

void
LiveWindowTabModel::update_window(WindowLiveId live_id, UpdateWindow query)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& windows = m_state->mutable_model().windows;
    auto window = std::ranges::find(windows, live_id, &LiveWindow::id);
    if(window == windows.end()) return;
    if(query.is_tab_group && !*query.is_tab_group
        && window->private_mode == PrivateMode::yes) return;
    if(query.name) window->name = std::move(*query.name);
    if(query.icon_name) window->icon_name = std::move(*query.icon_name);
    if(query.selected_tab_id) window->selected_tab_id = *query.selected_tab_id;
    if(query.keyboard_shortcut) window->keyboard_shortcut = std::move(*query.keyboard_shortcut);
    if(query.is_tab_group) window->is_tab_group = *query.is_tab_group;
}

void
LiveWindowTabModel::suspend_window(WindowLiveId live_id)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& windows = m_state->mutable_model().windows;
    auto window = std::ranges::find(windows, live_id, &LiveWindow::id);
    if(window != windows.end()) window->is_suspended = true;
}

void
LiveWindowTabModel::delete_saved_window(WindowLiveId live_id)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& windows = m_state->mutable_model().windows;
    auto window = std::ranges::find(windows, live_id, &LiveWindow::id);
    if(window == windows.end() || window->is_tab_group) return;
    remove_window(window->id);
}

void
LiveWindowTabModel::remove_window(WindowLiveId live_id)
{
    if(m_state->is_locked() || m_parent.expired()) return;
    auto& windows = m_state->mutable_model().windows;
    auto window = std::ranges::find(windows, live_id, &LiveWindow::id);
    if(window == windows.end()) return;

    windows.erase(window);
    for(auto& main_window : m_state->mutable_model().main_windows) {
        if(main_window.window_live_id == live_id) main_window.window_live_id = {};
    }
    std::erase_if(m_state->mutable_model().tabs,
        [live_id](const LiveTab& tab) {
            return tab.window_live_id == live_id;
        });
    for(size_t index = 0; index < windows.size(); ++index) {
        windows[index].index = static_cast<int>(index);
    }
    schedule_commit();
}

TabLiveId
LiveWindowTabModel::create_tab(CreateTab query)
{
    auto world = m_parent.lock();
    if(!world) return {};
    schedule_commit();
    if(m_state->is_locked()) return {};
    auto& model = m_state->mutable_model();
    // Validate explicit parents before allocating any automatic parents.
    if(query.main_window_live_id.value
        && std::ranges::find(model.main_windows, query.main_window_live_id, &LiveMainWindow::id)
            == model.main_windows.end()) return {};
    if(query.window_live_id.value) {
        if(std::ranges::find(model.windows, query.window_live_id, &LiveWindow::id)
            == model.windows.end()) return {};
    } else {
        if(!query.auto_create_window && !query.auto_create_main_window) return {};
        query.window_live_id = create_window({});
    }
    if(!query.main_window_live_id.value && query.auto_create_main_window) {
        auto main = std::ranges::find(model.main_windows, query.window_live_id,
            &LiveMainWindow::window_live_id);
        query.main_window_live_id = main != model.main_windows.end()
            ? main->id : create_main_window({});
    }
    if(query.main_window_live_id.value) {
        activate_window(query.main_window_live_id, query.window_live_id);
    }

    const auto index = std::ranges::count_if(model.tabs,
        [&query](const LiveTab& tab) {
            return tab.window_live_id == query.window_live_id;
        });
    TabId tab_id = query.tab_id.value
        ? query.tab_id
        : world->core_api().allocate_tab_id();
    if(!tab_id.value) return {};
    auto id = m_state->next_id<TabLiveId>();
    model.tabs.push_back(LiveTab{
        .id = id,
        .window_live_id = query.window_live_id,
        .name = std::move(query.name),
        .icon_name = std::move(query.icon_name),
        .is_pinned = query.is_pinned,
        .is_suspended = query.is_suspended,
        .index = static_cast<int>(index),
        .uri = std::move(query.uri),
        .session_state = std::move(query.session_state),
        .parent_tab_id = query.parent_tab_id,
        .is_collapsed = query.is_collapsed,
        .back_forward_list_index = query.back_forward_list_index,
        .webkit_back_forward_list_index = query.webkit_back_forward_list_index,
        .merged_back_forward_list_length = query.merged_back_forward_list_length,
        .snapshot_image = std::move(query.snapshot_image),
        .tab_id = tab_id,
    });
    return id;
}

void
LiveWindowTabModel::update_tab(TabLiveId tab_live_id, UpdateTab query)
{
    if(m_parent.expired()) return;
    schedule_commit();
    if(m_state->is_locked()) return;
    auto& tabs = m_state->mutable_model().tabs;
    auto tab = std::ranges::find(tabs, tab_live_id, &LiveTab::id);
    if(tab == tabs.end()) return;

    if(query.name) tab->name = std::move(*query.name);
    if(query.icon_name) tab->icon_name = std::move(*query.icon_name);
    if(query.is_pinned) tab->is_pinned = *query.is_pinned;
    if(query.is_suspended) tab->is_suspended = *query.is_suspended;
    if(query.uri) tab->uri = std::move(*query.uri);
    if(query.session_state) tab->session_state = std::move(*query.session_state);
    if(query.parent_tab_id) tab->parent_tab_id = *query.parent_tab_id;
    if(query.is_collapsed) tab->is_collapsed = *query.is_collapsed;
    if(query.back_forward_list_index)
        tab->back_forward_list_index = *query.back_forward_list_index;
    if(query.webkit_back_forward_list_index)
        tab->webkit_back_forward_list_index = *query.webkit_back_forward_list_index;
    if(query.merged_back_forward_list_length)
        tab->merged_back_forward_list_length = *query.merged_back_forward_list_length;
    if(query.snapshot_image) tab->snapshot_image = std::move(*query.snapshot_image);
}

void
LiveWindowTabModel::remove_tab(TabLiveId tab_live_id)
{
    if(m_state->is_locked() || m_parent.expired()) return;
    auto& tabs = m_state->mutable_model().tabs;
    auto tab = std::ranges::find(tabs, tab_live_id, &LiveTab::id);
    if(tab == tabs.end()) return;

    auto window_live_id = tab->window_live_id;
    tabs.erase(tab);
    std::vector<LiveTab*> remaining;
    for(auto& tab : tabs) {
        if(tab.window_live_id == window_live_id) remaining.push_back(&tab);
    }
    std::ranges::sort(remaining, [](const LiveTab* left, const LiveTab* right) {
        return left->index < right->index;
    });
    for(size_t position = 0; position < remaining.size(); ++position) {
        remaining[position]->index = static_cast<int>(position);
    }
    schedule_commit();
}

void
LiveWindowTabModel::move_tabs(
    std::vector<TabLiveId> tab_live_ids,
    WindowLiveId window_live_id,
    int index)
{
    if(m_parent.expired()) return;
    if(m_state->is_locked()) {
        // A locked model cannot accept a move, but the request still gets a commit.
        schedule_commit();
        return;
    }
    if(tab_live_ids.empty()) {
        // An empty move still gets a commit for its completion.
        schedule_commit();
        return;
    }

    auto& model = m_state->mutable_model();
    std::vector<LiveTab*> moving;
    std::set<TabLiveId> selected_ids;
    for(TabLiveId tab_live_id : tab_live_ids) {
        if(!selected_ids.insert(tab_live_id).second) continue;
        auto tab = std::ranges::find(model.tabs, tab_live_id, &LiveTab::id);
        if(tab == model.tabs.end()) {
            // No tabs move if any requested tab is missing.
            schedule_commit();
            return;
        }
        moving.push_back(&*tab);
    }

    std::map<WindowLiveId, std::vector<LiveTab*>> moving_by_window;
    std::set<WindowLiveId> affected_window_ids;
    std::set<WindowLiveId> moved_out_source_ids;
    for(LiveTab* tab : moving) {
        // A zero destination keeps each tab in its own current window.
        WindowLiveId destination = window_live_id.value ? window_live_id : tab->window_live_id;
        if(std::ranges::find(model.windows, destination, &LiveWindow::id)
            == model.windows.end()) {
            // A rejected move still has a commit so its completion can finish.
            schedule_commit();
            return;
        }
        affected_window_ids.insert(tab->window_live_id);
        affected_window_ids.insert(destination);
        if(tab->window_live_id != destination) {
            moved_out_source_ids.insert(tab->window_live_id);
        }
        moving_by_window[destination].push_back(tab);
    }

    std::map<WindowLiveId, std::vector<LiveTab*>> ordered_by_window;
    for(WindowLiveId affected_window_id : affected_window_ids) {
        ordered_by_window[affected_window_id];
    }
    for(auto& tab : model.tabs) {
        if(selected_ids.contains(tab.id)) continue;
        if(affected_window_ids.contains(tab.window_live_id)) {
            ordered_by_window[tab.window_live_id].push_back(&tab);
        }
    }

    auto by_index = [](const LiveTab* left, const LiveTab* right) {
        return left->index < right->index;
    };
    for(auto& entry : ordered_by_window) {
        std::ranges::sort(entry.second, by_index);
    }

    // The insertion index is measured after selected tabs leave the destination order.
    for(auto& [destination, tabs] : moving_by_window) {
        auto& order = ordered_by_window[destination];
        const int insertion_index = index == -1
            ? static_cast<int>(order.size())
            : std::clamp(index, 0, static_cast<int>(order.size()));
        order.insert(order.begin() + insertion_index, tabs.begin(), tabs.end());
    }
    for(const auto& entry : ordered_by_window) {
        const auto& order = entry.second;
        bool has_unpinned_tab = false;
        for(const LiveTab* tab : order) {
            if(!tab->is_pinned) {
                has_unpinned_tab = true;
            } else if(has_unpinned_tab) {
                // A pinned tab cannot follow an unpinned tab; no tabs in the batch move.
                schedule_commit();
                return;
            }
        }
    }
    // A hosted tab keeps its host when moved into a previously unloaded Window.
    // Load that destination without creating a native MainWindow.
    for(const auto& [destination, tabs] : moving_by_window) {
        if(std::ranges::any_of(tabs, [&model](const LiveTab* tab) {
            auto source = std::ranges::find(model.windows, tab->window_live_id, &LiveWindow::id);
            return source != model.windows.end() && source->has_host;
        })) {
            std::ranges::find(model.windows, destination, &LiveWindow::id)->has_host = true;
        }
    }
    for(auto& [parent_window_live_id, order] : ordered_by_window) {
        for(size_t position = 0; position < order.size(); ++position) {
            order[position]->window_live_id = parent_window_live_id;
            order[position]->index = static_cast<int>(position);
        }
    }

    std::vector<WindowLiveId> empty_saved_windows;
    for(WindowLiveId source_live_id : moved_out_source_ids) {
        auto source = std::ranges::find(model.windows, source_live_id, &LiveWindow::id);
        if(source == model.windows.end()) continue;
        std::vector<LiveTab*> remaining;
        for(auto& tab : model.tabs) {
            if(tab.window_live_id == source_live_id) remaining.push_back(&tab);
        }
        std::ranges::sort(remaining, by_index);
        if(std::ranges::none_of(remaining, [source](const LiveTab* tab) {
            return tab->tab_id == source->selected_tab_id;
        })) {
            source->selected_tab_id = remaining.empty()
                ? TabId{} : remaining.front()->tab_id;
        }
        if(remaining.empty() && !source->is_tab_group) {
            empty_saved_windows.push_back(source_live_id);
        }
    }

    // Match legacy move behavior for saved windows by keeping them usable.
    // The UI drag path closes empty unsaved groups because it can also account
    // for shared pinned pages, which are intentionally outside this model.
    for(WindowLiveId saved_live_id : empty_saved_windows) {
        auto tab_live_id = create_tab({
            .window_live_id = saved_live_id,
            .uri = "orion://newtab/",
        });
        auto tab = live_tab(tab_live_id);
        auto saved = std::ranges::find(model.windows, saved_live_id, &LiveWindow::id);
        if(saved != model.windows.end() && tab.id.value) {
            saved->selected_tab_id = tab.tab_id;
        }
    }
    schedule_commit();
}

void
LiveWindowTabModel::schedule_commit() {
    if(auto world = m_parent.lock()) {
        if(auto bridge = world->profile_bridge().lock()) {
            bridge->schedule_commit(*world);
        }
    }
}

LiveWindowTabModelLock
LiveWindowTabModel::begin()
{
    return LiveWindowTabModelLock{*this};
}

void
LiveWindowTabModel::apply_changes(
    LiveWindowTabModelLock& lock, LiveWindowTabModelChanges& changes) const
{
    if(!lock.m_lock_acquired || lock.m_model != this) return;
    changes.begin();
    const auto& state = *m_state;
    const auto& committed_model = state.committed_model();
    const auto& committed_windows = committed_model.windows;
    const auto& mutable_model = state.mutable_model();
    for(const auto& window : mutable_model.windows) {
        auto previous = std::ranges::find(committed_windows, window.id, &LiveWindow::id);
        if(previous == committed_windows.end()
            || (window.has_host && !previous->has_host)) {
            state.trace("create window live={} db={} index={} has_host={} tab_group={} focus={} suspended={}",
                window.id.value, window.window_id.value, window.index, window.has_host,
                window.is_tab_group, window.focus_mode == FocusMode::yes, window.is_suspended);
            changes.create_window(window);
        } else if(window.is_tab_group != previous->is_tab_group
            || window.name != previous->name || window.icon_name != previous->icon_name
            || window.keyboard_shortcut != previous->keyboard_shortcut
            || window.is_suspended != previous->is_suspended
            || window.selected_tab_id != previous->selected_tab_id
            || window.index != previous->index || window.focus_mode != previous->focus_mode) {
            state.trace("update window live={} db={} index={} tab_group={} suspended={}",
                window.id.value, window.window_id.value, window.index, window.is_tab_group, window.is_suspended);
            changes.update_window(window);
        }
    }
    // Release presentations which leave a shell before another shell can
    // attach them. Ordinary in-place switches are a single active call.
    for(const auto& main : committed_model.main_windows) {
        auto next = std::ranges::find(mutable_model.main_windows, main.id, &LiveMainWindow::id);
        bool transferred = std::ranges::any_of(mutable_model.main_windows,
            [&main](const LiveMainWindow& other) {
                return other.id != main.id && other.window_live_id == main.window_live_id;
            });
        if(main.window_live_id.value && (next == mutable_model.main_windows.end()
            || !next->window_live_id.value || transferred)) {
            state.trace("activate main_window live={} window_live=0", main.id.value);
            changes.activate_window(main.id, {});
        }
    }
    for(const auto& main : mutable_model.main_windows) {
        auto previous = std::ranges::find(committed_model.main_windows, main.id, &LiveMainWindow::id);
        if(previous == committed_model.main_windows.end()) {
            state.trace("create main_window live={} window_live={}", main.id.value, main.window_live_id.value);
            changes.create_main_window(main);
        }
        if(main.window_live_id.value && (previous == committed_model.main_windows.end()
            || previous->window_live_id != main.window_live_id)) {
            state.trace("activate main_window live={} window_live={}", main.id.value, main.window_live_id.value);
            changes.activate_window(main.id, main.window_live_id);
        }
    }
    // New tab indices assume removed tabs have already left the host.
    for(const auto& tab : committed_model.tabs) {
        if(std::ranges::find(mutable_model.tabs, tab.id, &LiveTab::id)
            == mutable_model.tabs.end()) {
            state.trace("remove tab live={} db={}", tab.id.value, tab.tab_id.value);
            changes.remove_tab(tab.id);
        }
    }
    auto previously_hosted = [&committed_model, &committed_windows](TabLiveId id) {
        auto tab = std::ranges::find(committed_model.tabs, id, &LiveTab::id);
        if(tab == committed_model.tabs.end()) return false;
        auto window = std::ranges::find(committed_windows, tab->window_live_id, &LiveWindow::id);
        return window != committed_windows.end() && window->has_host;
    };
    for(const auto& tab : mutable_model.tabs) {
        auto previous = std::ranges::find(committed_model.tabs, tab.id, &LiveTab::id);
        bool newly_created = previous == committed_model.tabs.end();
        if(newly_created || !previously_hosted(tab.id)) {
            auto window = std::ranges::find(
                mutable_model.windows,
                tab.window_live_id,
                &LiveWindow::id);
            if(newly_created
                || (window != mutable_model.windows.end() && window->has_host)) {
                state.trace("create tab live={} db={} window_live={} index={} pinned={} suspended={}",
                    tab.id.value, tab.tab_id.value, tab.window_live_id.value, tab.index, tab.is_pinned, tab.is_suspended);
                changes.create_tab(tab);
            }
        } else if(previously_hosted(tab.id)
            && std::tie(tab.name, tab.icon_name, tab.is_pinned, tab.is_suspended,
                tab.uri, tab.session_state, tab.parent_tab_id, tab.is_collapsed,
                tab.back_forward_list_index, tab.webkit_back_forward_list_index,
                tab.merged_back_forward_list_length, tab.snapshot_image)
                != std::tie(previous->name, previous->icon_name, previous->is_pinned,
                    previous->is_suspended, previous->uri, previous->session_state,
                    previous->parent_tab_id, previous->is_collapsed,
                    previous->back_forward_list_index,
                    previous->webkit_back_forward_list_index,
                    previous->merged_back_forward_list_length, previous->snapshot_image)) {
            state.trace("update tab live={} db={} pinned={} suspended={}",
                tab.id.value, tab.tab_id.value, tab.is_pinned, tab.is_suspended);
            changes.update_tab(tab);
        }
    }
    std::map<WindowLiveId, std::vector<std::pair<int, TabLiveId>>> moves_by_window;
    for(const auto& tab : mutable_model.tabs) {
        auto previous = std::ranges::find(
            committed_model.tabs, tab.id, &LiveTab::id);
        if(previous == committed_model.tabs.end()
            || (previous->window_live_id == tab.window_live_id
                && previous->index == tab.index)) continue;
        moves_by_window[tab.window_live_id].emplace_back(tab.index, tab.id);
    }
    // Index shifts move with the selected tabs so visible order matches the model.
    auto dispatch_move = [&changes, &state](
        const std::vector<TabLiveId>& tab_live_ids,
        WindowLiveId parent_live_id,
        int index) {
        // Inserting at one fixed index requires reverse order.
        for(auto tab = tab_live_ids.rbegin(); tab != tab_live_ids.rend(); ++tab) {
            state.trace("move tab live={} window_live={} index={}", tab->value, parent_live_id.value, index);
            changes.move(*tab, parent_live_id, index);
        }
    };
    for(auto& [window_live_id, moves] : moves_by_window) {
        std::ranges::sort(moves);
        std::vector<TabLiveId> tab_live_ids;
        int insertion_index = 0;
        // Each group has adjacent final indices and one insertion index.
        for(const auto& [tab_index, tab_live_id] : moves) {
            if(!tab_live_ids.empty()
                && tab_index != insertion_index
                    + static_cast<int>(tab_live_ids.size())) {
                dispatch_move(tab_live_ids, window_live_id, insertion_index);
                tab_live_ids.clear();
            }
            if(tab_live_ids.empty()) insertion_index = tab_index;
            tab_live_ids.push_back(tab_live_id);
        }
        if(!tab_live_ids.empty()) {
            dispatch_move(tab_live_ids, window_live_id, insertion_index);
        }
    }
    for(const auto& window : committed_windows) {
        auto next = std::ranges::find(mutable_model.windows, window.id, &LiveWindow::id);
        if(next == mutable_model.windows.end() || (window.has_host && !next->has_host)) {
            state.trace("remove window live={} db={}", window.id.value, window.window_id.value);
            changes.remove_window(window.id);
        }
    }
    for(const auto& main : committed_model.main_windows) {
        if(std::ranges::find(mutable_model.main_windows, main.id, &LiveMainWindow::id)
            == mutable_model.main_windows.end()) {
            state.trace("remove main_window live={}", main.id.value);
            changes.remove_main_window(main.id);
        }
    }
    changes.commit();
}

std::optional<LiveWindowTabModelSnapshot>
LiveWindowTabModel::snapshot(const LiveWindowTabModelLock& lock) const
{
    if(!lock.m_lock_acquired || lock.m_model != this) return std::nullopt;
    const auto& model = m_state->mutable_model();
    return LiveWindowTabModelSnapshot{
        .main_windows = model.main_windows,
        .windows = model.windows,
        .tabs = model.tabs,
    };
}

void
LiveWindowTabModel::commit(LiveWindowTabModelLock& lock)
{
    if(!lock.m_lock_acquired || lock.m_model != this) return;
    m_state->commit();
    m_state->unlock();
    lock.m_lock_acquired = false;
    lock.m_model = nullptr;
}

}
