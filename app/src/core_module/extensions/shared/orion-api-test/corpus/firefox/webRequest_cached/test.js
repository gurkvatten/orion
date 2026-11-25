"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          function replaceHeader(headers, newHeader) {
            headers = headers.filter(header => header.name !== newHeader.name);
            headers.push(newHeader);
            return headers;
          }
          let testHeaders = [
            {
              name: "Content-Security-Policy",
              value: "object-src 'none'; script-src 'none'",
            },
            {
              name: "Content-Security-Policy",
              value: "object-src 'none'; script-src https:",
            },
          ];
          browser.webRequest.onHeadersReceived.addListener(
            details => {
              if (!details.fromCache) {
                // Add a CSP header on the initial request
                details.responseHeaders.push(testHeaders[0]);
                return {
                  responseHeaders: details.responseHeaders,
                };
              }
              // Test that the header added during the initial request is
              // now in the cached response.
              let header = details.responseHeaders.filter(header => {
                browser.test.log(`header ${header.name} = ${header.value}`);
                return header.name == "Content-Security-Policy";
              });
              browser.test.assertEq(
                header[0].value,
                testHeaders[0].value,
                "pre-cached header exists"
              );
              // Replace the cached value so we can test overriding the header that was cached.
              return {
                responseHeaders: replaceHeader(
                  details.responseHeaders,
                  testHeaders[1]
                ),
              };
            },
            {
              urls: ["http://example.com/*/file_sample.html?r=*"],
            },
            ["blocking", "responseHeaders"]
          );
          browser.webRequest.onResponseStarted.addListener(
            details => {
              let needle = details.fromCache ? testHeaders[1] : testHeaders[0];
              let header = details.responseHeaders.filter(header => {
                browser.test.log(`header ${header.name} = ${header.value}`);
                return header.name == needle.name && header.value == needle.value;
              });
              browser.test.assertEq(
                header.length,
                1,
                "header exists with correct value"
              );
              if (details.fromCache) {
                browser.test.sendMessage("from-cache");
              }
            },
            {
              urls: ["http://example.com/*/file_sample.html?r=*"],
            },
            ["responseHeaders"]
          );
        
  },
  async function part_2() {
          let testHeaders = [
            {
              name: "Content-Security-Policy",
              value: "object-src 'none'; script-src 'none'",
            },
            {
              name: "Content-Security-Policy",
              value: "object-src 'none'; script-src https:",
            },
          ];
          browser.webRequest.onHeadersReceived.addListener(
            details => {
              if (!details.fromCache) {
                details.responseHeaders.push(testHeaders[0]);
                return {
                  responseHeaders: details.responseHeaders,
                };
              }
              browser.test.log("cached request received");
              details.responseHeaders.push(testHeaders[1]);
              return {
                responseHeaders: details.responseHeaders,
              };
            },
            {
              urls: ["http://example.com/*/file_sample.html?r=*"],
            },
            ["blocking", "responseHeaders"]
          );
          browser.webRequest.onCompleted.addListener(
            details => {
              let { name, value } = testHeaders[0];
              if (details.fromCache) {
                value = `${value}, ${testHeaders[1].value}`;
              }
              let header = details.responseHeaders.filter(header => {
                browser.test.log(`header ${header.name} = ${header.value}`);
                return header.name == name && header.value == value;
              });
              browser.test.assertEq(
                header.length,
                1,
                "header exists with correct value"
              );
              if (details.fromCache) {
                browser.test.sendMessage("from-cache");
              }
            },
            {
              urls: ["http://example.com/*/file_sample.html?r=*"],
            },
            ["responseHeaders"]
          );
        
  },
  async function part_3() {
          browser.webRequest.onBeforeSendHeaders.addListener(
            details => {
              browser.test.log(`onBeforeSendHeaders ${JSON.stringify(details)}\n`);
            },
            {
              urls: ["http://example.com/status*"],
            },
            ["blocking", "requestHeaders"]
          );
          browser.webRequest.onHeadersReceived.addListener(
            details => {
              browser.test.log(`onHeadersReceived ${JSON.stringify(details)}\n`);
              if (!details.fromCache) {
                browser.test.sendMessage("statusCode", details.statusCode);
                const mime = details.responseHeaders.find(header => {
                  return header.value && header.name === "content-type";
                });
                if (mime) {
                  mime.value = "text/plain";
                } else {
                  details.responseHeaders.push({
                    name: "content-type",
                    value: "text/plain",
                  });
                }
                return {
                  responseHeaders: details.responseHeaders,
                };
              }
            },
            {
              urls: ["http://example.com/status*"],
            },
            ["blocking", "responseHeaders"]
          );
          browser.webRequest.onCompleted.addListener(
            details => {
              browser.test.log(`onCompleted ${JSON.stringify(details)}\n`);
              const mime = details.responseHeaders.find(header => {
                return header.value && header.name === "content-type";
              });
              browser.test.sendMessage("contentType", mime.value);
            },
            {
              urls: ["http://example.com/status*"],
            },
            ["responseHeaders"]
          );
        
  },
]);
