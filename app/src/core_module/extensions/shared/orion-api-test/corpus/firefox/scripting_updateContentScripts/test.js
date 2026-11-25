"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const script = {
            id: "a-script",
            js: ["script-1.js"],
            matches: ["http://*/*/*.html"],
            persistAcrossSessions: false,
          };
    
          await browser.scripting.registerContentScripts([script]);
          await browser.scripting.updateContentScripts([
            {
              id: script.id,
              js: ["script-2.js"],
            },
          ]);
          let scripts = await browser.scripting.getRegisteredContentScripts();
          browser.test.assertEq(1, scripts.length, "expected 1 registered script");
    
          browser.test.sendMessage("background-ready");
        
  },
  async function part_2() {
            const script = {
              id: "a-script",
              allFrames: true,
              matches: ["http://*/*/*.html"],
              matchOriginAsFallback: true,
              runAt: "document_start",
              world: "MAIN",
              persistAcrossSessions: false,
              css: ["style.js"],
              excludeMatches: ["http://*/*/foobar.html"],
              js: ["script.js"],
            };
    
            await browser.scripting.registerContentScripts([script]);
    
            // This should not modify the previously registered script.
            await browser.scripting.updateContentScripts([{ id: script.id }]);
    
            let scripts = await browser.scripting.getRegisteredContentScripts();
            browser.test.assertEq(
              JSON.stringify([script]),
              JSON.stringify(scripts),
              "expected unmodified registered script"
            );
    
            browser.test.sendMessage("background-done");
          
  },
]);
