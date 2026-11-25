"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(msg => {
            if (msg === "verify-same-site-cookie-moz-extension") {
              let xhr = new XMLHttpRequest();
              try {
                xhr.open(
                  "GET",
                  "http://example.com/same_site_cookies?loadXHR",
                  true
                );
                xhr.onload = function () {
                  browser.test.assertEq(
                    "myKey=mySameSiteExtensionCookie",
                    xhr.responseText,
                    "cookie should be accessible from moz-extension context"
                  );
                  browser.test.sendMessage("same-site-cookie-test-done");
                };
                xhr.onerror = function () {
                  browser.test.fail("xhr onerror");
                  browser.test.sendMessage("same-site-cookie-test-done");
                };
              } catch (e) {
                browser.test.fail("xhr failure: " + e);
              }
              xhr.send();
            }
          });
        
  },
]);
