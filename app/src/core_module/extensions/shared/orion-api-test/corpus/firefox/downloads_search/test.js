"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let complete = new Map();
    
      function waitForComplete(id) {
        if (complete.has(id)) {
          return complete.get(id).promise;
        }
    
        let promise = new Promise(resolve => {
          complete.set(id, { resolve });
        });
        complete.get(id).promise = promise;
        return promise;
      }
    
      browser.downloads.onChanged.addListener(change => {
        if (change.state && change.state.current == "complete") {
          // Make sure we have a promise.
          waitForComplete(change.id);
          complete.get(change.id).resolve();
        }
      });
    
      browser.test.onMessage.addListener(async (msg, ...args) => {
        if (msg == "download.request") {
          try {
            let id = await browser.downloads.download(args[0]);
            browser.test.sendMessage("download.done", { status: "success", id });
          } catch (error) {
            browser.test.sendMessage("download.done", {
              status: "error",
              errmsg: error.message,
            });
          }
        } else if (msg == "search.request") {
          try {
            let downloads = await browser.downloads.search(args[0]);
            browser.test.sendMessage("search.done", {
              status: "success",
              downloads,
            });
          } catch (error) {
            browser.test.sendMessage("search.done", {
              status: "error",
              errmsg: error.message,
            });
          }
        } else if (msg == "waitForComplete.request") {
          await waitForComplete(args[0]);
          browser.test.sendMessage("waitForComplete.done");
        }
      });
    
      browser.test.sendMessage("ready");
  },
  async function part_2() {
          browser.test.onMessage.addListener(async (msg, url) => {
            let id = await browser.downloads.download({ url });
            let full = await browser.downloads.search({ id });
    
            browser.test.assertEq(
              full.length,
              1,
              "Found new download in search results"
            );
            browser.test.assertEq(
              full[0].totalBytes,
              -1,
              "New download still has totalBytes == -1"
            );
    
            browser.downloads.onChanged.addListener(info => {
              if (info.id == id && info.state && info.state.current == "complete") {
                browser.test.notifyPass("done");
              }
            });
    
            browser.test.sendMessage("started");
          });
        
  },
]);
