"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let closedPromise = new Promise(resolve => {
            browser.notifications.onClosed.addListener(resolve);
          });
          let createdId = await browser.notifications.create("notifid", {
            type: "basic",
            title: "titl",
            message: "msg",
          });
          let closedId = await closedPromise;
          browser.test.assertEq(createdId, closedId, "ID of closed notification");
          browser.test.assertEq(
            "{}",
            JSON.stringify(await browser.notifications.getAll()),
            "no notifications left"
          );
          browser.test.sendMessage("notification_closed");
        
  },
]);
