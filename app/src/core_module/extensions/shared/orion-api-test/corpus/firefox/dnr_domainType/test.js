"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async (msg, ...args) => {
          let result;
          let createRejectionHandler = apiName => err => {
            browser.test.fail(`Got unexpected error from ${apiName}: ${err}`);
          };
          switch (msg) {
            case "updateSessionRules": {
              await browser.declarativeNetRequest
                .updateSessionRules(args[0])
                .catch(createRejectionHandler("updateSessionRules"));
              break;
            }
            default:
              browser.test.fail(`Got unexpected test message: ${msg}`);
          }
          browser.test.sendMessage(`${msg}:done`, result);
        });
        browser.test.sendMessage("bg:ready");
      
  },
]);
