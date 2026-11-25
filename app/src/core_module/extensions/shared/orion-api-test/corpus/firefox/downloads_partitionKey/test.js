"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      browser.test.onMessage.addListener(async (msg, ...args) => {
        if (msg == "download.request") {
          let options = args[0];
    
          try {
            let id = await browser.downloads.download(options);
            browser.test.sendMessage("download.done", { status: "success", id });
          } catch (error) {
            browser.test.sendMessage("download.done", {
              status: "error",
              errmsg: error.message,
            });
          }
        }
      });
    
      browser.test.sendMessage("ready");
  },
]);
