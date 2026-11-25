"use strict";

// Tiny self-contained JavaScript syntax highlighter (no external deps, so it
// works under the extension CSP). Returns HTML with <span class> wrappers;
// pair with the .tok-* rules in app.css. Good enough to skim what a test does,
// not a full parser.

(function () {
    const KEYWORDS = new Set([
        "const", "let", "var", "function", "return", "if", "else", "for", "while",
        "do", "switch", "case", "default", "break", "continue", "new", "typeof",
        "instanceof", "in", "of", "await", "async", "yield", "class", "extends",
        "super", "this", "try", "catch", "finally", "throw", "delete", "void",
        "null", "undefined", "true", "false",
    ]);

    function esc(s) {
        return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
    }

    // Order matters: comments and strings first so their contents aren't
    // re-tokenized, then numbers, then identifiers/keywords.
    const TOKEN = /(\/\/[^\n]*|\/\*[\s\S]*?\*\/)|(`(?:\\.|[^`\\])*`|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*')|(\b\d[\w.]*\b)|([A-Za-z_$][\w$]*)/g;

    function highlightJS(code) {
        let out = "";
        let last = 0;
        let m;
        TOKEN.lastIndex = 0;
        while ((m = TOKEN.exec(code)) !== null) {
            out += esc(code.slice(last, m.index));
            if (m[1]) out += `<span class="tok-c">${esc(m[1])}</span>`;
            else if (m[2]) out += `<span class="tok-s">${esc(m[2])}</span>`;
            else if (m[3]) out += `<span class="tok-n">${esc(m[3])}</span>`;
            else {
                const w = m[4];
                if (KEYWORDS.has(w)) out += `<span class="tok-k">${esc(w)}</span>`;
                else if (w === "browser" || w === "chrome") out += `<span class="tok-b">${esc(w)}</span>`;
                else out += esc(w);
            }
            last = TOKEN.lastIndex;
        }
        out += esc(code.slice(last));
        return out;
    }

    if (typeof window !== "undefined") {
        window.highlightJS = highlightJS;
        window.escapeHtml = esc;
    }
})();
