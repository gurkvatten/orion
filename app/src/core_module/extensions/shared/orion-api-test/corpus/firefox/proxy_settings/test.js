"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertEq(
              host,
              details.proxyInfo.host,
              "proxy host matched"
            );
            browser.test.assertEq(
              port,
              details.proxyInfo.port,
              "proxy port matched"
            );
          },
          { urls: ["http://example.com/*"] }
        );
        browser.webRequest.onCompleted.addListener(
          () => {
            browser.test.notifyPass("proxytest");
          },
          { urls: ["http://example.com/*"] }
        );
        browser.webRequest.onErrorOccurred.addListener(
          () => {
            browser.test.notifyFail("proxytest");
          },
          { urls: ["http://example.com/*"] }
        );
    
        // Wait for the settings before testing a request.
        await browser.proxy.settings.set({
          value: {
            proxyType: "manual",
            http: `${host}:${port}`,
          },
        });
        browser.test.sendMessage("ready");
      
  },
]);
