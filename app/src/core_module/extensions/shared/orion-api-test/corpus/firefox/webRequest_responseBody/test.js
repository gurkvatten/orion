"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        async function runTest(test, num, details) {
          browser.test.log(`Running test #${num}: ${details.url}`);
    
          let filter = browser.webRequest.filterResponseData(details.requestId);
    
          try {
            await new Promise(resolve => {
              test.task(filter, resolve, num, details);
            });
          } catch (e) {
            browser.test.fail(
              `Task #${num} threw an unexpected exception: ${e} :: ${e.stack}`
            );
          }
    
          browser.test.log(`Finished test #${num}: ${details.url}`);
          browser.test.sendMessage(`finished-${num}`);
        }
    
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            for (let [num, test] of TASKS.entries()) {
              if (details.url.endsWith(test.url)) {
                runTest(test, num, details);
                break;
              }
            }
          },
          {
            urls: ["http://example.com/*?test_num=*"],
          },
          ["blocking"]
        );
      
  },
  async function part_2() {
          browser.webRequest.onHeadersReceived.addListener(
            data => {
              let filter = browser.webRequest.filterResponseData(data.requestId);
    
              filter.onstop = () => {
                filter.close();
              };
              filter.ondata = event => {
                filter.write(event.data);
              };
    
              if (data.fromCache) {
                browser.test.sendMessage("from-cache");
              }
            },
            {
              urls: ["http://example.com/*/file_sample.html?r=*"],
            },
            ["blocking"]
          );
        
  },
  async function part_3() {
          browser.webRequest.onBeforeRequest.addListener(
            data => {
              let filter = browser.webRequest.filterResponseData(data.requestId);
    
              filter.onstop = () => {
                browser.test.fail("Should not receive onstop after close()");
                browser.test.assertEq(
                  "closed",
                  filter.status,
                  "Filter status should still be 'closed'"
                );
                browser.test.assertThrows(() => {
                  filter.close();
                });
              };
              filter.ondata = event => {
                filter.write(event.data);
                filter.close();
    
                browser.test.sendMessage(`done-${data.url}`);
              };
            },
            {
              urls: ["http://example.com/*/file_sample.html?*"],
            },
            ["blocking"]
          );
        
  },
  async function part_4() {
          browser.test.assertEq(
            undefined,
            browser.webRequest.filterResponseData,
            "filterResponseData is undefined without blocking permissions"
          );
        
  },
  async function part_5() {
          let filter = browser.webRequest.filterResponseData("34159628");
    
          await new Promise(resolve => {
            filter.onerror = resolve;
          });
    
          browser.test.assertEq(
            "Invalid request ID",
            filter.error,
            "Got expected error"
          );
    
          browser.test.notifyPass("invalid-request-id");
        
  },
]);
