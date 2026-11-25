#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>
#include <vector>

#include <common/src/coroutine.h>
#include <core/dto/extension_dto.h>
#include <core/dto/profile_dto.h>
#include <core/dto/window_dto.h>
#include <core/dto/settings_dto.h>
#include <core/dto/tab_dto.h>
#include <core/dto/window_tab_checkpoint.h>
#include <core/focus_mode.h>
#include <core/private_mode.h>
#include <core/webextensions/multicast_delegate.h>

namespace WebExtensions {
class WebExtension;
class WebExtensionsRuntime;
class WebExtensionsRuntimeImpl;
class ContextMenusV2Impl;
class RuntimeImpl;
enum class WebExtensionDocumentKind;
struct WebExtensionBackgroundPagesChange;
struct WebExtensionProxyRequest;
struct WebExtensionResourceRequest;
struct WebExtensionSchemeRegistration;
struct WebExtensionSchemeRequest;
struct WebExtensionSchemeResponse;

using WebExtensionContextMenuId = std::variant<int64_t, std::string>;

struct WebExtensionContextMenuScope {
    std::string extension_id;
    std::string extension_name;
};

struct WebExtensionContextMenuItem {
    WebExtensionContextMenuId id;
    std::optional<WebExtensionContextMenuId> parent_id;
    std::string type = "normal";
    std::string title;
    bool checked = false;
    std::vector<std::string> contexts {"page"};
    bool visible = true;
    bool enabled = true;
    std::vector<std::string> document_url_patterns;
    std::vector<std::string> target_url_patterns;
    std::map<std::string, std::string> icons;
    std::vector<std::string> view_types;
    std::optional<std::string> command;
};

// Context supplied by the application when an extension menu item is
// activated. Core adds the authoritative item and checked-state fields before
// delivering the WebExtension event.
struct WebExtensionContextMenuClick {
    WebExtensionContextMenuId menu_item_id;
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
    std::vector<std::string> modifiers;
    std::optional<int64_t> button;
    std::optional<int64_t> target_element_id;
};

struct WebExtensionContextMenuShown {
    std::vector<WebExtensionContextMenuId> menu_item_ids;
    std::vector<std::string> contexts;
    std::optional<int64_t> tab_id;
    std::optional<std::string> view_type;
    std::optional<std::string> media_type;
    std::optional<std::string> link_text;
    std::optional<std::string> link_url;
    std::optional<std::string> src_url;
    std::optional<std::string> page_url;
    std::optional<std::string> frame_url;
    std::optional<std::string> selection_text;
    bool editable = false;
    std::optional<int64_t> target_element_id;
};

struct WebExtensionContextMenuCallbacks {
    std::function<void(WebExtensionContextMenuClick click)> clicked;
    std::function<void(WebExtensionContextMenuShown shown)> shown;
    std::function<void()> hidden;
};

struct WebExtensionContextMenuOverride {
    std::string extension_id;
    bool show_defaults = false;
    std::optional<std::string> context;
    std::optional<std::string> bookmark_id;
    std::optional<int64_t> tab_id;
};

// Platform-decoded context for a native context-menu invocation. Keeping this
// independent of WebKit/GTK lets host implementations share the same
// presentation contract.
struct WebExtensionContextMenuHitTest {
    std::vector<std::string> contexts;
    std::optional<std::string> media_type;
    std::optional<std::string> link_text;
    std::optional<std::string> link_url;
    std::optional<std::string> src_url;
    std::optional<std::string> page_url;
    std::optional<std::string> frame_url;
    std::optional<int64_t> frame_id;
    std::optional<std::string> selection_text;
    bool editable = false;
    std::optional<int64_t> target_element_id;
    std::vector<std::string> modifiers;
    std::optional<int64_t> button;
};

using WebExtensionSchemeResponseCallback = std::function<void(WebExtensionSchemeResponse response)>;
using WebExtensionSchemeRequestHandler = std::function<void(
    const WebExtensionSchemeRequest& request,
    WebExtensionSchemeResponseCallback completion
)>;
}

namespace orion {

class CoreApi;
class Uri;

namespace bridge {
class ProfileBridge;
class ProfileHost;
class ProfileWorld;
struct CreateTab;
struct LiveTab;
struct UpdateTab;
struct UpdateWindow;
struct LiveWindow;
class MainWindowHost;
class WindowHost;
class TabHost;
class WebViewFactoryBridge;
class WebViewFactoryHost;
class WebViewBridge;
class WebViewHost;
class ContextMenuBridge;
class ContextMenuHost;
class ClipboardBridge;
class ClipboardHost;
class LiveWindowTabModel;
class WindowTabModelRepository;
}

enum class WebViewScriptInjectionTime {
    document_start,
    document_end,
};

enum class WebViewScriptFrames {
    top_frame,
    all_frames,
};

namespace bridge {

// IDs share one sequence within a ProfileWorld; they must be used with that world.
DECLARE_STRONG_TYPEDEF(MainWindowLiveId, int);
DECLARE_STRONG_TYPEDEF(WindowLiveId, int);
DECLARE_STRONG_TYPEDEF(TabLiveId, int);

class HostAttachment final {
public:
    HostAttachment() = default;
    ~HostAttachment();

