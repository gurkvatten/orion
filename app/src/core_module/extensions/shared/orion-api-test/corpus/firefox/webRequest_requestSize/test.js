"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.log("background script running");
    
        browser.webRequest.onBeforeSendHeaders.addListener(
          async details => {
            browser.test.assertTrue(details.requestSize == 0, "no requestSize");
            browser.test.assertTrue(details.responseSize == 0, "no responseSize");
            browser.test.log(`details.requestSize: ${details.requestSize}`);
            browser.test.log(`details.responseSize: ${details.responseSize}`);
            browser.test.sendMessage("check");
          },
          { urls: ["*://*/*"] }
        );
    
        browser.webRequest.onCompleted.addListener(
          async details => {
            browser.test.assertTrue(details.requestSize > 100, "have requestSize");
            browser.test.assertTrue(
              details.responseSize > 100,
              "have responseSize"
            );
            browser.test.log(`details.requestSize: ${details.requestSize}`);
            browser.test.log(`details.responseSize: ${details.responseSize}`);
            browser.test.sendMessage("done");
          },
          { urls: ["*://*/*"] }
        );
      
  },
]);
