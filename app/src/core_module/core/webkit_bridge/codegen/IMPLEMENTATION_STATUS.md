# IDL v2 implementation status

Status meanings:

- **IDL Full**: describes the intended current Chrome and Firefox API surface.
- **IDL Partial**: covers only a subset, or approximates schema availability or types.
- **C++ Full**: implements the declared behavior without known material limitations.
- **C++ Partial**: some declared operations work, while others are missing or simplified.
- **C++ Mock**: preserves the API shape but primarily returns placeholders or no-ops.
- **C++ Limited**: the declared operations work within a documented runtime limitation.

JavaScript implementation shims declared by `Implementation=JavaScript(...)` are included in the C++ status because they are installed and configured by the corresponding C++ implementation.

| IDL | IDL status | C++ status | Notes |
| --- | --- | --- | --- |
| `action.idlv2` | **Partial** | **Mock** | Declares badge background, badge text, and icon setters only. The native methods currently resolve without updating Orion's toolbar model. |
| `alarms.idlv2` | **Full** | **Limited** | Create, query, clear, and `onAlarm` scheduling work in memory. Alarms and `persistAcrossSessions` are not persisted across browser restarts. |
| `contextmenus.idlv2` | **Full** | **Partial** | Chrome `contextMenus` and Firefox `contextMenus`/`menus` are declared. Core owns validated menu state and event dispatch; the application-side bridge still needs to render the supplied model. |
| `extension.idlv2` | **Full** | **Partial** | Identity, URL, legacy request routing, and access checks are wired. `getViews` and `getBackgroundPage` return conservative placeholders, and `setUpdateUrlData` is a no-op. |
| `i18n.idlv2` | **Full** | **Limited** | Locale message lookup, substitutions, placeholders, predefined messages, and `escapeLt` are implemented. Accepted/preferred languages are fixed to `en-US`, and language detection returns a fixed result. |
| `notifications.idlv2` | **Full** | **Full** | Chrome and Firefox functions and events are declared and implemented. Display is delegated to the host's `orion::NotificationProvider`; host interaction drives `onShown` / `onClicked` / `onButtonClicked` / `onClosed`. Without a host presenter (GTK today) calls succeed but nothing is shown. |
| `permissions.idlv2` | **Full** | **Partial** | Manifest permissions can be queried and checked. New grants are not prompted or persisted, removal always fails, host-access requests are unimplemented, and change events are not emitted. |
| `runtime.idlv2` | **Partial** | **Partial** | Chrome/Firefox members and overloads are broadly represented, but manifest-version, permission, platform, and other feature predicates are approximated by tags. Messaging and extension ports work; many lifecycle, update, platform, context, native-messaging, and browser-control operations remain default rejections or limited shims. |
| `scripting.idlv2` | **Full** | **Mock** | The complete Chrome/Firefox surface is active on the v2 bridge. The native harness intentionally uses generated rejection stubs for every operation. |
| `storage.idlv2` | **Full** | **Partial** | CRUD, defaults, keys, byte accounting, quotas, access levels, and scoped change events work. `sync` has no remote synchronization backend, and `managed` has no enterprise-policy source. |
| `tabs.idlv2` | **Partial** | **Partial** | Declares `getCurrent`, `query`, and common tab events. The current or selected active Orion tab can be returned, but query filters are ignored and events are not connected to Orion's tab model. |
| `webnavigation.idlv2` | **Full** | **Mock** | Declares the Chrome and Firefox frame-query methods and navigation events. Frame queries return placeholders, and browser navigation is not yet dispatched to registered listeners. |
| `webrequest.idlv2` | **Partial** | **Mock** | Declares the common event set, with detailed schemas for the currently needed request stages and generic details elsewhere. Listener registrations are retained, but network requests are not dispatched to them. |
| `windows.idlv2` | **Full** | **Partial** | Declares the current Chrome and Firefox types, functions, constants, and events. `getCurrent` returns a synthetic window; other methods use generated rejection stubs and events are not emitted. |

## Per-API implementation notes

The v2 bridge creates API facades from the calling context's security profile and
the extension's manifest permissions. Native promises and event registrations
carry the runtime-owned extension identity, web view, script world, and security
profile; JavaScript arguments cannot select a different identity. Runtime state
described as "in memory" below is scoped to one `WebExtensionsRuntimeImpl`
(profile/private-mode sandbox) unless noted otherwise.

