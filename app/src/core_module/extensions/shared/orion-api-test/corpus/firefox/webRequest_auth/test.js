"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          try {
            browser.webRequest.onAuthRequired.addListener(
              () => {},
              { urls: ["https://example.org/*"] },
              ["blocking", "asyncBlocking"]
            );
            browser.test.fail(
              "onAuthRequired.addListener should have raised an exception"
            );
            browser.test.notifyFail("add-listener-error");
          } catch (err) {
            browser.test.assertEq(
              "'blocking' and 'asyncBlocking' are mutually exclusive",
              err.message,
              "Got the expected error when blocking and asyncBlocking are requested for the same listener"
            );
            browser.test.notifyPass("add-listener-error");
          }
        
  },
  async function part_2() {
        let proxyOk = false;
        browser.webRequest.onAuthRequired.addListener(
          details => {
            browser.test.log(
              `handlingExt onAuthRequired called with ${details.requestId} ${details.url}`
            );
            if (details.isProxy) {
              browser.test.succeed("providing proxy authorization");
              proxyOk = true;
              return { authCredentials: { username: "puser", password: "ppass" } };
            }
            browser.test.assertTrue(
              proxyOk,
              "providing www authorization after proxy auth"
            );
            browser.test.sendMessage("done");
            return { authCredentials: { username: "auser", password: "apass" } };
          },
          { urls: [permissionPath] },
          ["blocking"]
        );
      
  },
  async function part_3() {
        browser.webRequest.onAuthRequired.addListener(
          (detais, asyncBlockingCb) => {
            asyncBlockingCb({ cancel: true });
            browser.test.sendMessage("onAuthRequired");
          },
          { urls: ["http://example.com/*"] },
          ["asyncBlocking"]
        );
        browser.test.sendMessage("listener-added");
      
  },
]);
