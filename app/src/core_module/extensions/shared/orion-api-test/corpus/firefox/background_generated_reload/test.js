"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          if (location.hash !== "#firstrun") {
            browser.test.sendMessage("first run");
            location.hash = "#firstrun";
            browser.test.assertEq("#firstrun", location.hash);
            location.reload();
          } else {
            browser.test.notifyPass("second run");
          }
        
  },
]);
