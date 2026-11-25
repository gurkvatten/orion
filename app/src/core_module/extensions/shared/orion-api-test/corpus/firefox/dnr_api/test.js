"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          async function assertTestMatchOutcomeEnabled(expected, description) {
            let enabled;
            try {
              // testAvailability already checks the errors etc, so here we only
              // care about the method working vs not working.
              await browser.declarativeNetRequest.testMatchOutcome({
                url: "https://example.com/",
                type: "other",
              });
              enabled = true;
            } catch (e) {
              enabled = false;
            }
            browser.test.assertEq(expected, enabled, description);
          }
    
          await assertTestMatchOutcomeEnabled(false, "disabled when not granted");
    
          await new Promise(resolve => {
            // browser.test.withHandlingUserInput would have been simpler, but due
            // to bug 1598804 it cannot be used.
            browser.test.onMessage.addListener(async msg => {
              browser.test.assertEq("withHandlingUserInput_ok", msg, "Resuming");
              await browser.permissions.request({
                permissions: ["declarativeNetRequestFeedback"],
              });
              browser.test.sendMessage("withHandlingUserInput_done");
              resolve();
            });
            browser.test.sendMessage("withHandlingUserInput_wanted");
          });
    
          await assertTestMatchOutcomeEnabled(true, "enabled by permission");
    
          await browser.permissions.remove({
            permissions: ["declarativeNetRequestFeedback"],
          });
          await assertTestMatchOutcomeEnabled(false, "disabled after perm removal");
    
          browser.test.sendMessage("done");
        
  },
  async function part_2() {
          browser.test.assertEq(
            "_dynamic",
            browser.declarativeNetRequest.DYNAMIC_RULESET_ID,
            "Value of DYNAMIC_RULESET_ID constant"
          );
          browser.test.assertEq(
            "_session",
            browser.declarativeNetRequest.SESSION_RULESET_ID,
            "Value of SESSION_RULESET_ID constant"
          );
          const {
            GUARANTEED_MINIMUM_STATIC_RULES,
            MAX_NUMBER_OF_STATIC_RULESETS,
            MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
            MAX_NUMBER_OF_DISABLED_STATIC_RULES,
            MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES,
            MAX_NUMBER_OF_DYNAMIC_RULES,
            MAX_NUMBER_OF_SESSION_RULES,
            MAX_NUMBER_OF_REGEX_RULES,
          } = browser.declarativeNetRequest;
          browser.test.sendMessage("dnr-namespace-properties", {
            GUARANTEED_MINIMUM_STATIC_RULES,
            MAX_NUMBER_OF_STATIC_RULESETS,
            MAX_NUMBER_OF_ENABLED_STATIC_RULESETS,
            MAX_NUMBER_OF_DISABLED_STATIC_RULES,
            MAX_NUMBER_OF_DYNAMIC_AND_SESSION_RULES,
            MAX_NUMBER_OF_DYNAMIC_RULES,
            MAX_NUMBER_OF_SESSION_RULES,
            MAX_NUMBER_OF_REGEX_RULES,
          });
        
  },
]);
