"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let info = await browser.runtime.getBrowserInfo();
    
        browser.test.assertEq(info.name, "XPCShell", "name is valid");
        browser.test.assertEq(info.vendor, "Mozilla", "vendor is Mozilla");
        browser.test.assertEq(info.version, "48", "version is correct");
        browser.test.assertEq(info.buildID, "20160315", "buildID is correct");
    
        browser.test.notifyPass("runtime.getBrowserInfo");
      
  },
]);
