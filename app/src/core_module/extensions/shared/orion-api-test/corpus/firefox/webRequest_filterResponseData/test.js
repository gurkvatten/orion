"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            request => {
              let filter = browser.webRequest.filterResponseData(request.requestId);
              let decoder = new TextDecoder("utf-8");
              let encoder = new TextEncoder();
    
              filter.ondata = event => {
                let str = decoder.decode(event.data, { stream: true });
                filter.write(encoder.encode(str));
                filter.disconnect();
              };
            },
            {
              urls: ["http://example.com/lorem.html.gz"],
              types: ["main_frame"],
            },
            ["blocking"]
          );
        
  },
  async function part_2() {
          browser.webRequest.onBeforeRequest.addListener(
            request => {
              let filter = browser.webRequest.filterResponseData(request.requestId);
    
              let data = [];
              filter.ondata = event => {
                data.push(event.data);
              };
              filter.onstop = async () => {
                browser.test.sendMessage("phase", "original-onstop");
    
                // Make a few trips through the event loop.
                for (let i = 0; i < 10; i++) {
                  await new Promise(resolve => setTimeout(resolve, 0));
                }
    
                for (let buffer of data) {
                  filter.write(buffer);
                }
                browser.test.sendMessage("phase", "filter-onstop");
                filter.close();
              };
            },
            {
              urls: ["http://example.com/dummy.xhtml"],
            },
            ["blocking"]
          );
        
  },
  async function part_3() {
          let pending = [];
    
          browser.webRequest.onBeforeRequest.addListener(
            data => {
              let filter = browser.webRequest.filterResponseData(data.requestId);
    
              let url = new URL(data.url);
    
              if (url.searchParams.get("redirect_uri")) {
                pending.push(
                  new Promise(resolve => {
                    filter.onerror = resolve;
                  }).then(() => {
                    browser.test.assertEq(
                      "Channel redirected",
                      filter.error,
                      "Got correct error for redirected filter"
                    );
                  })
                );
              }
    
              filter.onstart = () => {
                filter.write(new TextEncoder().encode(data.url));
              };
              filter.ondata = event => {
                let str = new TextDecoder().decode(event.data);
                browser.test.assertEq(
                  "ok",
                  str,
                  `Got unfiltered data for ${data.url}`
                );
              };
              filter.onstop = () => {
                filter.close();
              };
            },
            {
              urls: ["<all_urls>"],
            },
            ["blocking"]
          );
    
          browser.test.onMessage.addListener(async msg => {
            if (msg === "done") {
              await Promise.all(pending);
              browser.test.notifyPass("stream-filter");
            }
          });
        
  },
  async function part_4() {
          browser.webRequest.onHeadersReceived.addListener(
            data => {
              if (data.statusCode !== 200) {
                return;
              }
              let filter = browser.webRequest.filterResponseData(data.requestId);
    
              filter.onstop = () => {
                filter.close();
                browser.test.notifyPass("stream-filter");
              };
              filter.onerror = () => {
                browser.test.fail(`unexpected ${filter.error}`);
              };
            },
            {
              urls: ["<all_urls>"],
            },
            ["blocking"]
          );
        
  },
  async function part_5() {
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              let filename = details.url.split("/").pop();
              let filter = browser.webRequest.filterResponseData(details.requestId);
              browser.test.sendMessage("filter-created");
    
              filter.ondata = () => {
                const script = "forceError();";
                filter.write(new Uint8Array(new TextEncoder().encode(script)));
                filter.close();
                browser.test.assertEq("script.js", filename, "Is redirect target");
                browser.test.sendMessage("filter-ondata");
              };
    
              filter.onerror = () => {
                browser.test.assertEq(filter.error, "Channel redirected");
                browser.test.assertEq("script302.js", filename, "Is initial file");
                browser.test.sendMessage("filter-redirect");
              };
            },
            {
              urls: ["http://example.com/*.js"],
            },
            ["blocking"]
          );
        
  },
  async function part_6() {
          let requestId;
          function checkStreamFilter(hasRedirected) {
            let prefix = hasRedirected ? "AFTER: " : "BEFORE: ";
            let filter = browser.webRequest.filterResponseData(requestId);
            filter.onstart = () => {
              const script = "forceAfterRedirError();";
              filter.write(new Uint8Array(new TextEncoder().encode(script)));
              filter.disconnect();
              browser.test.sendMessage("filter-result", prefix + "onstart_fired");
            };
            filter.onerror = () => {
              browser.test.sendMessage("filter-result", prefix + filter.error);
            };
          }
          browser.test.onMessage.addListener((msg, otherRequestId) => {
            // The test calls us when the request has redirected.
            browser.test.assertEq("checkStreamFilter", msg, "expected msg");
            browser.test.assertEq(requestId, otherRequestId, "Same requestId");
            checkStreamFilter(/* hasRedirected */ true);
            browser.test.sendMessage("checkStreamFilter_initialized");
          });
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              requestId = details.requestId;
              browser.test.log(`Seen request ${details.url} (${requestId})`);
              checkStreamFilter(/* hasRedirected */ false);
              browser.test.sendMessage("seenRequest");
            },
            { urls: ["http://example.net/script302.js"] },
            ["blocking"]
          );
        
  },
  async function part_7() {
          const { promise, resolve } = Promise.withResolvers();
          browser.test.onMessage.addListener(msg => {
            browser.test.assertEq("continueRedirect", msg, "Got continueRedirect");
            resolve();
          });
          browser.webRequest.onBeforeRequest.addListener(
            async details => {
              browser.test.sendMessage("detectedRedirect", details.requestId);
              browser.test.log(`Suspending request to ${details.url}`);
              await promise;
            },
            { urls: ["http://example.com/script.js"] },
            ["blocking"]
          );
        
  },
  async function part_8() {
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              let filter = browser.webRequest.filterResponseData(details.requestId);
              let decoder = new TextDecoder("utf-8");
              let encoder = new TextEncoder();
    
              filter.ondata = event => {
                let str = decoder.decode(event.data, { stream: true });
                filter.write(encoder.encode(str));
                filter.disconnect();
                browser.test.assertTrue(
                  str.startsWith(`"use strict";`),
                  "ondata received decoded data"
                );
                browser.test.sendMessage("onBeforeRequest");
              };
    
              filter.onerror = () => {
                // onBeforeRequest will always beat the cache race, so we should always
                // get valid data in ondata.
                browser.test.fail("error-received", filter.error);
              };
            },
            {
              urls: ["http://example.com/data/file_script_good.js"],
            },
            ["blocking"]
          );
          browser.webRequest.onHeadersReceived.addListener(
            details => {
              let filter = browser.webRequest.filterResponseData(details.requestId);
              let decoder = new TextDecoder("utf-8");
              let encoder = new TextEncoder();
    
              // Because cache is always a race, intermittently we will succesfully
              // beat the cache, in which case we pass in ondata.  If cache wins,
              // we pass in onerror.
              // Running the test with --verify hits this cache race issue, as well
              // it seems that the cache primarily looses on linux1804.
              let gotone = false;
              filter.ondata = event => {
                browser.test.assertFalse(gotone, "cache lost the race");
                gotone = true;
                let str = decoder.decode(event.data, { stream: true });
                filter.write(encoder.encode(str));
                filter.disconnect();
                browser.test.assertTrue(
                  str.startsWith(`"use strict";`),
                  "ondata received decoded data"
                );
                browser.test.sendMessage("onHeadersReceived");
              };
    
              filter.onerror = () => {
                browser.test.assertFalse(gotone, "cache won the race");
                gotone = true;
                browser.test.assertEq(
                  filter.error,
                  "Channel is delivering cached alt-data"
                );
                browser.test.sendMessage("onHeadersReceived");
              };
            },
            {
              urls: ["http://example.com/data/file_script_bad.js"],
            },
            ["blocking"]
          );
        
  },
]);
