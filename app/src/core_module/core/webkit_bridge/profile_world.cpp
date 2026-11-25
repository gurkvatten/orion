#include <core/webkit_bridge/API/bridge.h>

#include <core/webextensions/runtime.h>

#include <algorithm>
#include <cassert>
#include <utility>

namespace orion::bridge {

ProfileWorld::ProfileWorld(
    ProfileBridge& parent, CoreApi& core_api,
    ProfileId profile_id, PrivateMode private_mode)
    : m_profile_bridge(parent.weak_from_this())
    , m_core_api(&core_api)
    , m_profile_id(profile_id)
    , m_private_mode(private_mode)
    , m_main_window_operations(*this)
    , m_window_operations(*this)
    , m_tab_operations(*this)
    , m_context_menu_bridge(
        std::shared_ptr<ContextMenuBridge>(new ContextMenuBridge()))
    , m_clipboard_bridge(
        std::shared_ptr<ClipboardBridge>(new ClipboardBridge()))
{
}

ProfileWorld::~ProfileWorld()
{
    m_is_detaching = true;
    m_profile_bridge.reset();
    complete_all_requests();
    clear_host_slots();
    cleanup();
}

void
ProfileWorld::detach()
{
    m_is_detaching = true;
    m_profile_bridge.reset();
    complete_all_requests();
    clear_host_slots();
    cleanup();
}

void
ProfileWorld::clear_host_slots()
{
    for(auto& entry : m_main_windows) {
        if(!entry.second) continue;
        *entry.second = nullptr;
    }
    for(auto& entry : m_windows) {
        if(!entry.second) continue;
        *entry.second = nullptr;
    }
    for(auto& entry : m_tabs) {
        if(!entry.second) continue;
        *entry.second = nullptr;
    }
    m_main_windows.clear();
    m_windows.clear();
    m_tabs.clear();
}

WebExtensions::WebExtensionsRuntime*
ProfileWorld::runtime() const
{
    return m_runtime.get();
}

void
ProfileWorld::initialize()
{
    if(m_runtime) return;
    auto profile_bridge = m_profile_bridge.lock();
    assert(profile_bridge && "Cannot initialize a detached ProfileWorld");
    assert(m_web_view_factory_bridge->m_host && "Attach a factory host before activation");
    if(!profile_bridge || !m_web_view_factory_bridge->m_host) return;

    m_runtime = WebExtensions::WebExtensionsRuntime::create(*this);
    // Prepare native state and subscribe before runtime loading can emit updates.
    m_web_view_factory_bridge->initialize();
    if(m_runtime) m_runtime->initialize();
}

void
ProfileWorld::present()
{
    model_repository().present();
}

void
ProfileWorld::quit(std::function<void()> completion)
{
    if(!add_request_completion(std::move(completion))) return;
    live_model().quit(model_repository().preserve_windows_on_quit());
}

void
ProfileWorld::cleanup()
{
    if(m_web_view_factory_bridge) m_web_view_factory_bridge->cleanup();
    m_web_views.clear();
    m_runtime.reset();
}

LiveWindowTabModel&
ProfileWorld::live_model()
{
    if(!m_live_model) {
        m_live_model = std::make_unique<LiveWindowTabModel>(*this);
    }
    return *m_live_model;
}

WindowTabModelRepository&
ProfileWorld::model_repository()
{
    if(!m_model_repository) {
        m_model_repository = std::make_unique<WindowTabModelRepository>(*this);
    }
    return *m_model_repository;
}

std::weak_ptr<WebViewBridge>
ProfileWorld::create_web_view_bridge()
{
    auto bridge = std::shared_ptr<WebViewBridge>(new WebViewBridge(*this));
    std::weak_ptr<WebViewBridge> result = bridge;
    m_web_views.push_back(std::move(bridge));
    return result;
}

void
ProfileWorld::remove_web_view_bridge(WebViewBridge& bridge)
{
    m_web_views.erase(
        std::remove_if(
            m_web_views.begin(), m_web_views.end(),
            [&bridge](const auto& candidate) {
                return candidate.get() == &bridge;
            }),
        m_web_views.end());
}

HostAttachment
ProfileWorld::attach(MainWindowLiveId live_id, MainWindowHost& host)
{
    auto& slot = m_main_windows[live_id];
    if(!slot) slot = std::make_shared<MainWindowHost*>(nullptr);
    assert(!*slot || *slot == &host);
    *slot = &host;
    return HostAttachment([world = weak_from_this(), live_id, host = &host,
                              expected_slot = std::weak_ptr<MainWindowHost*>(slot)] {
        if(auto shared_world = world.lock()) {
            shared_world->detach(live_id, *host, expected_slot);
        }
    });
}

void
ProfileWorld::detach(MainWindowLiveId live_id, MainWindowHost& host,
    std::weak_ptr<MainWindowHost*> expected_slot)
{
    auto slot = expected_slot.lock();
    const bool is_attached = slot && *slot == &host;
    if(is_attached) *slot = nullptr;
    auto it = m_main_windows.find(live_id);
    if(is_attached && it != m_main_windows.end() && it->second == slot) {
        m_main_windows.erase(it);
    }
}

std::vector<MainWindowLiveId>
ProfileWorld::MainWindows::all() const
{
    return m_world.live_model().all_main_windows();
}

LiveMainWindow
ProfileWorld::MainWindows::to_live_main_window(MainWindowLiveId live_id) const
{
    return m_world.live_model().live_main_window(live_id);
}

void
ProfileWorld::MainWindows::create_main_window(
    CreateMainWindow query, MainWindowLiveId* out_live_id, std::function<void()> reply)
{
    if(out_live_id) *out_live_id = {};
    if(m_world.m_is_detaching) {
        if(reply) reply();
        return;
    }
    auto id = m_world.live_model().create_main_window(query);
    if(out_live_id) *out_live_id = id;
    m_world.add_request_completion(std::move(reply));
}

void
ProfileWorld::MainWindows::activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id,
    std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().activate_window(live_id, window_live_id);
}

