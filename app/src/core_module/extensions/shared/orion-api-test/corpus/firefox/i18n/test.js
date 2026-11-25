"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.assertEq(
            location.hostname,
            browser.i18n.getMessage("@@extension_id"),
            "builtin @@extension_id should be available despite no default_locale"
          );
          browser.test.sendMessage("done");
        
  },
  async function part_2() {
        browser.test.onMessage.addListener(([, expected]) => {
          browser.i18n.getAcceptLanguages().then(results => {
            checkResultsFn("background", results, expected);
    
            browser.test.sendMessage("background-done");
          });
        });
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(([, expected]) => {
          checkResultsFn("background", getResultsFn(), expected);
    
          browser.test.sendMessage("background-done");
        });
      
  },
  async function part_4() {
        browser.test.onMessage.addListener(([msg, expected]) => {
          browser.i18n.detectLanguage(msg).then(result => {
            checkResultFn("background", result, expected);
            browser.test.sendMessage("background-done");
          });
        });
      
  },
]);