    HostAttachment(const HostAttachment& other) = delete;
    HostAttachment& operator=(const HostAttachment& other) = delete;
    HostAttachment(HostAttachment&& other) noexcept;
    HostAttachment& operator=(HostAttachment&& other) noexcept;
    HostAttachment& operator=(std::nullptr_t null);

    void reset();

private:
    explicit HostAttachment(std::function<void()> detach);

    std::function<void()> m_detach;
    friend class ProfileWorld;
    friend class ProfileBridge;
    friend class WebViewFactoryBridge;
    friend class WebViewBridge;
    friend class ContextMenuBridge;
    friend class ClipboardBridge;
};

// The native shell displays one logical window at a time. Switching does not
// destroy the previous window or any of its tabs. An empty ID clears the shell.
class MainWindowHost {
public:
    virtual ~MainWindowHost() = default;
    MainWindowHost(const MainWindowHost&) = delete;
    MainWindowHost& operator=(const MainWindowHost&) = delete;

    virtual void activate_window(WindowLiveId window_live_id) = 0;
    virtual void remove() = 0;

protected:
    MainWindowHost() = default;
};

// Application-owned anchors for logical window and tab lifetimes.
class WindowHost {
public:
    virtual ~WindowHost() = default;

    WindowHost(const WindowHost& other) = delete;
    WindowHost& operator=(const WindowHost& other) = delete;

    virtual void create_tab(LiveTab tab) = 0;
    virtual void update(LiveWindow window) = 0;
    virtual void remove() = 0;

protected:
    WindowHost() = default;
};

class TabHost {
public:
    virtual ~TabHost() = default;

    TabHost(const TabHost& other) = delete;
    TabHost& operator=(const TabHost& other) = delete;

