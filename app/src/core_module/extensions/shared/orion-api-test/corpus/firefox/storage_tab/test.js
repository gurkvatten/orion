"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          function awaitMessage(expectedMsg, api = "test") {
            return new Promise(resolve => {
              browser[api].onMessage.addListener(function listener(msg) {
                if (msg === expectedMsg) {
                  browser[api].onMessage.removeListener(listener);
                  resolve();
                }
              });
            });
          }
    
          let tabReady = awaitMessage("tab-ready", "runtime");
    
          try {
            let storage = browser.storage.local;
    
            browser.test.sendMessage(
              "load-page",
              browser.runtime.getURL("tab.html")
            );
            await awaitMessage("page-loaded");
            await tabReady;
    
            let result = await storage.get("key");
            browser.test.assertEq(undefined, result.key, "Key should be undefined");
    
            await browser.runtime.sendMessage("tab-set-key");
    
            result = await storage.get("key");
            browser.test.assertEq(
              JSON.stringify({ foo: { bar: "baz" } }),
              JSON.stringify(result.key),
              "Key should be set to the value from the tab"
            );
    
            browser.test.sendMessage("remove-page");
            await awaitMessage("page-removed");
    
            result = await storage.get("key");
            browser.test.assertEq(
              JSON.stringify({ foo: { bar: "baz" } }),
              JSON.stringify(result.key),
              "Key should still be set to the value from the tab"
            );
    
            browser.test.notifyPass("storage-multiple");
          } catch (e) {
            browser.test.fail(`Error: ${e} :: ${e.stack}`);
            browser.test.notifyFail("storage-multiple");
          }
        
  },
  async function part_2() {
          let numberOfChanges = 0;
          browser.storage.onChanged.addListener((changes, areaName) => {
            if (areaName !== "local") {
              browser.test.fail(
                `Received unexpected storage changes for "${areaName}"`
              );
            }
    
            numberOfChanges++;
          });
    
          browser.test.onMessage.addListener(async ({ msg, values }) => {
            switch (msg) {
              case "storage-set": {
                await browser.storage.local.set(values);
                browser.test.sendMessage("storage-set:done");
                break;
              }
              case "storage-get": {
                const res = await browser.storage.local.get();
                browser.test.sendMessage("storage-get:done", res);
                break;
              }
              case "storage-changes": {
                browser.test.sendMessage("storage-changes-count", numberOfChanges);
                break;
              }
              default:
                browser.test.fail(`Received unexpected message: ${msg}`);
            }
          });
    
          browser.test.sendMessage(
            "ext-page-url",
            browser.runtime.getURL("tab.html")
          );
        
  },
]);
