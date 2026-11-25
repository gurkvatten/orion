"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const data = new Array(1000 * 1024).fill("x").join("");
          await browser.test.assertRejects(
            browser.storage.local.set({ data }),
            /QuotaExceededError/,
            "Got a rejection with the expected error message"
          );
          browser.test.sendMessage("data-stored");
        
  },
]);