void
ProfileWorld::MainWindows::remove(std::vector<MainWindowLiveId> live_ids,
    std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    for(auto id : live_ids) m_world.live_model().remove_main_window(id);
}

bridge::HostAttachment
ProfileWorld::attach(WindowLiveId live_id, WindowHost& host)
{
    auto& slot = m_windows[live_id];
    if(!slot) slot = std::make_shared<WindowHost*>(nullptr);
    assert(!*slot || *slot == &host);
    *slot = &host;
    return HostAttachment([world = weak_from_this(), live_id, host = &host,
                              expected_slot = std::weak_ptr<WindowHost*>(slot)] {
        if(auto shared_world = world.lock()) {
            shared_world->detach(live_id, *host, expected_slot);
        }
    });
}

void
ProfileWorld::detach(WindowLiveId live_id, WindowHost& host,
    std::weak_ptr<WindowHost*> expected_slot)
{
    auto slot = expected_slot.lock();
    const bool is_attached = slot && *slot == &host;
    if(is_attached) *slot = nullptr;
    auto it = m_windows.find(live_id);
    if(is_attached && it != m_windows.end() && it->second == slot) {
        m_windows.erase(it);
    }
}

bridge::HostAttachment
ProfileWorld::attach(TabLiveId live_id, TabHost& host)
{
    auto& slot = m_tabs[live_id];
    if(!slot) slot = std::make_shared<TabHost*>(nullptr);
    assert(!*slot || *slot == &host);
    *slot = &host;
    return HostAttachment([world = weak_from_this(), live_id, host = &host,
                              expected_slot = std::weak_ptr<TabHost*>(slot)] {
        if(auto shared_world = world.lock()) {
            shared_world->detach(live_id, *host, expected_slot);
        }
    });
}

void
ProfileWorld::detach(TabLiveId live_id, TabHost& host,
    std::weak_ptr<TabHost*> expected_slot)
{
    auto slot = expected_slot.lock();
    const bool is_attached = slot && *slot == &host;
    if(is_attached) *slot = nullptr;
    auto it = m_tabs.find(live_id);
    if(is_attached && it != m_tabs.end() && it->second == slot) {
        m_tabs.erase(it);
    }
}

std::vector<WindowLiveId>
ProfileWorld::Windows::all() const
{
    return m_world.live_model().all_windows();
}

