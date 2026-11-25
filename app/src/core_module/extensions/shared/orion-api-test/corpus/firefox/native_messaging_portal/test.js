"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let port;
      browser.test.onMessage.addListener(async (what, payload) => {
        if (what == "request") {
          await browser.permissions.request({ permissions: ["nativeMessaging"] });
          // connectNative requires permission
          port = browser.runtime.connectNative("echo");
          port.onMessage.addListener(msg => {
            browser.test.sendMessage("message", msg);
          });
          browser.test.sendMessage("ready");
        } else if (what == "send") {
          if (payload._json) {
            let json = payload._json;
            payload.toJSON = () => json;
            delete payload._json;
          }
          port.postMessage(payload);
        }
      });
  },
]);
