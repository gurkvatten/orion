"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let num = 0;
          let ports = {};
          let done = false;
    
          browser.runtime.onConnect.addListener(port => {
            num++;
            ports[num] = port;
    
            port.onMessage.addListener(msg => {
              browser.test.assertEq(msg, "port-2-response", "Got port 2 response");
              browser.test.sendMessage(msg + "-received");
              done = true;
            });
    
            port.onDisconnect.addListener(() => {
              if (port === ports[1]) {
                browser.test.log("Port 1 disconnected, sending message via port 2");
                ports[2].postMessage("port-2-msg");
              } else {
                browser.test.assertTrue(
                  done,
                  "Port 2 disconnected only after a full roundtrip received"
                );
              }
            });
    
            browser.test.sendMessage("port-connect-" + num);
          });
        
  },
]);
