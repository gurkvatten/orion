"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.userScripts.register([
            {
              id: "world global checker",
              matches: ["*://example.com/dummy"],
              js: [{ file: "world_checker.js" }],
              runAt: "document_start",
              world: "USER_SCRIPT",
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_2() {
          await browser.userScripts.register([
            // The document_start script always runs first, which initializes the
            // middle string that the document_end scripts will prepend/append to,
            // as a sign of execution.
            // The expected value of wrappedJSObject.r below is independent of the
            // execution order of the two blocks of document_end scripts. Test
            // coverage on actual execution order of separate scripts is provided
            // further below by test_default_and_many_non_default_worldIds.
            {
              id: "first scripts",
              matches: ["*://example.com/dummy"],
              js: [
                // All js within one script are guaranteed to run in order, and
                // they should run in the same default sandbox.
                { code: `var a = "";` },
                { code: `a += "1";` },
                { code: `a += "2";` },
                { code: `a += "3";` },
                { code: `a += "4";` },
                { code: `a += "5";` },
                // Prepends a to x, and expose in web page as variable "r".
                { code: `var x = x || ""; x = a + x; window.wrappedJSObject.r=x` },
              ],
              runAt: "document_end",
            },
            {
              id: "separate scripts",
              matches: ["*://example.com/dummy"],
              js: [
                { code: `var b = "";` },
                { code: `b += "6";` },
                { code: `b += "7";` },
                { code: `b += "8";` },
                { code: `b += "9";` },
                // Appends a to x, and expose in web page as variable "r".
                { code: `var x = x || ""; x = x + b; window.wrappedJSObject.r=x` },
              ],
              runAt: "document_end",
            },
            {
              id: "document_start, runs before other document_end scripts",
              matches: ["*://example.com/dummy"],
              // The other scripts prepend and append, and this becomes the middle.
              js: [{ code: `var x = x || "_"; window.wrappedJSObject.r=x` }],
              runAt: "document_start",
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_3() {
        const id = "single user script id";
        function testRegister(props) {
          // ^ Not async, so that callers can test the difference between sync vs
          // async errors from userScripts.register().
          return browser.userScripts.register([
            { id, includeGlobs: ["*"], js: [{ code: "// ..." }], ...props },
          ]);
        }
        async function doUnregister() {
          await browser.userScripts.unregister({ ids: [id] });
        }
    
        try {
          await browser.test.assertRejects(
            testRegister({ worldId: "_" }),
            "Invalid worldId: _",
            "worldId starting with underscore are reserved"
          );
          browser.test.log("worldId containing underscore after start is OK");
          await testRegister({ worldId: "x_" });
          await doUnregister();
    
          await browser.test.assertRejects(
            testRegister({ worldId: "x".repeat(257) }),
            /^Invalid worldId: x{257}$/,
            "Too long worldId is rejected"
          );
          browser.test.log("worldId length of 256 characters is OK");
          await testRegister({ worldId: "x".repeat(256) });
          await doUnregister();
          browser.test.log("worldId length of 256 double-byte characters is OK");
          await testRegister({ worldId: "\u{1234}".repeat(256) });
          await doUnregister();
          // The above shows that we do not count by the number of bytes, but by
          // the JS string length. The following assertion shows that we do not
          // somehow count by the number of Unicode characters.
          await browser.test.assertRejects(
            testRegister({ worldId: "\u{1f00d}".repeat(256) }),
            /^Invalid worldId: \u{1f00d}{256}$/u,
            "worldId length of 256 multi-code unit characters is rejected."
          );
    
          browser.test.assertThrows(
            () => testRegister({ worldId: 123 }),
            /worldId: Expected string instead of 123/,
            "Non-string worldId is rejected."
          );
    
          // Now test that worldId cannot be used with world "MAIN".
          await browser.test.assertRejects(
            testRegister({ world: "MAIN", worldId: "i" }),
            "worldId cannot be used with MAIN world.",
            "Should not support worldId with MAIN world"
          );
    
          await browser.test.assertRejects(
            testRegister({ world: "MAIN", worldId: "i" }),
            "worldId cannot be used with MAIN world.",
            "Should not support worldId with MAIN world"
          );
    
          // And not even with update().
          await testRegister({ world: "MAIN" });
          await browser.test.assertRejects(
            browser.userScripts.update([{ id, worldId: "y" }]),
            "worldId cannot be used with MAIN world.",
            "Should not update worldId to non-default world for world MAIN"
          );
          browser.test.log("Updating worldId + world=USER_SCRIPT at once is OK");
          await browser.userScripts.update([
            { id, world: "USER_SCRIPT", worldId: "y" },
          ]);
          await browser.test.assertRejects(
            browser.userScripts.update([{ id, world: "MAIN" }]),
            "worldId cannot be used with MAIN world.",
            "Should not change world to MAIN when worldId is non-default worldId"
          );
          browser.test.log("Update can set world=MAIN and clear worldId at once");
          await browser.userScripts.update([{ id, world: "MAIN", worldId: "" }]);
          await doUnregister();
        } catch (e) {
          browser.test.fail(`Unexpected error: ${e}`);
        }
        browser.test.sendMessage("done");
      
  },
  async function part_4() {
          const matches = ["*://example.com/dummy"];
          let scripts = [
            {
              id: "define res, so other scripts can push results",
              matches,
              js: [{ code: `window.res = [];` }],
              runAt: "document_start",
              world: "MAIN",
              // worldId not specified - not meaningful for world "MAIN".
            },
            {
              id: "default world (worldId not specified)",
              matches,
              js: [{ code: `var x = x ?? []; x.push(-3)` }],
              runAt: "document_start",
              world: "USER_SCRIPT",
              // worldId not specified = default world.
            },
            {
              id: "default world (worldId is empty string)",
              matches,
              js: [{ code: `var x = x ?? []; x.push(-2)` }],
              runAt: "document_start",
              world: "USER_SCRIPT",
              worldId: "", // worldId "" is the default world.
            },
            {
              id: "default world (worldId is null)",
              matches,
              js: [{ code: `var x = x ?? []; x.push(-1)` }],
              runAt: "document_start",
              world: "USER_SCRIPT",
              worldId: null, // worldId null defaults to default world.
            },
            {
              id: "default world (export result from previous scripts)",
              matches,
              js: [{ code: `window.wrappedJSObject.res.push(...x)` }],
              runAt: "document_end", // Runs after document_start in default world.
              world: "USER_SCRIPT",
              // worldId not specified = default world.
            },
          ];
          // expected result is [-3, -2, -1] is from default world above.
          const expectedResults = [-3, -2, -1]; // plus 1...50 from loop below.
          for (let i = 1; i <= 50; ++i) {
            expectedResults.push(i);
            // The first script initializes "x" if not done so before, the second
            // script exports it to the main world.
            scripts.push({
              id: `user script ${i} at document_start`,
              matches,
              js: [{ code: `var x = x ?? ${i};` }],
              runAt: "document_start",
              world: "USER_SCRIPT",
              worldId: `worldId ${i}`,
            });
            scripts.push({
              id: `user script ${i} at document_end`,
              matches,
              // If worlds were unexpectedly shared, x would be from another script
              // and false would be added to res instead of the number i.
              js: [{ code: `window.wrappedJSObject.res.push(x === ${i} && ${i})` }],
              runAt: "document_end",
              world: "USER_SCRIPT",
              worldId: `worldId ${i}`,
            });
          }
          // If the page loads very fast, it is possible for document_end scripts
          // to still be compiling and not having been executed yet. To ensure that
          // we only check the results after all scripts have run, schedule a
          // document_idle script that runs after everything else.
          scripts.push({
            id: "document_idle, runs after everything else",
            matches: ["*://example.com/dummy"],
            js: [{ code: `window.wrappedJSObject.allTestScriptsRan = true;` }],
            runAt: "document_idle",
          });
          await browser.userScripts.register(scripts);
          browser.test.sendMessage("registered_and_expected", expectedResults);
        
  },
]);
