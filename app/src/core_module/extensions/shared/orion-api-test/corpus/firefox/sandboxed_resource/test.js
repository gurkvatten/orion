"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        /* eslint-disable-next-line mozilla/balanced-listeners */
        window.addEventListener("message", event => {
          if (event.data == "undefined") {
            browser.test.notifyPass("webext-background-sandbox-privileges");
          } else {
            browser.test.notifyFail("webext-background-sandbox-privileges");
          }
        });
      
  },
]);
