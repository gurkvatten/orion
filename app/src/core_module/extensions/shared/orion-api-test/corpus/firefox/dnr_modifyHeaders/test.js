"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const makeModifyHostRule = (id, urlFilter, value) => ({
          id,
          condition: { urlFilter },
          action: {
            type: "modifyHeaders",
            requestHeaders: [{ operation: "set", header: "Host", value }],
          },
        });
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            makeModifyHostRule(1, "yes_host_permissions", "yes"),
            makeModifyHostRule(2, "no_host_permissions", "no"),
            makeModifyHostRule(3, "restricted_domain", "restricted"),
          ],
        });
    
        browser.test.assertEq(
          "yes",
          await (await fetch("http://dummy/host?yes_host_permissions")).text(),
          "Host header value allowed if extension has permission for new value"
        );
    
        browser.test.assertEq(
          "dummy",
          await (await fetch("http://dummy/host?no_host_permissions")).text(),
          "Host header value ignored if extension misses permission for new value"
        );
    
        browser.test.assertEq(
          "dummy",
          await (await fetch("http://dummy/host?restricted_domain")).text(),
          "Host header value ignored if new host is in restrictedDomains"
        );
    
        browser.test.notifyPass();
      
  },
  async function part_2() {
        const hostHeaderValue = browser.runtime.getManifest().name;
    
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: { resourceTypes: ["xmlhttprequest"] },
              action: {
                type: "modifyHeaders",
                requestHeaders: [
                  { operation: "set", header: "Host", value: hostHeaderValue },
                  // Add a unique header for each request to verify that the
                  // extension can still modify other headers despite failure to
                  // modify the host header.
                  { operation: "set", header: hostHeaderValue, value: "setbydnr" },
                ],
              },
            },
          ],
        });
        browser.test.notifyPass();
      
  },
  async function part_3() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { urlFilter: "/responseheadersFixture" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    { operation: "set", header: "a", value: "a-first" },
                    // remove after set should be ignored:
                    { operation: "remove", header: "a" },
                    // Second set should be ignored:
                    { operation: "set", header: "a", value: "a-second" },
                    // But append is permitted:
                    { operation: "append", header: "a", value: "a-third" },
                    // Another append is allowed too:
                    { operation: "append", header: "a", value: "a-fourth" },
                    // An unrelated set is accepted:
                    { operation: "set", header: "b", value: "b-dnr" },
                    // An unrelated remove is also accepted:
                    { operation: "remove", header: "c" },
                    // An unrelated append is also accepted:
                    { operation: "append", header: "d", value: "d-dnr" },
                    // The server also sends the "e" header, we don't touch that.
    
                    // The server sends the www-authenticate header on two lines,
                    // which should be removed.
                    { operation: "remove", header: "www-authenticate" },
                    // The server also sends the proxy-authenticate header on two
                    // lines, but we don't touch that.
                  ],
                },
              },
            ],
          });
    
          let { headers } = await fetch("http://dummy/responseheadersFixture");
          browser.test.assertEq(
            "a-first, a-third, a-fourth",
            headers.get("a"),
            "a set, ignored set + remove, 2x append"
          );
          browser.test.assertEq("b-dnr", headers.get("b"), "b set");
          browser.test.assertEq(null, headers.get("c"), "c removed");
          browser.test.assertEq("server_d, d-dnr", headers.get("d"), "d appended");
          browser.test.assertEq("server_e", headers.get("e"), "e not touched");
          browser.test.assertEq(
            null,
            headers.get("www-authenticate"),
            "multi-line www-authenticate header removed"
          );
    
          // Multi-line http headers cannot be tested through fetch/Headers. This is
          // a known limitation of that API, see e.g. note about Set-Cookie in the
          // fetch spec - https://fetch.spec.whatwg.org/#headers-class
          browser.test.assertEq(
            null, // Note: null because Headers does not see multi-line headers.
            headers.get("proxy-authenticate"),
            "multi-line proxy-authenticate header kept (but fetch cannot see it)"
          );
    
          // XMLHttpRequest can return multi-line values, so we use that instead.
          const xhr = new XMLHttpRequest();
          await new Promise(r => {
            xhr.onloadend = r;
            xhr.open("GET", "http://dummy/responseheadersFixture?xhr");
            xhr.send();
          });
          browser.test.assertEq(
            null,
            xhr.getResponseHeader("www-authenticate"),
            "multi-line www-authenticate header removed"
          );
          browser.test.assertEq(
            "first_line\nsecond_line",
            xhr.getResponseHeader("proxy-authenticate"),
            "multi-line proxy-authenticate header kept (seen through XHR)"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_4() {
          // By default, a DNR condition excludes the main frame. But to verify that
          // the CSP works, we have to modify the CSP header of a document request.
          const resourceTypes = ["main_frame"];
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { resourceTypes, urlFilter: "/csp?remove" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    { operation: "remove", header: "Content-Security-Policy" },
                  ],
                },
              },
              {
                id: 2,
                condition: { resourceTypes, urlFilter: "/csp?append_to_server" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    {
                      operation: "append",
                      header: "Content-Security-Policy",
                      // Server has "default-src http://yes http://maybe". When
                      // multiple CSP header lines are present, all policies should
                      // be enforced, thus "http://no" below should be ignored, and
                      // the "http://maybe" from the server be ignored.
                      value: "connect-src http://YES http://not-maybe http://no",
                    },
                  ],
                },
              },
              {
                id: 3,
                condition: { resourceTypes, urlFilter: "/csp?set_and_append" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    {
                      operation: "set",
                      header: "Content-Security-Policy",
                      value: "connect-src 1-of-2 http://yes http://maybe",
                    },
                    {
                      operation: "append",
                      header: "Content-Security-Policy",
                      value: "connect-src 2-of-2 http://yes",
                    },
                  ],
                },
              },
            ],
          });
    
          browser.test.notifyPass();
        
  },
  async function part_5() {
          // By default, a DNR condition excludes the main frame. But this test uses
          // a document load to verify that cookie header modifications (if any) are
          // reflected in document.cookie.
          const resourceTypes = ["main_frame"];
    
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { resourceTypes, urlFilter: "dnr_resp_drop_cookie" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [{ operation: "remove", header: "set-cookie" }],
                },
              },
              {
                id: 2,
                condition: { resourceTypes, urlFilter: "dnr_resp_set_cookie" },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    {
                      operation: "set",
                      header: "set-cookie",
                      value: "dnr_res=set; max-age=999",
                    },
                  ],
                },
              },
              {
                id: 3,
                condition: { resourceTypes, urlFilter: "dnr_set_cookie_to_req" },
                action: {
                  type: "modifyHeaders",
                  requestHeaders: [
                    { operation: "set", header: "cookie", value: "dnr_req=1" },
                  ],
                },
              },
              {
                id: 4,
                condition: {
                  resourceTypes,
                  urlFilter: "dnr_append_cookie_to_req_and_res",
                },
                action: {
                  type: "modifyHeaders",
                  requestHeaders: [
                    // Just for extra coverage, mix upper/lower case.
                    { operation: "append", header: "Cookie", value: "DNR_APP=1" },
                    { operation: "append", header: "cookie", value: "DNR_app=2" },
                  ],
                  responseHeaders: [
                    {
                      operation: "append",
                      header: "set-cookie",
                      value: "dnr_res=appended; max-age=999",
                    },
                  ],
                },
              },
              {
                id: 5,
                condition: {
                  resourceTypes,
                  urlFilter: "dnr_set_server_cookies_expired",
                },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    {
                      operation: "set",
                      header: "set-cookie",
                      value: "food=deletedbydnr; second=deletedbydnr; max-age=-1",
                    },
                    {
                      operation: "append",
                      header: "set-cookie",
                      value: "second=deletedbydnr; max-age=-1",
                    },
                  ],
                },
              },
              {
                id: 6,
                condition: {
                  resourceTypes,
                  urlFilter: "dnr_resp_append_expired_cookie",
                },
                action: {
                  type: "modifyHeaders",
                  responseHeaders: [
                    {
                      operation: "append",
                      header: "set-cookie",
                      value: "dnr_res=deleteme; max-age=-1",
                    },
                  ],
                },
              },
            ],
          });
    
          browser.test.notifyPass();
        
  },
]);
