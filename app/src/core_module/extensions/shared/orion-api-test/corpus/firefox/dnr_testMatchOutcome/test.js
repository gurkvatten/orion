"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const testMatchOutcome = browser.declarativeNetRequest.testMatchOutcome;
    
          browser.test.assertThrows(
            () => testMatchOutcome({ type: "image" }),
            /Type error for parameter request \(Property "url" is required\)/,
            "url is required"
          );
          browser.test.assertThrows(
            () => testMatchOutcome({ url: "https://example.com/" }),
            /Type error for parameter request \(Property "type" is required\)/,
            "resource type is required"
          );
    
          browser.test.assertDeepEq(
            { matchedRules: [] },
            await testMatchOutcome({ url: "https://example.com/", type: "image" }),
            "testMatchOutcome with url and type succeeds"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_2() {
          const testMatchOutcome = browser.declarativeNetRequest.testMatchOutcome;
    
          const url = "https://example.com/some-dummy-url";
    
          browser.test.assertThrows(
            () => testMatchOutcome({ url, type: "MAIN_FRAME" }),
            /Error processing type: Invalid enumeration value "MAIN_FRAME"/,
            "testMatchOutcome should expects a lowercase type"
          );
    
          // Check that at least one ResourceType exists.
          browser.test.assertEq(
            "main_frame",
            browser.declarativeNetRequest.ResourceType.MAIN_FRAME,
            "ResourceType.MAIN_FRAME exists"
          );
    
          for (let type of Object.values(
            browser.declarativeNetRequest.ResourceType
          )) {
            browser.test.assertDeepEq(
              { matchedRules: [] },
              await testMatchOutcome({ url, type }),
              `testMatchOutcome for type=${type} is allowed`
            );
          }
    
          browser.test.notifyPass();
        
  },
  async function part_3() {
          const otherExtensionUUID = browser.runtime.getManifest().description;
          const dnr = browser.declarativeNetRequest;
    
          const url = "https://do.not.look.here/look_at_initator_instead";
          const type = "other";
    
          browser.test.assertDeepEq(
            { matchedRules: [] },
            await dnr.testMatchOutcome({ url, type, initiator: location.origin }),
            "testMatchOutcome excludes other extensions by default"
          );
          browser.test.assertDeepEq(
            { matchedRules: [] },
            await dnr.testMatchOutcome(
              { url, type, initiator: location.origin },
              { includeOtherExtensions: true }
            ),
            "No matches when initiator is moz-extension:, different from DNR ext"
          );
          browser.test.assertDeepEq(
            {
              matchedRules: [
                { ruleId: 1, rulesetId: "_session", extensionId: "other@ext" },
              ],
            },
            await dnr.testMatchOutcome(
              { url, type, initiator: `moz-extension://${otherExtensionUUID}` },
              { includeOtherExtensions: true }
            ),
            "Simulated moz-extension: for original extension finds a match"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_4() {
              const otherExtensionUUID = browser.runtime.getManifest().description;
              const dnr = browser.declarativeNetRequest;
    
              const url = "https://do.not.look.here/look_at_initator_instead";
              const type = "other";
    
              // Sanity check: testMatchOutcome for moz-extension:-URL different
              // from the DNR extension and the current test extension.
              browser.test.assertDeepEq(
                {
                  matchedRules: [
                    { ruleId: 2, rulesetId: "_session", extensionId: "other@ext" },
                  ],
                },
                await dnr.testMatchOutcome(
                  { url, type, initiator: "moz-extension://random-uuid-here/" },
                  { includeOtherExtensions: true }
                ),
                "With pref, moz-extension: from unrelated extensions can match"
              );
    
              // Usually, DNR does not affect requests from other extensions. That
              // was checked in the previous test extension (without pref override).
              // Here, we check that with the pref override, testMatchOutcome can
              // return matches from other extensions for the given extension UUID.
              browser.test.assertDeepEq(
                {
                  matchedRules: [
                    { ruleId: 2, rulesetId: "_session", extensionId: "other@ext" },
                  ],
                },
                await dnr.testMatchOutcome(
                  { url, type, initiator: location.origin },
                  { includeOtherExtensions: true }
                ),
                "With pref, moz-extension:-initiator different from DNR ext matches"
              );
    
              // Identical test as in the previous test extension (that ran without
              // the pref override). This verifies that the pref does not affect the
              // behavior of request matching for requests within that extension.
              browser.test.assertDeepEq(
                {
                  matchedRules: [
                    { ruleId: 1, rulesetId: "_session", extensionId: "other@ext" },
                  ],
                },
                await dnr.testMatchOutcome(
                  { url, type, initiator: `moz-extension://${otherExtensionUUID}` },
                  { includeOtherExtensions: true }
                ),
                "With pref, moz-extension: for DNR ext still matches"
              );
    
              browser.test.notifyPass();
            
  },
]);
