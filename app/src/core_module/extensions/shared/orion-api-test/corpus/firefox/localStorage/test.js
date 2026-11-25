"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let hasRun = localStorage.getItem("has-run");
      let result;
      if (!hasRun) {
        localStorage.setItem("has-run", "yup");
        localStorage.setItem("test-item", "item1");
        result = "item1";
      } else {
        let data = localStorage.getItem("test-item");
        if (data == "item1") {
          localStorage.setItem("test-item", "item2");
          result = "item2";
        } else if (data == "item2") {
          localStorage.removeItem("test-item");
          result = "deleted";
        } else if (!data) {
          localStorage.clear();
          result = "cleared";
        }
      }
      browser.test.sendMessage("result", result);
      browser.test.notifyPass("localStorage");
  },
]);
