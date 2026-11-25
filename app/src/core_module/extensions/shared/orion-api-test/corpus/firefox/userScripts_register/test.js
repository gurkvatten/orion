"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const matches = ["<all_urls>"];
    
        await browser.test.assertRejects(
          browser.userScripts.register({
            js: [{ code: "" }],
            matches,
            cookieStoreId: "not_a_valid_cookieStoreId",
          }),
          /Invalid cookieStoreId/,
          "userScript.register with an invalid cookieStoreId"
        );
    
        await browser.test.assertRejects(
          browser.userScripts.register({
            js: [{ code: "" }],
            matches,
            cookieStoreId: "",
          }),
          /Invalid cookieStoreId/,
          "userScripts.register with an invalid cookieStoreId"
        );
    
        let cookieStoreIdJSArray = [
          {
            id: "firefox-container-1",
            code: `document.body.textContent += "1"`,
          },
          {
            id: ["firefox-container-2", "firefox-container-3"],
            code: `document.body.textContent += "2-3"`,
          },
          {
            id: "firefox-private",
            code: `document.body.textContent += "private"`,
          },
          {
            id: "firefox-default",
            code: `document.body.textContent += "default"`,
          },
        ];
    
        for (let { id, code } of cookieStoreIdJSArray) {
          await browser.userScripts.register({
            js: [{ code }],
            matches,
            runAt: "document_end",
            cookieStoreId: id,
          });
        }
    
        await browser.contentScripts.register({
          js: [
            {
              code: `browser.test.sendMessage("last-content-script");`,
            },
          ],
          matches,
          runAt: "document_end",
        });
    
        browser.test.sendMessage("background_ready");
      
  },
]);
