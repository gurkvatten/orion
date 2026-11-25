"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
            browser.test.onMessage.addListener(async (msg, args) => {
              if (msg !== "async-method-call") {
                browser.test.fail(`Unexpected test message: ${msg}`);
                return;
              }
    
              try {
                let result = await browser.testMockAPI.anAsyncAPIMethod(...args);
                browser.test.sendMessage("async-method-call:result", result);
              } catch (err) {
                browser.test.sendMessage("async-method-call:error", err.message);
              }
            });
          
  },
]);
