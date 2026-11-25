"use strict";

// Background script for tests that need a persistent background context.
// The popup's runtime.sendMessage ping/pong test hits this listener.

browser.runtime.onMessage.addListener((message, sender) => {
    if (message && message.type === "ping") {
        return Promise.resolve({ type: "pong" });
    }
});
