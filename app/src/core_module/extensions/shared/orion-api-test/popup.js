"use strict";

// Default page: runs the built-in Orion API suites inline (PASS/FAIL table),
// and shows links at the top to each corpus's aggregate run page (which runs
// ALL of that corpus's suites in one page). The flavor-matching corpus
// (Chromium for a chrome-extension build, Firefox for a moz build) is listed
// first; the other-flavor corpus is grayed out but still clickable.

document.addEventListener("DOMContentLoaded", async () => {
    const env = window.OrionEnv;
    const titleText = `Orion API Tests (${env.flavor})`;
    document.getElementById("page-title").textContent = titleText;
    document.title = titleText;
    document.getElementById("env").innerHTML =
        `flavor <code>${env.flavor}</code> · scheme <code>${env.scheme}</code> · ` +
        `manifest v<code>${env.manifestVersion}</code>`;

    // Corpus links (built from the catalog's groups, excluding the built-in one).
    const linksEl = document.getElementById("corpus-links");
    const catalog = await loadCatalog();
    const groups = new Map(); // group -> {count, matches}
    for (const s of catalog) {
        if (s.type === "builtin") continue;
        const g = groups.get(s.group) || { count: 0, matches: false };
        g.count++;
        if (s.flavors.includes(env.flavor)) g.matches = true;
        groups.set(s.group, g);
    }
    const ordered = [...groups.entries()].sort((a, b) =>
        (b[1].matches - a[1].matches) || a[0].localeCompare(b[0]));

    const ul = document.createElement("ul");
    ul.className = "suite-list";
    for (const [group, info] of ordered) {
        const li = document.createElement("li");
        const a = document.createElement("a");
        a.href = `aggregate.html?group=${encodeURIComponent(group)}`;
        a.textContent = `▶ ${group} Tests`;
        // Corpora for the other flavor (Firefox tests in a chrome build, and
        // vice versa) are grayed out but still runnable.
        if (!info.matches) a.classList.add("muted");
        li.appendChild(a);
        const meta = document.createElement("span");
        meta.className = "meta";
        meta.textContent = `${info.count} suites` + (info.matches ? " · this build" : " · other flavor");
        li.appendChild(meta);
        ul.appendChild(li);
    }
    linksEl.appendChild(ul);

    // Built-in suites (scripts already loaded by popup.html).
    const tbody = document.querySelector("#results tbody");
    const summary = document.getElementById("summary");
    const results = await TestRunner.run();
    window.__testResults = results;
    // Built-in test files are named after the first segment of the suite name
    // (e.g. suites "storage.local" and "storage.session" both live in
    // tests/storage.js; "runtime" -> tests/runtime.js).
    renderResults(results, tbody, summary, (r) => {
        const file = "tests/" + r.suite.split(".")[0] + ".js";
        return { suiteId: "builtin/" + r.suite, scripts: [file], title: "Built-in: " + r.suite };
    });
});
