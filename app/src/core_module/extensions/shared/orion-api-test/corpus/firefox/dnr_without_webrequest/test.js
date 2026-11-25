"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let req = await fetch("http://example.com/cors_202", { method: "get" });
          browser.test.assertEq(202, req.status, "not blocked by other extension");
          browser.test.assertEq("cors_response", await req.text());
          browser.test.sendMessage("other_extension_done");
        
  },
  async function part_2() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: { requestMethods: ["get"] },
              action: { type: "block" },
            },
            {
              id: 2,
              condition: { requestMethods: ["head"] },
              action: { type: "allow" },
            },
          ],
        });
        {
          // Request not matching DNR.
          let req = await fetch("http://example.com/cors_202", { method: "post" });
          browser.test.assertEq(202, req.status, "allowed without DNR rule");
          browser.test.assertEq("cors_response", await req.text());
        }
        {
          // Request with "allow" DNR action.
          let req = await fetch("http://example.com/cors_202", { method: "head" });
          browser.test.assertEq(202, req.status, "allowed by DNR rule");
          browser.test.assertEq("", await req.text(), "no response for HEAD");
        }
    
        // Request with "block" DNR action.
        await browser.test.assertRejects(
          fetch("http://example.com/never_reached", { method: "get" }),
          "NetworkError when attempting to fetch resource.",
          "blocked by DNR rule"
        );
    
        browser.test.sendMessage("tested_dnr_block");
      
  },
  async function part_3() {
          await browser.test.assertRejects(
            fetch("http://example.com/never_reached", { method: "get" }),
            "NetworkError when attempting to fetch resource.",
            "blocked by different extension with declarativeNetRequest permission"
          );
          browser.test.sendMessage("other_extension_done");
        
  },
  async function part_4() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [{ id: 1, condition: {}, action: { type: "block" } }],
          });
          browser.test.sendMessage("dnr_registered");
        
  },
  async function part_5() {
          await browser.declarativeNetRequest.updateSessionRules({
            addRules: [
              {
                id: 1,
                condition: { excludedRequestDomains: ["dummy"] },
                action: { type: "upgradeScheme" },
              },
              {
                id: 2,
                // HttpServer does not support https (bug 1742061).
                // As a work-around, we just redirect the https:-request to http.
                condition: { urlFilter: "|https:*" },
                action: {
                  type: "redirect",
                  redirect: { url: "http://dummy/cors_202?from_https" },
                },
                // The upgradeScheme and redirect actions have equal precedence. To
                // make sure that the redirect action is executed when both rules
                // match, we assign a higher priority to the redirect action.
                priority: 2,
              },
            ],
          });
    
          let req = await fetch("http://redir/never_reached");
          browser.test.assertEq(
            "http://dummy/cors_202?from_https",
            req.url,
            "upgradeScheme upgraded to https"
          );
          browser.test.assertEq("cors_response", await req.text());
    
          browser.test.sendMessage("tested_dnr_upgradeScheme");
        
  },
  async function part_6() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: {
                requestDomains: ["example.com"],
                requestMethods: ["get"],
              },
              action: {
                type: "redirect",
                redirect: {
                  url: "http://example.net/cors_202?1",
                },
              },
            },
            {
              id: 2,
              // Note: extension does not have example.org host permission.
              condition: { requestDomains: ["example.org"] },
              action: {
                type: "redirect",
                redirect: {
                  url: "http://example.net/cors_202?2",
                },
              },
            },
          ],
        });
        // The extension only has example.com permission, but the redirects to
        // example.net are still due to the CORS headers from the server.
        {
          // Simple GET request.
          let req = await fetch("http://example.com/never_reached");
          browser.test.assertEq(202, req.status, "redirected by DNR (simple)");
          browser.test.assertEq("http://example.net/cors_202?1", req.url);
          browser.test.assertEq("cors_response", await req.text());
        }
        {
          // GeT request should be matched despite having a different case.
          let req = await fetch("http://example.com/never_reached", {
            method: "GeT",
          });
          browser.test.assertEq(202, req.status, "redirected by DNR (GeT)");
          browser.test.assertEq("http://example.net/cors_202?1", req.url);
          browser.test.assertEq("cors_response", await req.text());
        }
        {
          // Host permission missing for request, request not redirected by DNR.
          // Response is readable due to the CORS response headers from the server.
          let req = await fetch("http://example.org/cors_202?noredir");
          browser.test.assertEq(202, req.status, "not redirected by DNR");
          browser.test.assertEq("http://example.org/cors_202?noredir", req.url);
          browser.test.assertEq("cors_response", await req.text());
        }
    
        browser.test.notifyPass();
      
  },
  async function part_7() {
          // The DNR extension has permissions for example.com, but not for this
          // extension. Therefore the "redirect" action should not apply.
          let req = await fetch("http://example.com/cors_202?other_ext");
          browser.test.assertEq(202, req.status, "not redirected by DNR");
          browser.test.assertEq("http://example.com/cors_202?other_ext", req.url);
          browser.test.assertEq("cors_response", await req.text());
          browser.test.sendMessage("other_extension_done");
        
  },
  async function part_8() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: {
                requestDomains: ["redir"],
                excludedRequestMethods: ["options"],
              },
              action: {
                type: "redirect",
                redirect: {
                  url: "http://example.com/preflight_count",
                },
              },
            },
            {
              id: 2,
              condition: {
                requestDomains: ["example.net"],
                excludedRequestMethods: ["nonsimple"], // note: redirects "options"
              },
              action: {
                type: "redirect",
                redirect: {
                  url: "http://example.com/preflight_count",
                },
              },
            },
          ],
        });
        let req = await fetch("http://redir/never_reached", {
          method: "NONSIMPLE",
        });
        // Extension has permission for "redir", but not for the redirect target.
        // The request is non-simple (see below for explanation of non-simple), so
        // a preflight (OPTIONS) request to /preflight_count is expected before the
        // redirection target is requested.
        browser.test.assertEq(
          "count=1",
          await req.text(),
          "Got preflight before redirect target because of missing host_permissions"
        );
    
        browser.test.sendMessage("continue_preflight_tests");
      
  },
  async function part_9() {
        // Set up redirects from example.com up until dummy.
        let hosts = ["example.com", "example.net", "example.org", "redir", "dummy"];
        let rules = [];
        for (let i = 1; i < hosts.length; ++i) {
          const from = hosts[i - 1];
          const to = hosts[i];
          const end = hosts.length - 1 === i;
          rules.push({
            id: i,
            condition: { requestDomains: [from] },
            action: {
              type: "redirect",
              redirect: {
                // All intermediate redirects should never hit the server, but the
                // last one should..
                url: end ? `http://${to}/?end` : `http://${to}/never_reached`,
              },
            },
          });
        }
        await browser.declarativeNetRequest.updateSessionRules({ addRules: rules });
        let req = await fetch("http://example.com/never_reached");
        browser.test.assertEq(200, req.status, "redirected by DNR (multiple)");
        browser.test.assertEq("http://dummy/?end", req.url, "Last URL in chain");
        browser.test.assertEq("Dummy page", await req.text());
    
        browser.test.notifyPass();
      
  },
  async function part_10() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              // requestMethods is mutually exclusive with the other rule.
              condition: { regexFilter: "^(.+)$", requestMethods: ["post"] },
              action: {
                type: "redirect",
                redirect: {
                  // Appends "?loop" to the request URL
                  regexSubstitution: "\\1?loop",
                },
              },
            },
            {
              id: 2,
              // requestMethods is mutually exclusive with the other rule.
              condition: { requestDomains: ["redir"], requestMethods: ["get"] },
              action: {
                type: "redirect",
                redirect: {
                  // Despite redirect.url matching the condition, the redirect loop
                  // should be caught because of the obvious fact that the URL did
                  // not change.
                  url: "http://redir/cors_202?loop",
                },
              },
            },
          ],
        });
    
        // Redirect where the redirect URL changes at every redirect.
        await browser.test.assertRejects(
          fetch("http://redir/cors_202?loop", { method: "post" }),
          "NetworkError when attempting to fetch resource.",
          "Redirect loop caught (redirect target differs at every redirect)"
        );
    
        async function assertRedirect(url, expected, description) {
          // method: "get" could only match rule 2.
          let res = await fetch(url);
          browser.test.assertDeepEq(
            expected,
            { status: res.status, url: res.url, redirected: res.redirected },
            description
          );
        }
    
        // Redirect with initially a different URL.
        await assertRedirect(
          "http://redir/never_reached?",
          { status: 202, url: "http://redir/cors_202?loop", redirected: true },
          "Redirect loop caught (initially different URL)"
        );
    
        // Redirect where redirect is exactly the same URL as requested.
        await assertRedirect(
          "http://redir/cors_202?loop",
          { status: 202, url: "http://redir/cors_202?loop", redirected: false },
          "Redirect loop caught (redirect target same as initial URL)"
        );
    
        browser.test.notifyPass();
      
  },
  async function part_11() {
        await browser.declarativeNetRequest.updateSessionRules({
          addRules: [
            {
              id: 1,
              condition: { requestDomains: ["redir"], requestMethods: ["post"] },
              action: {
                type: "redirect",
                redirect: {
                  extensionPath: "/war.txt?1",
                },
              },
            },
            {
              id: 2,
              condition: { requestDomains: ["redir"], requestMethods: ["put"] },
              action: {
                type: "redirect",
                redirect: {
                  extensionPath: "/nonwar.txt?2",
                },
              },
            },
          ],
        });
        {
          let req = await fetch("http://redir/never_reached", { method: "post" });
          browser.test.assertEq(200, req.status, "redirected to extensionPath");
          browser.test.assertEq(`${location.origin}/war.txt?1`, req.url);
          browser.test.assertEq("war_ext_res", await req.text());
        }
        // Redirects to extensionPath that is not in web_accessible_resources.
        // While the initiator (extension) would be allowed to read the resource
        // due to it being same-origin, the pre-redirect URL (http://redir) is not
        // matching web_accessible_resources[].matches, so the load is rejected.
        //
        // This behavior differs from Chrome (e.g. at least in Chrome 109) that
        // does allow the load to complete. Extensions who really care about
        // exposing a web-accessible resource to the world can just put an all_urls
        // pattern in web_accessible_resources[].matches.
        await browser.test.assertRejects(
          fetch("http://redir/never_reached", { method: "put" }),
          "NetworkError when attempting to fetch resource.",
          "Redirect to nowar.txt, but pre-redirect host is not in web_accessible_resources[].matches"
        );
    
        browser.test.notifyPass();
      
  },
]);
