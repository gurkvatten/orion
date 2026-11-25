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
            if (details.url.includes("_original")) {
              let redirectUrl = details.url
                .replace("example.org", "example.com")
                .replace("_original", "_redirected");
              return { redirectUrl };
            }
            return {};
          },
          { urls: ["<all_urls>"] },
          ["blocking"]
        );
      
  },
  async function part_2() {
        const expectedError =
          "Using webRequest.addListener with the blocking option " +
          "requires the 'webRequestBlocking' permission.";
        const expectedErrorOnAuthRequired =
          "Using webRequest.onAuthRequired.addListener with the blocking option " +
          "requires either the 'webRequestBlocking' or 'webRequestAuthProvider' permission.";
    
        const blockingEvents = [
          "onBeforeRequest",
          "onBeforeSendHeaders",
          "onHeadersReceived",
          "onAuthRequired",
        ];
    
        for (let eventName of blockingEvents) {
          browser.test.assertThrows(
            () => {
              browser.webRequest[eventName].addListener(
                () => {},
                { urls: ["<all_urls>"] },
                ["blocking"]
              );
            },
            eventName === "onAuthRequired"
              ? expectedErrorOnAuthRequired
              : expectedError,
            `Got the expected exception for a blocking webRequest.${eventName} listener`
          );
        }
      
  },
]);
