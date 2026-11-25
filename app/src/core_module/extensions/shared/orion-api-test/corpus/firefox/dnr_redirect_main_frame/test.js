"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { urlFilter: "from", resourceTypes: ["main_frame"] },
                action: { type: "redirect", redirect: { extensionPath: "/t.htm" } },
              },
            ],
          });
          browser.test.sendMessage("redirect_setup");
        
  },
]);
