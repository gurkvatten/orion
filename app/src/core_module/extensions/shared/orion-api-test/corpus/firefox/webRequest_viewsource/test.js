"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.proxy.onRequest.addListener(
          details => {
            if (details.url === `http://example.com:${serverPort}/dummy`) {
              browser.test.assertTrue(
                true,
                "viewsource protocol worked in proxy request"
              );
              browser.test.sendMessage("proxied");
            }
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertEq(
              `http://example.com:${serverPort}/redirect`,
              details.url,
              "viewsource protocol worked in webRequest"
            );
            browser.test.sendMessage("viewed");
            return { redirectUrl: `http://example.com:${serverPort}/dummy` };
          },
          { urls: ["http://example.com/redirect"] },
          ["blocking"]
        );
    
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertEq(
              `http://example.com:${serverPort}/dummy`,
              details.url,
              "viewsource protocol worked in webRequest"
            );
            browser.test.sendMessage("redirected");
            return { cancel: true };
          },
          { urls: ["http://example.com/dummy"] },
          ["blocking"]
        );
    
        browser.webRequest.onCompleted.addListener(
          () => {
            // If cancel fails we get onCompleted.
            browser.test.fail("onCompleted received");
          },
          { urls: ["http://example.com/dummy"] }
        );
    
        browser.webRequest.onErrorOccurred.addListener(
          details => {
            browser.test.assertEq(
              details.error,
              "NS_ERROR_ABORT",
              "request cancelled"
            );
            browser.test.sendMessage("cancelled");
          },
          { urls: ["http://example.com/dummy"] }
        );
      
  },
]);
