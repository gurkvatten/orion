"use strict";

// Detects the environment the extension is running in, so the launcher can
// offer the appropriate test corpora:
//   - flavor:          "firefox" (moz-extension://) or "chrome" (chrome-extension://)
//   - manifestVersion: 2 or 3, read from the loaded manifest
//
// Orion picks the scheme from the installed extension's type (firefox vs
// chrome), and the build picks the manifest version (moz build = MV2, chrome
// build = MV3). Keeping the two axes independent here means a future
// chrome-extension MV2 build (or the reverse) is detected correctly without
// code changes.

const OrionEnv = (() => {
    const scheme = (location.protocol || "").replace(/:$/, "");
    const flavor =
        scheme === "moz-extension" ? "firefox" :
        scheme === "chrome-extension" ? "chrome" :
        "unknown";

    let manifestVersion = 2;
    try {
        const api = (typeof browser !== "undefined" && browser.runtime) ? browser : chrome;
        manifestVersion = api.runtime.getManifest().manifest_version || 2;
    } catch (e) {
        // Leave the default; a missing runtime means we can't tell.
    }

    return { scheme, flavor, manifestVersion };
})();

if (typeof window !== "undefined") {
    window.OrionEnv = OrionEnv;
}
