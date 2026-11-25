"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          // Only register the content script if it wasn't registered before. Since
          // there is only one script, we don't check its ID.
          if (!scripts.length) {
            const script = {
              id: "script",
              js: ["script.js"],
              matches: ["http://*/*/file_sample.html"],
              persistAcrossSessions: true,
            };
    
            await browser.scripting.registerContentScripts([script]);
            browser.test.sendMessage("script-registered");
            return;
          }
    
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
          browser.test.sendMessage("script-already-registered");
        
  },
  async function part_2() {
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          // Only register the content script if it wasn't registered before. Since
          // there is only one script, we don't check its ID.
          if (!scripts.length) {
            const script = {
              id: "script",
              js: ["script.js"],
              matches: ["http://*/*/file_sample.html"],
              persistAcrossSessions: true,
            };
    
            await browser.scripting.registerContentScripts([script]);
            browser.test.sendMessage("script-registered");
            return;
          }
    
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
          await browser.scripting.updateContentScripts([
            { id: scripts[0].id, persistAcrossSessions: false },
          ]);
          browser.test.sendMessage("script-updated");
        
  },
  async function part_3() {
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          // Only register the content script if it wasn't registered before. Since
          // there is only one script, we don't check its ID.
          if (!scripts.length) {
            const script = {
              id: "script",
              js: ["script.js"],
              matches: ["http://*/*/file_sample.html"],
              persistAcrossSessions: true,
            };
    
            await browser.scripting.registerContentScripts([script]);
            browser.test.sendMessage("script-registered");
            return;
          }
    
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
          await browser.scripting.unregisterContentScripts();
          browser.test.sendMessage("script-unregistered");
        
  },
  async function part_4() {
          browser.test.onMessage.addListener(msg => {
            browser.test.assertEq("reload-extension", msg, `expected msg: ${msg}`);
            browser.runtime.reload();
          });
    
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          // Only register the content script if it wasn't registered before. Since
          // there is only one script, we don't check its ID.
          if (!scripts.length) {
            const script = {
              id: "script",
              js: ["script.js"],
              matches: ["http://*/*/file_sample.html"],
              persistAcrossSessions: true,
            };
    
            await browser.scripting.registerContentScripts([script]);
            browser.test.sendMessage("script-registered");
            return;
          }
    
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
          browser.test.sendMessage("script-already-registered");
        
  },
  async function part_5() {
          const script = {
            id: "script",
            js: ["script-1.js"],
            matches: ["http://*/*/file_sample.html"],
            persistAcrossSessions: false,
          };
    
          const scripts = await browser.scripting.getRegisteredContentScripts();
    
          browser.test.onMessage.addListener(async msg => {
            switch (msg) {
              case "persist-script":
                await browser.scripting.updateContentScripts([
                  { id: script.id, persistAcrossSessions: true },
                ]);
                browser.test.sendMessage(`${msg}-done`);
                break;
    
              case "add-new-js":
                await browser.scripting.updateContentScripts([
                  { id: script.id, js: ["script-1.js", "script-2.js"] },
                ]);
                browser.test.sendMessage(`${msg}-done`);
                break;
    
              case "verify-script":
                // We expect a single registered script, which is the one declared
                // above but at this point we should have 2 JS files and the
                // `persistAcrossSessions` option set to `true`.
                browser.test.assertEq(
                  JSON.stringify([
                    {
                      id: script.id,
                      allFrames: false,
                      matches: script.matches,
                      matchOriginAsFallback: false,
                      runAt: "document_idle",
                      world: "ISOLATED",
                      persistAcrossSessions: true,
                      js: ["script-1.js", "script-2.js"],
                    },
                  ]),
                  JSON.stringify(scripts),
                  "expected scripts"
                );
                browser.test.sendMessage(`${msg}-done`);
                break;
    
              default:
                browser.test.fail(`unexpected message: ${msg}`);
            }
          });
    
          // Only register the content script if it wasn't registered before. Since
          // there is only one script, we don't check its ID.
          if (!scripts.length) {
            await browser.scripting.registerContentScripts([script]);
            browser.test.sendMessage("script-registered");
          } else {
            browser.test.sendMessage("script-already-registered");
          }
        
  },
  async function part_6() {
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          if (!scripts.length) {
            await browser.scripting.registerContentScripts([
              {
                id: "0",
                js: ["script-1.js"],
                matches: ["http://*/*/file_sample.html"],
                // We should persist this script by default.
              },
              {
                id: "/",
                js: ["script-2.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: true,
              },
              {
                id: "3",
                js: ["script-3.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: false,
              },
            ]);
            browser.test.sendMessage("scripts-registered");
            return;
          }
    
          browser.test.assertEq(2, scripts.length, "expected 2 registered scripts");
          browser.test.sendMessage("scripts-already-registered");
        
  },
  async function part_7() {
          let scripts = await browser.scripting.getRegisteredContentScripts();
    
          if (!scripts.length) {
            await browser.scripting.registerContentScripts([
              {
                id: "1",
                js: ["script-1.js"],
                matches: ["http://*/*/file_sample.html"],
                // We should persist this script by default.
              },
              {
                id: "2",
                js: ["script-2.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: false,
              },
              {
                id: "\uFFFD 🍕 Boö",
                js: ["script-3.js"],
                matches: ["http://*/*/file_sample.html"],
                persistAcrossSessions: true,
              },
            ]);
            browser.test.sendMessage("scripts-registered");
            return;
          }
    
          browser.test.assertEq(2, scripts.length, "expected 2 registered scripts");
          browser.test.assertEq(
            JSON.stringify(["script-1.js"]),
            JSON.stringify(scripts[0].js),
            "expected a single 'script-1.js' js file"
          );
          browser.test.assertEq(
            "\uFFFD 🍕 Boö",
            scripts[1].id,
            "expected correct ID"
          );
          browser.test.sendMessage("scripts-already-registered");
        
  },
  async function part_8() {
        browser.test.onMessage.addListener(async (msg, ...args) => {
          switch (msg) {
            case "registerContentScripts":
              await browser.scripting.registerContentScripts(...args);
              break;
            case "unregisterContentScripts":
              await browser.scripting.unregisterContentScripts(...args);
              break;
            default:
              browser.test.fail(`Unexpected test message: ${msg}`);
          }
          browser.test.sendMessage(`${msg}:done`);
        });
      
  },
]);
