"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let onInstalledDetails = null;
      let onStartupFired = false;
      let eventPage = browser.runtime.getManifest().background.persistent === false;
    
      browser.runtime.onInstalled.addListener(details => {
        onInstalledDetails = details;
      });
    
      browser.runtime.onStartup.addListener(() => {
        onStartupFired = true;
      });
    
      browser.test.onMessage.addListener(message => {
        if (message === "get-on-installed-details") {
          onInstalledDetails = onInstalledDetails || { fired: false };
          browser.test.sendMessage("on-installed-details", onInstalledDetails);
        } else if (message === "did-on-startup-fire") {
          browser.test.sendMessage("on-startup-fired", onStartupFired);
        } else if (message === "reload-extension") {
          browser.runtime.reload();
        }
      });
    
      browser.runtime.onUpdateAvailable.addListener(() => {
        browser.test.sendMessage("reloading");
        browser.runtime.reload();
      });
    
      if (eventPage) {
        browser.runtime.onSuspend.addListener(() => {
          browser.test.sendMessage("suspended");
        });
        // an event we use to restart the background
        browser.browserSettings.homepageOverride.onChange.addListener(() => {
          browser.test.sendMessage("homepageOverride");
        });
      }
  },
]);
