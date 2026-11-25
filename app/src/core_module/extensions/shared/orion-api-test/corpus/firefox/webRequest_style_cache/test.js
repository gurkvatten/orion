"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const SHEET_URI = "http://example.com/data/file_stylesheet_cache.css";
          let firstFound = false;
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              browser.test.assertEq(
                details.url,
                firstFound ? SHEET_URI + "?2" : SHEET_URI
              );
              firstFound = true;
              browser.test.sendMessage("stylesheet found");
            },
            { urls: ["<all_urls>"], types: ["stylesheet"] },
            ["blocking"]
          );
        
  },
]);
