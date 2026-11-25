"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.webRequest.onCompleted.addListener(
          details => {
            browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);
            browser.test.assertEq(
              "localhost",
              details.proxyInfo.host,
              "proxy host"
            );
            browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
            browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
            browser.test.assertEq(
              "",
              details.proxyInfo.username,
              "proxy username not set"
            );
            browser.test.assertEq(
              proxyToken,
              details.proxyInfo.proxyAuthorizationHeader,
              "proxy authorization header"
            );
            browser.test.assertEq(
              proxyToken,
              details.proxyInfo.connectionIsolationKey,
              "proxy connection isolation"
            );
    
            browser.test.notifyPass("requestCompleted");
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onAuthRequired.addListener(
          () => {
            // Using proxyAuthorizationHeader should prevent an auth request coming to us in the extension.
            browser.test.fail("onAuthRequired");
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
    
        // Handle the proxy request.
        browser.proxy.onRequest.addListener(
          details => {
            browser.test.log(`onRequest ${JSON.stringify(details)}`);
            return [
              {
                host: "localhost",
                port,
                type: "http",
                proxyAuthorizationHeader: proxyToken,
                connectionIsolationKey: proxyToken,
              },
            ];
          },
          { urls: ["<all_urls>"] },
          ["requestHeaders"]
        );
      
  },
]);