    virtual void update(LiveTab tab) = 0;
    virtual void move(WindowLiveId parent_id, int index) = 0;
    virtual void remove() = 0;

protected:
    TabHost() = default;
};

// Application-owned native factory contract. Attachment does not start work.
class WebViewFactoryHost {
public:
    virtual ~WebViewFactoryHost() = default;
    WebViewFactoryHost(const WebViewFactoryHost& other) = delete;
    WebViewFactoryHost& operator=(const WebViewFactoryHost& other) = delete;

protected:
    WebViewFactoryHost() = default;

private:
    virtual void initialize_platform() = 0;
    virtual void register_scheme(
        const WebExtensions::WebExtensionSchemeRegistration& registration,
        WebExtensions::WebExtensionSchemeRequestHandler handler) = 0;
    virtual void update_background_pages(
        const WebExtensions::WebExtensionBackgroundPagesChange& change) = 0;
    friend class WebViewFactoryBridge;
};

// Core-owned per-world factory logic. Native resources remain in the host.
class WebViewFactoryBridge final
    : public std::enable_shared_from_this<WebViewFactoryBridge> {
public:
    ~WebViewFactoryBridge();
    WebViewFactoryBridge(const WebViewFactoryBridge& other) = delete;
    WebViewFactoryBridge& operator=(const WebViewFactoryBridge& other) = delete;

    [[nodiscard]] HostAttachment attach(WebViewFactoryHost& host);
    std::weak_ptr<ProfileWorld> parent_world() const { return m_parent; }
    bool is_ready() const;
    void when_ready(std::function<void()> reply);
    ASYNC_TASK_REPLY_WRAPPER(WebViewFactoryBridge, when_ready);
    void register_schemes() const;
    std::weak_ptr<WebViewBridge> create_web_view_bridge();
    void configure_web_view(WebViewBridge& web_view);
    std::optional<std::string> document_uri(
        ExtensionId extension_id,
        WebExtensions::WebExtensionDocumentKind kind) const;

private:
    explicit WebViewFactoryBridge(ProfileWorld& parent);
    void initialize();
    void cleanup();
    void detach();
    struct State;

    void update_background_pages();
    std::shared_ptr<WebExtensions::WebExtension> extension_for_uri(
        const Uri& uri) const;
    WebExtensions::WebExtensionSchemeResponse extension_resource_for_request(
        const WebExtensions::WebExtensionResourceRequest& request) const;
    void extension_proxy_for_request(
        const WebExtensions::WebExtensionProxyRequest& request,
        WebExtensions::WebExtensionSchemeResponseCallback completion) const;

    std::weak_ptr<ProfileWorld> m_parent;
    WebViewFactoryHost* m_host = nullptr;
    // A fresh state per attachment prevents old callbacks reaching a new host.
    std::shared_ptr<State> m_state;
    friend class ProfileWorld;
    friend class ProfileBridge;
};

struct LiveMainWindow {
    MainWindowLiveId id{};
    WindowLiveId window_live_id{};
    PrivateMode private_mode = PrivateMode::no;
};

struct CreateMainWindow {
    WindowLiveId window_live_id{};
};

struct LiveWindow {
    WindowLiveId id{};
    PrivateMode private_mode = PrivateMode::no;
    FocusMode focus_mode = FocusMode::no;
    bool is_tab_group = true;
    // The logical Window host/view is loaded, including when it is inactive.
    bool has_host = false;
    std::string name{};
    std::string icon_name{};
    int index = 0;
    WindowId window_id{};
    TabId selected_tab_id{};
    std::string keyboard_shortcut{};
    bool is_suspended = false;
};

// New windows inherit the profile and private mode of their ProfileWorld.
struct CreateWindow {
    FocusMode focus_mode = FocusMode::no;
    bool is_tab_group = true;
    // Defer the logical Window host/view until activation. The default creates
    // the host/view immediately at commit, without creating a MainWindow.
    bool dont_create_host = false;
    std::string name{};
    std::string icon_name{};
    WindowId window_id{};
    TabId selected_tab_id{};
    // Load into this existing shell; its previous logical window stays alive.
    MainWindowLiveId main_window_live_id{};
    // Create and activate a new MainWindow when no main_window_live_id is supplied.
    // Activation creates the host even if dont_create_host is set.
    // An explicit ID always takes precedence.
    bool auto_create_main_window = false;
    std::string keyboard_shortcut{};
    bool is_suspended = false;
};

// Update one window in the current live model. Omitted fields remain unchanged.
struct UpdateWindow {
    std::optional<std::string> name{};
    std::optional<std::string> icon_name{};
    std::optional<TabId> selected_tab_id{};
    std::optional<std::string> keyboard_shortcut{};
    std::optional<bool> is_tab_group{};
};

struct LiveTab {
    TabLiveId id{};
    WindowLiveId window_live_id{};
    std::string name{};
    std::string icon_name{};
    bool is_pinned = false;
    bool is_suspended = false;
    int index = 0;
    std::string uri{};
    std::vector<uint8_t> session_state{};
    TabId parent_tab_id{};
    bool is_collapsed = false;
    int64_t back_forward_list_index = 0;
    int64_t webkit_back_forward_list_index = 0;
    int64_t merged_back_forward_list_length = 0;
    std::vector<uint8_t> snapshot_image{};
    TabId tab_id{};
};

struct CreateTab {
    WindowLiveId window_live_id{};
    std::string name{};
    std::string icon_name{};
    bool is_pinned = false;
    bool is_suspended = false;
    std::string uri{};
    std::vector<uint8_t> session_state{};
    TabId parent_tab_id{};
    bool is_collapsed = false;
    int64_t back_forward_list_index = 0;
    int64_t webkit_back_forward_list_index = 0;
    int64_t merged_back_forward_list_length = 0;
    std::vector<uint8_t> snapshot_image{};
    TabId tab_id{};
    // Optionally activate the target Window in an existing MainWindow.
    MainWindowLiveId main_window_live_id{};
    // If window_live_id is empty, create a Window and its host/view.
    bool auto_create_window = false;
    // Implies auto_create_window. Reuse the target Window's current MainWindow,
    // or create one if absent. An explicit main_window_live_id takes precedence.
    // Nonempty invalid parent IDs reject the request without creating anything.
    bool auto_create_main_window = false;
};

// Update one tab in the current live model. Omitted fields remain unchanged.
// Parent and index changes go through Tabs::move().
struct UpdateTab {
    std::optional<std::string> name{};
    std::optional<std::string> icon_name{};
    std::optional<bool> is_pinned{};
    std::optional<bool> is_suspended{};
    std::optional<std::string> uri{};
    std::optional<std::vector<uint8_t>> session_state{};
    std::optional<TabId> parent_tab_id{};
    std::optional<bool> is_collapsed{};
    std::optional<int64_t> back_forward_list_index{};
    std::optional<int64_t> webkit_back_forward_list_index{};
    std::optional<int64_t> merged_back_forward_list_length{};
    std::optional<std::vector<uint8_t>> snapshot_image{};
};

class LiveWindowTabModelChanges {
public:
    virtual ~LiveWindowTabModelChanges() {};

