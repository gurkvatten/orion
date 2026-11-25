"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("request_intercepted");
              return { cancel: true };
            },
            {
              urls: ["https://example.com/downloadtest"],
            },
            ["blocking"]
          );
    
          browser.downloads.onChanged.addListener(delta => {
            browser.test.assertEq(delta.state.current, "interrupted");
            browser.test.sendMessage("done");
          });
    
          await browser.downloads.download({
            url: "https://example.com/downloadtest",
            filename: "example.txt",
          });
        
  },
]);
