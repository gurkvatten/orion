"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const TEST_CASES = [
            {
              title: "runAt: document_idle",
              params: [
                {
                  id: "script-idle",
                  js: ["script-idle.js"],
                  matches: ["http://*/*/file_sample.html"],
                  runAt: "document_idle",
                  persistAcrossSessions: false,
                },
              ],
            },
            {
              title: "no runAt specified",
              params: [
                {
                  id: "script-idle-default",
                  js: ["script-idle-default.js"],
                  matches: ["http://*/*/file_sample.html"],
                  // `runAt` defaults to `document_idle`.
                  persistAcrossSessions: false,
                },
              ],
            },
            {
              title: "runAt: document_end",
              params: [
                {
                  id: "script-end",
                  js: ["script-end.js"],
                  matches: ["http://*/*/file_sample.html"],
                  runAt: "document_end",
                  persistAcrossSessions: false,
                },
              ],
            },
            {
              title: "runAt: document_start",
              params: [
                {
                  id: "script-start",
                  js: ["script-start.js"],
                  matches: ["http://*/*/file_sample.html"],
                  runAt: "document_start",
                  persistAcrossSessions: false,
                },
              ],
            },
          ];
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(0, scripts.length, "expected no registered script");
    
          for (const { title, params } of TEST_CASES) {
            const res = await browser.scripting.registerContentScripts(params);
            browser.test.assertEq(undefined, res, `${title} - expected no result`);
          }
    
          scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(
            TEST_CASES.length,
            scripts.length,
            `expected ${TEST_CASES.length} registered scripts`
          );
          browser.test.assertEq(
            JSON.stringify([
              {
                id: "script-idle",
                allFrames: false,
                matches: ["http://*/*/file_sample.html"],
                matchOriginAsFallback: false,
                runAt: "document_idle",
                world: "ISOLATED",
                persistAcrossSessions: false,
                js: ["script-idle.js"],
              },
              {
                id: "script-idle-default",
                allFrames: false,
                matches: ["http://*/*/file_sample.html"],
                matchOriginAsFallback: false,
                runAt: "document_idle",
                world: "ISOLATED",
                persistAcrossSessions: false,
                js: ["script-idle-default.js"],
              },
              {
                id: "script-end",
                allFrames: false,
                matches: ["http://*/*/file_sample.html"],
                matchOriginAsFallback: false,
                runAt: "document_end",
                world: "ISOLATED",
                persistAcrossSessions: false,
                js: ["script-end.js"],
              },
              {
                id: "script-start",
                allFrames: false,
                matches: ["http://*/*/file_sample.html"],
                matchOriginAsFallback: false,
                runAt: "document_start",
                world: "ISOLATED",
                persistAcrossSessions: false,
                js: ["script-start.js"],
              },
            ]),
            JSON.stringify(scripts),
            "got expected scripts"
          );
    
          browser.test.sendMessage("background-ready");
        
  },
  async function part_2() {
          await browser.scripting.registerContentScripts([
            {
              id: "main-script-start",
              js: ["main_start.js"],
              matches: ["http://*/*/file_simple_inline_script.html"],
              runAt: "document_start",
              world: "MAIN",
              persistAcrossSessions: false,
            },
            {
              id: "main-script-end",
              js: ["main_end.js"],
              matches: ["http://*/*/file_simple_inline_script.html"],
              runAt: "document_end",
              world: "MAIN",
              persistAcrossSessions: false,
            },
          ]);
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq("MAIN", scripts[0]?.world, "registered world:MAIN");
    
          browser.test.sendMessage("background-ready");
        
  },
  async function part_3() {
          const script = {
            id: "a-script",
            js: ["script.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          };
    
          let results = await Promise.allSettled([
            browser.scripting.registerContentScripts([script]),
            browser.scripting.unregisterContentScripts(),
          ]);
    
          browser.test.assertEq(
            2,
            results.filter(result => result.status === "fulfilled").length,
            "got expected number of fulfilled promises"
          );
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(0, scripts.length, "expected no registered script");
    
          browser.test.sendMessage("background-done");
        
  },
  async function part_4() {
          // We use the same script `id` on purpose in this test.
          let results = await Promise.allSettled([
            browser.scripting.registerContentScripts([
              {
                id: "a-script",
                js: ["script-1.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: false,
              },
            ]),
            browser.scripting.unregisterContentScripts(),
            browser.scripting.registerContentScripts([
              {
                id: "a-script",
                js: ["script-2.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: false,
              },
            ]),
            browser.scripting.unregisterContentScripts(),
            browser.scripting.registerContentScripts([
              {
                id: "a-script",
                js: ["script-3.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: false,
              },
            ]),
          ]);
    
          browser.test.assertEq(
            5,
            results.filter(result => result.status === "fulfilled").length,
            "got expected number of fulfilled promises"
          );
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
    
          browser.test.sendMessage("background-done");
        
  },
  async function part_5() {
          const script = {
            id: "a-script",
            js: ["script-1.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          };
          const updatedScript1 = { ...script, js: ["script-2.js"] };
          const updatedScript2 = { ...script, js: ["script-3.js"] };
    
          let results = await Promise.allSettled([
            browser.scripting.registerContentScripts([script]),
            browser.scripting.updateContentScripts([updatedScript1]),
            browser.scripting.updateContentScripts([updatedScript2]),
            browser.scripting.getRegisteredContentScripts(),
            browser.scripting.unregisterContentScripts(),
            browser.scripting.updateContentScripts([script]),
          ]);
    
          browser.test.assertEq(6, results.length, "expected 6 results");
          browser.test.assertEq(
            "fulfilled",
            results[0].status,
            "expected fulfilled promise (registeredContentScripts)"
          );
          browser.test.assertEq(
            "fulfilled",
            results[1].status,
            "expected fulfilled promise (updateContentScripts)"
          );
          browser.test.assertEq(
            "fulfilled",
            results[2].status,
            "expected fulfilled promise (updateContentScripts)"
          );
          browser.test.assertEq(
            "fulfilled",
            results[3].status,
            "expected fulfilled promise (getRegisteredContentScripts)"
          );
          browser.test.assertEq(
            JSON.stringify([
              {
                id: "a-script",
                allFrames: false,
                matches: ["http://*/*/file_sample.html"],
                matchOriginAsFallback: false,
                runAt: "document_idle",
                world: "ISOLATED",
                persistAcrossSessions: false,
                js: ["script-3.js"],
              },
            ]),
            JSON.stringify(results[3].value),
            "expected updated content script"
          );
          browser.test.assertEq(
            "fulfilled",
            results[4].status,
            "expected fulfilled promise (unregisterContentScripts)"
          );
          browser.test.assertEq(
            "rejected",
            results[5].status,
            "expected rejected promise because script should have been unregistered"
          );
          browser.test.assertEq(
            `Content script with id "${script.id}" does not exist.`,
            results[5].reason.message,
            "expected error message about script not found"
          );
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(0, scripts.length, "expected no registered script");
    
          browser.test.sendMessage("background-done");
        
  },
  async function part_6() {
            await browser.contentScripts.register({
              runAt: "document_start",
              js: [{ file: "testcs.js" }],
              matches: ["<all_urls>"],
            });
    
            const scriptingScripts =
              await browser.scripting.getRegisteredContentScripts();
            browser.test.assertDeepEq(
              [],
              scriptingScripts,
              "Expect an empty array"
            );
            browser.test.sendMessage("background-done");
          
  },
]);
