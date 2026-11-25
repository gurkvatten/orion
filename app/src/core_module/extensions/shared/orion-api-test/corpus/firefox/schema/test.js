"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.assertEq(
          undefined,
          browser.manifest,
          "browser.manifest is not defined"
        );
        browser.test.assertTrue(
          !!browser.storage,
          "browser.storage should be defined"
        );
        browser.test.assertEq(
          undefined,
          browser.contextMenus,
          "browser.contextMenus should not be defined"
        );
        browser.test.notifyPass("schema");
      
  },
]);