    virtual void begin() = 0;
    virtual void create_main_window(LiveMainWindow window) = 0;
    virtual void activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id) = 0;
    virtual void remove_main_window(MainWindowLiveId live_id) = 0;
    virtual void create_window(LiveWindow window) = 0;
    virtual void update_window(LiveWindow window) = 0;
    virtual void remove_window(WindowLiveId live_id) = 0;
    virtual void create_tab(LiveTab tab) = 0;
    virtual void update_tab(LiveTab tab) {}
    virtual void remove_tab(TabLiveId tab_live_id) = 0;
    virtual void move(
        TabLiveId tab_live_id,
        WindowLiveId parent_live_id,
        int index) = 0;
    virtual void commit() = 0;
};

using ApplyLiveWindowTabModelChanges =
    std::function<void(LiveWindowTabModelChanges& changes)>;

class ProfileWorld : public std::enable_shared_from_this<ProfileWorld> {
public:
    // Create requests fill out_live_id synchronously (empty on rejection). The
    // optional completion runs after commit; it never retains the output pointer.
    class MainWindows {
    public:
        std::vector<MainWindowLiveId> all() const;
        LiveMainWindow to_live_main_window(MainWindowLiveId live_id) const;
        void create_main_window(
            CreateMainWindow query, MainWindowLiveId* out_live_id = nullptr,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(MainWindows, create_main_window);
        void activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(MainWindows, activate_window);
        void remove(std::vector<MainWindowLiveId> live_ids,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(MainWindows, remove);

    private:
        explicit MainWindows(ProfileWorld& world) : m_world(world) {}
        ProfileWorld& m_world;
        friend class ProfileWorld;
    };

    class Windows {
    public:
        std::vector<WindowLiveId> all() const;
        // Copy from this world's current model, including windows without a host.
        // Missing IDs return a default LiveWindow with an empty .id. Bulk lookup
        // preserves input order and duplicates, including missing entries.
        LiveWindow to_live_window(WindowId window_id) const;
        LiveWindow to_live_window(WindowLiveId live_id) const;
        std::vector<LiveWindow> to_live_windows(const std::vector<WindowId>& window_ids) const;
        std::vector<LiveWindow> to_live_windows(const std::vector<WindowLiveId>& live_ids) const;
        // Creates a logical window. It becomes visible only when assigned to
        // a main window (supplied explicitly or requested by the auto-create flag).
        void create_window(
            CreateWindow query, WindowLiveId* out_live_id = nullptr,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, create_window);
        // Clone an existing logical window's unpinned tabs into a new unsaved
        // tab group and present it in a newly-created MainWindow.
        void duplicate_window(
            WindowLiveId live_id, WindowLiveId* out_live_id = nullptr,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, duplicate_window);
        void update(WindowLiveId live_id, UpdateWindow query,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, update);
        // Activation clears suspension again; both transitions are checkpointed.
        void suspend(WindowLiveId live_id, std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, suspend);
        // A live ID from this world; ignores missing IDs and never deletes
        // an unsaved tab group.
        void delete_saved(WindowLiveId live_id,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, delete_saved);
        void remove(
            std::vector<WindowLiveId> live_ids, std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Windows, remove);

    private:
        explicit Windows(ProfileWorld& world) : m_world(world) {}
        ProfileWorld& m_world;
        friend class ProfileWorld;
    };

    class Tabs {
    public:
        std::vector<TabLiveId> all() const;
        // Copy from this world's current model. Missing IDs return a default
        // LiveTab with an empty .id.
        LiveTab to_live_tab(TabId tab_id) const;
        LiveTab to_live_tab(TabLiveId live_id) const;
        void create_tab(
            CreateTab query, TabLiveId* out_live_id = nullptr,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Tabs, create_tab);
        void update(TabLiveId tab_live_id, UpdateTab query,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Tabs, update);
        void remove(
            std::vector<TabLiveId> live_ids, std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Tabs, remove);
        // A zero destination reorders each tab in its current window.
        // An index of -1 inserts the tabs at the end.
        void move(
            std::vector<TabLiveId> tab_live_ids,
            WindowLiveId window_live_id,
            int index,
            std::function<void()> reply = nullptr);
        ASYNC_TASK_REPLY_WRAPPER(Tabs, move);

    private:
        explicit Tabs(ProfileWorld& world) : m_world(world) {}
        ProfileWorld& m_world;
        friend class ProfileWorld;
    };

    ~ProfileWorld();

    std::weak_ptr<ProfileBridge> profile_bridge() const {
        return m_profile_bridge;
    }
    CoreApi& core_api() const { return *m_core_api; }
    ProfileId profile_id() const { return m_profile_id; }
    PrivateMode private_mode() const { return m_private_mode; }
    void initialize();
    // Request native presentation of this world's restored model.  The request
    // is deferred until hydration completes and is a no-op while a shell exists.
    void present();
    // Close every native presentation in this world. The completion runs after
    // the live-model transition commits.
    void quit(std::function<void()> completion = nullptr);
    // Non-owning and null before initialize() or after cleanup().
    WebExtensions::WebExtensionsRuntime* runtime() const;
    // Available before activation; lookup and attachment have no side effects.
    std::weak_ptr<WebViewFactoryBridge> web_view_factory_bridge() const {
        return m_web_view_factory_bridge;
    }
    void cleanup();
    std::weak_ptr<ContextMenuBridge> context_menu_bridge() const {
        return m_context_menu_bridge;
    }
    std::weak_ptr<ClipboardBridge> clipboard_bridge() const {
        return m_clipboard_bridge;
    }

    HostAttachment attach(MainWindowLiveId live_id, MainWindowHost& host);
    HostAttachment attach(WindowLiveId live_id, WindowHost& host);
    HostAttachment attach(TabLiveId live_id, TabHost& host);
    MainWindows& main_windows() { return m_main_window_operations; }
    Windows& windows() { return m_window_operations; }
    Tabs& tabs() { return m_tab_operations; }
    MulticastDelegate<ApplyLiveWindowTabModelChanges>& changes() { return m_changes; }

private:
    ProfileWorld(
        ProfileBridge& parent, CoreApi& core_api,
        ProfileId profile_id, PrivateMode private_mode);
    void detach();
    void clear_host_slots();
    
    void detach(MainWindowLiveId live_id, MainWindowHost& host,
        std::weak_ptr<MainWindowHost*> expected_slot);
    void detach(WindowLiveId live_id, WindowHost& host,
        std::weak_ptr<WindowHost*> expected_slot);
    void detach(TabLiveId live_id, TabHost& host,
        std::weak_ptr<TabHost*> expected_slot);
    bool add_request_completion(std::function<void()> reply);
    void complete_all_requests();
    void commit(ProfileHost& host);
    std::weak_ptr<WebViewBridge> create_web_view_bridge();
    void remove_web_view_bridge(WebViewBridge& bridge);

    LiveWindowTabModel& live_model();
    WindowTabModelRepository& model_repository();

    std::weak_ptr<ProfileBridge> m_profile_bridge;
    CoreApi* m_core_api;
    ProfileId m_profile_id;
    PrivateMode m_private_mode;
    MainWindows m_main_window_operations;
    Windows m_window_operations;
    Tabs m_tab_operations;
    std::unique_ptr<LiveWindowTabModel> m_live_model;
    std::unique_ptr<WindowTabModelRepository> m_model_repository;
    MulticastDelegate<ApplyLiveWindowTabModelChanges> m_changes;
    std::map<MainWindowLiveId, std::shared_ptr<MainWindowHost*>> m_main_windows;
    std::map<WindowLiveId, std::shared_ptr<WindowHost*>> m_windows;
    std::map<TabLiveId, std::shared_ptr<TabHost*>> m_tabs;
    std::vector<std::function<void()>> m_request_completions;
    bool m_is_detaching = false;
    std::shared_ptr<ContextMenuBridge> m_context_menu_bridge;
    std::shared_ptr<ClipboardBridge> m_clipboard_bridge;
    std::vector<std::shared_ptr<WebViewBridge>> m_web_views;
    std::unique_ptr<WebExtensions::WebExtensionsRuntime> m_runtime;
    std::shared_ptr<WebViewFactoryBridge> m_web_view_factory_bridge;
    friend class ProfileBridge;
    friend class WebViewFactoryBridge;
    friend class WebViewBridge;
    friend class WindowTabModelRepository;
};

// Application-owned profile lifetime anchor. Per-world operations use child hosts.
class ProfileHost {
public:
    virtual ~ProfileHost() = default;

