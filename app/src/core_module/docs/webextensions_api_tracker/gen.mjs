// One-time generator: BCD webextensions.api -> api-data.js for the tracker.
import { readFileSync, writeFileSync } from 'node:fs';

const bcd = JSON.parse(readFileSync('./bcd.json', 'utf8'));
const api = bcd.webextensions.api;
const BROWSERS = ['chrome', 'edge', 'firefox', 'firefox_android', 'opera', 'safari', 'safari_ios'];

function supportCell(stmt) {
    if (stmt == null) return { v: '?' };
    if (Array.isArray(stmt)) stmt = stmt[0]; // primary statement
    let v = stmt.version_added;
    if (v === true) v = 'Yes';
    else if (v === false || v == null) v = 'No';
    if (stmt.version_removed) v = `${v}–${stmt.version_removed === true ? '' : stmt.version_removed}`;
    const cell = { v: String(v) };
    if (stmt.partial_implementation) cell.p = 1;
    return cell;
}

function matrix(compat) {
    const out = {};
    for (const b of BROWSERS) out[b] = supportCell(compat?.support?.[b]);
    return out;
}

function kindOf(name) {
    if (/^on[A-Z]/.test(name)) return 'event';
    if (/^[A-Z]/.test(name)) return 'type';
    return '';
}

function walk(node, path, depth, items) {
    const keys = Object.keys(node).filter(k => k !== '__compat').sort((a, b) => a.localeCompare(b));
    for (const key of keys) {
        const child = node[key];
        if (typeof child !== 'object' || child === null) continue;
        const id = `${path}.${key}`;
        items.push({
            id,
            name: key,
            depth,
            kind: depth === 0 ? kindOf(key) : '',
            mdnUrl: child.__compat?.mdn_url || null,
            support: matrix(child.__compat),
        });
        walk(child, id, depth + 1, items);
    }
}

const namespaces = [];
for (const ns of Object.keys(api).filter(k => k !== '__compat').sort((a, b) => a.localeCompare(b))) {
    const node = api[ns];
    const items = [];
    walk(node, ns, 0, items);
    namespaces.push({
        name: ns,
        mdnUrl: node.__compat?.mdn_url
            || `https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/${ns}`,
        support: matrix(node.__compat),
        items,
    });
}

const data = {
    generated: new Date().toISOString().slice(0, 10),
    bcdVersion: bcd.__meta.version,
    browsers: BROWSERS,
    namespaces,
};

writeFileSync('./api-data.js', 'window.API_DATA = ' + JSON.stringify(data) + ';\n');
const total = namespaces.reduce((n, ns) => n + ns.items.length, 0);
console.log(`namespaces: ${namespaces.length}, items: ${total}, bytes: ${JSON.stringify(data).length}`);
