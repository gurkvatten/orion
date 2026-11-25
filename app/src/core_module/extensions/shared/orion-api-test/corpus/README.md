# Corpus test suites

Extension API tests copied from other browsers' test suites, run inside
APITestExtension through the `chrome.test`/`browser.test` harness
(`TestHarness`, injected natively by the core module's `test_extern.js`).
Most are expected to FAIL — that is the point: the corpus is a running TODO
list for Orion's `browser.*` API implementation.

## Layout

- `chromium/<api>/<suite>/…` — verbatim background scripts from Chromium's
  `chrome/test/data/extensions/api_test` (BSD). Seeded by
  `scripts/seed_corpus_tests.py` from the vendored Chromium tree.
- `firefox/<api>/test.js` — `background()` bodies extracted from
  mozilla-central xpcshell tests (MPL-2.0). Seeded by
  `scripts/fetch_firefox_tests.py` (network). See `firefox/README.md`.
- `index.json` — generated catalog of every imported suite (id, group, api,
  scripts, upstream manifest version, `nativeCoop` flag). The launcher
  (`../popup.html`) and runner (`../runner.html`) read it via
  `catalog.js::loadCatalog()`, which merges it with the static built-in
  entry.

## These are the source of truth

The copied/extracted files are committed and meant to be hand-adjusted as
needed. The seed scripts are one-time importers: re-running them OVERWRITES
`chromium/`+`firefox/` and regenerates `index.json`, discarding manual edits.
`build.py.cmd` does not run them — it only packages `src/` as-is.

## Re-importing

From the repo root:

    uv run python scripts/seed_corpus_tests.py --chromium     # local, fast
    uv run python scripts/fetch_firefox_tests.py              # network

## What is imported

- Only suites with a runnable background entry point (background scripts /
  service worker, or an extracted `background()` body). Page/tab/content-script
  driven suites are skipped — the in-page harness can't drive them.
- `nativeCoop`-flagged suites need a native test server (`getConfig().testServer`)
  or a C++ message peer (`sendMessage`); they are imported anyway and fail
  visibly. `sendMessage` is native but only echoes back through `onMessage`
  (no real peer); `getConfig` is stubbed.
