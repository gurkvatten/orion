"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener((...args) => {
            browser.runtime.sendMessage(...args);
          });
    
          let frame = document.createElement("iframe");
          frame.src = "page.html";
          document.body.appendChild(frame);
        
  },
  async function part_2() {
          browser.runtime.onMessageExternal.addListener((msg, sender) => {
            browser.test.sendMessage("received-external", { msg, sender });
          });
        
  },
  async function part_3() {
          await browser.test.assertRejects(
            browser.runtime.sendMessage("badid@test-extension", "fake-message"),
            /Could not establish connection. Receiving end does not exist./,
            "Got the expected error message on sendMessage to badid ext"
          );
          browser.test.sendMessage("test-done");
        
  },
]);
