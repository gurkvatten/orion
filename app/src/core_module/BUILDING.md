# core_module build, install, and use

**Requirements**

- CMake 3.24+.
- A C++20 compiler.
- Go toolchain for code generation. If `GO_EXECUTABLE` is not provided, CMake will download
  a pinned Go toolchain during configure (see `cmake/go.cmake`).

**Configuration options**

- `ORION_CORE_BUILD_SHARED` (default `ON`): build `orion_core` as a shared library.
- `GO_EXECUTABLE`: override the Go toolchain used for code generation.

**Configure, build and install (presets)**

From `app/src/core_module`:

```bash
cmake --preset linux
cmake --build --target install --preset linux-release
```

Windows presets are also provided:

In `Developer Command Prompt for VS`:

```cmd
cmake --preset windows
cmake --build --target install --preset windows-release
```

**General use**

When included from the application build, `orion_core` is already available. To consume
from another CMake project:

```cmake
add_subdirectory(path/to/core_module)

target_link_libraries(your_target
    PRIVATE orion_core
)
```

Include headers from `core/`, for example:

```cpp
#include <core/core_api.h>
```

## Embedding: hosts and bridges

The application implements native operations in `orion::bridge::*Host` classes.
WebExtension service hosts attach to corresponding core-owned `*Bridge` objects,
which forward requests to their hosts. Window/tab presentation hosts instead
attach directly to a `ProfileWorld` by live ID. Native events and asynchronous
results travel back through bridge or world methods, such as
`context_menu_clicked()`, `clipboard_read_text_completed()`, and
`ProfileWorld::tabs().update()`.

```cpp
#include <core/core_api.h>
#include <core/webkit_bridge/API/bridge.h>
```

Password storage and downloads remain separate integrations through
`CoreApi::set_password_storage()` and `CoreApi::set_downloader_factory()`.

### Where to start

Read these files in order (links are relative to this directory):

1. [Host and bridge contracts](core/webkit_bridge/API/bridge.h): interfaces,
   attachment tokens, world accessors, and factory operations.
2. [CoreApi](core/core_api.h): `attach_profile_host()` and `get_profile_world()`.
3. GTK host implementations: [profile](../webextensions/profile_host.cpp),
   [clipboard](../webextensions/clipboard_host.cpp), and
   [web view](../webextensions/webview_host.cpp). The profile host attaches the
   per-world menu/clipboard hosts without activating either world.
4. [Application activation and ownership](../app.cpp):
   `orion_app_activate_profile_world()` creates the requested world's native
   factory, retains it, and then initializes that world.
5. [Application factory](../webextensions/webview_factory_host.cpp): implements
   `WebViewFactoryHost`; `sync_web_view_common()` creates, attaches, retains,
   and configures each native view.
6. [Core factory logic](core/webextensions/web_view.cpp): dispatch, readiness,
   and resource handling. Bridge lifecycle implementations are split by class:
   [attachment tokens](core/webkit_bridge/host_attachment.cpp),
   [profiles](core/webkit_bridge/profile_bridge.cpp),
   [worlds](core/webkit_bridge/profile_world.cpp),
   [web views](core/webkit_bridge/webview_bridge.cpp),
   [context menus](core/webkit_bridge/context_menu_bridge.cpp), and
   [clipboard](core/webkit_bridge/clipboard_bridge.cpp).
7. [GTK context-menu host](../webextensions/context_menu_host.cpp):
   native presentation and events returned to core.

The GTK code is the platform integration example. Non-GTK branches provide
fallback implementations rather than a complete native browser integration.

### Ownership and hierarchy

```text
CoreApi
└── ProfileBridge (one per attached ProfileId) <-- application ProfileHost
    ├── ProfileWorld (PrivateMode::no, normal browsing)
    │   ├── ContextMenuBridge               <-- application ContextMenuHost
    │   ├── ClipboardBridge                 <-- application ClipboardHost
    │   ├── WebViewFactoryBridge            <-- application WebViewFactoryHost
    │   ├── WebExtensionsRuntime             [only while activated]
    │   ├── WebViewBridge (one per view)     <-- application WebViewHost
    │   └── live window/tab model
    │       ├── MainWindowLiveId             <-- application MainWindowHost
    │       ├── WindowLiveId                 <-- application WindowHost
    │       └── TabLiveId                    <-- application TabHost
    └── ProfileWorld (PrivateMode::yes, private browsing)
        └── same per-world structure, with separate instances
```

