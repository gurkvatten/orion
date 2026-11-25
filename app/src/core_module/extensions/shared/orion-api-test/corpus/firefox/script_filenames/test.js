"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.sendMessage("background-script-loaded");
      
  },
  async function part_2() {
          const script = document.createElement("script");
          script.src = "./invalid_background.xyz";
          document.head.appendChild(script);
    
          const validScript = document.createElement("script");
          validScript.src = "./background.js";
          document.head.appendChild(validScript);
        
  },
  async function part_3() {
        const script = document.createElement("script");
        script.src = "./invalid_background.xyz";
        document.head.appendChild(script);
    
        const validScript = document.createElement("script");
        validScript.src = "./injected_background.js";
        document.head.appendChild(validScript);
      
  },
  async function part_4() {
          await browser.contentScripts.register({
            js: [{ file: "/content_script" }],
            matches: ["<all_urls>"],
          });
          browser.test.sendMessage("background-script-loaded");
        
  },
]);