### `action`

- **Implemented:** `setBadgeBackgroundColor`, `setBadgeText`, and `setIcon`
  accept the declared Promise and callback forms and resolve successfully.
- **Gotchas:** the values are discarded. There is no per-tab/per-window action
  state, icon decoding, toolbar update, or action click event in this v2 IDL.
- **TODO:** connect mutations to Orion's toolbar/action model, add state/query
  methods and click events, and honor tab/window targeting.

### `alarms`

- **Implemented:** named and unnamed alarms support absolute time, delay,
  repetition, lookup, clearing, and extension-scoped `onAlarm` delivery.
  Replacing or clearing an alarm invalidates its pending timer safely.
- **Gotchas:** all state is in memory. `persistAcrossSessions` is reported but
  does not persist anything, browser restarts lose every alarm, and `clearAll`
  currently returns `true` even when the extension had no alarms. Browser
  minimum-delay and throttling policies are not enforced.
- **TODO:** persist eligible alarms, restore/reschedule them during runtime
  startup, align return values and timing constraints with the target browser,
  and define behavior for suspend/resume and system clock changes.

### `contextMenus`

- **Implemented:** the current Chrome `contextMenus` and Firefox
  `contextMenus`/`menus` surfaces are declared, including Firefox lifecycle,
  override, refresh, metadata, and target-element members. CRUD maintains an
  extension/runtime-scoped normalized model with duplicate/unknown-ID errors,
  parent validation, descendant removal, default values, and checked-state
  updates. Complete snapshots are forwarded by `bridge::ContextMenuBridge` to
  its attached `bridge::ContextMenuHost`. Application click, shown, and hidden
  signals enter core through `ContextMenuBridge` methods and dispatch
  extension-scoped events with tab/context data. Realm-local
  `onclick` handlers are driven from the same click event.
- **Implemented (GTK):** runtime snapshots are scoped by profile/private mode and
  projected into WebKit page menus with hierarchy, separators, enabled/visible
  and checked state, context/view/match-pattern filtering, and extension grouping.
  Click, shown, and hidden lifecycle callbacks are connected, and Firefox
  one-shot `overrideContext` is applied to the invoking WebView.
- **Gotchas:** WebKit's public GTK hit-test API does not expose selected text,
  frame identity, target-element tokens, or an explicit audio/video distinction.
  Selection text is therefore omitted, the top frame is reported, and media type
  is inferred from common audio URL extensions. WebKitGTK also copies and clears
  its public menu model immediately after the context-menu signal. `refresh` is
  accepted and updates already feed the next projection, but it cannot redraw
  an already-open native menu. Context-menu item icons and command execution are
  not exposed by the GTK projection; command items still dispatch click events.
  Generated numeric IDs must be returned synchronously, so the shim uses a
  realm-randomized negative sequence while native duplicate checking remains
  authoritative.
- **TODO:** expose WebKit target-token/selection metadata and a live GTK menu
  model hook so Firefox `getTargetElement`, selection substitution, exact
  frame/media data, and in-place `refresh` can be implemented.

### `extension`

- **Implemented:** `id`, `lastError`, `inIncognitoContext`, `getURL`, legacy
  `sendRequest`/request events, and basic incognito/file access queries are
  available. Legacy requests route through `runtime` messaging.
- **Gotchas:** `getViews` can only return the caller's own `window`; live window
  objects cannot cross the JSON bridge. `getBackgroundPage` only returns the
  current window for service-worker emulation. `isAllowedIncognitoAccess`
  describes whether this runtime is private rather than the extension's granted
  incognito policy, file-scheme access is always `false`, and
  `setUpdateUrlData` is accepted but discarded.
- **TODO:** add a safe cross-context view/background-page representation or
  same-process proxy, consult actual incognito and file-access grants, normalize
  URL construction, and implement update-URL data if Orion consumes it.

### `i18n`

- **Implemented:** the extension's default-locale `messages.json` is embedded in
  its facade. Case-insensitive lookup, predefined `@@` messages, positional and
  named placeholders, substitutions, `$$`, bidi values, and Firefox `escapeLt`
  are handled.
- **Gotchas:** locale selection comes from the extension's `default_locale`, not
  Orion's current UI locale. There is no locale fallback chain.
  `getAcceptLanguages` and `getPreferredSystemLanguages` always return
  `en-US`; `detectLanguage` always reports an unreliable `en-US` result.
