"use strict";

// Runs every suite in a corpus group (?group=Chromium|Firefox) on one page.
// Each suite runs in a hidden iframe (runner.html?suite=..&embed=1) that
// isolates its globals and STREAMS each result row up via postMessage. A
// worker pool runs CONCURRENCY suites at once, so a suite stuck on a timeout
// only occupies its own worker — the other workers keep pulling later suites.
//
// Every suite gets a fixed <tbody> up front (in catalog order) with a grayed
// "queued" placeholder + spinner; the placeholder is replaced by that suite's
// real result rows when they arrive. Because each suite owns its tbody, running
// suites in parallel never scrambles the on-page order.
//
// The suite's catalog entry is passed to the iframe via the query string, so
// the runner does NOT re-fetch/parse the (large) corpus/index.json per iframe —
// that per-iframe cost is what made high concurrency expensive.

const CONCURRENCY = 16;
const IDLE_TIMEOUT_MS = 15000;

function suiteOpts(suite) {
    return { suiteId: suite.id, scripts: suite.scripts, title: suite.title };
}

function runSuiteInIframe(suite, framesEl, onRow, onTimeout) {
    return new Promise((resolve) => {
        let done = false;
        let timer = null;
        const iframe = document.createElement("iframe");

        function finish(timedOut) {
            if (done) return;
            done = true;
            clearTimeout(timer);
            window.removeEventListener("message", onMessage);
            iframe.remove();
            if (timedOut) {
                onRow({
                    suite: suite.title, name: "(suite)", pass: false,
                    error: `no activity for ${IDLE_TIMEOUT_MS / 1000}s — timed out (partial results kept)`,
                    ms: IDLE_TIMEOUT_MS,
                });
            }
            resolve();
        }
        function armTimer() {
            clearTimeout(timer);
            timer = setTimeout(() => finish(true), IDLE_TIMEOUT_MS);
        }
        function onMessage(e) {
            const d = e.data;
            if (!d || d.suiteId !== suite.id) return;
            if (d.type === "orion-suite-progress") {
                onRow(d.result);
                armTimer();
            } else if (d.type === "orion-suite-done") {
                finish(false);
            }
        }

        window.addEventListener("message", onMessage);
        armTimer();
        // Pass the entry inline so the iframe skips loadCatalog()/index.json.
        const entryParam = encodeURIComponent(JSON.stringify({
            id: suite.id, title: suite.title, type: suite.type, scripts: suite.scripts,
        }));
        iframe.src = `embed_runner.html?suite=${encodeURIComponent(suite.id)}&embed=1&entry=${entryParam}`;
        framesEl.appendChild(iframe);
    });
}

document.addEventListener("DOMContentLoaded", async () => {
    const group = new URLSearchParams(location.search).get("group") || "Chromium";
    const titleEl = document.getElementById("title");
    const summary = document.getElementById("summary");
    const table = document.getElementById("results");
    const framesEl = document.getElementById("frames");

    const catalog = await loadCatalog();
    const suites = catalog.filter((s) => s.group === group);
    titleEl.textContent = `${group} corpus — ${suites.length} suites`;

    if (suites.length === 0) {
        summary.className = "fail";
        summary.textContent = `No suites in group "${group}".`;
        return;
    }

    // Pre-render one <tbody> per suite, in order, each with a queued placeholder.
    const state = new Map(); // suiteId -> { tbody, statusRow, gotResult }
    for (const suite of suites) {
        const tb = document.createElement("tbody");
        const row = document.createElement("tr");
        row.className = "suite-status queued";

        const suiteCell = document.createElement("td");
        suiteCell.textContent = suite.title;
        const statusCell = document.createElement("td");
        statusCell.className = "suite-state";
        statusCell.textContent = "queued";
        const c3 = document.createElement("td");
        const c4 = document.createElement("td");
        const c5 = document.createElement("td");
        row.append(suiteCell, statusCell, c3, c4, c5);

        tb.appendChild(row);
        table.appendChild(tb);
        state.set(suite.id, { tbody: tb, statusRow: row, statusCell, gotResult: false });
    }

    let passed = 0;
    let failed = 0;
    let total = 0;
    let completedSuites = 0;

    function progress() {
        summary.className = "running";
        summary.textContent =
            `${completedSuites}/${suites.length} suites · ${passed} passed, ${failed} failed, ${total} tests`;
    }
    progress();

    function markRunning(suite) {
        const st = state.get(suite.id);
        st.statusRow.className = "suite-status running";
        st.statusCell.innerHTML = `<span class="spinner"></span>running…`;
    }
    function onRow(suite, r) {
        const st = state.get(suite.id);
        if (!st.gotResult) {
            st.gotResult = true;
            st.tbody.textContent = ""; // drop the placeholder row
        }
        if (appendResultRow(r, st.tbody, suiteOpts(suite))) passed++;
        else failed++;
        total++;
        progress();
    }
    function finishSuite(suite) {
        const st = state.get(suite.id);
        if (!st.gotResult) {
            st.statusRow.className = "suite-status";
            st.statusCell.textContent = "(no result)";
        }
        completedSuites++;
        progress();
    }

    let next = 0;
    async function worker() {
        while (next < suites.length) {
            const suite = suites[next++];
            markRunning(suite);
            await runSuiteInIframe(suite, framesEl, (r) => onRow(suite, r));
            finishSuite(suite);
        }
    }

    await Promise.all(Array.from({ length: Math.min(CONCURRENCY, suites.length) }, worker));

    setSummary(summary, passed, failed, total, `${suites.length} suites`);
});
