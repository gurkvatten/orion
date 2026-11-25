"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const FILE_DUMMY_URL = browser.runtime.getManifest().description;
    
          await browser.test.assertRejects(
            fetch(FILE_DUMMY_URL),
            /NetworkError when attempting to fetch resource/,
            "block request to file from background page despite file permission"
          );
    
          // Regression test for bug 1420296 .
          await browser.test.assertRejects(
            fetch(FILE_DUMMY_URL, { mode: "same-origin" }),
            /NetworkError when attempting to fetch resource/,
            "block request to file from background page despite 'same-origin' mode"
          );
    
          browser.test.sendMessage("done");
        
  },
  async function part_2() {
          browser.webRequest.onBeforeRequest.addListener(
            details => {
              browser.test.fail(`Unexpected request from ${details.originUrl}`);
            },
            { urls: ["http://example.net/intercept_by_webRequest.js"] }
          );
        
  },
  async function part_3() {
          browser.webRequest.onBeforeRequest.addListener(
            ({ originUrl }) => {
              browser.test.assertTrue(
                /^file:.*file_do_load_script_subresource.html/.test(originUrl),
                `expected script to be loaded from a local file (${originUrl})`
              );
              let redirectUrl = browser.runtime.getURL("testDONE.html");
              return {
                redirectUrl: `data:text/javascript,location.href='${redirectUrl}';`,
              };
            },
            { urls: ["http://example.net/intercept_by_webRequest.js"] },
            ["blocking"]
          );
        
  },
]);