    ProfileHost(const ProfileHost& other) = delete;
    ProfileHost& operator=(const ProfileHost& other) = delete;

    virtual void create_main_window(LiveMainWindow window) {};
    virtual void create_window(LiveWindow window) {};

protected:
    ProfileHost() = default;

    friend class ProfileBridge;
};

// Owning snapshot of live records and their final indices. Database ordering
// belongs to the repository's separate ModelState.
struct LiveWindowTabModelSnapshot {
    std::deque<LiveMainWindow> main_windows{};
    std::deque<LiveWindow> windows{};
    std::deque<LiveTab> tabs{};
};

class LiveWindowTabModelLock {
public:
    LiveWindowTabModelLock(const LiveWindowTabModelLock& other) = delete;
    LiveWindowTabModelLock& operator=(const LiveWindowTabModelLock& other) = delete;
    ~LiveWindowTabModelLock();

    explicit operator bool() const { return m_lock_acquired; }

private:
    explicit LiveWindowTabModelLock(LiveWindowTabModel& model);

    LiveWindowTabModel* m_model = nullptr;
    bool m_lock_acquired = false;

    friend class LiveWindowTabModel;
};

class LiveWindowTabModel {
public:
    using LiveWindowTabModelChanges = orion::bridge::LiveWindowTabModelChanges;

