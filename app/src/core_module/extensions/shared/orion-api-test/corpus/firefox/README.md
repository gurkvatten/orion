# Firefox extension test corpus

Imported from mozilla-central `toolkit/components/extensions/test/xpcshell/`
(via github.com/mozilla/gecko-dev) by `scripts/fetch_firefox_tests.py`. Each
`test_ext_<api>.js` there embeds the extension in the Gecko harness
(`ExtensionTestUtils.loadExtension`); the fetcher extracts the `background`
function body — portable `browser.test` code — wraps it in a self-running async
IIFE, and writes it to `firefox/<api>/test.js`. Entries are added to
`../index.json` with `flavors: ["firefox"]`.

Scope and caveats (this is a best-effort extraction, not a full port):

- The extractor matches named `function background()`/`backgroundScript()`
  declarations plus inline `background:` function/arrow/method forms, and
  extracts EVERY background in a file (~213 of ~322 files, ~458 bodies) — a
  file's `add_task()`s each load their own little extension, so the bodies are
  sequenced as parts of one `browser.test.runTests()` call. Each generated
  file is validated with `node --check` before landing; files whose extraction
  doesn't parse are dropped (or reduced to their parseable parts).
- Bodies reference only what was inside the background function. Helpers or
  constants defined at module scope in the original test are NOT included, so
  some failures are extraction artifacts (ReferenceError) rather than real API
  gaps. Treat the corpus as a TODO signal, not a precise conformance score.
- `nativeCoop`-flagged suites use `sendMessage`/`onMessage`/`awaitMessage`
  handshakes with the (absent) test-file peer and will fail standalone.

To widen coverage, improve the extractor's pattern set in
`scripts/fetch_firefox_tests.py`, or hand-fix individual `firefox/<api>/test.js`
files (they are the committed source of truth; re-running the fetcher
overwrites them).