- **TODO:** use browser/OS language preferences, implement locale negotiation
  and fallback, load the selected locale rather than only the default, and add a
  real language detector.

### `notifications`

- **Implemented:** create / update (supplied fields merged) / clear / getAll
  (notification-id -> `true` map) / getPermissionLevel (`granted`). Display goes
  through the host's `orion::NotificationProvider` (`core/notification_provider.h`,
  set with `CoreApi::set_notification_provider`); extension-package and `data:`
  icon, image, and button-icon URLs are written under
  `<data dir>/notifications/<extension>/` so hosts only see file paths (http(s)
  URLs pass through). Host events from `CoreApi::notification_event()` drive
  `onShown`, `onClicked`, `onButtonClicked`, and `onClosed`; activation counts as
  dismissal (Closed with `byUser = true`) and `clear()` emits Closed with
  `byUser = false`.
- **Gotchas:** with no provider registered (the GTK app today) calls succeed but
  nothing is displayed and no events fire. `onPermissionLevelChanged` and
  `onShowSettings` are never emitted. Records disappear with the runtime and are
  not removed from the platform when an extension unloads.
- **TODO:** permission policy / user settings for extension notifications, a GTK
  provider (libnotify / GNotification), clear an extension's notifications on
  unload.

### `permissions`

- **Implemented:** `getAll` and `contains` derive named permissions,
  `host_permissions`, manifest origin permissions, content-script match origins,
  and Firefox required data-collection permissions from extension resources.
  Requests for permissions that are already active return `true`.
- **Gotchas:** no optional grant store or browser prompt exists, so requesting a
  new permission returns `false` and `remove` always returns `false`.
  `optional_permissions`/`optional_host_permissions` are not modeled. Origin
  containment only handles exact matches, `<all_urls>`, and a special
  `*://*/*` case; it is not a general match-pattern comparison. `onAdded` and
  `onRemoved` are never emitted, and host-access request methods reject as
  unimplemented.
- **TODO:** consume the shared parsed manifest model instead of reparsing a
  private subset, add optional declarations and a persisted per-profile grant
  store, implement prompts/revocation/events, and use the canonical match-pattern
  engine for origin containment and host-access requests.

### `runtime`

- **Implemented:** static identity/manifest/version/URL helpers, local and
  cross-extension one-shot messaging, callback/Promise/send-response handling,
  extension ports, startup replay for buffered port connections, pending port
  messages, disconnects, and extension reload are wired. Sender IDs, URLs, and
  origins are created natively. Reload clears `storage.session` before replacing
  the extension runtime.
- **Gotchas:** `getFrameId` always returns `0`; sender objects do not yet include
  tab, frame, document, TLS-channel, or lifecycle fields. Port and pending reply
  state is memory-only and has no timeout or complete context-destruction cleanup.
  Native messaging is intentionally rejected (`connectNative` is unimplemented;
  `sendNativeMessage` is forbidden). `setUninstallURL` is a no-op. Lifecycle,
  update, restart, platform, package-directory, context enumeration, browser
  information, options-page, native-connect, and performance events either use
  generated default rejection or an empty event facade.
- **TODO:** add lifecycle/context teardown hooks, bounded pending-message
  lifetimes, richer sender metadata, the remaining browser/platform queries and
  events, options-page/update behavior, and a permission-checked native-messaging
  host implementation.

### `scripting`

- **Implemented:** the full Chrome/Firefox v2 facade, overload resolution,
  `scripting` permission filtering, execution-context filtering, typed native
  boundary, and runtime implementation registration are wired.
- **Gotchas:** the native class is only a harness. All operations use the
  generated default rejection stubs; scripts and styles are not injected, and
  registered content scripts are not stored or applied.
- **TODO:** implement host-access enforcement, programmatic script/style
  injection, extension-scoped registration state, validation, filters,
  persistence, and content-script installation/removal.

### `storage`

- **Implemented:** Promise and callback forms of `get`, `getKeys`,
  `getBytesInUse`, `set`, `remove`, `clear`, and `setAccessLevel` work for all
  four areas. Values are JSON-serialized; defaults, empty arrays, missing keys,
  and empty-string keys are supported. Mutations and change calculation are
  atomic. Per-area and global `onChanged` events are extension-scoped and are
  filtered for content scripts according to the persisted access level.
  `session` uses a dedicated `:memory:` database and is cleared on disable,
  resource update, reload, uninstall, and database shutdown.
