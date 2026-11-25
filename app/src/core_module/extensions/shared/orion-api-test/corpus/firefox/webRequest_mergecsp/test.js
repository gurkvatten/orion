"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let csp_value = undefined;
        browser.test.onMessage.addListener(function (msg) {
          csp_value = msg;
          browser.test.sendMessage("csp-set");
        });
        browser.webRequest.onHeadersReceived.addListener(
          e => {
            browser.test.log(`onHeadersReceived ${e.requestId} ${e.url}`);
            if (csp_value === undefined) {
              browser.test.assertTrue(false, "extension called before CSP was set");
            }
            if (csp_value !== null) {
              e.responseHeaders = e.responseHeaders.filter(
                i => i.name.toLowerCase() != "content-security-policy"
              );
              if (csp_value !== "") {
                e.responseHeaders.push({
                  name: "Content-Security-Policy",
                  value: csp_value,
                });
              }
            }
            return { responseHeaders: e.responseHeaders };
          },
          { urls: ["*://example.net/*"] },
          ["blocking", "responseHeaders"]
        );
      
  },
]);
