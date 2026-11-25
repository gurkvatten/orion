"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { resourceTypes: ["xmlhttprequest", "stylesheet"] },
                action: { type: "block" },
              },
              {
                id: 2,
                condition: { urlFilter: "blockme", resourceTypes: ["main_frame"] },
                action: { type: "block" },
              },
            ],
          });
          browser.test.sendMessage("dnr_registered");
        
  },
  async function part_2() {
            await browser.declarativeNetRequest.updateSessionRules({
              addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
            });
            const type = "other"; // matches the condition of the above rule.
    
            browser.test.assertDeepEq(
              { matchedRules: [] },
              await browser.declarativeNetRequest.testMatchOutcome({
                url: "http://restricted/",
                type,
              }),
              "testMatchOutcome ignores restricted url"
            );
            browser.test.assertDeepEq(
              { matchedRules: [] },
              await browser.declarativeNetRequest.testMatchOutcome({
                url: "http://example.com/",
                initiator: "http://restricted/",
                type,
              }),
              "testMatchOutcome ignores restricted initiator"
            );
            browser.test.sendMessage("done");
          
  },
  async function part_3() {
            await browser.declarativeNetRequest.updateSessionRules({
              addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
            });
            const type = "other"; // matches the condition of the above rule.
    
            browser.test.assertDeepEq(
              { matchedRules: [] },
              await browser.declarativeNetRequest.testMatchOutcome({
                url: "about:logo",
                type,
              }),
              "testMatchOutcome ignores url with disallowed schema"
            );
            browser.test.assertDeepEq(
              { matchedRules: [] },
              await browser.declarativeNetRequest.testMatchOutcome({
                url: "http://example.com/",
                initiator: "about:logo",
                type,
              }),
              "testMatchOutcome ignores initiator with disallowed schema"
            );
            browser.test.sendMessage("done");
          
  },
]);
