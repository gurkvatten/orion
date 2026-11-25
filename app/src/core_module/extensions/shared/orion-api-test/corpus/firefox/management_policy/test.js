"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let addon = await browser.management.get(TEST_ADDON_ID);
        browser.test.assertTrue(addon.enabled, "addon is enabled");
        await browser.management.setEnabled(addon.id, false);
        addon = await browser.management.get(TEST_ADDON_ID);
        browser.test.assertFalse(addon.enabled, "addon is disabled");
        await browser.management.setEnabled(addon.id, true);
        addon = await browser.management.get(TEST_ADDON_ID);
        browser.test.assertTrue(addon.enabled, "addon is enabled");
        browser.test.sendMessage("done");
      
  },
  async function part_2() {
        let addon = await browser.management.get(TEST_ADDON_ID);
        browser.test.assertTrue(addon.enabled, "addon is enabled");
        await browser.test.assertRejects(
          browser.management.setEnabled(addon.id, false),
          /setEnabled can only be used/,
          "setEnabled should fail"
        );
        addon = await browser.management.get(TEST_ADDON_ID);
        browser.test.assertTrue(addon.enabled, "addon is still enabled");
        browser.test.sendMessage("done");
      
  },
]);
