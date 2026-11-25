"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let received_ports_number = 0;
    
      const expected_received_ports_number = 1;
    
      function countReceivedPorts(port) {
        received_ports_number++;
    
        if (port.name == "check-results") {
          browser.runtime.onConnect.removeListener(countReceivedPorts);
    
          browser.test.assertEq(
            expected_received_ports_number,
            received_ports_number,
            "invalid connect should not create a port"
          );
    
          browser.test.notifyPass("runtime.connect invalid params");
        }
      }
    
      browser.runtime.onConnect.addListener(countReceivedPorts);
    
      let childFrame = document.createElement("iframe");
      childFrame.src = "extensionpage.html";
      document.body.appendChild(childFrame);
  },
]);
