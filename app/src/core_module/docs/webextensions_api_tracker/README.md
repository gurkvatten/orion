# WebExtensions API Tracker

Single-purpose issue tracker for the `browser.*` API port, mirroring MDN's
[Browser support for JavaScript APIs](https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Browser_support_for_JavaScript_APIs)
page. Pure static HTML — **open `index.html` directly in a browser** (`file://`
works, no server, no Node).

## Files

| File | What |
|---|---|
| `index.html` | The app (all CSS/JS inline) |
| `api-data.js` | The "scraped MDN" data: all namespaces/items + the browser-support matrix, generated from [`@mdn/browser-compat-data`](https://github.com/mdn/browser-compat-data) (the exact dataset MDN renders those tables from) |
| `gen.mjs` | Regenerates `api-data.js` (only needed to refresh the API list; requires Node: `curl -L -o bcd.json https://unpkg.com/@mdn/browser-compat-data/data.json && node gen.mjs`) |
| `status.json` | Your tracking state — created via Export / Link file (see below) |

## Usage

- Namespaces are collapsed; click a header to expand, or use **Expand** /
  **Collapse** in the top bar for all of them. Names link to MDN (new tab).
- Each header shows: item count, done % for **C**ore / **L**inux / **W**indows
  (out of items not marked Not Planned; Core-only items count as done for
  Linux/Windows once core is done), In Progress %, Not Planned %, and the
  assignee when the whole namespace is assigned to one person.
- Click a row to select it (replacing the selection); **Ctrl+click** toggles
  rows in and out, **Shift+click** selects a range, and clicking the
  **checkbox area** toggles without clearing the rest. The namespace
  checkbox selects/deselects every item in it. A toolbar appears with
  **Assign** / **Edit Note** and the selection count on the first row, and a
  **Mark:** row of **Core Done**, **Linux Done**, **Windows Done**,
  **In Progress**, **Core Only**, **Planned**, **Unplanned**. When everything
  selected already has a flag, the button flips to its "undo" form (e.g.
  *Core Not Done*, or *Undecided* for the plan buttons — clearing a plan
  returns the item to the default **Undecided** state; planned items carry
  no tag).
- Row backgrounds show state at a glance: green = done or not planned
  (resolved), yellow = in progress, light purple = undecided. Namespace
  headers use the same hues more intensely (neutral tint by default).
- **Assign** lists existing assignees; type a new name + Enter to add one,
  and the **Clear** button removes the assignee from the selection.
- **Notes**: with a single item selected — or a whole namespace selected via
  its checkbox — an **Add/Edit Note** button appears in the toolbar and opens
  a monospace editor (Ctrl+Enter saves, empty text removes the note).
  Existing notes show as a **Note** chip on the row / after the namespace's
  item counter (hover for a preview, click to edit).
- **Other Browsers** (top right) toggles the MDN browser-support columns
  (Chrome/Edge/Firefox/Firefox Android/Opera/Safari/Safari iOS; `*` =
  partial). Hover a column header or cell for the full browser name and what
  the value means.

## Where the state lives

On a browser that has never used the tracker, a help dialog explains all of
this on first launch, with shortcuts to link or import a file. The **?**
button in the top bar reopens it any time.

A `file://` page cannot silently read/write files in its folder, so:

- Every change is saved to **localStorage** immediately (survives reloads).
- **Undo / redo** (↶ ↷ in the top bar, or Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y)
  covers every status, assignment, note and import change. Selection and
  expanded namespaces are restored with it, so you can see what changed.
- **Copy Path & Export** saves `status.json`, **Copy Path & Import** loads one — use
  these to commit the file or share state between people/browsers. Both copy
  this folder's path to the clipboard and open a real save/open dialog on
  Chromium, so you can paste it into the *File name* field to write or read
  straight back here. Where there is no save-dialog API (Firefox, Safari),
  Export opens a panel with a download link instead: right-click it and
  choose *Save Link As…* for a real save dialog to paste the path into — a
  plain click just saves to your downloads folder. Import uses the normal
  file picker there, which already accepts a pasted path.
- A red **Unsaved Changes** tag appears in the top bar when edits have not
  reached a `status.json` yet. It is tracked against the undo history, so
  undoing your way back to the last saved point clears it again (and redoing
  brings it back). Exporting or importing marks the current point as saved;
  it never appears while a file is linked, since those changes auto-save. The
  flag survives reloads, so a pending change from an earlier session shows.
- **Link file** (Chromium only — the button is grayed out elsewhere, since
  the File System Access API doesn't exist in Firefox or Safari): binds a
  `status.json` on disk and
  auto-saves every change straight into it via the File System Access API.
  The button opens a short explainer with two choices; both copy this
  folder's path to the clipboard first, so you can paste it into the
  dialog's *File name* field to land in the right folder.
  - **Import & Link…** opens an existing `status.json`, loads it and links
    to it. If that would discard real local changes you get a choice —
    *Discard My Changes* (file wins, Ctrl+Z undoes it), *Overwrite File*
    (browser wins), or *Cancel* (nothing linked or written). On a page you
    just opened and haven't touched, the file is simply loaded. A file that
    isn't valid JSON is left alone and not linked.
  - **Export & Link…** writes the browser's current state into a new file
    and links to it. It shows a warning first, because the save picker
    **replaces** whatever file you point it at — the file's previous
    contents are never read. (This is also why importing must use the open
    picker: Chrome truncates a save-picked file the moment the dialog
    closes, before the page can read it.)
  The link **survives reloads** (the file handle is kept in IndexedDB) and
  reconnects by itself: either silently on load, or — when Chrome wants a
  user gesture first — on your next click anywhere on the page, which shows
  a one-time "let this site edit the file?" prompt. Refusing that prompt
  simply means "not linked": the tracker forgets the file and carries on
  with localStorage. On reconnect the file on disk wins, so edits made
  elsewhere are picked up.

Note: localStorage is per-origin — `file://` and `http://localhost` are
different origins, and Orion/WebKit vs Chrome each have their own. Use
Export/Import to move state across.

### status.json format

Written to be diff- and merge-friendly, so it can live in git:

```json
{
  "version": 1,
  "assignees": [
    "aki"
  ],
  "namespaces": {
    "alarms": {
      "note": "handled entirely by core"
    }
  },
  "items": {
    "tabs.create": {
      "assignee": "aki",
      "core": true,
      "coreOnly": false,
      "inProgress": false,
      "linux": false,
      "note": "",
      "plan": "",
      "windows": true
    }
  }
}
```

- **Every key is sorted**, so the same logical state always produces
  byte-identical output no matter what order things were clicked in — no
  spurious diffs from reordering.
- **Every field is written explicitly, one per line**, defaults included.
  Each entry is therefore a fixed block of lines, so an edit is always a
  *modified* line rather than an inserted one: two people changing different
  fields of the same item (one marks it Planned, the other assigns it)
  touch different lines and git merges both. Multi-line notes stay on one
  line via `\n` escapes.
- **Every namespace and item is always written**, untouched ones included
  (~16,900 lines / ~370 KB). The file therefore always has the same shape,
  so a change can only ever be a *modified* line — never an inserted or
  deleted one — which is what makes merges reliable. In memory the state is
  pruned back to non-default fields.
- Trailing commas are deliberately *not* used: they aren't legal JSON, and
  supporting them would mean shipping a JSON5 parser instead of using the
  browser's built-in `JSON.parse`. With fixed-size entries, new entries land
  between existing ones anyway, where a trailing comma wouldn't help.
