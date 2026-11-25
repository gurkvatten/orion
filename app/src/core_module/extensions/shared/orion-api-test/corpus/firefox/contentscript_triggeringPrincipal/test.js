"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        // Use webRequest to detect CSP violations; see CSP_REPORT_PATH
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            try {
              let rawBody = "";
              const decoder = new TextDecoder();
              for (let [i, chunk] of details.requestBody.raw.entries()) {
                const stream = i < details.requestBody.raw.length - 1;
                rawBody += decoder.decode(chunk.bytes, { stream });
              }
              const body = JSON.parse(rawBody);
              const request = { body };
              browser.test.sendMessage("CSP_REPORT_PATH", request);
            } catch (e) {
              browser.test.fail(
                `Failed to parse CSP report: ${e} :: ${JSON.stringify(details)}`
              );
            }
          },
          { urls: ["*://example.com/csp-report.sjs"], types: ["csp_report"] },
          ["blocking", "requestBody"]
        );
      
  },
]);
