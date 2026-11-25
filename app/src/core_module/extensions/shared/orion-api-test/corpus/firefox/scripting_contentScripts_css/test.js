"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          // This script is injected in all frames after the styles so that we can
          // verify the registered styles.
          const checkAppliedStyleScript = {
            id: "check-applied-styles",
            allFrames: true,
            matches: ["http://*/*/*.html"],
            matchOriginAsFallback: false,
            runAt: "document_idle",
            world: "ISOLATED",
            persistAcrossSessions: false,
            js: ["check-applied-styles.js"],
          };
    
          // Listen to the `load-test-case` message and unregister/register new
          // content scripts.
          browser.test.onMessage.addListener(async (msg, data) => {
            switch (msg) {
              case "load-test-case":
                const { title, params, skipCheckScriptRegistration } = data;
                const expectedScripts = [];
    
                await browser.scripting.unregisterContentScripts();
    
                if (!skipCheckScriptRegistration) {
                  await browser.scripting.registerContentScripts([
                    checkAppliedStyleScript,
                  ]);
    
                  expectedScripts.push(checkAppliedStyleScript);
                }
    
                expectedScripts.push(...params);
    
                const res = await browser.scripting.registerContentScripts(params);
                browser.test.assertEq(
                  res,
                  undefined,
                  `${title} - expected no result`
                );
                const scripts =
                  await browser.scripting.getRegisteredContentScripts();
                browser.test.assertEq(
                  expectedScripts.length,
                  scripts.length,
                  `${title} - expected ${expectedScripts.length} registered scripts`
                );
                browser.test.assertEq(
                  JSON.stringify(expectedScripts),
                  JSON.stringify(scripts),
                  `${title} - got expected registered scripts`
                );
    
                browser.test.sendMessage(`${msg}-done`);
                break;
              default:
                browser.test.fail(`received unexpected message: ${msg}`);
            }
          });
    
          browser.test.sendMessage("background-ready");
        
  },
]);
