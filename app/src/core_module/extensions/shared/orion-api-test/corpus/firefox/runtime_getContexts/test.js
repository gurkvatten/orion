"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async (msg, ...args) => {
          if (msg === "runtime.getContexts") {
            try {
              const filter = args[0];
              if (!filter) {
                // Expected to be rejected.
                await browser.runtime.getContexts();
              } else {
                // Expected to be resolved.
                const result = await browser.runtime.getContexts(filter);
                browser.test.sendMessage(`${msg}:result`, result);
              }
            } catch (err) {
              browser.test.log(`runtime.getContexts error: ${err}\n`);
              browser.test.sendMessage(`${msg}:error`, String(err));
            }
          }
        });
        browser.test.sendMessage("bgpage:loaded");
      
  },
]);
