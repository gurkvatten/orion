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
    
            if (details.proxyInfo) {
              browser.test.assertEq(
                "localhost",
                details.proxyInfo.host,
                "proxy host"
              );
              browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
              browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
            }
    
            browser.test.sendMessage(
              "requestCompleted",
              details.proxyInfo ? "proxied" : "not_proxied"
            );
          },
          { urls: ["<all_urls>"] }
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
              },
            ];
          },
          { urls: ["<all_urls>"] },
          ["requestHeaders"]
        );
      
  },
]);
