"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            async details => {
              browser.test.assertEq(
                "firefox-container-2",
                details.cookieStoreId,
                "cookieStoreId is set"
              );
              browser.test.notifyPass("allowed");
            },
            { urls: ["http://example.com/dummy"] }
          );
        
  },
]);
