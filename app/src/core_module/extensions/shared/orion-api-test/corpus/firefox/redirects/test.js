"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let expectedUrls = ["/redirect", "/dummy"];
        let expected = [
          "onBeforeRequest",
          "onHeadersReceived",
          "onBeforeRedirect",
          "onBeforeRequest",
          "onHeadersReceived",
          "onResponseStarted",
          "onCompleted",
        ];
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            browser.test.assertTrue(
              details.url.includes(expectedUrls.shift()),
              "onBeforeRequest url matches"
            );
            browser.test.assertEq(
              expected.shift(),
              "onBeforeRequest",
              "onBeforeRequest matches"
            );
          },
          { urls: [serverUrl] }
        );
        browser.webRequest.onHeadersReceived.addListener(
          () => {
            browser.test.assertEq(
              expected.shift(),
              "onHeadersReceived",
              "onHeadersReceived matches"
            );
          },
          { urls: [serverUrl] }
        );
        browser.webRequest.onResponseStarted.addListener(
          () => {
            browser.test.assertEq(
              expected.shift(),
              "onResponseStarted",
              "onResponseStarted matches"
            );
          },
          { urls: [serverUrl] }
        );
        browser.webRequest.onBeforeRedirect.addListener(
          details => {
            browser.test.assertTrue(
              details.redirectUrl.includes("/dummy"),
              "onBeforeRedirect matches redirectUrl"
            );
            browser.test.assertEq(
              expected.shift(),
              "onBeforeRedirect",
              "onBeforeRedirect matches"
            );
          },
          { urls: [serverUrl] }
        );
        browser.webRequest.onCompleted.addListener(
          details => {
            browser.test.assertTrue(
              details.url.includes("/dummy"),
              "onCompleted expected url received"
            );
            browser.test.assertEq(
              expected.shift(),
              "onCompleted",
              "onCompleted matches"
            );
            browser.test.notifyPass("requestCompleted");
          },
          { urls: [serverUrl] }
        );
        browser.webRequest.onErrorOccurred.addListener(
          details => {
            browser.test.log(`onErrorOccurred ${JSON.stringify(details)}`);
            browser.test.notifyFail("requestCompleted");
          },
          { urls: [serverUrl] }
        );
      
  },
  async function part_2() {
        browser.webRequest.onBeforeRequest.addListener(
          details => {
            let expect = expected.shift();
            browser.test.assertEq(
              expect.event,
              "onBeforeRequest",
              "onBeforeRequest event matches"
            );
            browser.test.assertTrue(
              details.url.startsWith(expect.url),
              "onBeforeRequest url matches"
            );
            if (expected.length === 0) {
              browser.test.notifyPass("requestCompleted");
            }
          },
          { urls: [serverUrl] }
        );
      
  },
  async function part_3() {
        browser.webRequest.onHeadersReceived.addListener(
          details => {
            let expect = expected.shift();
            browser.test.assertEq(
              expect.event,
              "onHeadersReceived",
              "onHeadersReceived event matches"
            );
            browser.test.assertTrue(
              details.url.startsWith(expect.url),
              "onHeadersReceived url matches"
            );
            if (expected.length === 0) {
              browser.test.notifyPass("requestCompleted");
            }
          },
          { urls: ["<all_urls>"] }
        );
      
  },
  async function part_4() {
        browser.webRequest.onCompleted.addListener(
          details => {
            let expect = expected.shift();
            browser.test.assertEq(
              expect.event,
              "onCompleted",
              "onCompleted event matches"
            );
            browser.test.assertTrue(
              details.url.startsWith(expect.url),
              "onCompleted url matches"
            );
            if (expected.length === 0) {
              browser.test.notifyPass("requestCompleted");
            }
          },
          { urls: [serverUrl] }
        );
      
  },
  async function part_5() {
        browser.webRequest.onCompleted.addListener(
          () => {
            browser.test.notifyPass("requestCompleted");
          },
          { urls: ["<all_urls>"] }
        );
    
        browser.webRequest.onHeadersReceived.addListener(
          () => {
            // Redirect to a different URL when we receive the headers of the
            // initial request.
            return { redirectUrl };
          },
          { urls: [initialUrl] },
          ["blocking"]
        );
      
  },
  async function part_6() {
        browser.webRequest.onCompleted.addListener(
          ({ responseHeaders }) => {
            // Make sure that the `Location` header is set by `onHeadersReceived`.
            browser.test.assertTrue(
              responseHeaders.some(({ name, value }) => {
                return name.toLowerCase() === "location" && value === redirectUrl;
              }),
              "Location header is set"
            );
    
            browser.test.notifyPass("requestCompleted");
          },
          { urls: ["<all_urls>"] },
          ["responseHeaders"]
        );
    
        browser.webRequest.onHeadersReceived.addListener(
          ({ responseHeaders }) => {
            return {
              responseHeaders: [
                ...responseHeaders,
                // Although we set a Location header here, the request shouldn't be
                // redirected to `redirectUrl` because the status code hasn't been
                // change (and cannot be changed from there).
                { name: "Location", value: redirectUrl },
              ],
            };
          },
          { urls: [initialUrl] },
          ["blocking", "responseHeaders"]
        );
      
  },
]);
