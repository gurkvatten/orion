"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        await new Promise(resolve => {
          browser.runtime.onMessage.addListener(msg => {
            browser.test.assertEq("do-check-cookies", msg, "expected message");
            resolve();
          });
        });
    
        const url = "https://example.org/";
    
        // Baseline. Every cookie must have the expected sameSite.
        let cookie = await browser.cookies.get({ url, name: "test1" });
        browser.test.assertEq(
          "unspecified",
          cookie.sameSite,
          "Expected sameSite for test1"
        );
    
        cookie = await browser.cookies.get({ url, name: "test2" });
        browser.test.assertEq(
          "lax",
          cookie.sameSite,
          "Expected sameSite for test2"
        );
    
        cookie = await browser.cookies.get({ url, name: "test3" });
        browser.test.assertEq(
          "strict",
          cookie.sameSite,
          "Expected sameSite for test3"
        );
    
        cookie = await browser.cookies.get({ url, name: "test4" });
        browser.test.assertEq(
          "no_restriction",
          cookie.sameSite,
          "Expected sameSite for test4"
        );
    
        // Testing cookies.getAll + cookies.set
        let cookies = await browser.cookies.getAll({ url, name: "test3" });
        browser.test.assertEq(1, cookies.length, "There is only one test3 cookie");
    
        cookie = await browser.cookies.set({
          url,
          name: "test3",
          value: "newvalue",
        });
        browser.test.assertEq(
          "unspecified",
          cookie.sameSite,
          "sameSite defaults to unspecified"
        );
    
        for (let sameSite of ["unspecified", "no_restriction", "lax", "strict"]) {
          cookie = await browser.cookies.set({ url, name: "test3", sameSite });
          browser.test.assertEq(
            sameSite,
            cookie.sameSite,
            `Expected sameSite=${sameSite} in return value of cookies.set`
          );
          cookies = await browser.cookies.getAll({ url, name: "test3" });
          browser.test.assertEq(
            1,
            cookies.length,
            `test3 is still the only cookie after setting sameSite=${sameSite}`
          );
          browser.test.assertEq(
            sameSite,
            cookies[0].sameSite,
            `test3 was updated to sameSite=${sameSite}`
          );
        }
    
        browser.test.notifyPass("cookies");
      
  },
]);
