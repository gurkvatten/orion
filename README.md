# Orion for Linux

Orion is a web browser for Linux built on the WebKit engine, with a native
GTK4/libadwaita interface. It ships with built-in content blocking, password
management with autofill, tab groups, profiles, and early WebExtensions
support.

This repository is the public archive of the Linux browser. See the
[Public archive](#public-archive) section below.

## Public archive

Kagi ended development of Orion for Linux and Windows in October 2026 and
open-sourced both so the community can carry them forward. This repository is
that public release of the Linux browser.

Read the announcement for full details: [An Update on Orion for Linux and Windows](https://blog.kagi.com/update-orion-linux-windows)

The code is released under the Apache License 2.0. Bundled third-party
components keep their own licenses; see [NOTICE](NOTICE) for the list.

## What this browser is

- **WebKit-based, not a Chromium fork.** The engine is upstream WebKitGTK,
  built from source as a git submodule. Orion adds the browser shell, the
  extension engine, and the privacy layer around it.
- **Native Linux UI.** The interface is GTK4 with libadwaita. Windows,
  dialogs, and widgets follow GNOME conventions. UI layouts are authored in
  Blueprint (`.blp`) files and compiled at build time.
- **Privacy-focused by default.** EasyList and EasyPrivacy ad and tracker
  blocking are enabled out of the box, along with WebKit's Intelligent
  Tracking Prevention.
- **Beta quality.** The last Kagi release was version 0.4.3 beta. Kagi does
  not recommend it as a main browser. Many features are complete, and several
  expected browser features are not implemented yet. See
  [Feature status](#feature-status).

## Architecture

The codebase has two layers:

- **Application (UI)** — `app/`: the GTK4/libadwaita program. Entry points are
  `app/src/main.cpp` (process entry) and `app/src/app.cpp` (application
  lifecycle).
- **Core library** — `app/src/core_module/`: the `orion_core` C++20 library.
  It is UI-agnostic and owns storage, models, the extension engine, and the
  WebKit bridge.

### UI layer (`app/`)

| Directory | Contents |
| --- | --- |
| `app/src/window/` | The main window and about 90 dialogs, each as a Blueprint (`.blp`) file plus C++ code. Includes the tab bar, sidebars, settings, password and bookmark managers. |
| `app/src/window/orion_webview_tabview_widgets/` | `OrionWebkitWebview`, a `WebKitWebView` subclass. Handles permission prompts, certificate errors, autofill script injection, and web-process crashes. |
| `app/src/action/` | The action map that wires menu and toolbar commands to behavior. `app/src/action/main_window/action_map.cpp` is the source of truth for which features are live. |
| `app/src/accel/` | Keyboard accelerator registration (about 99 shortcuts). |
| `app/src/content_block/` | Compiles bundled and custom filter lists into WebKit content filters. |
| `app/src/webextensions/` | GTK-side host objects the core attaches extensions to: webviews, tabs, context menus, clipboard, profiles. |
| `app/src/data_item/` | GTK list models for tabs, bookmarks, history, passwords, and downloads views. |
| `app/src/platform/libsecret/` | GNOME Keyring backend for stored passwords. |
| `app/data/` | GResources: CSS, themes, icons, internal HTML pages, scripts, and the bundled filter lists. |

### Core layer (`app/src/core_module/`)

| Directory | Contents |
| --- | --- |
| `core/core_api.h` | The `orion::CoreApi` façade. The UI reaches all core services through it: profiles, history, bookmarks, settings, downloads, extensions. |
| `core/db/` | Thin sqlite3 wrapper: prepared statements, transactions, migrations. |
| `core/profiles_db/` | The persistent model on sqlite: bookmarks, history, passwords, downloads, favicons, extensions, filter lists, search engines, settings, and session snapshots. |
| `core/webextensions/` | The extension engine: install store, background pages and service workers, extension storage. |
| `core/webkit_bridge/` | The generated RPC bridge between core and UI (see below). |
| `core/form_analyzer/` | Parses login forms on pages to drive password autofill. |
| `core/sync/` and `sync/` | Profile export/import to a folder, plus the (incomplete) account sync library. |
| `common/src/` | Shared utilities: C++20 coroutines, run loop, crypto, zip, JSON, CSV. |
| `third_party/` | Git submodules: WebKit, sqlite, libzip, libuv, libsecret, zlib, pugixml, and others. |

### UI ↔ core bridge

The UI calls the core through the `CoreApi` façade using C++20 coroutines
(`AWAIT(...)` macros). The core calls back into the UI through a generated
RPC bridge:

1. Interface definitions live in `core_module/core/webkit_bridge/codegen/` as
   IDL files.
2. Go generators (downloaded automatically by CMake) turn the IDL into C++
   headers and sources at build time.
3. The core attaches its side of each interface (`ProfileBridge`,
   `ProfileWorld`, `WebViewBridge`, `ContextMenuBridge`, `ClipboardBridge`) to
   GTK host objects in `app/src/webextensions/`.

Two services are injected in the opposite direction: password storage
(libsecret) and the file downloader are implemented in the UI layer and
registered with the core at startup.

### Engine and process model

- WebKit is a git submodule (`core_module/third_party/WebKit`) built from
  source with the GTK port and Web Extensions support enabled.
- The browser uses the standard WebKitGTK multi-process model: one UI process
  plus separate web processes per `WebKitWebContext`. Orion creates a
  dedicated web context per profile world (normal and private), so extension
  pages and private browsing are process-isolated from normal browsing.
- Web-process crashes are detected and shown in a crash reporter dialog.
- Hardware-accelerated compositing is enabled by default.
- In Flatpak builds, sandboxing comes from the Flatpak bubblewrap sandbox.
  The manifest shares network, Wayland/X11, audio, the download directory,
  and DRI devices.

### Storage

- All structured data lives in one sqlite database (`profiles.db`) under the
  user data directory. It holds profiles, bookmarks, history, downloads,
  favicons, extension state, filter lists, settings, and session snapshots.
- Passwords are stored through the freedesktop.org Secret Service (GNOME Keyring)
  via libsecret, or the Portal Secret API inside Flatpak. The database keeps
  only password metadata.
- The core runs its own run loop with coroutine-based APIs. The GTK UI stays
  single-threaded.

### Extensions

Orion implements a WebExtensions engine that accepts both Chrome and Firefox
manifest formats. Extensions load from disk as archives. The supported API
surface is defined in the bridge IDL files: `runtime`, `storage` (local),
`tabs`, `windows`, `alarms`, `notifications`, `contextMenus`, `action`,
`permissions`, `i18n`, and others. Support is partial; see
[Feature status](#feature-status). The repository includes an API conformance
test extension and a support tracker under
`core_module/docs/webextensions_api_tracker/`.

## Feature status

The menu bar is a port of the macOS Orion menu. Some menu and settings items
appear but are disabled because no handler exists yet. The registered actions
in `app/src/action/main_window/action_map.cpp` are the source of truth for
what works.

### Implemented

**Tabs, windows, profiles**

- Tabs: create, close, duplicate, pin, mute, suspend, reorder, nested (parent/child) tabs, tab switcher, hover previews
- Session restore: reopen last closed tab or window, restore all windows from last session, workspace snapshots
- Vertical tabs and compact tab layouts
- Multiple profiles with icons and colors, per-profile windows and settings
- Private windows and private tabs
- Focus mode windows

**Data**

- Bookmarks: folders, bookmarks bar, sidebar, import/export (Netscape HTML format)
- History view and clear-history with time range options
- Downloads: pause, resume, open, show in file manager, automatic cleanup
- Password manager: add, edit, delete, remember and update prompts, CSV import/export, and TOTP two-factor codes
- Password autofill using an injected script world and form analysis
- Profile export and import to a folder

**Privacy and content blocking**

- Ad and tracker blocking with ABP/EasyList syntax filters. Bundled lists:
  EasyList, EasyPrivacy, AdGuard site-specific rules, Fanboy's Annoyance,
  Hagezi Pro Plus Mini, Privacy Essentials. Custom filter URLs are supported
  and lists auto-update.
- Intelligent Tracking Prevention (WebKit ITP), globally and per site
- Per-page privacy popover showing blocked requests
- Element hiding ("Erase Elements") for per-page cleanup
- Cookie controls: scheduled cookie deletion, per-site cookie settings, website data manager
- Tracker parameter stripping from URLs

**Extensions**

- Load unpacked or packaged (`.xpi`) Chrome and Firefox extensions from disk
- Extension manager UI, per-extension permissions view, toolbar buttons and popovers, keyboard shortcuts
- Background pages and service workers

**Navigation and search**

- Many built-in search engines (Kagi, Google, DuckDuckGo, Startpage, and others), custom engines, per-profile defaults
- Address bar navigation and search
- Start page with configurable search recommendations

**Page experience**

- Reader mode with font, width, and color customization
- Zoom, per-site default zoom
- User agent switching (Chrome, Firefox, Safari, mobile, custom), per site
- Compatibility mode (Safari user agent) globally and per site
- Fullscreen, PDF viewing (WebKit built-in), export page as PDF
- Install site as app (PWA-like)
- Themes: light, dark, system, and custom accent colors
- Customizable toolbar

**Security and developer tools**

- HTTP authentication dialogs
- TLS certificate viewer and certificate error pages
- Permission prompts for geolocation, notifications, clipboard, pointer lock, screen sharing, camera, and microphone
- Per-site permission and content defaults (JavaScript, popups, autoplay, downloads, and more)
- WebKit Web Inspector (developer tools)
- Crash reporter UI

### Not implemented

These are commonly expected browser features that are missing, stubbed, or
only partially working in this release.

**Core browser gaps**

- **Cloud sync.** Cross-device sync is disabled in settings. Only local
  profile export/import to a folder works. The account sync library exists
  but its server interface is not implemented.
- **Auto-update.** Update checking is not wired. The update dialogs are UI
  placeholders. Flatpak users must update through their package manager.
- **Printing.** The print dialog opens but produces no output.
- **Spell check.** Settings toggles exist but are disabled.
- **Address bar autocomplete and search suggestions.** The completion UI
  exists but is not connected to any model. Settings for suggestions
  persist but do nothing.
- **Extension store installation.** "Install from Chrome Web Store" and
  "Install from Firefox Add-ons" only open the store website. There is no
  install pipeline. Extensions must be loaded from disk.
- **Page translation.** The menu item has no handler.
- **Screenshots.** Page and full-page screenshot actions are stubs.
- **Notes.** The toolbar button and menu item are stubs.
- **RSS feed indicator.** Disabled.
- **Picture-in-picture menu action.** Stub. WebKit's native video player PiP
  may still work.
- **Language switching and default font picker.** Disabled in settings.

**Security and media gaps**

- **DRM (EME/Widevine).** No CDM is bundled and the media key permission
  handler is empty. Streaming services that require DRM will not play.
- **Web push notifications.** Not implemented.
- **Camera and microphone persistent grants.** Prompts appear, but the
  allow/deny-persistently paths are not implemented.
- **Automatic HTTPS upgrades.** Setting exists but is disabled.
- **Credit card and address autofill.** Not present. Only password autofill
  exists.

**WebExtensions API gaps**

The extension engine is early-stage. Per the in-tree status tracker:

- `scripting` is a mock: no programmatic injection, and content scripts are
  not applied. This blocks many popular extensions.
- `webRequest` is a mock: listeners do not receive or block requests.
- `webNavigation` is a mock: no navigation events are emitted.
- `storage.sync` and `storage.managed` have no backend.
- `permissions` does not prompt or persist host grants.
- `runtime.connectNative` / native messaging is rejected.
- `notifications` calls succeed but display nothing.
- Badge and icon setters on `action`/`browserAction` discard values.
- `tabs`/`windows` query filters are ignored and lifecycle events are not
  emitted.
- `alarms` do not persist across restarts. `i18n` is fixed to `en-US`.

**Developer tools beyond the inspector**

Menu items for console, network, storage, layers, performance profiling,
responsive design mode, and the experimental features submenu are stubs. The
WebKit Web Inspector itself works.

**Ported macOS-only items**

The ported menu includes macOS-only items (text substitutions, speech,
dictation, window tiling) that are disabled on Linux.

## Repository layout

```
app/                    Application sources, resources, and build definitions
  src/                  GTK UI sources (window/, action/, accel/, ...)
  src/core_module/      orion_core library (storage, extensions, WebKit bridge)
  src/core_module/third_party/   Git submodules (WebKit, sqlite, libzip, ...)
  data/                 GResources: CSS, icons, filter lists, scripts
  tools/blueprint-compiler/      Vendored Blueprint compiler
cmake/                  Packaging and Flatpak helpers
docs/                   Doxygen sources and generated documentation
scripts/                Helper scripts for building, installing, running
```

## Build setup with WebKit (flatpak)
Install flatpak tools:
```
sudo apt update
sudo apt install -y flatpak gnome-software-plugin-flatpak golang-go ninja-build appstream-compose cmake
```

Checkout submodules:
```
git submodule update --init --recursive
```

Configure:
```
cmake --preset flatpak-webkit-release
```

To build, choose number of CPU cores wisely (`taskset` `-c` parameter),
a safe choice is cca 3-4 GiB of available RAM per core, for example:
```
taskset -c 0-15 cmake --build --preset flatpak-webkit-release
```

Install flatpak package and run it:
```
flatpak install --user install/flatpak-webkit-release/oriongtk.flatpak
flatpak run com.kagi.Orion
```

Export flatpak repo and package (useful for Flatpak Manager release):
```
( BRANCH=$(git branch --show-current) ARCH=$(uname -m) && mkdir -p build/flatpak-webkit-release/flatpak-export/$BRANCH/$ARCH && flatpak build-export build/flatpak-webkit-release/flatpak-export/$BRANCH/$ARCH/repo build/flatpak-webkit-release/export beta && cp install/flatpak-webkit-release/oriongtk.flatpak build/flatpak-webkit-release/flatpak-export/$BRANCH/$ARCH/ && ( cd build/flatpak-webkit-release/flatpak-export && zip -qr orion-$ARCH.zip $BRANCH/$ARCH/ ) )
```
The resulting ZIP file contains both flatpak package and flatpak repo directory and is in `build/flatpak-webkit-release/flatpak-export/orion-$ARCH.zip` where $ARCH is host architecture (currently, x86_64 or aarch64).

## Build notes
- It is recommended to build on bare metal machine (not in virtual machine, nor in WSL) because virtualization can cause issues and errors during the build.
- It is recommended to have at least 3 or 4 GiB of RAM (approximately) per each CPU core.

The build (which takes around 30 minutes) is tested on a bare metal machine with:
- RAM: 64 GiB
- CPU: 20 physical cores

## Other build variants

See [Development.md](Development.md) for native (non-Flatpak) builds:

- `linux-webkit-release`: native build with the embedded WebKit build. Needs
  the full system dependency list from Development.md.
- `linux-release`: UI-only build against system libraries, without WebKit.
  Useful for UI development and faster iteration. Some features are not
  available in this mode.

All presets come in `debug` and `release` variants. The build needs GTK 4.18+
and libadwaita 1.7+ for full functionality; older libadwaita falls back to
compatibility layouts for some dialogs. CMake downloads a pinned Go toolchain
by itself for code generation.

## Development documentation

- [Development.md](Development.md) — build variants and environment setup
- [docs/mainpage.md](docs/mainpage.md) — Doxygen landing page with an architecture overview
- [NewDialog.md](NewDialog.md) — how to add a new dialog with Blueprint
- [app/src/core_module/BUILDING.md](app/src/core_module/BUILDING.md) — core library build details
- `app/src/core_module/docs/webextensions_api_tracker/` — WebExtensions API support tracker

## License

Apache License 2.0. See [LICENSE](LICENSE). Some bundled third-party
components use other licenses; see [NOTICE](NOTICE). WebKit and the other git
submodules keep their upstream licenses.
