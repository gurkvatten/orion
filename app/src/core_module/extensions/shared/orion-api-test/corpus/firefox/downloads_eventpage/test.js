"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
            let downloadId;
            browser.downloads.onChanged.addListener(async info => {
              if (info.state && info.state.current === "complete") {
                browser.test.sendMessage("downloadComplete");
              }
            });
            browser.test.onMessage.addListener(async (msg, opts) => {
              if (msg == "download") {
                downloadId = await browser.downloads.download(opts);
              }
              if (msg == "erase") {
                await browser.downloads.removeFile(downloadId);
                await browser.downloads.erase({ id: downloadId });
              }
            });
          
  },
  async function part_2() {
            browser.downloads.onChanged.addListener(() => {
              browser.test.sendMessage("onChanged");
            });
            browser.downloads.onCreated.addListener(() => {
              browser.test.sendMessage("onCreated");
            });
            browser.downloads.onErased.addListener(() => {
              browser.test.sendMessage("onErased");
            });
            browser.test.sendMessage("ready");
          
  },
]);
