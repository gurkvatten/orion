"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let expectedValues = {
            screenX: 0,
            screenY: 0,
            outerWidth: 0,
            outerHeight: 0,
          };
    
          for (let k in window) {
            try {
              if (k in expectedValues) {
                browser.test.assertEq(
                  expectedValues[k],
                  window[k],
                  `should return the expected value for window property: ${k}`
                );
              } else {
                void window[k];
              }
            } catch (e) {
              browser.test.assertEq(
                null,
                e,
                `unexpected exception accessing window property: ${k}`
              );
            }
          }
    
          browser.test.notifyPass("background.testWindowProperties.done");
        
  },
]);
