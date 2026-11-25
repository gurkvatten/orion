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
              details.url,
              "https://example.com/",
              "request upgraded and sent"
            );
            browser.test.notifyPass();
            return { cancel: true };
          },
          {
            urls: ["https://example.com/*"],
          },
          ["blocking"]
        );
    
        await browser.test.assertRejects(
          fetch("http://example.com/"),
          "NetworkError when attempting to fetch resource.",
          "request was upgraded"
        );
      
  },
  async function part_2() {
        let req = await fetch("http://example.com/");
        browser.test.assertEq(
          req.url,
          "http://example.com/",
          "request not upgraded"
        );
        browser.test.notifyPass();
      
  },
]);
