"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async msg => {
            if (msg !== "startTest") {
              return;
            }
            try {
              let res = await browser.runtime.sendMessage("msg_from_bg");
              browser.test.assertEq(0, res, "Result from onMessage");
              browser.test.notifyPass("handled_onMessage");
            } catch (e) {
              browser.test.fail(`Unexpected error: ${e.message} :: ${e.stack}`);
              browser.test.notifyFail("handled_onMessage");
            }
          });
        
  },
]);
