"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const scriptIn = {
            id: "test1",
            matches: ["https://example.com/*"],
            js: [{ file: "file1.js" }, { file: "file2.js" }],
          };
          const expectedScriptOut = {
            ...scriptIn,
            allFrames: false,
            excludeMatches: null,
            includeGlobs: null,
            excludeGlobs: null,
            runAt: "document_idle",
            // Notable call-out: All other content script APIs default to "ISOLATED"
            // as the default world. In the userScripts API, we want to default to
            // "USER_SCRIPT" when the world is not specified.
            world: "USER_SCRIPT",
            worldId: "",
          };
          browser.runtime.onInstalled.addListener(async ({ reason }) => {
            browser.test.assertEq("install", reason, "onInstalled reason");
            await browser.userScripts.register([scriptIn]);
            browser.test.assertDeepEq(
              [expectedScriptOut],
              await browser.userScripts.getScripts(),
              "getScripts() after register"
            );
            browser.test.sendMessage("onInstalled");
          });
          browser.test.onMessage.addListener(async msg => {
            browser.test.assertEq("testStillRegistered", msg, "Expected message");
            browser.test.assertDeepEq(
              [expectedScriptOut],
              await browser.userScripts.getScripts(),
              "getScripts() still finds scripts"
            );
            browser.test.sendMessage("testStillRegistered:done");
          });
        
  },
  async function part_2() {
          const id = "userScript_id";
          const scriptDefaults = {
            id,
            allFrames: false,
            js: [],
            // matches or includeGlobs must be non-empty, we cannot use [] here.
            matches: ["https://example.com/*"],
            excludeMatches: [],
            includeGlobs: [],
            excludeGlobs: [],
            runAt: "document_idle",
            world: "USER_SCRIPT",
            worldId: "",
          };
          const scriptDefaultsOut = {
            ...scriptDefaults,
            // An input of [] is equivalent to omitted, which is returned as null.
            // Chrome does the same.
            excludeMatches: null,
            includeGlobs: null,
            excludeGlobs: null,
          };
          // Keep this object in sync with expectedScriptsInContent below.
          const scriptNonDefaults = {
            id,
            allFrames: true,
            js: [
              { code: "// Hello world!" },
              { file: "file1.js" },
              { code: "//".repeat(1000) },
            ],
            matches: ["https://example.org/path/*"],
            excludeMatches: ["*://*/excludeme"],
            includeGlobs: ["*"],
            excludeGlobs: ["*excludeme*"],
            runAt: "document_end",
            world: "MAIN",
            // We cannot test a non-default worldId, because worldId can only take
            // a non-empty string with world USER_SCRIPT. Test coverage for worldId
            // is in test_ext_userScripts_mv3_worlds.js, test_worldId_validation.
            worldId: "",
          };
    
          const nullUpdate = Object.fromEntries(
            Object.keys(scriptDefaults).map(k => [k, null])
          );
    
          async function testRegisterDefaults() {
            await browser.userScripts.register([scriptDefaults]);
            browser.test.assertDeepEq(
              [scriptDefaultsOut],
              await browser.userScripts.getScripts(),
              "getScripts() after register with near-default properties"
            );
    
            await browser.userScripts.update([{ id }]);
            browser.test.assertDeepEq(
              [scriptDefaultsOut],
              await browser.userScripts.getScripts(),
              "userScripts.update call without update should not change anything"
            );
    
            await browser.userScripts.update([{ ...nullUpdate, id }]);
            browser.test.assertDeepEq(
              [scriptDefaultsOut],
              await browser.userScripts.getScripts(),
              "userScripts.update with all fields null should not change anything"
            );
    
            await browser.userScripts.update([scriptNonDefaults]);
            browser.test.assertDeepEq(
              [scriptNonDefaults],
              await browser.userScripts.getScripts(),
              "userScripts.update can change every property to a non-default value"
            );
    
            await browser.userScripts.update([scriptDefaults]);
            browser.test.assertDeepEq(
              [scriptDefaultsOut],
              await browser.userScripts.getScripts(),
              "userScripts.update restores non-default values when not null"
            );
          }
    
          async function testRegisterNonDefaults() {
            await browser.userScripts.register([scriptNonDefaults]);
            browser.test.assertDeepEq(
              [scriptNonDefaults],
              await browser.userScripts.getScripts(),
              "getScripts() after register with non-default properties"
            );
    
            await browser.userScripts.update([{ id }]);
            browser.test.assertDeepEq(
              [scriptNonDefaults],
              await browser.userScripts.getScripts(),
              "userScripts.update call without update should not change anything"
            );
    
            await browser.userScripts.update([{ ...nullUpdate, id }]);
            browser.test.assertDeepEq(
              [scriptNonDefaults],
              await browser.userScripts.getScripts(),
              "userScripts.update with all fields null should not change anything"
            );
    
            await browser.userScripts.update([scriptDefaults]);
            browser.test.assertDeepEq(
              [scriptDefaultsOut],
              await browser.userScripts.getScripts(),
              "userScripts.update can change every property to a near-default value"
            );
    
            // Sanity check that we can register the original js/code again:
            await browser.userScripts.update([scriptNonDefaults]);
            browser.test.assertDeepEq(
              [scriptNonDefaults],
              await browser.userScripts.getScripts(),
              "userScripts.update can restore original non-default values"
            );
          }
    
          browser.runtime.onInstalled.addListener(async () => {
            await testRegisterDefaults();
            await browser.userScripts.unregister({ ids: [id] });
            await testRegisterNonDefaults();
            browser.test.sendMessage("start_done");
          });
        
  },
  async function part_3() {
        try {
          const scriptBase = {
            matches: ["https://example.com/*"],
            js: [{ code: "// original" }, { file: "file1.js" }],
          };
          // For extra coverage, verify that we can call these APIs without
          // awaiting the promise. They are not expected to interfere with each
          // other.
          let promiseDuplicateSameCall = browser.userScripts.register([
            { ...scriptBase, id: "valid:ignore_me_1" },
            { ...scriptBase, id: "duplic" },
            { ...scriptBase, id: "duplic" },
            { ...scriptBase, id: "valid:ignore_me_2" },
          ]);
          let promiseReservedId = browser.userScripts.register([
            { ...scriptBase, id: "valid:ignore_me_3" },
            { ...scriptBase, id: "_reserved" },
            { ...scriptBase, id: "valid:ignore_me_4" },
          ]);
          let promiseValidBefore = browser.userScripts.register([
            { ...scriptBase, id: "validBefore1" },
            { ...scriptBase, id: "validBefore2" },
          ]);
          let promiseDuplicateBefore = browser.userScripts.register([
            { ...scriptBase, id: "validBefore1" }, // duplicate
          ]);
          let promiseUpdateNonExistent = browser.userScripts.update([
            { id: "validBefore1", js: [{ code: "// ignore me 1" }] },
            { ...scriptBase, id: "non_existent" },
          ]);
          let promiseUpdateDuplicate = browser.userScripts.update([
            { id: "validBefore1", js: [{ code: "// ignore me 2" }] },
            { id: "validBefore2", js: [{ code: "// ignore me 3" }] },
            { id: "validBefore2", js: [{ code: "// ignore me 4" }] },
          ]);
          let promiseUpdateNoMatchesNorGlobs = browser.userScripts.update([
            { id: "validBefore1", js: [{ code: "// ignore me 5" }] },
            { id: "validBefore2", matches: [] }, // = matches + includeGlobs empty
          ]);
    
          await browser.test.assertRejects(
            promiseDuplicateSameCall,
            "Duplicate script id: duplic",
            "userScripts.register() should reject two scripts with same id"
          );
          await browser.test.assertRejects(
            promiseReservedId,
            "Invalid id for RegisteredUserScript.",
            "userScripts.register() should reject script with reserved ID"
          );
    
          browser.test.assertEq(
            await promiseValidBefore,
            undefined,
            "userScripts.register() succeeded (validBefore1 + validBefore2)"
          );
          await browser.test.assertRejects(
            promiseDuplicateBefore,
            `User script with id "validBefore1" is already registered.`,
            "userScripts.register() should fail for existing ID"
          );
          await browser.test.assertRejects(
            promiseUpdateNonExistent,
            `User script with id "non_existent" does not exist.`,
            "userScripts.update() should fail for non-existing ID"
          );
          await browser.test.assertRejects(
            promiseUpdateDuplicate,
            "Duplicate script id: validBefore2",
            "userScripts.update() should reject two scripts with same id"
          );
          await browser.test.assertRejects(
            promiseUpdateNoMatchesNorGlobs,
            "matches or includeGlobs must be specified.",
            "userScripts.update() should reject update resulting in no matches"
          );
    
          const summarize = ({ id, matches, js }) => ({ id, matches, js });
          browser.test.assertDeepEq(
            [
              { ...scriptBase, id: "validBefore1" },
              { ...scriptBase, id: "validBefore2" },
            ],
            (await browser.userScripts.getScripts()).map(summarize),
            "When failures happen, unrelated scripts should not register."
          );
    
          // To complement promiseUpdateNoMatchesNorGlobs, verify that we can
          // indeed set to an empty matches, when includeGlobs is set.
          await browser.userScripts.update([
            { id: "validBefore2", matches: [], includeGlobs: ["ht*"] },
          ]);
          browser.test.assertDeepEq(
            [
              {
                matches: null,
                js: scriptBase.js,
                id: "validBefore2",
                includeGlobs: ["ht*"],
              },
            ],
            Array.from(
              await browser.userScripts.getScripts({ ids: ["validBefore2"] }),
              s => ({ ...summarize(s), includeGlobs: s.includeGlobs })
            ),
            "userScripts.update() with empty matches is OK if includeGlobs is set."
          );
    
          await browser.userScripts.unregister({
            ids: ["not_existing_start", "validBefore2", "not_existing_end"],
          });
    
          browser.test.assertDeepEq(
            [{ ...scriptBase, id: "validBefore1" }],
            (await browser.userScripts.getScripts()).map(summarize),
            "unregister() with non-existing IDs is not an error."
          );
        } catch (e) {
          browser.test.fail(`Unexpected failure: ${e}`);
        }
        browser.test.sendMessage("done");
      
  },
]);
