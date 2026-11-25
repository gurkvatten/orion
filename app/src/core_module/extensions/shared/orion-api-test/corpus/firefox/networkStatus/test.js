"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.networkStatus.onConnectionChanged.addListener(async details => {
            browser.test.log(`connection status ${JSON.stringify(details)}`);
            browser.test.sendMessage("connect-changed", {
              details,
              linkInfo: await browser.networkStatus.getLinkInfo(),
            });
          });
          browser.test.sendMessage(
            "linkdata",
            await browser.networkStatus.getLinkInfo()
          );
        
  },
]);
