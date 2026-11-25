"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            async details => {
              browser.test.assertTrue(details.incognito, "incognito flag is set");
            },
            { urls: ["<all_urls>"], incognito: true },
            ["blocking"]
          );
    
          browser.webRequest.onBeforeRequest.addListener(
            async details => {
              browser.test.assertFalse(
                details.incognito,
                "incognito flag is not set"
              );
              browser.test.notifyPass("webRequest.spanning");
            },
            { urls: ["<all_urls>"], incognito: false },
            ["blocking"]
          );
        
  },
  async function part_2() {
          browser.webRequest.onBeforeRequest.addListener(
            async details => {
              browser.test.assertFalse(
                details.incognito,
                "incognito flag is not set"
              );
              browser.test.notifyPass("webRequest");
            },
            { urls: ["<all_urls>"] },
            ["blocking"]
          );
        
  },
]);
