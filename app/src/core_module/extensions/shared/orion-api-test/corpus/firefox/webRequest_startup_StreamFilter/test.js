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
            let filter = browser.webRequest.filterResponseData(details.requestId);
            filter.onstop = () => {
              let encoded = new TextEncoder().encode(data);
              filter.write(encoded);
              filter.close();
            };
          },
          { urls: ["http://example.com/data/file_sample.html"] },
          ["blocking"]
        );
      
  },
]);
