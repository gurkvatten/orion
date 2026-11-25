"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const closeCounts = {};
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              let expectedError = "Channel redirected";
              if (details.type === "main_frame" || details.type === "sub_frame") {
                // Message differs for the reason stated at the top of this file.
                // TODO bug 1683862: Make error message more accurate.
                expectedError = "Invalid request ID";
              }
    
              closeCounts[details.requestId] = 0;
    
              let filter = browser.webRequest.filterResponseData(details.requestId);
              filter.onstart = () => {
                filter.disconnect();
                browser.test.fail("Unexpected filter.onstart");
              };
              filter.onerror = function () {
                closeCounts[details.requestId]++;
                browser.test.assertEq(expectedError, filter.error, "filter.error");
              };
            },
            { urls: ["*://*/redir"] },
            ["blocking"]
          );
          browser.webRequest.onCompleted.addListener(
            details => {
              // filter.onerror from the redirect request should be called before
              // webRequest.onCompleted of the redirection target. Regression test
              // for bug 1683189.
              browser.test.assertEq(
                1,
                closeCounts[details.requestId],
                "filter from initial, redirected request should have been closed"
              );
              browser.test.log("Intentionally canceling view-source request");
              browser.test.sendMessage("req_end", details.type);
            },
            { urls: ["*://*/target"] }
          );
        
  },
]);
