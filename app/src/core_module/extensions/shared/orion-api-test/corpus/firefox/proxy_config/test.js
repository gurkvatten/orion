"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async (msg, value) => {
          let apiObj = browser.proxy.settings;
          let result = await apiObj.set({ value });
          if (msg === "set") {
            browser.test.assertTrue(result, "set returns true.");
            browser.test.sendMessage("settingData", await apiObj.get({}));
          } else {
            browser.test.assertFalse(result, "set returns false for a no-op.");
            browser.test.sendMessage("no-op set");
          }
        });
      
  },
  async function part_2() {
        const permObj = { permissions: ["proxy"] };
        browser.test.onMessage.addListener(async (msg, value) => {
          if (msg === "request") {
            browser.test.log("requesting proxy permission");
            await browser.permissions.request(permObj);
            browser.test.log("setting proxy values");
            await browser.proxy.settings.set({ value });
            browser.test.sendMessage("set");
          } else if (msg === "remove") {
            await browser.permissions.remove(permObj);
            browser.test.sendMessage("removed");
          }
        });
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(async msg => {
          if (msg === "get-proxy-settings") {
            let proxySettings = await browser.proxy.settings.get({});
            browser.test.sendMessage("return-proxy-settings", proxySettings.value);
          }
        });
      
  },
]);
