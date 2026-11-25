"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.captivePortal.canonicalURL.onChange.addListener(details => {
          browser.test.sendMessage("url", details);
        });
        let url = await browser.captivePortal.canonicalURL.get({});
        browser.test.sendMessage("url", url);
      
  },
]);
