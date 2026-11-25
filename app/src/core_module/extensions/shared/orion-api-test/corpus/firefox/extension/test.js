"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let allowed = await browser.extension.isAllowedIncognitoAccess();
    
        browser.test.assertEq(true, allowed, "isAllowedIncognitoAccess is true");
        browser.test.notifyPass("isAllowedIncognitoAccess");
      
  },
  async function part_2() {
        let allowed = await browser.extension.isAllowedIncognitoAccess();
    
        browser.test.assertEq(false, allowed, "isAllowedIncognitoAccess is false");
        browser.test.notifyPass("isNotAllowedIncognitoAccess");
      
  },
  async function part_3() {
        browser.test.assertEq(
          false,
          browser.extension.inIncognitoContext,
          "inIncognitoContext returned false"
        );
        browser.test.notifyPass("inIncognitoContext");
      
  },
  async function part_4() {
        let allowed = await browser.extension.isAllowedFileSchemeAccess();
    
        browser.test.assertEq(false, allowed, "isAllowedFileSchemeAccess is false");
        browser.test.notifyPass("isAllowedFileSchemeAccess");
      
  },
]);