    explicit LiveWindowTabModel(ProfileWorld& parent);
    ~LiveWindowTabModel();

    LiveWindowTabModel(const LiveWindowTabModel& other) = delete;
    LiveWindowTabModel& operator=(const LiveWindowTabModel& other) = delete;

    MainWindowLiveId create_main_window(CreateMainWindow query);
    void activate_window(MainWindowLiveId live_id, WindowLiveId window_live_id);
    void remove_main_window(MainWindowLiveId live_id);
    void quit(bool preserve_windows_on_quit);
    WindowLiveId create_window(CreateWindow query);
    WindowLiveId duplicate_window(WindowLiveId live_id);
    void update_window(WindowLiveId live_id, UpdateWindow query);
    void suspend_window(WindowLiveId live_id);
    void delete_saved_window(WindowLiveId live_id);
    void remove_window(WindowLiveId live_id);
    TabLiveId create_tab(CreateTab query);
    void update_tab(TabLiveId tab_live_id, UpdateTab query);
    void remove_tab(TabLiveId tab_live_id);
    void move_tabs(
        std::vector<TabLiveId> tab_live_ids,
        WindowLiveId window_live_id,
        int index);
    std::vector<MainWindowLiveId> all_main_windows() const;
    std::vector<WindowLiveId> all_windows() const;
    std::vector<TabLiveId> all_tabs() const;

    // Lock the current state so it can be applied to multiple change sinks before
    // advancing the committed baseline.
    [[nodiscard]] LiveWindowTabModelLock begin();
    [[nodiscard]] std::optional<LiveWindowTabModelSnapshot> snapshot(const LiveWindowTabModelLock& lock) const;
    void apply_changes(LiveWindowTabModelLock& lock, LiveWindowTabModelChanges& changes) const;
    void commit(LiveWindowTabModelLock& lock);

private:
    void schedule_commit();
    LiveWindow live_window(WindowId window_id) const;
    LiveWindow live_window(WindowLiveId live_id) const;
    LiveMainWindow live_main_window(MainWindowLiveId live_id) const;
    LiveTab live_tab(TabId tab_id) const;
    LiveTab live_tab(TabLiveId live_id) const;

private:
    std::weak_ptr<ProfileWorld> m_parent;
    
    class Impl;
    std::unique_ptr<Impl> m_state;

    friend class LiveWindowTabModelLock;
    friend class ProfileWorld::MainWindows;
    friend class ProfileWorld::Windows;
    friend class ProfileWorld::Tabs;
};

class WindowTabModelRepository {
public:
    struct ModelState {
        std::map<WindowId, WindowDto> windows;
        std::map<TabId, TabDto> tabs;
    };

    explicit WindowTabModelRepository(ProfileWorld& parent);
    WindowTabModelRepository(const WindowTabModelRepository& other) = delete;
    WindowTabModelRepository& operator=(const WindowTabModelRepository& other) = delete;

    void hydrate();
    void present();
    void checkpoint(const LiveWindowTabModelSnapshot& snapshot);

    // Compare the live snapshot to DB records and advance ModelState before
    // returning the batch, without performing any database calls.
    static WindowTabCheckpoint reconcile_window_tab_state(ProfileId profile_id,
        const LiveWindowTabModelSnapshot& snapshot, ModelState& state,
        const std::set<double>& reserved = {});

private:
    bool can_checkpoint() const;
    bool preserve_windows_on_quit() const;

    enum class HydrationState {
        not_started,
        querying,
        applying,
        hydrated,
        failed,
    };

    std::weak_ptr<ProfileWorld> m_parent;
    HydrationState m_hydration_state = HydrationState::not_started;
    bool m_presentation_requested = false;
    settings::AppOpensWith m_app_opens_with = settings::AppOpensWith::not_set;
    WindowId m_selected_window_id{};
    // Last checkpoint, advanced before host changes, without DB acknowledgements.
    ModelState m_state;
    // Hidden windows in this world only.
    std::set<double> m_reserved_window_keys;

