"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let blobUrl;
      browser.test.onMessage.addListener(async (msg, ...args) => {
        if (msg == "download.request") {
          let options = args[0];
    
          if (options.blobme) {
            let blob = new Blob(options.blobme);
            delete options.blobme;
            blobUrl = options.url = window.URL.createObjectURL(blob);
          }
    
          try {
            let id = await browser.downloads.download(options);
            browser.test.sendMessage("download.done", { status: "success", id });
          } catch (error) {
            browser.test.sendMessage("download.done", {
              status: "error",
              errmsg: error.message,
            });
          }
        } else if (msg == "killTheBlob") {
          window.URL.revokeObjectURL(blobUrl);
          blobUrl = null;
        }
      });
    
      browser.test.sendMessage("ready");
  },
  async function part_2() {
        let dlid = 0;
        let expectedState;
        browser.test.onMessage.addListener(async options => {
          try {
            expectedState = options.allowHttpErrors ? "complete" : "interrupted";
            dlid = await browser.downloads.download(options);
          } catch (err) {
            browser.test.fail(`Unexpected error in downloads.download(): ${err}`);
          }
        });
        function onChanged({ id, state }) {
          if (dlid !== id || !state || state.current === "in_progress") {
            return;
          }
          browser.test.assertEq(state.current, expectedState, "correct state");
          browser.downloads.search({ id }).then(([download]) => {
            browser.test.sendMessage("done", download.error);
          });
        }
        browser.downloads.onChanged.addListener(onChanged);
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(async options => {
          try {
            await browser.downloads.download(options);
          } catch (err) {
            browser.test.sendMessage("done", { err: err.message });
          }
        });
        browser.downloads.onChanged.addListener(({ state }) => {
          if (state && state.current === "complete") {
            browser.test.sendMessage("done", { ok: true });
          }
        });
      
  },
]);
