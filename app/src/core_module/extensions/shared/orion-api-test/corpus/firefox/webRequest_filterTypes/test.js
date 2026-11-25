"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async msg => {
            browser.webRequest[msg.event].addListener(
              () => {},
              { urls: ["*://example.com/*"], ...msg.filter },
              ["blocking"]
            );
            // Call an API method implemented in the parent process to
            // be sure that the webRequest listener has been registered
            // in the parent process as well.
            await browser.runtime.getBrowserInfo();
            browser.test.sendMessage(`webRequest-listener-registered`);
          });
        
  },
]);
