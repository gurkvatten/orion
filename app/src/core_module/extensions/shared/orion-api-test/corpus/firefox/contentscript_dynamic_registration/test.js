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
              js: ["script.js"],
              matches: ["http://*/*/*.html"],
              persistAcrossSessions: false,
            };
    
            await browser.scripting.registerContentScripts([script]);
    
            browser.test.sendMessage("background-done");
          
  },
  async function part_2() {
            await browser.contentScripts.register({
              js: [{ file: "script.js" }],
              matches: ["http://*/*/*.html"],
            });
    
            browser.test.sendMessage("background-done");
          
  },
  async function part_3() {
            await browser.userScripts.register({
              js: [{ file: "script.js" }],
              matches: ["http://*/*/*.html"],
            });
    
            browser.test.sendMessage("background-done");
          
  },
]);
