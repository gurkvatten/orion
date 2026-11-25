"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.runtime.onMessage.addListener(msg => {
          browser.test.assertEq(
            msg,
            "ping",
            "Background page received ping message"
          );
          return Promise.resolve("pong");
        });
    
        // addListener() returns right away but make a round trip to the
        // main process to ensure the persistent onMessage listener is recorded.
        await browser.runtime.getBrowserInfo();
        browser.test.sendMessage("bg-ran");
      
  },
  async function part_2() {
        browser.runtime.onConnect.addListener(port => {
          port.onMessage.addListener(msg => {
            browser.test.assertEq(
              msg,
              "ping",
              "Background page received ping message"
            );
            port.postMessage("pong");
          });
        });
    
        // addListener() returns right away but make a round trip to the
        // main process to ensure the persistent onMessage listener is recorded.
        await browser.runtime.getBrowserInfo();
        browser.test.sendMessage("bg-ran");
      
  },
  async function part_3() {
          browser.runtime.onMessage.addListener(() => {
            browser.test.notifyPass("startup");
          });
    
          // addListener() returns right away but make a round trip to the
          // main process to ensure the persistent onMessage listener is recorded.
          await browser.runtime.getBrowserInfo();
          browser.test.sendMessage("bg-ran");
        
  },
]);
