"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const MATCHES = [
            { id: "script-1", matches: ["<all_urls>"] },
            { id: "script-2", matches: ["file:///*"] },
            { id: "script-3", matches: ["file://*/*dummy_page.html"] },
            { id: "fail-if-executed", matches: ["*://*/*"] },
          ];
    
          await browser.scripting.registerContentScripts(
            MATCHES.map(({ id, matches }) => ({
              id,
              js: [`${id}.js`],
              matches,
              persistAcrossSessions: false,
            }))
          );
    
          browser.test.sendMessage("background-ready");
        
  },
]);
