"use strict";

TestRunner.suite("runtime", (test) => {
    test("runtime.id is a non-empty string", async () => {
        assertType(api.runtime.id, "string", "runtime.id");
        assert(api.runtime.id.length > 0, "runtime.id should not be empty");
    });

    test("getManifest returns valid object", async () => {
        const m = api.runtime.getManifest();
        assertType(m, "object", "getManifest()");
        assert(m !== null, "manifest should not be null");
        // The Chrome flavor is packaged from the MV3 manifest, the Firefox
        // flavor from the MV2 one.
        assertEqual(m.manifest_version, OrionApi.isChrome ? 3 : 2, "manifest_version");
        assertType(m.name, "string", "manifest.name");
        assertType(m.version, "string", "manifest.version");
    });

    test("getURL produces extension URL", async () => {
        const url = api.runtime.getURL("popup.html");
        assertType(url, "string", "getURL()");
        assert(url.includes("popup.html"), "should contain the path");
        assert(
            url.startsWith("moz-extension://") || url.startsWith("chrome-extension://"),
            `unexpected scheme: ${url}`
        );
    });

    test("getURL with subdirectory path", async () => {
        const url = api.runtime.getURL("lib/test_runner.js");
        assert(url.includes("lib/test_runner.js"), "should preserve path structure");
    });

    test("getURL resolves against this page's own origin", async () => {
        // Inside an extension page, getURL must produce URLs on that page's
        // origin. If the runtime script of another installed extension is
        // active here (e.g. injected without an origin match), the scheme
        // and/or id differ from the address bar.
        const url = api.runtime.getURL("popup.html");
        assertEqual(url, `${location.origin}/popup.html`, "getURL(\"popup.html\")");
    });

    test("runtime.id matches this page's origin", async () => {
        // Orion uses the extension's id as the extension-URL host in both
        // flavors, so the identity the runtime script carries must be the
        // one the page was loaded from.
        assertEqual(api.runtime.id, location.host, "runtime.id vs location.host");
    });

    test("sendMessage to background (ping/pong)", async () => {
        try {
            const response = await call(api.runtime, "sendMessage", { type: "ping" });
            assertEqual(response.type, "pong");
        } catch (e) {
            // sendMessage may not be fully implemented — mark as known gap
            throw new Error("sendMessage not working: " + e.message);
        }
    });
});
