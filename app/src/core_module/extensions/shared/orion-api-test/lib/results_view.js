"use strict";

// Rendering for results of shape {suite, name, pass, error, ms, stack}.
//
// appendResultRow(r, tbody, opts) adds one row. When opts = {suiteId, scripts,
// title} is given:
//   - the SUITE cell links to source.html (the test's JS source);
//   - the DETAILS cell shows the error message, reveals the full stack trace on
//     hover, and on click opens source.html with the stack shown.
// Without opts (e.g. no source known) the cells are plain text.
//
// renderResults(results, tbody, summary, optsFor) fills a table in one shot;
// optsFor(r) -> opts is optional.

// ---- shared floating tooltip ----------------------------------------------
let _tip = null;
function tipEl() {
    if (!_tip) {
        _tip = document.createElement("div");
        _tip.className = "tip";
        _tip.style.display = "none";
        document.body.appendChild(_tip);
    }
    return _tip;
}
function positionTip(t, x, y) {
    const pad = 14;
    const w = t.offsetWidth;
    const h = t.offsetHeight;
    let left = x + pad;
    let top = y + pad;
    if (left + w > window.innerWidth - 8) left = Math.max(8, x - w - pad);
    if (top + h > window.innerHeight - 8) top = Math.max(8, y - h - pad);
    t.style.left = left + "px";
    t.style.top = top + "px";
}
function showTip(html, x, y) {
    const t = tipEl();
    t.innerHTML = html;
    t.style.display = "block";
    positionTip(t, x, y);
}
function hideTip() {
    if (_tip) _tip.style.display = "none";
}
// Attach a hover tooltip whose content comes from provider() (string or
// Promise<string>); results are cached on the element after first load.
function attachHoverHtml(el, provider) {
    let cached = null;
    el.addEventListener("mouseenter", async (e) => {
        if (cached !== null) { showTip(cached, e.clientX, e.clientY); return; }
        const r = provider();
        if (r && typeof r.then === "function") {
            showTip("<em>loading…</em>", e.clientX, e.clientY);
            cached = await r;
            if (_tip && _tip.style.display === "block") showTip(cached, e.clientX, e.clientY);
        } else {
            cached = r;
            showTip(cached, e.clientX, e.clientY);
        }
    });
    el.addEventListener("mousemove", (e) => {
        if (_tip && _tip.style.display === "block") positionTip(_tip, e.clientX, e.clientY);
    });
    el.addEventListener("mouseleave", hideTip);
}

function stashDetail(detail) {
    try {
        sessionStorage.setItem("orion-detail", JSON.stringify(detail));
    } catch (e) { /* private mode etc. */ }
}

// ---- rows ------------------------------------------------------------------
function appendResultRow(r, tbody, opts) {
    const row = document.createElement("tr");
    row.className = r.pass ? "pass" : "fail";

    const suiteCell = document.createElement("td");
    const nameCell = document.createElement("td");
    nameCell.textContent = r.name;
    const resultCell = document.createElement("td");
    resultCell.textContent = r.pass ? "PASS" : "FAIL";
    const errorCell = document.createElement("td");
    errorCell.className = "error";
    errorCell.textContent = r.error || "";
    const timeCell = document.createElement("td");
    timeCell.className = "ms";
    timeCell.textContent = (r.ms ?? 0) + " ms";

    if (opts && opts.scripts && opts.scripts.length) {
        const detail = {
            suiteId: opts.suiteId,
            title: opts.title || r.suite,
            scripts: opts.scripts,
            test: r.name,
            error: r.error || "",
            stack: r.stack || "",
        };
        const href = `source.html?suite=${encodeURIComponent(opts.suiteId || "")}`;

        // SUITE cell: links to the full source page (no hover preview — the
        // file header is mostly comments/boilerplate, not the test body).
        const a = document.createElement("a");
        a.className = "suite-link";
        a.href = href;
        a.textContent = r.suite;
        a.title = "View test source";
        a.addEventListener("click", () => stashDetail(detail));
        suiteCell.appendChild(a);

        // DETAILS cell: stack on hover, click to open the source+stack page.
        if (r.error || r.stack) {
            errorCell.classList.add("clickable");
            errorCell.title = "";
            errorCell.addEventListener("click", () => {
                stashDetail(detail);
                location.href = href;
            });
            if (r.stack) {
                attachHoverHtml(errorCell, () =>
                    `<div class="tip-title">Stack trace</div><pre class="code">${window.escapeHtml(r.stack)}</pre>`);
            }
        }
    } else {
        suiteCell.textContent = r.suite;
        errorCell.title = r.error || "";
    }

    row.append(suiteCell, nameCell, resultCell, errorCell, timeCell);
    tbody.appendChild(row);
    return r.pass;
}

function setSummary(summary, passed, failed, total, extra) {
    summary.textContent = `${passed} passed, ${failed} failed, ${total} total` +
        (extra ? ` · ${extra}` : "");
    summary.className = failed > 0 ? "fail" : "pass";
}

function renderResults(results, tbody, summary, optsFor) {
    let passed = 0;
    let failed = 0;
    for (const r of results) {
        const opts = optsFor ? optsFor(r) : null;
        if (appendResultRow(r, tbody, opts)) passed++;
        else failed++;
    }
    setSummary(summary, passed, failed, results.length);
}

if (typeof window !== "undefined") {
    window.appendResultRow = appendResultRow;
    window.setSummary = setSummary;
    window.renderResults = renderResults;
}
