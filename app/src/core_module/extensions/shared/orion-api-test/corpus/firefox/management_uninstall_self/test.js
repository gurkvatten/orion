"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(() => {
          browser.management.uninstallSelf();
        });
      
  },
  async function part_2() {
        browser.test.onMessage.addListener(() => {
          browser.management.uninstallSelf({ showConfirmDialog: true });
        });
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(async () => {
          await browser.test.assertRejects(
            browser.management.uninstallSelf({ showConfirmDialog: true }),
            "User cancelled uninstall of extension",
            "Expected rejection when user declines uninstall"
          );
    
          browser.test.sendMessage("uninstall-rejected");
        });
      
  },
]);
