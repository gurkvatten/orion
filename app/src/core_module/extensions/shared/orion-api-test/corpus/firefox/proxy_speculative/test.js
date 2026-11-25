"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        // Handle the proxy request.
        browser.proxy.onRequest.addListener(
          details => {
            browser.test.log(`onRequest ${JSON.stringify(details)}`);
            browser.test.assertEq(
              details.type,
              "speculative",
              "Should have seen a speculative proxy request."
            );
            return [{ type: "direct" }];
          },
          { urls: ["<all_urls>"], types: ["speculative"] }
        );
      
  },
]);