The tree shows core ownership; arrows show attachments. A child **host attaches
to its matching bridge or live-ID slot**, not to another host. The application
owns all native hosts. It can keep menu/clipboard hosts inside its profile host,
factories in per-world application state, window/tab hosts in its presentation
objects, and each web-view host with its native view.

`ProfileHost` is a profile lifetime anchor. Factory operations belong to
`WebViewFactoryHost`; there is no factory subclass owned by core and no
`ProfileHost::create_web_view_factory()` hook. `ProfileWorld` owns the runtime
and factory bridge, while the application owns the native factory host.

Pass bridge arguments by reference. Return bridges through `weak_ptr` getters,
then lock them locally before use and handle expiration. Store weak bridge
references in asynchronous callback state rather than raw host pointers.
A bridge may store a raw pointer to **its own matching host**, whose attachment
clears that pointer. Do not store raw pointers to parent or child hosts. Resolve
native hosts locally when needed; raw host getters should be private/protected
and used only locally. The app's factory lookup returns a weak reference to its
owned native factory and never creates or activates one.

### 1. Attach the profile without starting either world

Derive a native profile class from `ProfileHost`. Attach it to an existing
`CoreApi` and retain the token as a member. This constructor excerpt shows the
profile lifetime anchor:

```cpp
class NativeProfileHost final : public orion::bridge::ProfileHost {
public:
    NativeProfileHost(orion::CoreApi& core, orion::ProfileId id)
        : attachment_(core.attach_profile_host(id, *this))
    {
        // Obtain both worlds and attach their menu/clipboard hosts here.
        // Do not activate either world from this constructor.
    }

private:
    orion::bridge::HostAttachment attachment_;
    // Declare owned child hosts after attachment_ so they are destroyed first.
};
```

`attach_profile_host()` creates the profile bridge, both worlds, and their
menu/clipboard/factory bridges. Do not construct these bridge types directly.
`get_profile_world(id, PrivateMode::no/yes)` only looks up an existing world;
lookup before profile attachment returns an empty weak pointer.

For each world, lock `context_menu_bridge()` and `clipboard_bridge()`, create
native hosts, and retain each host's `bridge.attach(*this)` token. They can be
attached eagerly, provided their construction does not start native work.

### 2. Activate only the world that is being opened

**World construction and host attachment must not start downloads, native
sessions, or runtimes.** Both worlds may exist while only one is active. In
particular, opening a normal window must not initialize the private factory,
load its filter lists, or start its Kagi service.

When opening the first window in a particular world:

1. Lock that world's `web_view_factory_bridge()`.
2. Create an application factory implementing `WebViewFactoryHost`, passing the
   bridge by reference. Retain its weak bridge reference and attachment token.
3. Store the factory in application state keyed by profile and private mode.
   Finish construction and attachment before activating the world.
4. Call `world->initialize()`. Core creates the runtime, calls the factory host's
   `initialize_platform()`, subscribes to extension updates, and initializes the
   runtime. Native setup therefore precedes initial background-page updates.
5. Start application services such as Kagi onboarding only for this active world.
   GTK does this from window setup, after factory activation.

Further windows reuse the active world's factory and runtime; `initialize()`
is idempotent while the runtime exists. Runtime loading is asynchronous: use
`WebViewFactoryBridge::when_ready()` or `is_ready()` for operations that need
loaded extensions. `runtime()` is null before activation and after cleanup.
The factory bridge getter remains available while the world exists.

The factory host implements these native operations:

| Host operation | Responsibility |
| --- | --- |
| `initialize_platform()` | Set up native state and start this active world's filter setup. |
| `register_scheme(...)` | Register the supplied scheme in this world's browser context and route requests/responses through its handler. |
| `update_background_pages(...)` | Create/remove this world's requested native background views. |

