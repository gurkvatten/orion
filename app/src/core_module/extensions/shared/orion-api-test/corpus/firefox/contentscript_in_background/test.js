"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const FRAME_URL = "http://example.com:8888/dummyFrame";
        await browser.contentScripts.register({
          matches: ["http://example.com/dummyFrame"],
          js: [{ file: "contentscript.js" }],
          allFrames: true,
        });
    
        let f = document.createElement("iframe");
        f.src = FRAME_URL;
        document.body.appendChild(f);
      
  },
]);
