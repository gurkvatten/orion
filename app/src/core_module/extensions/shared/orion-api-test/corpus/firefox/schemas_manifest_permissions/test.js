"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
            // Test hasPermission method implemented in ExtensionChild.sys.mjs.
            browser.test.assertTrue(
              "testManifestPermission" in browser,
              "The API namespace is defined as expected"
            );
            browser.test.assertEq(
              undefined,
              browser.testManifestPermission &&
                browser.testManifestPermission.testMethod,
              "The property with nested manifest property permission should not be available "
            );
            browser.test.notifyPass("test-extension-manifest-without-nested-prop");
          
  },
  async function part_2() {
            // Test hasPermission method implemented in ExtensionChild.sys.mjs.
            browser.test.assertTrue(
              "testManifestPermission" in browser,
              "The API namespace is defined as expected"
            );
            browser.test.assertEq(
              "function",
              browser.testManifestPermission &&
                typeof browser.testManifestPermission.testMethod,
              "The property with nested manifest property permission should be available "
            );
            browser.test.notifyPass("test-extension-manifest-with-nested-prop");
          
  },
]);
