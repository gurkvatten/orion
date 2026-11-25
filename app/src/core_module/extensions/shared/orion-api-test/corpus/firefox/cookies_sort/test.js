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
    
        const cookies = await browser.cookies.getAll({ name: "a" });
        // Cookies should be sorted according to RFC 6256, like document.cookie.
        browser.test.assertEq(4, cookies.length, "4 cookies expected");
        browser.test.assertEq("4", cookies[0].value, "4");
        browser.test.assertEq("3", cookies[1].value, "3");
        browser.test.assertEq("2", cookies[2].value, "2");
        browser.test.assertEq("1", cookies[3].value, "1");
    
        const cookie = await browser.cookies.get({
          name: "a",
          url: "https://example.org/sub/dir",
        });
        browser.test.assertEq("/sub/dir", cookie?.path, "Expected exact path");
    
        browser.test.notifyPass("cookies");
      
  },
]);
