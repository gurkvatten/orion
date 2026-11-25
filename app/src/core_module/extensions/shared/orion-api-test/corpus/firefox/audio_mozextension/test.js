"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await new Promise(resolve => {
            const mozExtensionUrl = browser.runtime.getURL("test.mp3");
            browser.test.log(`Loading Audio: ${mozExtensionUrl}`);
            let audio = new Audio(mozExtensionUrl);
            audio.onloadedmetadata = resolve;
          });
          browser.test.sendMessage("done");
        
  },
]);
