"use strict";

// Runs a single suite selected by ?suite=<id> and renders its results.
//   - builtin   : loads TestRunner + tests/*.js, awaits TestRunner.run()
//   - chrometest: loads chrome_test_shim.js + the suite's scripts (which call
//                 chrome.test/browser.test.runTests or notifyPass), awaits the
//                 shim's session promise
//
// Scripts are injected sequentially (same-origin extension resources, allowed
// under the MV3 script-src 'self' CSP; no inline/eval).

function loadScript(src) {
    return new Promise((resolve, reject) => {
        const el = document.createElement("script");
        el.src = src;
        el.onload = () => resolve();
        el.onerror = () => reject(new Error(`failed to load ${src}`));
        document.head.appendChild(el);
    });
}

function timeout(ms) {
    return new Promise((resolve) => setTimeout(() => resolve("__timeout__"), ms));
}

// When embedded in the aggregate page (?embed=1, inside a hidden iframe),
// stream each result row up to the parent as it lands, then signal completion.
// Streaming (rather than posting everything at the end) means the aggregate
// keeps partial results even if it has to time the suite out.
const EMBEDDED = new URLSearchParams(location.search).get("embed") === "1";
const SUITE_ID = new URLSearchParams(location.search).get("suite");

function postProgress(result) {
    if (EMBEDDED && window.parent !== window) {
        window.parent.postMessage({ type: "orion-suite-progress", suiteId: SUITE_ID, result }, "*");
    }
}

function postDone() {
    if (EMBEDDED && window.parent !== window) {
        window.parent.postMessage({ type: "orion-suite-done", suiteId: SUITE_ID }, "*");
    }
}

function reportEmbedded(suiteId, results) {
    for (const r of results || []) postProgress(r);
    postDone();
}

document.addEventListener("DOMContentLoaded", async () => {
    // In embed mode the iframe is hidden and results are streamed to the parent,
    // so there is no table/summary to fill (embed_runner.html omits them). These
    // are null then, and all rendering is skipped.
    const tbody = document.querySelector("#results tbody");
    const summary = document.getElementById("summary");
    const titleEl = document.getElementById("title");

    const params = new URLSearchParams(location.search);
    const id = params.get("suite");

    // The aggregate page passes the full entry inline (?entry=<json>) so we can
    // skip fetching/parsing the large corpus/index.json in every iframe. Fall
    // back to the catalog only for direct navigation (no entry param).
    let entry = null;
    const entryParam = params.get("entry");
    if (entryParam) {
        try { entry = JSON.parse(entryParam); } catch (e) { entry = null; }
    }
    if (!entry) {
        const catalog = await loadCatalog();
        entry = catalog.find((s) => s.id === id);
    }

    const optsFor = () => ({ suiteId: entry && entry.id, scripts: entry && entry.scripts, title: entry && entry.title });

    if (!entry) {
        if (titleEl) titleEl.textContent = "Unknown suite";
        if (summary) { summary.className = "fail"; summary.textContent = `No suite with id "${id}". Return to the suite list.`; }
        reportEmbedded(id, [{ suite: id, name: "(suite)", pass: false, error: "unknown suite id", ms: 0 }]);
        return;
    }

    if (titleEl) titleEl.textContent = entry.title;

    try {
        if (entry.type === "builtin") {
            for (const s of entry.scripts) await loadScript(s);
            const results = await TestRunner.run();
            window.__testResults = results;
            if (!EMBEDDED) renderResults(results, tbody, summary, optsFor);
            reportEmbedded(entry.id, results);
            return;
        }

        // chrometest: the native bridge injects test_extern.js at document
        // start, which creates window.TestHarness.  No shim to load.
        if (!window.TestHarness) {
            throw new Error("TestHarness not found — native bridge (test_extern.js) did not inject");
        }
        window.TestHarness.setSuite(entry.title);
        if (EMBEDDED) window.TestHarness.onResult = postProgress;

        // Capture a top-level throw from a corpus script so it becomes a
        // failing result instead of an unresolved session.
        window.addEventListener("error", (e) => {
            if (window.TestHarness) {
                const err = e.error;
                window.TestHarness.recordUncaught((err && err.message) || e.message || String(err), err && err.stack);
            }
        });
        window.addEventListener("unhandledrejection", (e) => {
            if (window.TestHarness) {
                const r = e.reason;
                window.TestHarness.recordUncaught((r && r.message) || String(r), r && r.stack);
            }
        });

        for (const s of entry.scripts) await loadScript(s);

        const outcome = await Promise.race([window.TestHarness.session, timeout(20000)]);
        if (outcome === "__timeout__") {
            window.TestHarness.recordUncaught("suite timed out after 20s (a test never called succeed/notifyPass)");
        }
        if (!EMBEDDED) renderResults(window.TestHarness.results, tbody, summary, optsFor);
        postDone(); // rows already streamed via onResult
    } catch (err) {
        if (summary) { summary.className = "fail"; summary.textContent = `Runner error: ${(err && err.message) || err}`; }
        reportEmbedded(entry.id, [{ suite: entry.title, name: "(suite)", pass: false, error: String((err && err.message) || err), ms: 0 }]);
    }
});
