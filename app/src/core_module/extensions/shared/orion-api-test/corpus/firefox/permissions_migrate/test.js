"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          if (browser.browserSettings) {
            browser.browserSettings.cacheEnabled.set({ value: false });
          }
          browser.privacy.services.passwordSavingEnabled.set({ value: false });
        
  },
  async function part_2() {
            browser.test.onMessage.addListener(async (msg, args) => {
              let result;
              if (msg === "permissions-request") {
                await browser.permissions.request({
                  permissions: args.permissions,
                  origins: [],
                  data_collection: args.data_collection,
                });
                if (browser.browserSettings) {
                  browser.browserSettings.cacheEnabled.set({ value: false });
                }
                browser.privacy.services.passwordSavingEnabled.set({
                  value: false,
                });
              } else if (msg === "permissions-getAll") {
                result = await browser.permissions.getAll();
              } else {
                browser.test.fail(`Got unexpected test message ${msg}`);
              }
              browser.test.sendMessage(`${msg}:done`, result);
            });
          
  },
]);
