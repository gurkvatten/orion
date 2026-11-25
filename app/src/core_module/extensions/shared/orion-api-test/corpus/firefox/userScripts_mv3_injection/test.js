"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async msg => {
            browser.test.assertEq("revoke_permission", msg, "Expected msg");
            await browser.permissions.remove({ permissions: ["userScripts"] });
            browser.test.assertEq(undefined, browser.userScripts, "API gone");
            browser.test.sendMessage("revoke_permission:done");
          });
          await browser.userScripts.register([
            {
              id: "basic",
              matches: ["*://example.com/resultCollector"],
              js: [
                { code: "window.resultCollector ??= [];" },
                { code: "resultCollector.push('1.code');dump('1.code ran\\n');" },
                { code: "resultCollector.push('2.code');dump('2.code ran\\n');" },
                { file: "3.file.js" },
                { code: "resultCollector.push('4.code');dump('4.code ran\\n');" },
                { code: "resultCollector.push('5.code');dump('5.code ran\\n');" },
                { file: "6.file.js" },
              ],
              runAt: "document_start",
              world: "MAIN",
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_2() {
          await browser.userScripts.register([
            {
              id: "basic",
              matches: ["*://*/resultCollector"],
              js: [
                {
                  code: `window.resultCollector ??= [];
                         resultCollector.push(origin);`,
                },
              ],
              runAt: "document_start",
              world: "MAIN",
            },
            {
              // For extra coverage use includeGlobs without matches, since most
              // other tests do use "matches". The underlying userScripts-specific
              // matches+includeGlobs matching logic is extensively covered by
              // test_WebExtensionContentScript_isUserScript in
              // test_WebExtensionContentScript.js.
              id: "includeGlobs without matches",
              includeGlobs: ["*resultCollector"],
              js: [{ code: "resultCollector.push(origin)" }],
              runAt: "document_start",
              world: "MAIN",
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_3() {
          async function register() {
            await browser.userScripts.register([
              {
                id: "userScripts ID (for register and update)",
                matches: ["*://example.com/resultCollector"],
                js: [
                  { file: "1.file.js" },
                  // 1.file.js defines the "resultCollector_push" function, and that
                  // function should be available to the other scripts since they all
                  // run in the same USER_SCRIPT world.
                  { code: "resultCollector_push('2.code');dump('1.code ran\\n');" },
                  { file: "3.file.js" },
                  { code: "resultCollector_push('4.code');dump('4.code ran\\n');" },
                ],
                runAt: "document_start",
                world: "USER_SCRIPT",
              },
            ]);
          }
          async function update() {
            await browser.userScripts.update([
              {
                id: "userScripts ID (for register and update)",
                js: [
                  { file: "1.file.js" },
                  { code: "resultCollector_push('2.updated');" },
                ],
              },
            ]);
          }
    
          browser.test.onMessage.addListener(async msg => {
            browser.test.assertEq("update_userScripts", msg, "Expected msg");
            await update();
            browser.test.sendMessage("update_userScripts:done");
          });
          await register();
          browser.test.sendMessage("registered");
        
  },
]);
