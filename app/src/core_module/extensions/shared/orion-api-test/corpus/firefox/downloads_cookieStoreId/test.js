"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let url = "http://example.net/download";
          // Add default cookie
          await browser.cookies.set({
            url,
            name: "cookie_normal",
            value: "1",
          });
    
          // Add private cookie
          await browser.cookies.set({
            url,
            storeId: "firefox-private",
            name: "cookie_private",
            value: "1",
          });
    
          // Add container cookie
          await browser.cookies.set({
            url,
            storeId: "firefox-container-1",
            name: "cookie_container",
            value: "1",
          });
          browser.test.sendMessage("cookies set");
        
  },
]);
