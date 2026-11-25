"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      function background() {
        browser.test.log("running background script");
    
        browser.test.onMessage.addListener((x, y) => {
          browser.test.assertEq(x, 10, "x is 10");
          browser.test.assertEq(y, 20, "y is 20");
    
          browser.test.notifyPass("background test passed");
        });
    
        browser.test.sendMessage("running", 1);
      }
    
      let extensionData = {
        background,
        manifest: {
          name: "Simple extension test",
          version: "1.0",
          manifest_version: 2,
          description: "",
        },
      };
    
      let extension = ExtensionTestUtils.loadExtension(extensionData);
    
      let [, x] = await Promise.all([
        extension.startup(),
        extension.awaitMessage("running"),
      ]);
      equal(x, 1, "got correct value from extension");
    
      extension.sendMessage(10, 20);
      await extension.awaitFinish();
      await extension.unload();
  },
  async function part_2() {
          browser.test.assertEq(
            typeof browser.extensionTypes,
            "object",
            "browser.extensionTypes exists"
          );
          browser.test.assertEq(
            typeof browser.extensionTypes.RunAt,
            "object",
            "browser.extensionTypes.RunAt exists"
          );
          browser.test.notifyPass("extentionTypes test passed");
        
  },
]);
