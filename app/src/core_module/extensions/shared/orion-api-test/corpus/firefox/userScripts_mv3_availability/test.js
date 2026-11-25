"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.assertEq(
            undefined,
            browser.userScripts,
            "Legacy userScripts API unavailable in MV3"
          );
          browser.test.sendMessage("bg_done");
        
  },
  async function part_2() {
          browser.test.assertDeepEq(
            ["UserScriptOptions", "onBeforeScript", "register"],
            Object.keys(browser.userScripts).sort(),
            "MV2 background script should only see legacy userScripts (not MV3) API"
          );
    
          // Due to a quirk of the Schemas internals, any property defined in the
          // schema has a lazy getter on the exported namespace object. Dereference
          // the lazy getter to trigger the full access check to reveal only the
          // APIs that are really available:
          Object.values(browser.userScripts);
    
          browser.test.assertDeepEq(
            ["register"],
            Object.keys(browser.userScripts).sort(),
            "Only the legacy userScripts.register method is available in MV2"
          );
          try {
            let retval = await browser.userScripts.register({
              js: [{ file: "userscript.js" }],
              runAt: "document_end",
              matches: ["*://example.com/dummy"],
            });
            browser.test.assertEq(
              "function",
              typeof retval.unregister,
              "Legacy register() should return object with unregister() method"
            );
            browser.test.assertThrows(
              () => browser.userScripts.register([]),
              "Incorrect argument types for userScripts.register.",
              "Expected error when MV3 userScripts API signature is called"
            );
          } catch (e) {
            browser.test.fail(`Unexpected error: ${e}`);
          }
          browser.test.sendMessage("bg_done");
        
  },
  async function part_3() {
          browser.test.assertTrue(browser.userScripts, "userScripts API is in MV3");
          browser.test.assertEq(
            "function",
            typeof browser.userScripts.register,
            "userScripts.register should be defined in MV3"
          );
          browser.test.assertFalse(
            "onBeforeScript" in browser.userScripts,
            "Legacy userScripts.onBeforeScript is not exposed in MV3 extension"
          );
          browser.test.assertFalse(
            "UserScriptOptions" in browser.userScripts,
            "Legacy userScripts.UserScriptOptions is not exposed in MV3 extension"
          );
    
          // Schema quirk: The type is defined but resolves to nothing.
          browser.test.assertTrue(
            "RegisteredUserScript" in browser.userScripts,
            "New userScripts.RegisteredUserScript is exposed in MV3 extension"
          );
          browser.test.assertEq(
            undefined,
            browser.userScripts.RegisteredUserScript,
            "userScripts.RegisteredUserScript has no value"
          );
    
          browser.test.assertThrows(
            () => {
              browser.userScripts.register({
                js: [{ file: "userscript.js" }],
                runAt: "document_end",
                matches: ["*://example.com/dummy"],
              });
            },
            "Incorrect argument types for userScripts.register.",
            "Expected error when legacy userScripts API signature is called"
          );
          try {
            let retval = await browser.userScripts.register([]);
            browser.test.assertEq(
              undefined,
              retval,
              "userScripts.register() should NOT return an object from legacy API"
            );
          } catch (e) {
            browser.test.fail(`Unexpected error: ${e}`);
          }
          browser.test.sendMessage("bg_done");
        
  },
  async function part_4() {
          browser.test.assertEq(
            undefined,
            browser.userScripts,
            "userScripts API unavailable when off by pref"
          );
          browser.test.sendMessage("bg_done");
        
  },
  async function part_5() {
            let prom;
            browser.test.withHandlingUserInput(() => {
              prom = browser.permissions.request({ permissions: ["userScripts"] });
            });
            await browser.test.assertRejects(
              prom,
              "Cannot request permission userScripts since it was not declared in optional_permissions",
              "userScripts permission cannot be requested when off by pref"
            );
            browser.test.assertEq(undefined, browser.userScripts, "No API please");
            browser.test.sendMessage("done");
          
  },
  async function part_6() {
            let prom;
            browser.test.withHandlingUserInput(() => {
              prom = browser.permissions.request({ permissions: ["userScripts"] });
            });
            browser.test.assertTrue(
              await prom,
              "permissions.request() can grant userScripts permission"
            );
            browser.test.assertTrue(browser.userScripts, "userScripts API granted");
            browser.test.sendMessage("done");
          
  },
]);
