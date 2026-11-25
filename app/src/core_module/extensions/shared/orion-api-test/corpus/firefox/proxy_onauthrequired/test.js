"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let expecting = [
          "onBeforeSendHeaders",
          "onSendHeaders",
          "onAuthRequired",
          "onBeforeSendHeaders",
          "onSendHeaders",
          "onCompleted",
        ];
        browser.webRequest.onBeforeSendHeaders.addListener(
          details => {
            browser.test.log(`onBeforeSendHeaders ${JSON.stringify(details)}\n`);
            browser.test.assertEq(
              "onBeforeSendHeaders",
              expecting.shift(),
              "got expected event"
            );
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
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onSendHeaders.addListener(
          details => {
            browser.test.log(`onSendHeaders ${JSON.stringify(details)}\n`);
            browser.test.assertEq(
              "onSendHeaders",
              expecting.shift(),
              "got expected event"
            );
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onAuthRequired.addListener(
          details => {
            browser.test.log(`onAuthRequired ${JSON.stringify(details)}\n`);
            browser.test.assertEq(
              "onAuthRequired",
              expecting.shift(),
              "got expected event"
            );
            browser.test.assertTrue(details.isProxy, "proxied request");
            browser.test.assertEq(
              "localhost",
              details.proxyInfo.host,
              "proxy host"
            );
            browser.test.assertEq(port, details.proxyInfo.port, "proxy port");
            browser.test.assertEq("http", details.proxyInfo.type, "proxy type");
            browser.test.assertEq(
              "localhost",
              details.challenger.host,
              "proxy host"
            );
            browser.test.assertEq(port, details.challenger.port, "proxy port");
            return { authCredentials: { username: "puser", password: "ppass" } };
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
    
        browser.webRequest.onCompleted.addListener(
          details => {
            browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);
            browser.test.assertEq(
              "onCompleted",
              expecting.shift(),
              "got expected event"
            );
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
              "proxy username not set by onAuthRequired"
            );
            browser.test.assertEq(
              undefined,
              details.proxyInfo.password,
              "no proxy password"
            );
            browser.test.assertEq(expecting.length, 0, "got all expected events");
            browser.test.sendMessage("done");
          },
          { urls: ["<all_urls>"] }
        );
    
        // Handle the proxy request.
        browser.proxy.onRequest.addListener(
          details => {
            browser.test.log(`onRequest ${JSON.stringify(details)}`);
            return [{ host: "localhost", port, type: "http" }];
          },
          { urls: ["<all_urls>"] },
          ["requestHeaders"]
        );
        browser.test.sendMessage("ready");
      
  },
  async function part_2() {
        let authReceived = false;
    
        browser.webRequest.onBeforeSendHeaders.addListener(
          () => {
            if (authReceived) {
              browser.test.sendMessage("done");
              return { cancel: true };
            }
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
    
        browser.webRequest.onAuthRequired.addListener(
          () => {
            authReceived = true;
            return { authCredentials: { username: "puser", password: "ppass" } };
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
    
        // Handle the proxy request.
        browser.proxy.onRequest.addListener(
          details => {
            browser.test.log(`onRequest ${JSON.stringify(details)}`);
            return [{ host: "localhost", port, type: "http" }];
          },
          { urls: ["<all_urls>"] },
          ["requestHeaders"]
        );
        browser.test.sendMessage("ready");
      
  },
  async function part_3() {
        browser.webRequest.onBeforeRequest.addListener(
          () => {
            browser.test.fail("onBeforeRequest");
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onAuthRequired.addListener(
          () => {
            browser.test.sendMessage("onAuthRequired");
            // cancel is silently ignored, if it were not (e.g someone messes up in
            // WebRequest.sys.mjs and allows cancel) this test would fail.
            return {
              cancel: true,
              authCredentials: { username: "puser", password: "ppass" },
            };
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
    
        // Handle the proxy request.
        browser.proxy.onRequest.addListener(
          details => {
            browser.test.log(`onRequest ${JSON.stringify(details)}`);
            return { host: "localhost", port, type: "http" };
          },
          { urls: ["<all_urls>"] }
        );
        browser.test.sendMessage("ready");
      
  },
]);
