"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      browser.runtime.onPerformanceWarning.addListener(details => {
        browser.test.assertDeepEq(
          {
            category: "content_script",
            severity: "high",
            description:
              "Slow extension content script caused a page hang, user was warned.",
          },
          details,
          "Performance warning event contains expected details."
        );
        browser.test.notifyPass("performance-warning");
      });
  },
]);
