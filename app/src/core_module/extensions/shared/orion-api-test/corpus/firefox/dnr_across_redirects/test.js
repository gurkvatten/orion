"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: {
                  requestMethods: ["get", "post"],
                  urlFilter: "/redirect_from",
                },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    // Because the request is redirected, the fetch() caller should
                    // not observe this header:
                    { operation: "append", header: "outcome", value: "pre_redir" },
                  ],
                },
              },
              {
                id: 2,
                // HTTP 302 and 303 should convert redirects to GET:
                condition: {
                  requestMethods: ["get"],
                  urlFilter: "/redirect_to",
                },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    { operation: "append", header: "outcome", value: "match_get" },
                  ],
                },
              },
              {
                // HTTP 307 should preserve HTTP method.
                id: 3,
                condition: {
                  requestMethods: ["post"],
                  urlFilter: "/redirect_to",
                },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    { operation: "append", header: "outcome", value: "match_post" },
                  ],
                },
              },
            ],
          });
    
          async function getOutcome(method, path) {
            const url = `http://example.com${path}`;
            const { headers } = await fetch(url, { method });
            return headers.get("outcome");
          }
    
          // Regression test for bug 1909081:
          browser.test.assertEq(
            "server-GET, match_get",
            await getOutcome("POST", "/redirect_from_303"),
            "HTTP 303 changes POST to GET, and DNR should match GET"
          );
    
          browser.test.assertEq(
            "server-POST, match_post",
            await getOutcome("POST", "/redirect_from_307"),
            "HTTP 307 keeps POST as POST, and DNR should match POST"
          );
    
          browser.test.sendMessage("done");
        
  },
]);