The factory bridge owns extension tracking, resource/proxy handling, readiness,
document URI lookup, and view configuration. These operations call the matching
factory host directly, without forwarding through `ProfileHost`.
Call `bridge.register_schemes()` when the native browser context is available,
before loading extension URLs; GTK does this in `sync_web_context()`.

### 3. Attach and configure each native web view

Implement every `WebViewHost` operation: JavaScript calls, script injection,
message-handler registration, security origin/URI lookup, and optional tab,
window, and active-tab IDs. Preserve script world, injection time, and frame
selection. Return `std::nullopt` where an ID is unavailable.

For every content, popup, or background view:

1. Create the native view in the correct world's context/session.
2. Call `factory_bridge.create_web_view_bridge()` and lock the result. Inactive
   or detached factories cannot create bridges; release the native view if this
   fails.
3. Construct the native `WebViewHost`, call `view_bridge.attach(*host)`, and retain
   both the host and token with the native view.
4. Call `factory_bridge.configure_web_view(*view_bridge)` **after attachment**,
   so scripts/message handlers can reach the host, and before loading the page.

The common attachment pattern is (native overrides omitted):

```cpp
class NativeWebViewHost final : public orion::bridge::WebViewHost {
public:
    explicit NativeWebViewHost(orion::bridge::WebViewBridge& bridge)
        : bridge_(bridge.weak_from_this())
        , attachment_(bridge.attach(*this))
    {}

    ~NativeWebViewHost() override
    {
        // Stop/disconnect native callbacks while their state is still valid.
        attachment_.reset();
    }

private:
    std::weak_ptr<orion::bridge::WebViewBridge> bridge_;
    orion::bridge::HostAttachment attachment_;
    // Store native view state and implement all WebViewHost operations.
};
```

Use the same token pattern for factory, menu, and clipboard hosts. Attaching a
factory does not call `initialize_platform()`: activation is explicit.

For script message registration, the context pointer is opaque. Invoke its
supplied `destroy_context` callback exactly once on registration failure or when
the native handler is disconnected. Disconnect handlers before host teardown.

Return menu clicked/shown/hidden events through the matching `ContextMenuBridge`,
preserving extension scope. Clipboard writes return `std::nullopt` on success
or an error string. Complete reads through
`clipboard_read_text_completed(request, text, error)` with the original request
ID. Use `std::nullopt` for the absent result/error; empty text is a valid success.

### Attachment lifetime and shutdown

`HostAttachment` is a move-only RAII token. Destruction, `reset()`, or assignment
of `nullptr` detaches the host; moving transfers responsibility. Store every
returned token: a discarded temporary detaches immediately. Keep hosts at stable
addresses and alive until their attachments are released. Use one attachment per
bridge, and do not overwrite an active token with another attachment to it.

| Attachment released | Effect |
| --- | --- |
| Profile | Removes the profile bridge from `CoreApi`; bridge destruction detaches and cleans both worlds. |
| Factory | Invalidates the attachment's subscriptions/callbacks and cleans its world's runtime and view bridges. |
| Web view | Removes that view bridge from its world's owned view list. |
| Main window, window, or tab | Clears that live ID's host slot. It does not itself mutate or remove the live-model object. |
| Context menu | Clears the attached host pointer. |
| Clipboard | Clears the host pointer and fails pending reads with `clipboard is unavailable`. |

For world shutdown, stop new work and cancel/drain native asynchronous operations
(including Kagi and filter operations), then close native views and disconnect
handlers. Call `world->cleanup()` while the profile and child hosts are valid.
This clears the factory attachment and subscriptions, drops view bridges, and
destroys the runtime. **It does not destroy the application-owned factory**;
release that factory from application state separately. Keep `CoreApi` alive
through all profile/world teardown.

World cleanup preserves the menu/clipboard bridges and their attachments. To
reopen, create and attach a new native factory and initialize the world again.
Recreate all view bridges. Old factory scheme handlers return service unavailable
and readiness callbacks from a retired attachment do not reach its replacement.
Native asynchronous operations still need cancellation/draining before destroying
native resources. Avoid retaining strong world/bridge references across shutdown;
a retained world does not keep its parent profile or `CoreApi` alive.

### Integration checklist

