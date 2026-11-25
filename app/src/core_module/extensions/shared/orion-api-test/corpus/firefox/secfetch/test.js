"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let site = await new Promise(resolve => {
            browser.test.onMessage.addListener(msg => {
              resolve(msg);
            });
          });
    
          let results = await Promise.all([
            fetch(`${site}/return_headers`).then(res => res.json()),
            // Non GET or HEAD request, triggers CORS preflight.
            fetch(`${site}/return_headers`, { method: "PUT" }).then(res =>
              res.json()
            ),
          ]);
          browser.test.sendMessage("background_results", results);
        
  },
]);
