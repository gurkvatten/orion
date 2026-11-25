"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.log("background script executed");
    
          browser.test.sendMessage("background-script-load");
    
          let img = document.createElement("img");
          img.src =
            "data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7";
          document.body.appendChild(img);
    
          img.onload = () => {
            browser.test.log("image loaded");
    
            let iframe = document.createElement("iframe");
            iframe.src = "about:blank?1";
    
            iframe.onload = () => {
              browser.test.log("iframe loaded");
              setTimeout(() => {
                browser.test.notifyPass("background sub-window test done");
              }, 0);
            };
            document.body.appendChild(iframe);
          };
        
  },
]);
