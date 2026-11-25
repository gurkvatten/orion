"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let port = browser.runtime.connect();
        port.onDisconnect.addListener(() => {
          browser.test.assertEq(
            "Could not establish connection. Receiving end does not exist.",
            port.error && port.error.message
          );
          browser.test.notifyPass("port.onDisconnect was called");
        });
      
  },
]);