- **Quotas:** local enforces 10 MiB unless the native manifest lookup finds
  `unlimitedStorage`; sync enforces 100 KiB total, 8 KiB per item, 512 items,
  120 mutations/minute, and 1,800 mutations/hour; session enforces 10 MiB.
  Size mutations are evaluated atomically against the post-write state.
- **Gotchas:** `sync` is only a separate local namespace; it does not synchronize
  between devices. `managed` is read-only but has no enterprise policy provider.
  Byte accounting is key UTF-8 bytes plus stored JSON bytes for every writable
  area; Chromium session storage instead estimates dynamically allocated memory.
  Managed byte usage is always zero. Sync rate counters are in-memory runtime
  state and reset when that runtime is recreated.
- **TODO:** add a sync transport/conflict model, populate managed storage from
  policy, implement Chromium-compatible session memory estimation (or document
  the intentional shared logical metric), and decide whether sync rate history
  must survive runtime recreation.

### `tabs`

- **Implemented:** `getCurrent` returns the calling web view's tab ID and URL
  when it is attached to a tab. `query` resolves the web view's active tab or the
  selected window's active tab through `CoreApi`, returning a useful subset of
  the common `tabs.Tab` fields.
- **Gotchas:** `queryInfo` is ignored, so every query effectively means "active
  tab for this context/profile". `getCurrent` is especially sparse, and query
  results hard-code `incognito=false` and `status=complete`. URL/title fields are
  not redacted according to `tabs`, host, or `activeTab` permissions. Declared
  created/updated/removed/activated/highlighted/replaced events are never emitted.
- **TODO:** implement profile-bound tab enumeration and every declared filter,
  normalize complete tab objects and private-mode values, enforce sensitive-field
  permissions, and bridge Orion tab lifecycle changes to events.

### `webNavigation`

- **Implemented:** the API facade and listener registration surface exist.
  `getFrame` resolves `undefined` and `getAllFrames` resolves an empty array.
- **Gotchas:** no Orion navigation or frame-tree state is connected, so none of
  the declared navigation events fire and filters cannot have an effect.
- **TODO:** expose the WebKit frame/document lifecycle through stable tab/frame/
  document IDs, implement both frame queries, and dispatch committed/history/
  fragment/error/completion events with extension and permission filtering.

### `webRequest`

- **Implemented:** generated event objects retain extension-scoped listener
  registrations and their filter/extra-info arguments.
- **Gotchas:** the network stack never dispatches requests to those listeners.
  Observational listeners see nothing, and blocking/async-blocking responses
  cannot modify or cancel traffic. Several request-detail schemas remain generic.
- **TODO:** bridge request lifecycle stages from the network layer, apply URL/
  tab/type filters, populate stable request/frame/document metadata, enforce host
  permissions, implement blocking response aggregation, and complete schemas for
  every declared stage.

### `windows`

- `getCurrent` returns a synthetic minimal window. It uses web-view IDs when
  available and otherwise the profile's selected window, but ignores query options,
  does not load full tabs, and hard-codes focus/state/type/private-mode fields.
  The remaining methods use the v2 generator's rejection stubs, and window
  events are not emitted.
- **TODO:** return CoreApi-backed profile-bound window/tab objects, respect
  populate and window filters, report private mode correctly, implement window
  creation/update/removal, and bridge window lifecycle events.

## Internal test API

`browser.test` uses the v2 generator (`test.idlv2`) and requires the `debug`
permission. Its JavaScript helpers are injected only for extensions with that
permission. This is internal extension-test scaffolding, not a production
compatibility API; exclude its helpers and events from feature completeness claims.

## Legacy IDL surfaces still in the runtime

These APIs still use the v1 generator and therefore do not benefit from v2's
declarative security-profile and permission filtering. Transitioning them to
IDL v2 is part of completing the public API surface.

### `browserAction` and `pageAction`

- The legacy browser/page-action schemas expose event placeholders only. The
  runtime includes `pageAction` but does not dispatch clicks; `browserAction` is
  not included in the active implementation list. New work should target the v2
  `action` API instead of extending these shims.
