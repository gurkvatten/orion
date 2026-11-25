"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async (msg, data, setting) => {
          // The second argument is the end of the api name,
          // e.g., "network.networkPredictionEnabled".
          let apiObj = setting.split(".").reduce((o, i) => o[i], browser.privacy);
          let settingData;
          switch (msg) {
            case "get":
              settingData = await apiObj.get(data);
              browser.test.sendMessage("gotData", settingData);
              break;
    
            case "set":
              await apiObj.set(data);
              settingData = await apiObj.get({});
              browser.test.sendMessage("afterSet", settingData);
              break;
    
            case "clear":
              await apiObj.clear(data);
              settingData = await apiObj.get({});
              browser.test.sendMessage("afterClear", settingData);
              break;
          }
        });
      
  },
  async function part_2() {
        await browser.test.assertRejects(
          browser.privacy.network.networkPredictionEnabled.set({
            value: true,
            scope: "regular_only",
          }),
          "Firefox does not support the regular_only settings scope.",
          "Expected rejection calling set with invalid scope."
        );
    
        await browser.test.assertRejects(
          browser.privacy.network.networkPredictionEnabled.clear({
            scope: "incognito_persistent",
          }),
          "Firefox does not support the incognito_persistent settings scope.",
          "Expected rejection calling clear with invalid scope."
        );
    
        browser.test.notifyPass("exceptionTests");
      
  },
]);
