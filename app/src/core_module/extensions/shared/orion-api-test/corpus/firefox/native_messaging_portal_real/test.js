"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      try {
        let res = await browser.runtime.sendNativeMessage(
          "pingpong_nativemessaging_test",
          "ping"
        );
        browser.test.assertEq(
          "native app sends pong",
          res,
          "Expected reply from native messaging host"
        );
        await browser.test.assertRejects(
          browser.runtime.sendNativeMessage(
            "non_existent_nativemessaging_host",
            "ping"
          ),
          "No such native application non_existent_nativemessaging_host",
          "Expected error for non-existing native messaging host"
        );
      } catch (e) {
        browser.test.fail(`Unexpected error: ${e}`);
      }
      browser.test.sendMessage("done");
  },
]);