void
ProfileWorld::Windows::create_window(
    CreateWindow query, WindowLiveId* out_live_id, std::function<void()> reply)
{
    if(out_live_id) *out_live_id = {};
    if(m_world.m_is_detaching) {
        if(reply) reply();
        return;
    }
    auto id = m_world.live_model().create_window(std::move(query));
    if(out_live_id) *out_live_id = id;
    m_world.add_request_completion(std::move(reply));
}

void
ProfileWorld::Windows::duplicate_window(
    WindowLiveId live_id, WindowLiveId* out_live_id, std::function<void()> reply)
{
    if(out_live_id) *out_live_id = {};
    if(m_world.m_is_detaching) {
        if(reply) reply();
        return;
    }
    auto id = m_world.live_model().duplicate_window(live_id);
    if(out_live_id) *out_live_id = id;
    m_world.add_request_completion(std::move(reply));
}

void
ProfileWorld::Windows::update(
    WindowLiveId live_id, UpdateWindow query, std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().update_window(live_id, std::move(query));
}

LiveWindow
ProfileWorld::Windows::to_live_window(WindowId window_id) const
{
    return m_world.live_model().live_window(window_id);
}

LiveWindow
ProfileWorld::Windows::to_live_window(WindowLiveId live_id) const
{
    return m_world.live_model().live_window(live_id);
}

std::vector<LiveWindow>
ProfileWorld::Windows::to_live_windows(const std::vector<WindowId>& window_ids) const
{
    std::vector<LiveWindow> windows;
    windows.reserve(window_ids.size());
    for(auto window_id : window_ids) windows.push_back(to_live_window(window_id));
    return windows;
}

std::vector<LiveWindow>
ProfileWorld::Windows::to_live_windows(const std::vector<WindowLiveId>& live_ids) const
{
    std::vector<LiveWindow> windows;
    windows.reserve(live_ids.size());
    for(auto live_id : live_ids) windows.push_back(to_live_window(live_id));
    return windows;
}

void
ProfileWorld::Windows::suspend(WindowLiveId live_id, std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().suspend_window(live_id);
}

void
ProfileWorld::Windows::delete_saved(WindowLiveId live_id,
    std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().delete_saved_window(live_id);
}

void
ProfileWorld::Windows::remove(
    std::vector<WindowLiveId> live_ids, std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    for(WindowLiveId live_id : live_ids) {
        m_world.live_model().remove_window(live_id);
    }
}

std::vector<TabLiveId>
ProfileWorld::Tabs::all() const
{
    return m_world.live_model().all_tabs();
}

LiveTab
ProfileWorld::Tabs::to_live_tab(TabId tab_id) const
{
    return m_world.live_model().live_tab(tab_id);
}

LiveTab
ProfileWorld::Tabs::to_live_tab(TabLiveId live_id) const
{
    return m_world.live_model().live_tab(live_id);
}

void
ProfileWorld::Tabs::create_tab(
    CreateTab query, TabLiveId* out_live_id, std::function<void()> reply)
{
    if(out_live_id) *out_live_id = {};
    if(m_world.m_is_detaching) {
        if(reply) reply();
        return;
    }
    auto id = m_world.live_model().create_tab(std::move(query));
    if(out_live_id) *out_live_id = id;
    m_world.add_request_completion(std::move(reply));
}

void
ProfileWorld::Tabs::update(
    TabLiveId tab_live_id, UpdateTab query, std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().update_tab(tab_live_id, std::move(query));
}

void
ProfileWorld::Tabs::remove(
    std::vector<TabLiveId> live_ids, std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    for(TabLiveId live_id : live_ids) {
        m_world.live_model().remove_tab(live_id);
    }
}

void
ProfileWorld::Tabs::move(
    std::vector<TabLiveId> tab_live_ids,
    WindowLiveId window_live_id,
    int index,
    std::function<void()> reply)
{
    if(!m_world.add_request_completion(std::move(reply))) return;
    m_world.live_model().move_tabs(
        std::move(tab_live_ids), window_live_id, index);
}

