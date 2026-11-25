"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const logs = [];
    
          browser.webRequest.handlerBehaviorChanged().then(() => {
            browser.webRequest.onBeforeRequest.addListener(
              details => {
                logs.push(details.url);
                if (logs.length == 4) {
                  browser.test.sendMessage("logs", logs);
                } else if (logs.length > 4) {
                  browser.test.fail(`Unexpected request: ${details.url}`);
                }
              },
              {
                urls: [
                  "http://example.com/*file_webRequest_handlerBehaviorChanged*",
                ],
              }
            );
    
            browser.test.sendMessage("ready");
          });
        
  },
  async function part_2() {
          browser.webRequest.handlerBehaviorChanged(result => {
            browser.test.assertEq(
              result,
              undefined,
              "handlerBehaviorChanged should call callback with undefined"
            );
            browser.test.sendMessage("done");
          });
        
  },
  async function part_3() {
          browser.webRequest.handlerBehaviorChanged().then(result => {
            browser.test.assertEq(
              result,
              undefined,
              "handlerBehaviorChanged should resolve the promise with undefined"
            );
            browser.test.sendMessage("done");
          });
        
  },
  async function part_4() {
          browser.test.onMessage.addListener((msg, count) => {
            browser.test.assertEq(msg, `call handlerBehaviorChanged`);
            const promises = [];
            for (let i = 0; i < count; i++) {
              promises.push(browser.webRequest.handlerBehaviorChanged());
            }
            Promise.all(promises).then(() => {
              browser.test.sendMessage("done");
            });
          });
        
  },
]);
