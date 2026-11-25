"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      async function background() {
        const res = await fetch("http://example.org/echocookies", {
          credentials: "include",
        });
        const cookies = await res.text();
        browser.test.sendMessage("fetchcookies", cookies);
      }
    
      const tests = [
        {
          permissions: ["http://example.org/*"],
          cookies: "none=a; lax=b; strict=c",
        },
        {
          permissions: [],
          cookies: "none=a",
        },
      ];
    
      for (let test of tests) {
        let extension = ExtensionTestUtils.loadExtension({
          background,
          manifest: {
            permissions: test.permissions,
          },
        });
    
        server.registerPathHandler("/echocookies", (request, response) => {
          response.setStatusLine(request.httpVersion, 200, "OK");
          response.setHeader("Content-Type", "text/html; charset=utf-8", false);
          response.setHeader(
            "Access-Control-Allow-Origin",
            `moz-extension://${extension.uuid}`,
            false
          );
          response.setHeader("Access-Control-Allow-Credentials", "true", false);
          response.write(
            request.hasHeader("Cookie") ? request.getHeader("Cookie") : ""
          );
        });
    
        await extension.startup();
        equal(
          await extension.awaitMessage("fetchcookies"),
          test.cookies,
          "extension with permissions can see SameSite-restricted cookies"
        );
    
        await extension.unload();
      }
  },
]);
