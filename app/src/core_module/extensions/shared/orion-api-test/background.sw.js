"use strict";

// MV3 service-worker background (Chrome flavor). Mirrors background.js (the
// MV2 persistent background of the Firefox flavor): a ping/pong listener for
// the runtime messaging test, written against chrome.* like the rest of the
// Chrome flavor. Returning a promise from an onMessage listener is the
// promise-style reply Orion's bridge supports in both flavors.

chrome.runtime.onMessage.addListener((message, sender) => {
    if (message && message.type === "ping") {
        return Promise.resolve({ type: "pong" });
    }
});
