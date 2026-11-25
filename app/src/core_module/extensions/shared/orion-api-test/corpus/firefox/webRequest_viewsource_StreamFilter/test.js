"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertEq(`${BASE_URL}/dummy`, details.url, "expected URL");
            browser.test.assertEq("main_frame", details.type, "details.type");
    
            let filter = browser.webRequest.filterResponseData(details.requestId);
            filter.onstart = () => {
              filter.write(new TextEncoder().encode("PREFIX_"));
            };
            filter.ondata = event => {
              filter.write(event.data);
            };
            filter.onstop = () => {
              filter.write(new TextEncoder().encode("_SUFFIX"));
              filter.disconnect();
              browser.test.notifyPass("filter_end");
            };
            filter.onerror = () => {
              browser.test.fail(`Unexpected error: ${filter.error}`);
              browser.test.notifyFail("filter_end");
            };
          },
          { urls: ["*://*/dummy"] },
          ["blocking"]
        );
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertEq(`${BASE_URL}/redir`, details.url, "Got redirect");
    
            let filter = browser.webRequest.filterResponseData(details.requestId);
            filter.onstop = () => {
              filter.disconnect();
              browser.test.fail("Unexpected onstop for redirect");
              browser.test.sendMessage("redirect_done");
            };
            filter.onerror = () => {
              browser.test.assertEq(
                // TODO bug 1683862: must be "Channel redirected", but it is not
                // because document requests are handled differently compared to
                // other requests, see the comment at the top of
                // test_ext_webRequest_redirect_StreamFilter.js.
                "Invalid request ID",
                filter.error,
                "Expected error in filter.onerror"
              );
              browser.test.sendMessage("redirect_done");
            };
          },
          { urls: ["*://*/redir"] },
          ["blocking"]
        );
      
  },
  async function part_2() {
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              let filter = browser.webRequest.filterResponseData(details.requestId);
              filter.onstart = () => {
                filter.disconnect();
                browser.test.fail("Unexpected filter.onstart");
                browser.test.notifyFail("filter_end");
              };
              filter.onerror = () => {
                browser.test.assertEq("Invalid request ID", filter.error, "Error?");
                browser.test.notifyPass("filter_end");
              };
            },
            { urls: ["*://*/dummy"] },
            ["blocking"]
          );
          browser.webRequest.onHeadersReceived.addListener(
            () => {
              browser.test.log("Intentionally canceling view-source request");
              return { cancel: true };
            },
            { urls: ["*://*/dummy"] },
            ["blocking"]
          );
        
  },
]);
