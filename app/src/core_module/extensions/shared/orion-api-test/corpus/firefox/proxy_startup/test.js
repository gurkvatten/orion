"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.proxy.onRequest.addListener(
          details => {
            // ignore speculative requests
            if (details.type == "xmlhttprequest") {
              browser.test.sendMessage("saw-request");
            }
            return proxyInfo;
          },
          { urls: ["<all_urls>"] }
        );
      
  },
  async function part_2() {
        browser.webRequest.onBeforeRequest.addListener(
          () => {
            return { redirectUrl: "data:,response_from_webRequest" };
          },
          {
            urls: ["*://example.com/wr"],
            types: ["xmlhttprequest"],
          },
          ["blocking"]
        );
        browser.proxy.onRequest.addListener(
          () => {
            browser.test.sendMessage("seen_proxy_request");
          },
          {
            urls: ["*://example.com/?proxy"],
            types: ["xmlhttprequest"],
          }
        );
      
  },
]);
