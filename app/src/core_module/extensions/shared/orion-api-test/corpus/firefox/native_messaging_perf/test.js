"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(msg => {
            if (msg != "run-tests") {
              return;
            }
    
            let port = browser.runtime.connectNative("echo");
    
            function next() {
              port.postMessage({
                Lorem: {
                  ipsum: {
                    dolor: [
                      "sit amet",
                      "consectetur adipiscing elit",
                      "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
                    ],
                    "Ut enim": [
                      "ad minim veniam",
                      "quis nostrud exercitation ullamco",
                      "laboris nisi ut aliquip ex ea commodo consequat.",
                    ],
                    Duis: [
                      "aute irure dolor in reprehenderit in",
                      "voluptate velit esse cillum dolore eu",
                      "fugiat nulla pariatur.",
                    ],
                    Excepteur: [
                      "sint occaecat cupidatat non proident",
                      "sunt in culpa qui officia deserunt",
                      "mollit anim id est laborum.",
                    ],
                  },
                },
              });
            }
    
            const COUNT = 1000;
            let now;
            function finish() {
              let roundTripTime = (Date.now() - now) / COUNT;
    
              port.disconnect();
              browser.test.sendMessage("result", roundTripTime);
            }
    
            let count = 0;
            port.onMessage.addListener(() => {
              if (count == 0) {
                // Skip the first round, since it includes the time it takes
                // the app to start up.
                now = Date.now();
              }
    
              if (count++ <= COUNT) {
                next();
              } else {
                finish();
              }
            });
    
            next();
          });
        
  },
]);
