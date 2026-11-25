"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeSendHeaders.addListener(
            () => {
              // Make sure that returning undefined or a promise that resolves to
              // undefined does not break later handlers.
            },
            { urls: ["<all_urls>"] },
            ["blocking", "requestHeaders"]
          );
    
          browser.webRequest.onBeforeSendHeaders.addListener(
            () => {
              return Promise.resolve();
            },
            { urls: ["<all_urls>"] },
            ["blocking", "requestHeaders"]
          );
    
          browser.webRequest.onBeforeSendHeaders.addListener(
            details => {
              let requestHeaders = details.requestHeaders.concat({
                name: "Foo",
                value: "Bar",
              });
    
              return new Promise(resolve => {
                // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
                setTimeout(resolve, 500);
              }).then(() => {
                return { requestHeaders };
              });
            },
            { urls: ["<all_urls>"] },
            ["blocking", "requestHeaders"]
          );
        
  },
  async function part_2() {
          browser.webRequest.onBeforeSendHeaders.addListener(
            details => {
              browser.test.log(`onBeforeSendHeaders({url: ${details.url}})`);
    
              if (details.url === "http://example.com/dummy") {
                browser.test.sendMessage("got-before-send-headers");
              }
            },
            { urls: ["<all_urls>"] },
            ["blocking"]
          );
    
          browser.webRequest.onErrorOccurred.addListener(
            details => {
              browser.test.log(`onErrorOccurred({url: ${details.url}})`);
    
              if (details.url === "http://example.com/dummy") {
                browser.test.sendMessage("got-error-occurred");
              }
            },
            { urls: ["<all_urls>"] }
          );
        
  },
  async function part_3() {
          browser.webRequest.onHeadersReceived.addListener(
            details => {
              browser.test.log(`onHeadersReceived({url: ${details.url}})`);
    
              details.responseHeaders.push({ name: "foo", value: "bar" });
    
              return { responseHeaders: details.responseHeaders };
            },
            { urls: ["http://example.com/?modify_headers"] },
            ["blocking", "responseHeaders"]
          );
        
  },
  async function part_4() {
          async function onBeforeRequest() {
            throw new Error("Expected webRequest exception from a promise result");
          }
    
          let exceptionRaised = false;
    
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              if (exceptionRaised) {
                return;
              }
    
              // We only need to raise the exception once.
              exceptionRaised = true;
              return onBeforeRequest();
            },
            {
              urls: ["http://example.com/*"],
              types: ["main_frame"],
            },
            ["blocking"]
          );
    
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("web-request-event-received");
            },
            {
              urls: ["http://example.com/*"],
              types: ["main_frame"],
            },
            ["blocking"]
          );
        
  },
]);
