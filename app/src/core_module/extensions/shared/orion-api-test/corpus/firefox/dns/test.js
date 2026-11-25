"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async (msg, data) => {
            if (msg == "proxy") {
              await browser.proxy.settings.set({ value: data });
              browser.test.sendMessage("proxied");
              return;
            }
            browser.test.log(`=== dns resolve test ${JSON.stringify(data)}`);
            browser.dns
              .resolve(data.hostname, data.flags)
              .then(result => {
                browser.test.log(
                  `=== dns resolve result ${JSON.stringify(result)}`
                );
                browser.test.sendMessage("resolved", result);
              })
              .catch(e => {
                browser.test.log(`=== dns resolve error ${e.message}`);
                browser.test.sendMessage("resolved", { message: e.message });
              });
          });
          browser.test.sendMessage("ready");
        
  },
]);
