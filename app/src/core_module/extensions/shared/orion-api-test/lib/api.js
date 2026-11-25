"use strict";

// Flavor-native API root for the built-in suites.
//
// The Firefox flavor (moz-extension://) talks to `browser.*` and expects every
// async method to return a promise. The Chrome flavor (chrome-extension://)
// talks to `chrome.*` and uses the callback convention: the result arrives as
// the callback argument and a failure is reported by `chrome.runtime.lastError`
// being set for the duration of the callback.
//
// Tests are written once against `api` (the flavor's root) and `call(ns,
// "method", ...args)`, which returns a promise in both flavors:
//   Firefox: `ns.method(...args)` — the promise the API returns.
//   Chrome:  `ns.method(...args, callback)` — resolved with the callback's
//            argument, rejected with `runtime.lastError.message` when set.
// A synchronous throw (argument validation) rejects the promise in both cases.
//
// Synchronous APIs (`runtime.id`, `runtime.getManifest()`, `i18n.getMessage()`,
// event `addListener`s, ...) are the same in both conventions: call them on
// `api.*` directly.
//
// Orion installs both `browser.*` and `chrome.*` in every flavor, so this is a
// choice of convention, not availability. The corpora (Chromium's and
// Firefox's own tests) are not routed through here: each already targets its
// browser's native convention.

const OrionApi = (() => {
    const flavor = (window.OrionEnv && window.OrionEnv.flavor) ||
        (location.protocol === "chrome-extension:" ? "chrome" : "firefox");
    const isChrome = flavor === "chrome";
    const root = isChrome
        ? chrome
        : (typeof browser !== "undefined" ? browser : chrome);

    function call(namespace, method, ...args) {
        if (!isChrome) {
            return namespace[method](...args);
        }
        return new Promise((resolve, reject) => {
            namespace[method](...args, (result) => {
                const err = chrome.runtime.lastError;
                if (err) reject(new Error(err.message || String(err)));
                else resolve(result);
            });
        });
    }

    return { flavor, isChrome, root, call };
})();

const api = OrionApi.root;
const call = OrionApi.call;

if (typeof window !== "undefined") {
    window.OrionApi = OrionApi;
}
