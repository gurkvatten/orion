"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.runtime.onMessage.addListener(msg => {
            browser.test.assertEq("msg from child", msg);
            browser.test.sendMessage(
              "sendMessage did not call same-frame onMessage"
            );
          });
    
          browser.test.onMessage.addListener(msg => {
            browser.test.assertEq(
              "sendMessage with a listener in another frame",
              msg
            );
            browser.runtime.sendMessage("should only reach another frame");
          });
    
          await browser.test.assertRejects(
            browser.runtime.sendMessage("should not trigger same-frame onMessage"),
            "Could not establish connection. Receiving end does not exist."
          );
    
          browser.runtime.onConnect.addListener(port => {
            browser.test.assertEq("from-frame", port.name);
            browser.runtime.connect({ name: "from-bg-2" });
          });
    
          await new Promise(resolve => {
            let port = browser.runtime.connect({ name: "from-bg-1" });
            port.onDisconnect.addListener(() => {
              browser.test.assertEq(
                "Could not establish connection. Receiving end does not exist.",
                port.error.message
              );
              resolve();
            });
          });
    
          let anotherFrame = document.createElement("iframe");
          anotherFrame.src = browser.runtime.getURL("extensionpage.html");
          document.body.appendChild(anotherFrame);
        
  },
]);