bool
ProfileWorld::add_request_completion(std::function<void()> reply)
{
    // Teardown cannot have a next commit. Complete reentrant requests now
    // without letting them mutate the retiring model.
    if(m_is_detaching) {
        if(reply) reply();
        return false;
    }
    if(reply) m_request_completions.push_back(std::move(reply));
    return true;
}

void
ProfileWorld::complete_all_requests()
{
    auto pending = std::move(m_request_completions);
    m_request_completions.clear();
    for(auto& completion : pending) completion();
}

void
ProfileWorld::commit(ProfileHost& host)
{
    if(m_is_detaching) return;
    auto& repository = model_repository();
    if(!repository.can_checkpoint()) return;

    class ProfileWorldMutationAdapter : public LiveWindowTabModel::LiveWindowTabModelChanges {
    public:
        ProfileWorldMutationAdapter(ProfileHost& host, ProfileWorld& world)
            : m_host(host), m_world(world) {}

        void begin() override {}

        void create_main_window(LiveMainWindow window) override {
            m_host.create_main_window(window);
        }

        void activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id) override {
            auto it = m_world.m_main_windows.find(live_id);
            if(it != m_world.m_main_windows.end() && it->second && *it->second) {
                (*it->second)->activate_window(window_live_id);
            }
        }

        void remove_main_window(MainWindowLiveId live_id) override {
            auto it = m_world.m_main_windows.find(live_id);
            if(it != m_world.m_main_windows.end() && it->second && *it->second) {
                (*it->second)->remove();
            }
        }

        void create_window(LiveWindow window) override {
            if(!window.has_host) return;
            m_host.create_window(std::move(window));
        }

        void update_window(LiveWindow window) override {
            auto it = m_world.m_windows.find(window.id);
            if(it != m_world.m_windows.end() && it->second && *it->second) {
                (*it->second)->update(std::move(window));
            }
        }

        void remove_window(WindowLiveId live_id) override {
            auto it = m_world.m_windows.find(live_id);
            if(it != m_world.m_windows.end() && it->second && *it->second) {
                (*it->second)->remove();
            }
        }

        void create_tab(LiveTab tab) override {
            auto it = m_world.m_windows.find(tab.window_live_id);
            if(it != m_world.m_windows.end() && it->second && *it->second) {
                (*it->second)->create_tab(std::move(tab));
            }
        }

        void update_tab(LiveTab tab) override {
            auto it = m_world.m_tabs.find(tab.id);
            if(it != m_world.m_tabs.end() && it->second && *it->second) {
                (*it->second)->update(std::move(tab));
            }
        }

        void remove_tab(TabLiveId live_id) override {
            auto it = m_world.m_tabs.find(live_id);
            if(it != m_world.m_tabs.end() && it->second && *it->second) {
                (*it->second)->remove();
            }
        }

        void move(
            TabLiveId tab_live_id,
            WindowLiveId parent_live_id,
            int index) override {
            auto it = m_world.m_tabs.find(tab_live_id);
            if(it != m_world.m_tabs.end() && it->second && *it->second) {
                (*it->second)->move(parent_live_id, index);
            }
        }

        void commit() override {}

    private:
        ProfileHost& m_host;
        ProfileWorld& m_world;
    };

    auto completions = std::move(m_request_completions);
    m_request_completions.clear();

    ProfileWorldMutationAdapter adapter{host, *this};
    {
        auto model_mutation_lock = live_model().begin();
        if(!model_mutation_lock) {
            // An uncommitted request must keep its reply until a later successful
            // commit or unconditional teardown, including its original queue order.
            auto pending = std::move(m_request_completions);
            m_request_completions = std::move(completions);
            for(auto& completion : pending) {
                m_request_completions.push_back(std::move(completion));
            }
            return;
        }

        auto snapshot = live_model().snapshot(model_mutation_lock);
        if(snapshot) repository.checkpoint(*snapshot);
        live_model().apply_changes(model_mutation_lock, adapter);
        ApplyLiveWindowTabModelChanges apply_changes =
            [this, &model_mutation_lock](LiveWindowTabModelChanges& changes) {
                live_model().apply_changes(model_mutation_lock, changes);
            };
        m_changes(apply_changes);
        live_model().commit(model_mutation_lock);
    }

    for(auto& completion : completions) completion();
}

} // namespace orion::bridge
