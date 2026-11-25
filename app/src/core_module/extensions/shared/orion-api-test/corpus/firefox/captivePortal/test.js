"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.captivePortal.onConnectivityAvailable.addListener(details => {
            browser.test.log(
              `onConnectivityAvailable received ${JSON.stringify(details)}`
            );
            browser.test.sendMessage("connectivity", details);
          });
    
          browser.captivePortal.onStateChanged.addListener(details => {
            browser.test.log(`onStateChanged received ${JSON.stringify(details)}`);
            browser.test.sendMessage("state", details);
          });
    
          browser.captivePortal.canonicalURL.onChange.addListener(details => {
            browser.test.sendMessage("url", details);
          });
    
          browser.test.onMessage.addListener(async msg => {
            if (msg == "getstate") {
              browser.test.sendMessage(
                "getstate",
                await browser.captivePortal.getState()
              );
            }
          });
        
  },
]);
