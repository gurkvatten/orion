"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      globalThis.onsecuritypolicyviolation = event => {
        browser.test.assertEq("wasm-eval", event.blockedURI, "blockedURI");
        if (browser.runtime.getManifest().version === 2) {
          // In MV2, wasm eval violations are advisory only, as a transition tool.
          browser.test.assertEq(event.disposition, "report", "MV2 disposition");
        } else {
          browser.test.assertEq(event.disposition, "enforce", "MV3 disposition");
        }
        browser.test.sendMessage("violated_csp", event.originalPolicy);
      };
      try {
        let wasm = new WebAssembly.Module(
          new Uint8Array([0, 0x61, 0x73, 0x6d, 0x1, 0, 0, 0])
        );
        browser.test.assertEq(wasm.toString(), "[object WebAssembly.Module]");
        browser.test.sendMessage("result", "allowed");
      } catch (e) {
        browser.test.assertEq(
          "call to WebAssembly.Module() blocked by CSP",
          e.message,
          "Expected error when blocked"
        );
        browser.test.sendMessage("result", "blocked");
      }
  },
]);
