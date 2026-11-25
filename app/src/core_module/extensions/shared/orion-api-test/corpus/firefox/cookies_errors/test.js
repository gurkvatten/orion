"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const url = "http://example.com/";
          const name = "dummyname";
          await browser.cookies.set({ url, name, value: "from_setup:normal" });
          await browser.cookies.set({
            url,
            name,
            value: "from_setup:private",
            storeId: "firefox-private",
          });
          await browser.cookies.set({
            url,
            name,
            value: "from_setup:container",
            storeId: "firefox-container-1",
          });
          browser.test.sendMessage("setup_done");
        
  },
  async function part_2() {
          async function checkCookie(storeId, value) {
            let cookies = await browser.cookies.getAll({ storeId });
            let index = cookies.findIndex(c => c.value === value);
            browser.test.assertTrue(index !== -1, `Found cookie: ${value}`);
            if (index >= 0) {
              cookies.splice(index, 1);
            }
            browser.test.assertEq(
              "[]",
              JSON.stringify(cookies),
              `No more cookies left in cookieStoreId=${storeId}`
            );
          }
          // Added in setup.
          await checkCookie("firefox-default", "from_setup:normal");
          await checkCookie("firefox-private", "from_setup:private");
          await checkCookie("firefox-container-1", "from_setup:container");
          browser.test.sendMessage("final_check_done");
        
  },
]);