- [ ] Link `orion_core` and implement the needed native host interfaces.
- [ ] Retain one profile attachment per profile and keep `CoreApi` alive through teardown.
- [ ] Obtain normal/private worlds after profile attachment; keep attachment and lookup inactive.
- [ ] Attach separate menu/clipboard hosts for each world.
- [ ] Create, retain, and attach a native factory only when its world is opened.
- [ ] Activate only the requested world, then start that world's Kagi service.
- [ ] Verify normal-window startup performs no private factory/filter/Kagi work.
- [ ] Register schemes before extension navigation; wait for runtime readiness where needed.
- [ ] For each view: create native view, create bridge, attach host, configure, load.
- [ ] Pass bridges by reference; use weak getters and callback references; resolve hosts locally.
- [ ] Keep exactly one live attachment per bridge and release callback contexts correctly.
- [ ] Test scripts, messaging, popup/background pages, menus, and clipboard in both worlds.
- [ ] Cancel/drain operations, clean the world, release its native factory, then release profile hosts.
- [ ] Verify closing one world preserves the other and reopening starts a fresh factory.

## Migrating windows and tabs from the legacy CoreApi

New application code must use the `ProfileWorld` live model as the source of
truth for browser windows and tabs. The legacy `CoreApi` window/tab methods are
kept for backwards compatibility and for internal persistence work, but they do
not update the live presentation model.

> **This migration is required for WebExtension `windows` and `tabs` API
> support.** Those APIs query and mutate the live model and derive their events
> from live-model changes. If the application continues to create, update, move,
> or remove UI windows and tabs through legacy `CoreApi` calls, extensions will
> see stale state, miss events, and may create a second native presentation.

The intended data flow is:

```text
native user action
    -> ProfileWorld::{main_windows,windows,tabs}()
    -> live model transaction
    -> database checkpoint + host callbacks + change delegates
    -> native presentation and WebExtension events
```

Do not also mutate the native UI or legacy database model on the request path.
Apply the resulting `*Host` callback to the UI, while suppressing native signals
that would submit the same change again.

### Model objects and IDs

The live model separates the native shell from the logical browser window it
currently displays:

| Object | Identity | Purpose |
| --- | --- | --- |
| `LiveMainWindow` | `MainWindowLiveId` | A native top-level shell. Its `window_live_id` is the logical window currently shown in that shell. |
| `LiveWindow` | `WindowLiveId` and persistent `WindowId` | A logical window/tab group. It can exist without a shell or native host. |
| `LiveTab` | `TabLiveId` and persistent `TabId` | A tab belonging to a logical window. |

Live IDs are allocated from one sequence inside one `ProfileWorld`. They are
only meaningful in that world and must never be passed to another profile or to
the other private-mode world. Store the owning world's weak reference together
with every live ID used by a native object.

Persistent IDs remain useful at application boundaries. Convert them at the
world boundary rather than querying the database:

```cpp
auto window = world->windows().to_live_window(window_id);
auto tab = world->tabs().to_live_tab(tab_id);
if(!window.id.value || !tab.id.value) {
    // The object does not exist in this world.
}
```

The `id` member of a `LiveWindow` or `LiveTab` is its live ID; `window_id` and
`tab_id` are the persistent IDs. A failed lookup returns a default record with
an empty live `id`.

### Adopt the host tree first

