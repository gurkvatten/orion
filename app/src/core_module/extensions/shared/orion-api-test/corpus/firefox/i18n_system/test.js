"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener((msg, expected) => {
          browser.test.assertEq("expect-results", msg, "Expected message");
          browser.i18n.getPreferredSystemLanguages().then(results => {
            browser.test.assertDeepEq(
              expected,
              results,
              "got expected languages in background"
            );
    
            const isNonEmptyArray = Array.isArray(results) && !!results.length;
            browser.test.assertTrue(isNonEmptyArray, "Is non empty array");
    
            const canonicalLocales = Intl.getCanonicalLocales(results);
            browser.test.assertDeepEq(
              results,
              canonicalLocales,
              "Result gives canonical versions"
            );
    
            browser.test.sendMessage("background-done");
          });
        });
      
  },
]);
