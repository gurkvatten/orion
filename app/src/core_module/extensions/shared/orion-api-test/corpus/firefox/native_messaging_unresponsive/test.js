"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let port = browser.runtime.connectNative("wontdie");
    
        const MSG = "echo me";
        // bounce a message to make sure the process actually starts
        port.onMessage.addListener(msg => {
          browser.test.assertEq(msg, MSG, "Received echoed message");
          browser.test.sendMessage("ready");
        });
        port.postMessage(MSG);
      
  },
]);
