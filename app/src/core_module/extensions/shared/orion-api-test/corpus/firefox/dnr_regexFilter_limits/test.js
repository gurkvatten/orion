"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async testName => {
          try {
            browser.test.log(`Starting backgroundFn for ${testName}`);
            await testCases.find(({ name }) => name === testName).backgroundFn();
          } catch (e) {
            browser.test.fail(`Unexpected error for ${testName}: ${e}`);
          }
          browser.test.log(`Completed backgroundFn for ${testName}`);
          browser.test.sendMessage(`${testName}:done`);
        });
        browser.test.sendMessage("background_started");
      
  },
]);
