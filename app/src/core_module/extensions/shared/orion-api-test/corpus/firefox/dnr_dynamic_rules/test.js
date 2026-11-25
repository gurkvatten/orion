"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const dnr = browser.declarativeNetRequest;
    
          await dnr.updateDynamicRules({
            addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
          });
    
          const url = "https://example.com/some-dummy-url";
          const type = "font";
          browser.test.assertDeepEq(
            { matchedRules: [{ ruleId: 1, rulesetId: "_dynamic" }] },
            await dnr.testMatchOutcome({ url, type }),
            "Dynamic rule matched after registration"
          );
    
          await dnr.updateDynamicRules({
            removeRuleIds: [
              1,
              1234567890, // Invalid rules should be ignored.
            ],
            addRules: [{ id: 2, condition: {}, action: { type: "block" } }],
          });
          browser.test.assertDeepEq(
            { matchedRules: [{ ruleId: 2, rulesetId: "_dynamic" }] },
            await dnr.testMatchOutcome({ url, type }),
            "Dynamic rule matched after update"
          );
    
          await dnr.updateDynamicRules({ removeRuleIds: [2] });
          browser.test.assertDeepEq(
            { matchedRules: [] },
            await dnr.testMatchOutcome({ url, type }),
            "Dynamic rule not matched after unregistration"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_2() {
          const dnr = browser.declarativeNetRequest;
          const [dyamicRules, sessionRules] = await Promise.all([
            dnr.getDynamicRules(),
            dnr.getSessionRules(),
          ]);
    
          browser.test.assertDeepEq(
            { session: [], dynamic: [] },
            { session: sessionRules, dynamic: dyamicRules },
            "Expect no session and no dynamic rules"
          );
    
          const { MAX_NUMBER_OF_DYNAMIC_RULES } = dnr;
          const DUMMY_RULE = {
            action: { type: "block" },
            condition: { resourceTypes: ["main_frame"] },
          };
          const rules = [];
          for (let i = 0; i < MAX_NUMBER_OF_DYNAMIC_RULES; i++) {
            rules.push({ ...DUMMY_RULE, id: i + 1 });
          }
    
          await browser.test.assertRejects(
            dnr.updateDynamicRules({
              addRules: [
                ...rules,
                { ...DUMMY_RULE, id: MAX_NUMBER_OF_DYNAMIC_RULES + 1 },
              ],
            }),
            `Number of rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_DYNAMIC_RULES.`,
            "Got the expected rejection of exceeding the number of dynamic rules allowed"
          );
    
          await dnr.updateDynamicRules({
            addRules: rules,
          });
          browser.test.assertEq(
            5000,
            (await dnr.getDynamicRules()).length,
            "Got the expected number of dynamic rules stored"
          );
    
          await dnr.updateDynamicRules({
            removeRuleIds: rules.map(r => r.id),
          });
    
          browser.test.assertEq(
            0,
            (await dnr.getDynamicRules()).length,
            "All dynamic rules should have been removed"
          );
    
          browser.test.log(
            "Verify rules count limits with multiple async API calls"
          );
    
          const [updateDynamicRulesSingle, updateDynamicRulesTooMany] =
            await Promise.allSettled([
              dnr.updateDynamicRules({
                addRules: [
                  {
                    ...DUMMY_RULE,
                    id: MAX_NUMBER_OF_DYNAMIC_RULES + 1,
                  },
                ],
              }),
              dnr.updateDynamicRules({ addRules: rules }),
            ]);
    
          browser.test.assertDeepEq(
            updateDynamicRulesSingle,
            { status: "fulfilled", value: undefined },
            "Expect the first updateDynamicRules call to be successful"
          );
    
          await browser.test.assertRejects(
            updateDynamicRulesTooMany?.status === "rejected"
              ? Promise.reject(updateDynamicRulesTooMany.reason)
              : Promise.resolve(),
            `Number of rules in ruleset "_dynamic" exceeds MAX_NUMBER_OF_DYNAMIC_RULES.`,
            "Got the expected rejection on the second call exceeding the number of dynamic rules allowed"
          );
    
          browser.test.assertDeepEq(
            (await dnr.getDynamicRules()).map(rule => rule.id),
            [MAX_NUMBER_OF_DYNAMIC_RULES + 1],
            "Got the expected dynamic rules"
          );
    
          await dnr.updateDynamicRules({
            removeRuleIds: [MAX_NUMBER_OF_DYNAMIC_RULES + 1],
          });
    
          const [updateSessionResult, updateDynamicResult] =
            await Promise.allSettled([
              dnr.updateSessionRules({ addRules: rules }),
              dnr.updateDynamicRules({ addRules: rules }),
            ]);
    
          browser.test.assertDeepEq(
            updateDynamicResult,
            { status: "fulfilled", value: undefined },
            "Expect the number of dynamic rules to be still allowed, despite the session rule added"
          );
    
          // NOTE: In this test we do not exceed the quota of session rules. The
          // updateSessionRules call here is to verify that the quota of session and
          // dynamic rules are separate. The limits for session rules are tested
          // by session_rules_total_rule_limit in test_ext_dnr_session_rules.js.
          browser.test.assertDeepEq(
            updateSessionResult,
            { status: "fulfilled", value: undefined },
            "Got expected success from the updateSessionRules request"
          );
    
          browser.test.assertDeepEq(
            { sessionRulesCount: 5000, dynamicRulesCount: 5000 },
            {
              sessionRulesCount: (await dnr.getSessionRules()).length,
              dynamicRulesCount: (await dnr.getDynamicRules()).length,
            },
            "Got expected session and dynamic rules counts"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_3() {
          const dnr = browser.declarativeNetRequest;
    
          browser.test.onMessage.addListener(async (msg, ...args) => {
            switch (msg) {
              case "createDynamicRules": {
                const [{ updateRuleOptions }] = args;
                await dnr.updateDynamicRules(updateRuleOptions);
                break;
              }
              case "assertGetDynamicRulesCount": {
                const [{ expectedRulesCount }] = args;
                browser.test.assertEq(
                  expectedRulesCount,
                  (await dnr.getDynamicRules()).length,
                  "getDynamicRules() resolves to the expected number of dynamic rules"
                );
                break;
              }
              default:
                browser.test.fail(
                  `Got unexpected unhandled test message: "${msg}"`
                );
                break;
            }
            browser.test.sendMessage(`${msg}:done`);
          });
          browser.test.sendMessage("bgpage:ready");
        
  },
  async function part_4() {
          browser.test.assertEq(
            browser.declarativeNetRequest,
            undefined,
            "Expect DNR API namespace to not be available"
          );
          browser.test.sendMessage("bgpage:ready");
        
  },
  async function part_5() {
          const dnr = browser.declarativeNetRequest;
    
          browser.runtime.onInstalled.addListener(async () => {
            browser.test.assertDeepEq(
              await dnr.getDynamicRules(),
              [],
              "No DNR rules at install time"
            );
            browser.test.sendMessage("onInstalled");
          });
          browser.test.onMessage.addListener(async msg => {
            if (msg === "get_rule_count") {
              browser.test.sendMessage(
                "get_rule_count:done",
                (await dnr.getDynamicRules()).length
              );
              return;
            } else if (msg === "zero_to_one_rule") {
              await dnr.updateDynamicRules({
                addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
              });
            } else if (msg === "one_to_zero_rules") {
              await dnr.updateDynamicRules({ removeRuleIds: [1] });
            } else {
              browser.test.fail(`Unexpected message: ${msg}`);
            }
            browser.test.sendMessage(`${msg}:done`);
          });
          browser.runtime.onStartup.addListener(async () => {
            browser.test.sendMessage("onStartup");
          });
        
  },
  async function part_6() {
          const dnr = browser.declarativeNetRequest;
    
          browser.test.onMessage.addListener(async (msg, ...args) => {
            switch (msg) {
              case "getDynamicRules": {
                browser.test.sendMessage(
                  `${msg}:done`,
                  await dnr.getDynamicRules()
                );
                break;
              }
              case "updateDynamicRules": {
                const { addRules, removeRuleIds } = args[0];
                await dnr.updateDynamicRules({
                  addRules,
                  removeRuleIds,
                });
                browser.test.sendMessage(
                  `${msg}:done`,
                  await dnr.getDynamicRules()
                );
                break;
              }
              default: {
                browser.test.fail(`Unexpected test message: ${msg}`);
                browser.test.sendMessage(`${msg}:done`);
                break;
              }
            }
          });
          browser.test.sendMessage("bgpage:ready");
        
  },
]);
