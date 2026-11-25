"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.runtime.onMessage.addListener(msg => {
          if (msg.name === "image-loading") {
            browser.test.assertTrue(msg.success, `Image was ${msg.expectedAction}`);
            browser.test.sendMessage(`image-${msg.expectedAction}`);
          } else {
            browser.test.sendMessage(msg);
          }
        });
    
        browser.test.sendMessage("background-ready");
      
  },
]);
