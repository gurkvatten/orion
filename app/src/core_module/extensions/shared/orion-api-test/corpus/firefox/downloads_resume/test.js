"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let dlid = 0;
      let expectedState;
      browser.test.onMessage.addListener(async msg => {
        try {
          if (msg.resume) {
            expectedState = "complete";
            await browser.downloads.resume(dlid);
          } else {
            expectedState = "interrupted";
            dlid = await browser.downloads.download({ url: msg.url });
          }
        } catch (err) {
          browser.test.fail(`Unexpected error in test.onMessage: ${err}`);
        }
      });
      browser.downloads.onChanged.addListener(({ id, state }) => {
        if (dlid !== id || !state || state.current !== expectedState) {
          return;
        }
    
        browser.downloads.search({ id }).then(([download]) => {
          browser.test.sendMessage(expectedState, download);
        });
      });
    
      browser.test.sendMessage("ready");
  },
]);
