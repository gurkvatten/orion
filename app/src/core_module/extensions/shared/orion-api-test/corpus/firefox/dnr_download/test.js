"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { urlFilter: "|http://example.com/downloadtest" },
                action: { type: "block" },
              },
            ],
          });
    
          browser.downloads.onChanged.addListener(delta => {
            browser.test.assertEq(delta.state.current, "interrupted");
            browser.test.sendMessage("done");
          });
    
          await browser.downloads.download({
            url: "http://example.com/downloadtest",
            filename: "example.txt",
          });
        
  },
  async function part_2() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: {
                  urlFilter: "|http://example.com/downloadtest",
                  domainType: "thirdParty",
                },
                action: { type: "block" },
              },
            ],
          });
          browser.downloads.onChanged.addListener(delta => {
            browser.test.assertEq(delta.state.current, "interrupted");
            browser.test.sendMessage("download:done");
          });
    
          await browser.downloads.download({
            url: "http://example.com/downloadtest",
            filename: "example.txt",
          });
        
  },
  async function part_3() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { urlFilter: "|http://example.com/downloadtest" },
                action: { type: "block" },
              },
            ],
          });
    
          browser.test.sendMessage("dnr_registered");
        
  },
  async function part_4() {
          let downloadDonePromise = new Promise(resolve => {
            browser.downloads.onChanged.addListener(delta => {
              if (delta.state.current === "interrupted") {
                browser.test.fail("Download was unexpectedly interrupted");
                browser.test.notifyFail("done");
              } else if (delta.state.current === "complete") {
                resolve();
              }
            });
          });
    
          // This download should not have been interrupted by the other extension,
          // because declarativeNetRequest cannot match requests from other
          // extensions.
          let downloadId = await browser.downloads.download({
            url: "http://example.com/downloadtest",
            filename: "example_from_other_ext.txt",
          });
          await downloadDonePromise;
          browser.test.log("Download completed, removing file...");
          // TODO bug 1654819: On Windows the file may be recreated.
          await browser.downloads.removeFile(downloadId);
          browser.test.notifyPass("done");
        
  },
  async function part_5() {
            browser.downloads.onChanged.addListener(delta => {
              browser.test.assertEq(delta.state.current, "interrupted");
              browser.test.sendMessage("done");
            });
            await browser.downloads.download({
              url: "http://example.com/downloadtest",
              filename: "example_from_other_ext_with_pref.txt",
            });
          
  },
]);
