"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              browser.test.assertEq(
                "ws:",
                new URL(details.url).protocol,
                "ws protocol worked"
              );
              browser.test.notifyPass("websocket");
            },
            { urls: ["ws://example.com/*"] },
            ["blocking"]
          );
    
          browser.test.onMessage.addListener(() => {
            let ws = new WebSocket("ws://example.com/dummy");
            ws.onopen = () => {
              ws.send("data");
            };
            ws.onclose = () => {};
            ws.onerror = () => {};
            ws.onmessage = () => {
              ws.close();
            };
          });
          browser.test.sendMessage("ready");
        
  },
]);
