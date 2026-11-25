"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let { fileName } = new Error();
    
          browser.test.sendMessage("fileName", fileName);
    
          browser.runtime.sendMessage("Meh.", () => {});
    
          await browser.test.assertRejects(
            browser.runtime.sendMessage("Meh"),
            error => {
              return error.fileName === fileName && error.lineNumber === 9;
            }
          );
    
          browser.test.notifyPass("error-location");
        
  },
]);
