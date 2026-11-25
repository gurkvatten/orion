"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async message => {
          let failure = true;
          try {
            await browser.cookies.set({
              ...message.cookie,
              url: "https://example.com",
            });
            failure = false;
          } catch (e) {
            browser.test.assertEq(
              e.message,
              message.errorString,
              `${message.title} - correct exception`
            );
          } finally {
            browser.test.assertEq(failure, message.failure, message.title);
            browser.test.sendMessage("completed");
          }
        });
    
        browser.test.sendMessage("ready");
      
  },
]);