    friend class ProfileWorld;
};

// Core-owned profile node. An application-owned ProfileHost may attach to it.
class ProfileBridge final : public std::enable_shared_from_this<ProfileBridge> {
public:
    ~ProfileBridge();

    ProfileBridge(const ProfileBridge& other) = delete;
    ProfileBridge& operator=(const ProfileBridge& other) = delete;

    ProfileId profile_id() const;
    CoreApi& core_api() const;
    std::weak_ptr<ProfileWorld> main_world() const;
    std::weak_ptr<ProfileWorld> private_world() const;
    std::weak_ptr<ProfileWorld> world(PrivateMode private_mode) const;

    void schedule_commit(ProfileWorld& world);
    void commit(ProfileWorld& world);

private:
    ProfileBridge(CoreApi& core_api, ProfileId profile_id);
    void initialize_worlds();
    [[nodiscard]] HostAttachment attach(ProfileHost& host);
    void detach(ProfileHost& host);

    CoreApi& m_core_api;
    ProfileId m_profile_id;
    std::shared_ptr<ProfileWorld> m_main_world;
    std::shared_ptr<ProfileWorld> m_private_world;
    ProfileHost* m_host = nullptr;

    friend class orion::CoreApi;
    friend class ProfileWorld;
    friend class WebViewFactoryBridge;
    friend class WebViewBridge;
};

// Application-implemented core-to-application web-view contract.
class WebViewHost {
public:
    using WebViewMessageHandler = void(*)(const std::string& message, void* context);

    virtual ~WebViewHost() = default;

    WebViewHost(const WebViewHost& other) = delete;
    WebViewHost& operator=(const WebViewHost& other) = delete;

protected:
    WebViewHost() = default;

public:
    // Core-to-application operations. These are the only operations exposed by
    // the application-implemented host contract.
    virtual void web_view_call(
        const std::string& body, const std::string& argument0_json,
        const std::string& script_world = {}) = 0;
    virtual void web_view_add_script(
        const std::vector<std::string>& matches,
        const std::vector<std::string>& exclude_matches,
        const std::string& script, const std::string& script_world = {},
        WebViewScriptInjectionTime injection_time = WebViewScriptInjectionTime::document_start,
        WebViewScriptFrames frames = WebViewScriptFrames::all_frames) = 0;
    // context is opaque to the host. The caller supplies its teardown; the
    // host invokes destroy_context if registration fails or when the native
    // handler is disconnected.
    virtual bool web_view_register_script_message_handler(
        const std::string& name, WebViewMessageHandler handler,
        void* context, void(*destroy_context)(void* context),
        const std::string& script_world = {}) = 0;
    virtual std::string web_view_security_origin_get_host() = 0;
    virtual std::string web_view_uri() = 0;
    virtual std::optional<int64_t> web_view_tab_id() = 0;
    virtual std::optional<int64_t> web_view_window_id() = 0;
    virtual std::optional<int64_t> web_view_active_tab_id() = 0;

private:
    friend class WebViewBridge;
};

class WebViewBridge final : public std::enable_shared_from_this<WebViewBridge> {
public:
    ~WebViewBridge();

    WebViewBridge(const WebViewBridge& other) = delete;
    WebViewBridge& operator=(const WebViewBridge& other) = delete;

    [[nodiscard]] HostAttachment attach(WebViewHost& host);
    void web_view_call(
        const std::string& body, const std::string& argument0_json,
        const std::string& script_world = {}) const;
    void web_view_add_script(
        const std::vector<std::string>& matches,
        const std::vector<std::string>& exclude_matches,
        const std::string& script, const std::string& script_world = {},
        WebViewScriptInjectionTime injection_time = WebViewScriptInjectionTime::document_start,
        WebViewScriptFrames frames = WebViewScriptFrames::all_frames) const;
    bool web_view_register_script_message_handler(
        const std::string& name, WebViewHost::WebViewMessageHandler handler,
        void* context, void(*destroy_context)(void* context),
        const std::string& script_world = {}) const;
    std::string web_view_security_origin_get_host() const;
    std::string web_view_uri() const;
    std::optional<int64_t> web_view_tab_id() const;
    std::optional<int64_t> web_view_window_id() const;
    std::optional<int64_t> web_view_active_tab_id() const;

private:
    explicit WebViewBridge(ProfileWorld& parent);
    void detach(WebViewHost& host);
    WebViewHost* host() const { return m_host; }

