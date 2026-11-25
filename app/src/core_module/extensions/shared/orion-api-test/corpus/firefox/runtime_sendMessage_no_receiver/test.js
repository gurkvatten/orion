"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        await browser.test.assertRejects(
          browser.runtime.sendMessage("msg"),
          "Could not establish connection. Receiving end does not exist.",
          "Correct error when there are no receivers from background"
        );
    
        browser.test.sendMessage("sendMessage-error-bg");
      
  },
  async function part_2() {
        /* globals chrome */
        browser.test.assertEq(
          null,
          chrome.runtime.lastError,
          "no lastError before call"
        );
        let retval = chrome.runtime.sendMessage("msg");
        browser.test.assertEq(
          null,
          chrome.runtime.lastError,
          "no lastError after call"
        );
        browser.test.assertEq(
          undefined,
          retval,
          "return value of chrome.runtime.sendMessage without callback"
        );
    
        let isAsyncCall = false;
        retval = chrome.runtime.sendMessage("msg", reply => {
          browser.test.assertEq(undefined, reply, "no reply");
          browser.test.assertTrue(
            isAsyncCall,
            "chrome.runtime.sendMessage's callback must be called asynchronously"
          );
          browser.test.assertEq(
            undefined,
            retval,
            "return value of chrome.runtime.sendMessage with callback"
          );
          browser.test.assertEq(
            "Could not establish connection. Receiving end does not exist.",
            chrome.runtime.lastError.message
          );
          browser.test.notifyPass("finished chrome.runtime.sendMessage");
        });
        isAsyncCall = true;
      
  },
]);
