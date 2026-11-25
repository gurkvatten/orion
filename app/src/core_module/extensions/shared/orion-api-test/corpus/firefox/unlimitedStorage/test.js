"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async msg => {
          switch (msg) {
            case "set-storage":
              browser.test.log(`storing data in storage.local`);
              await browser.storage.local.set({ akey: "somevalue" });
              browser.test.log(`data stored in storage.local successfully`);
              break;
            case "has-storage": {
              browser.test.log(`checking data stored in storage.local`);
              const data = await browser.storage.local.get(["akey"]);
              browser.test.assertEq(
                data.akey,
                "somevalue",
                "Got storage.local data"
              );
              break;
            }
            default:
              browser.test.fail(`Unexpected test message: ${msg}`);
          }
    
          browser.test.sendMessage(`${msg}:done`);
        });
      
  },
]);
