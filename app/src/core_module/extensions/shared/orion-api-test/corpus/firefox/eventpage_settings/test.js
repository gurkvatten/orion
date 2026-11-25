"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async (msg, apiName, value) => {
            let apiObj = apiName.split(".").reduce((o, i) => o[i], browser);
            let result = await apiObj.set({ value });
            if (msg === "set") {
              browser.test.assertTrue(result, "set returns true.");
            } else {
              browser.test.assertFalse(result, "set returns false for a no-op.");
            }
          });
        
  },
  async function part_2() {
          browser.browserSettings.cacheEnabled.onChange.addListener(() => {
            browser.test.log("cacheEnabled received");
            browser.test.sendMessage("cacheEnabled");
          });
          browser.browserSettings.homepageOverride.onChange.addListener(() => {
            browser.test.sendMessage("homepageOverride");
          });
          browser.browserSettings.newTabPageOverride.onChange.addListener(() => {
            browser.test.sendMessage("newTabPageOverride");
          });
          browser.privacy.services.passwordSavingEnabled.onChange.addListener(
            () => {
              browser.test.sendMessage("passwordSavingEnabled");
            }
          );
        
  },
]);
