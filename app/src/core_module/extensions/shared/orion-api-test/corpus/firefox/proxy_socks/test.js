"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        function checkProxyData(details) {
          browser.test.assertEq("127.0.0.1", details.proxyInfo.host, "proxy host");
          browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
          browser.test.assertEq("socks", details.proxyInfo.type, "proxy type");
          browser.test.assertEq(
            "foo",
            details.proxyInfo.username,
            "proxy username not set"
          );
          browser.test.assertEq(
            undefined,
            details.proxyInfo.password,
            "no proxy password passed to webrequest"
          );
        }
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            checkProxyData(details);
          },
          { urls: ["<all_urls>"] }
        );
        browser.webRequest.onAuthRequired.addListener(
          () => {
            // We should never get onAuthRequired for socks proxy
            browser.test.fail("onAuthRequired");
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
        browser.webRequest.onCompleted.addListener(
          details => {
            checkProxyData(details);
            browser.test.sendMessage("done");
          },
          { urls: ["<all_urls>"] }
        );
        browser.proxy.onRequest.addListener(
          () => {
            return [
              {
                type: "socks",
                host: "127.0.0.1",
                port,
                username: "foo",
                password: "bar",
              },
            ];
          },
          { urls: ["<all_urls>"] }
        );
      
  },
  async function part_2() {
        browser.proxy.onRequest.addListener(
          () => {
            return [
              {
                type: "socks",
                host: "127.0.0.1",
                port,
                username: "foo",
                password: "bar",
              },
            ];
          },
          { urls: ["<all_urls>"] }
        );
      
  },
]);
