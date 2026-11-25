"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.assertEq(
            window,
            browser.extension.getBackgroundPage(),
            "Caller should be able to access itself as a background page"
          );
          browser.test.assertEq(
            window,
            await browser.runtime.getBackgroundPage(),
            "Caller should be able to access itself as a background page"
          );
    
          browser.test.assertEq(
            browser.extension.inIncognitoContext,
            true,
            "inIncognitoContext is true for permanent private browsing"
          );
    
          browser.test.notifyPass("incognito");
        
  },
]);
