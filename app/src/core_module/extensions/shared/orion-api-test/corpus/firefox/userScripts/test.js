"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.assertEq(
          undefined,
          browser.userScripts,
          "userScripts API namespace should be undefined in the extension page"
        );
        browser.test.sendMessage("background-page:done");
      
  },
  async function part_2() {
        async function registerUserScriptWithMatches(matches) {
          const scripts = await browser.userScripts.register({
            js: [{ code: "" }],
            matches,
          });
          await scripts.unregister();
        }
    
        // These matches are supposed to be denied until the extension has been granted the
        // <all_urls> origin permission.
        const testMatches = [
          "<all_urls>",
          "file://*/*",
          "https://localhost/*",
          "http://example.com/*",
        ];
    
        browser.test.onMessage.addListener(async msg => {
          if (msg === "test-denied-matches") {
            for (let testMatch of testMatches) {
              await browser.test.assertRejects(
                registerUserScriptWithMatches([testMatch]),
                /Permission denied to register a user script for/,
                "Got the expected rejection when the extension permission does not subsume the userScript matches"
              );
            }
          } else if (msg === "grant-all-urls") {
            await browser.permissions.request({ origins: ["<all_urls>"] });
          } else if (msg === "test-allowed-matches") {
            for (let testMatch of testMatches) {
              try {
                await registerUserScriptWithMatches([testMatch]);
              } catch (err) {
                browser.test.fail(
                  `Unexpected rejection ${err} on matching ${JSON.stringify(
                    testMatch
                  )}`
                );
              }
            }
          } else {
            browser.test.fail(`Received an unexpected ${msg} test message`);
          }
    
          browser.test.sendMessage(`${msg}:done`);
        });
    
        browser.test.sendMessage("background-ready");
      
  },
  async function part_3() {
        function userScript() {
          this.sendTestMessage("user-script-loaded", {
            url: window.location.href,
            documentReadyState: document.readyState,
          });
        }
    
        await browser.userScripts.register({
          js: [
            {
              code: `(${userScript})();`,
            },
          ],
          runAt: "document_start",
          matches: ["http://localhost/*/file_sample.html"],
        });
    
        browser.test.sendMessage("user-script-registered");
      
  },
  async function part_4() {
          let promise = (async () => {
            await browser.userScripts.register({
              js: [
                {
                  code: "throw new Error('This userScripts should not be registered')",
                },
              ],
              runAt: "document_start",
              matches: ["<all_urls>"],
            });
          })();
    
          await browser.test.assertRejects(
            promise,
            /userScripts APIs are currently experimental/,
            "Got the expected error from userScripts.register when the userScripts API is disabled"
          );
    
          browser.test.sendMessage("background-page:done");
        
  },
  async function part_5() {
        for (let scriptMetadata of getTestCases(true)) {
          await browser.userScripts.register({
            js: [{ file: "userscript.js" }],
            runAt: "document_end",
            matches: ["http://localhost/*/file_sample.html"],
            scriptMetadata,
          });
        }
    
        browser.test.sendMessage("background-page:done");
      
  },
]);
