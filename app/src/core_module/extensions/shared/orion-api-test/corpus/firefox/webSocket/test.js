"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeSendHeaders.addListener(
            details => {
              let header = details.requestHeaders.find(h => h.name === "Origin");
              browser.test.sendMessage("ws_request", {
                ws_scheme: new URL(details.url).protocol,
                originHeader: header?.value,
              });
            },
            { urls: ["wss://example.com/*", "ws://example.com/*"] },
            ["requestHeaders", "blocking"]
          );
        
  },
]);
