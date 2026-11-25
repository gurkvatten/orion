"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async (msg, ...args) => {
            switch (msg) {
              case "registerContentScripts":
                await browser.scripting.registerContentScripts(...args);
                break;
              case "unregisterContentScripts":
                await browser.scripting.unregisterContentScripts(...args);
                break;
              default:
                browser.test.fail(`Unexpected test message: ${msg}`);
            }
            browser.test.sendMessage(`${msg}:done`);
          });
          browser.test.sendMessage("bgpage:ready");
        
  },
]);