    std::weak_ptr<ProfileWorld> m_parent;
    WebViewHost* m_host = nullptr;

    friend class ProfileWorld;
    friend class WebViewFactoryBridge;
    friend class WebExtensions::WebExtensionsRuntimeImpl;
    friend class ContextMenuBridge;
};

// Application-implemented core-to-application context-menu contract.
class ContextMenuHost {
public:
    virtual ~ContextMenuHost() = default;

    ContextMenuHost(const ContextMenuHost& other) = delete;
    ContextMenuHost& operator=(const ContextMenuHost& other) = delete;

protected:
    ContextMenuHost() = default;

private:
    virtual void web_extension_context_menus_update(
        const WebExtensions::WebExtensionContextMenuScope& scope,
        const std::vector<WebExtensions::WebExtensionContextMenuItem>& items) = 0;
    virtual void web_extension_context_menus_clear(
        const WebExtensions::WebExtensionContextMenuScope& scope) = 0;
    virtual void web_extension_context_menus_refresh(
        const WebExtensions::WebExtensionContextMenuScope& scope) = 0;
    virtual void web_extension_context_menus_override(
        WebViewBridge& web_view,
        const WebExtensions::WebExtensionContextMenuOverride& options) = 0;

    friend class ContextMenuBridge;
};

class ContextMenuBridge final
    : public std::enable_shared_from_this<ContextMenuBridge> {
public:
    ~ContextMenuBridge();

    ContextMenuBridge(const ContextMenuBridge& other) = delete;
    ContextMenuBridge& operator=(const ContextMenuBridge& other) = delete;

    [[nodiscard]] HostAttachment attach(ContextMenuHost& host);
    void context_menu_clicked(
        const WebExtensions::WebExtensionContextMenuScope& scope,
        WebExtensions::WebExtensionContextMenuClick click);
    void context_menu_shown(
        const WebExtensions::WebExtensionContextMenuScope& scope,
        WebExtensions::WebExtensionContextMenuShown shown);
    void context_menu_hidden(
        const WebExtensions::WebExtensionContextMenuScope& scope);

private:
    ContextMenuBridge();
    void detach(ContextMenuHost& host);

    ContextMenuHost* m_host = nullptr;
    std::map<std::string,
        WebExtensions::WebExtensionContextMenuCallbacks> m_callbacks;

    void update_from_core(
        const WebExtensions::WebExtensionContextMenuScope& scope,
        const std::vector<WebExtensions::WebExtensionContextMenuItem>& items,
        WebExtensions::WebExtensionContextMenuCallbacks callbacks);
    void clear_from_core(const WebExtensions::WebExtensionContextMenuScope& scope);
    void refresh_from_core(const WebExtensions::WebExtensionContextMenuScope& scope);
    void override_from_core(
        WebViewBridge& web_view,
        const WebExtensions::WebExtensionContextMenuOverride& options);

    friend class WebExtensions::ContextMenusV2Impl;
    friend class ProfileWorld;
};

// Application-implemented core-to-application clipboard contract.
class ClipboardHost {
public:
    using ReadRequest = uint64_t;
    virtual ~ClipboardHost() = default;

    ClipboardHost(const ClipboardHost& other) = delete;
    ClipboardHost& operator=(const ClipboardHost& other) = delete;

protected:
    ClipboardHost() = default;

private:
    virtual std::optional<std::string>
    clipboard_write_text(const std::string& text) = 0;
    virtual void clipboard_read_text(ReadRequest request) = 0;

    friend class ClipboardBridge;
};

class ClipboardBridge final
    : public std::enable_shared_from_this<ClipboardBridge> {
public:
    using ReadRequest = ClipboardHost::ReadRequest;
    using ReadCompletion = std::function<void(
        std::optional<std::string> text, std::optional<std::string> error)>;

    ~ClipboardBridge();

    ClipboardBridge(const ClipboardBridge& other) = delete;
    ClipboardBridge& operator=(const ClipboardBridge& other) = delete;

    [[nodiscard]] HostAttachment attach(ClipboardHost& host);
    void clipboard_read_text_completed(
        ReadRequest request, std::optional<std::string> text,
        std::optional<std::string> error);

private:
    ClipboardBridge();
    void detach(ClipboardHost& host);

    ClipboardHost* m_host = nullptr;
    ReadRequest m_next_request = 1;
    std::map<ReadRequest, ReadCompletion> m_read_completions;

    std::optional<std::string> write_from_core(const std::string& text);
    void read_from_core(ReadCompletion completion);

    friend class WebExtensions::RuntimeImpl;
    friend class ProfileWorld;
};

} // namespace bridge

} // namespace orion
