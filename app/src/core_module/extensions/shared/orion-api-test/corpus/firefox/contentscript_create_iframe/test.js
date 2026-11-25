"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.runtime.onMessage.addListener(msg => {
          let { name, availableAPIs, manifest, testGetManifest } = msg;
          let hasExtTabsAPI = availableAPIs.indexOf("tabs") > 0;
          let hasExtWindowsAPI = availableAPIs.indexOf("windows") > 0;
    
          browser.test.assertFalse(
            hasExtTabsAPI,
            "the created iframe should not be able to use privileged APIs (tabs)"
          );
          browser.test.assertFalse(
            hasExtWindowsAPI,
            "the created iframe should not be able to use privileged APIs (windows)"
          );
    
          let {
            browser_specific_settings: {
              gecko: { id: expectedManifestGeckoId },
            },
          } = chrome.runtime.getManifest();
          let {
            browser_specific_settings: {
              gecko: { id: actualManifestGeckoId },
            },
          } = manifest;
    
          browser.test.assertEq(
            actualManifestGeckoId,
            expectedManifestGeckoId,
            "the add-on manifest should be accessible from the created iframe"
          );
    
          let {
            browser_specific_settings: {
              gecko: { id: testGetManifestGeckoId },
            },
          } = testGetManifest;
    
          browser.test.assertEq(
            testGetManifestGeckoId,
            expectedManifestGeckoId,
            "GET_MANIFEST() returns manifest data before extension unload"
          );
    
          browser.test.sendMessage(name);
        });
      
  },
]);
