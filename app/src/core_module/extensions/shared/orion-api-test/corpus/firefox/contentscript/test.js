"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.runtime.onMessage.addListener(
          ([msg, expectedStates, readyState]) => {
            if (msg == "chrome-namespace-ok") {
              browser.test.sendMessage(msg);
              return;
            }
    
            browser.test.assertEq("script-run", msg, "message type is correct");
            browser.test.assertTrue(
              expectedStates.includes(readyState),
              `readyState "${readyState}" is one of [${expectedStates}]`
            );
            browser.test.sendMessage("script-run-" + expectedStates[0]);
          }
        );
      
  },
]);