Follow [Embedding: hosts and bridges](#embedding-hosts-and-bridges) to attach one
`ProfileHost` per application profile and obtain its normal and private worlds.
Attaching the profile host starts asynchronous hydration of both worlds, but it
does not create native windows.

The application must implement the following presentation callbacks:

| Host | Required live-model behavior |
| --- | --- |
| `ProfileHost` | Create and retain `MainWindowHost` and `WindowHost` instances for `create_main_window()` and `create_window()`. |
| `MainWindowHost` | Show/switch the requested logical window in `activate_window()` and close the native shell in `remove()`. |
| `WindowHost` | Apply window metadata, create tab presentations, and remove the logical window presentation. |
| `TabHost` | Apply tab metadata, move the tab to the requested window/index, and remove its native presentation. |

Each create callback must construct and attach the corresponding host before it
returns. A single transaction can call `create_window()`, then
`create_main_window()`/`activate_window()`, then `create_tab()` and `move()`.
Late attachment would lose the later callbacks in that transaction. Retain every
`HostAttachment`, keep the host address stable, and release the attachment before
a host callback destroys its native object.

Host callbacks are model-to-UI notifications. They must not call the live-model
operation that caused the callback. For example, `TabHost::move()` moves the
native page while blocking the native reorder signal; a user-originated drag
calls `world->tabs().move()` and lets `TabHost::move()` apply the result.

### Separate hydration, runtime activation, and presentation

These lifecycle operations have different responsibilities:

- `CoreApi::attach_profile_host()` creates both worlds and starts hydration.
- `world->initialize()` starts that world's WebExtension runtime. Attach its
  `WebViewFactoryHost` first. Initialization is idempotent while the runtime is
  alive.
- `world->present()` requests native presentation. If hydration is still in
  progress, presentation is delayed until it completes. Startup behavior such
  as `AppOpensWith` is resolved here; the application must not independently
  query saved windows and open them.
- `world->quit(completion)` closes the world's native presentation and applies
  `preserve_windows_on_quit` semantics in the live model. The completion runs
  after the transition commits.
- `world->cleanup()` tears down the runtime and WebView bridges; it is not a
  substitute for `quit()`.

For an activated world, the usual order is: attach the native factory,
`initialize()`, then `present()`. Reopening an already hydrated world only needs
activation/presentation; do not hydrate it manually.

### Replace legacy reads and mutations

Application UI code should use the following replacements:

| Legacy pattern | Live-model replacement | Notes |
| --- | --- | --- |
| `CoreApi::get_profile_ids()` for UI routing | The application's profile-host repository | A live world only exists after its profile host is attached. |
| `get_windows()` / `get_windows_by_query()` | `world->windows().all()` and `to_live_window()` | The world already scopes profile and private mode. Filter copied `LiveWindow` records in the application if needed. |
| `get_tabs()` / `get_tabs_by_query()` | `world->tabs().all()` and `to_live_tab()` | Filter by `LiveTab::window_live_id`; do not read the database to reconstruct a native view. |
| `get_ui_windows_by_window_id()` and related UI-window lookups | `main_windows().all()` plus `to_live_main_window()` | Compare `LiveMainWindow::window_live_id`. `LiveWindow::has_host` is not shell ownership. |
| `create_window()` / `db_create_window()` | `world->windows().create_window()` | Use `main_window_live_id` or `auto_create_main_window` when presentation is also required. |
| `update_window()` | `world->windows().update(live_id, changes)` | Only populated optionals change. Selection uses persistent `TabId`. |
| Duplicate a window through application/CoreApi code | `world->windows().duplicate_window()` | Produces an unsaved logical window and a new native shell. |
| `delete_windows()` | `windows().remove()`, `windows().delete_saved()`, or `main_windows().remove()` | Choose logical deletion, saved-window deletion, or shell close explicitly. |
| `create_tab()` / `db_create_tab()` | `world->tabs().create_tab()` | Supply a `WindowLiveId`, or request automatic window/shell creation. |
| `update_tab()` | `world->tabs().update(tab_live_id, changes)` | URL, title, pinned state, suspension, hierarchy, history, and snapshot data belong here. |
| `delete_tabs()` / `db_delete_tabs()` | `world->tabs().remove()` | Pass live IDs from the same world. |
| Legacy tab reorder/window-transfer methods | `world->tabs().move()` | Pass destination `WindowLiveId` and final insertion index. |
| Application-side quit deletion/preservation loops | `world->quit(completion)` | The repository's hydrated setting determines whether transient windows are preserved. |

The old CoreApi methods and the live operations must not both handle the same
action. The repository itself uses database APIs to hydrate and checkpoint the
model; that is an implementation detail and is not a reason for application UI
code to call them. The supported application-facing contract is the
`ProfileWorld` facade in
[the bridge API](core/webkit_bridge/API/bridge.h); do not call
`LiveWindowTabModel`, `WindowTabModelRepository`, or checkpoint APIs directly.

### Operation examples

Create a tab and let the model create and present its parent window when one is
not already known:

```cpp
orion::bridge::TabLiveId tab_live_id;
world->tabs().create_tab({
    .uri = "https://example.com/",
    .auto_create_window = true,
    .auto_create_main_window = true,
}, &tab_live_id, [] {
    // The model and native host changes have committed. Persistence was queued.
});

if(!tab_live_id.value) {
    // The request was rejected synchronously.
}
```

Update tab and window state from native events:

```cpp
world->tabs().update(tab_live_id, {
    .name = page_title,
    .is_pinned = pinned,
    .uri = page_uri,
});

const auto tab = world->tabs().to_live_tab(tab_live_id);
world->windows().update(tab.window_live_id, {
    .selected_tab_id = tab.tab_id,
});
```

Move a tab to another logical window, or reorder it in its current window:

```cpp
world->tabs().move({tab_live_id}, destination_window_live_id, index);
world->tabs().move({tab_live_id}, {}, index); // Keep its current window.
world->tabs().move({tab_live_id}, destination_window_live_id, -1); // Append.
```

Window naming and saved/unsaved state are one update. There is no separate save
operation:

```cpp
world->windows().update(window_live_id, {
    .name = "Research",
    .is_tab_group = false,
});
```

Closing a `MainWindow` is different from deleting its logical `Window`.
`main_windows().remove()` closes the shell; the model removes an unsaved tab
group or keeps a saved window available for reopening. `windows().remove()`
always removes the logical window and its tabs. `windows().delete_saved()` only
accepts a saved window.

### Commit and query rules

Live-model requests are staged synchronously and committed asynchronously:

- Create methods fill their optional output live ID before returning. An empty
  ID means the request was rejected.
- The optional completion runs after the model commit and host application. The
  persistence checkpoint has been submitted, but its database callback may
  finish later. Use the completion when later work depends on committed native
  or model state.
- The generated `async_*` wrappers can be used from `async_task` coroutines, for
  example `co_await world->tabs().async_remove(ids)`.
- `all()` returns IDs from the committed model. `to_live_*()` resolves against
  the current model and can see a valid object staged for the next commit.
- Invalid IDs and no-op requests do not throw. Validate lookups and create
  output IDs rather than assuming a mutation succeeded.

This distinction is especially important when one action creates a parent and
children before the next commit: use the returned live ID to chain those
requests, and use a completion before enumerating with `all()`.

### Route every native state change through the model

Migrating only create/remove calls is insufficient. Route all user-visible
state changes through the owning world:

- navigation URL, title, icon, pinned/suspended state, session state, hierarchy,
  and snapshots: `tabs().update()`;
- selected tab and logical-window name/icon/shortcut/saved state:
  `windows().update()`;
- tab drag/reorder or transfer between logical windows: `tabs().move()`;
- native tab close: `tabs().remove()`;
- shell switch: `main_windows().activate_window()`;
- user shell close: `main_windows().remove()`;
- application quit: `world->quit()`.

Native objects should retain their `ProfileWorld` weak reference and live ID so
callbacks can issue these operations directly. Do not add reverse `host()`
lookups to core or recover a host through persistent database records.

### Observing model changes

Code that needs the same mutation stream as the native host tree, such as a
WebExtension API adapter, can subscribe to `world->changes()`. The delegate
stores a weak reference to the callback, so retain the callback for the whole
subscription:

```cpp
using ChangesCallback = MulticastDelegate<
    orion::bridge::ApplyLiveWindowTabModelChanges>::Func;

changes_callback = std::make_shared<ChangesCallback>(
    [this](orion::bridge::ApplyLiveWindowTabModelChanges apply) {
        MyChangesAdapter adapter{*this};
        apply(adapter);
    });
world->changes() += changes_callback;
```

Invoke `apply` synchronously inside the callback; it is only valid for that
notification and must not be stored or dispatched. The adapter receives one
ordered transaction between `begin()` and `commit()`.

The WebExtension `windows` and `tabs` implementations use this pattern to emit
events and use the same `ProfileWorld` operations for extension-originated
mutations. Therefore every native mutation path must be migrated before those
APIs are considered supported: otherwise extension queries, actions, and events
do not describe the browser the user is looking at.
