"use strict";

// Shows a test suite's JS source (syntax-highlighted) and, when arrived at by
// clicking a failing result, the error + full stack trace for that test.
// Context comes from sessionStorage['orion-detail'] (set on click); falls back
// to resolving ?suite=<id> against the catalog.

function esc(s) {
    return window.escapeHtml ? window.escapeHtml(s) : String(s);
}

document.addEventListener("DOMContentLoaded", async () => {
    const titleEl = document.getElementById("title");
    const detailEl = document.getElementById("detail");
    const codeEl = document.getElementById("code");
    const suiteId = new URLSearchParams(location.search).get("suite");

    let detail = null;
    try {
        const raw = sessionStorage.getItem("orion-detail");
        if (raw) {
            const d = JSON.parse(raw);
            if (!suiteId || d.suiteId === suiteId) detail = d;
        }
    } catch (e) { /* ignore */ }

    let title = (detail && detail.title) || suiteId || "Test source";
    let scripts = detail && detail.scripts;

    if (!scripts) {
        // Fall back to the catalog for a directly-opened source page.
        try {
            const catalog = await loadCatalog();
            const entry = catalog.find((s) => s.id === suiteId);
            if (entry) { scripts = entry.scripts; title = entry.title; }
        } catch (e) { /* ignore */ }
    }

    titleEl.textContent = title;

    // Error + stack panel (only when opened from a failing result).
    if (detail && (detail.error || detail.stack)) {
        const parts = [`<div class="detail-test">Test: <code>${esc(detail.test || "")}</code></div>`];
        if (detail.error) parts.push(`<div class="detail-error">${esc(detail.error)}</div>`);
        if (detail.stack) parts.push(`<h2>Stack trace</h2><pre class="code block">${esc(detail.stack)}</pre>`);
        detailEl.innerHTML = parts.join("");
    }

    if (!scripts || !scripts.length) {
        codeEl.textContent = "(no source available for this suite)";
        return;
    }

    const chunks = [];
    for (const s of scripts) {
        try {
            const res = await fetch(s);
            chunks.push(`// ===== ${s} =====\n` + (res.ok ? await res.text() : `// (could not load ${s})`));
        } catch (e) {
            chunks.push(`// ===== ${s} =====\n// (error loading: ${e})`);
        }
    }
    const raw = chunks.join("\n\n");
    codeEl.innerHTML = window.highlightJS ? window.highlightJS(raw) : esc(raw);
});
