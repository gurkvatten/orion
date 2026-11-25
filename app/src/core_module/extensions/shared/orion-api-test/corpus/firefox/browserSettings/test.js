"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        await browser.test.assertRejects(
          browser.browserSettings.contextMenuShowEvent.set({ value: "bad" }),
          /bad is not a valid value for contextMenuShowEvent/,
          "contextMenuShowEvent.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.overrideDocumentColors.set({ value: 2 }),
          /2 is not a valid value for overrideDocumentColors/,
          "overrideDocumentColors.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.overrideDocumentColors.set({ value: "bad" }),
          /bad is not a valid value for overrideDocumentColors/,
          "overrideDocumentColors.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.overrideContentColorScheme.set({ value: 0 }),
          /0 is not a valid value for overrideContentColorScheme/,
          "overrideContentColorScheme.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.overrideContentColorScheme.set({ value: "bad" }),
          /bad is not a valid value for overrideContentColorScheme/,
          "overrideContentColorScheme.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.zoomFullPage.set({ value: 0 }),
          /0 is not a valid value for zoomFullPage/,
          "zoomFullPage.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.zoomFullPage.set({ value: "bad" }),
          /bad is not a valid value for zoomFullPage/,
          "zoomFullPage.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.zoomSiteSpecific.set({ value: 0 }),
          /0 is not a valid value for zoomSiteSpecific/,
          "zoomSiteSpecific.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.zoomSiteSpecific.set({ value: "bad" }),
          /bad is not a valid value for zoomSiteSpecific/,
          "zoomSiteSpecific.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.set({ value: 0 }),
          /0 is not a valid value for verticalTabs/,
          "verticalTabs.set rejects with an invalid value."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.set({ value: "bad" }),
          /bad is not a valid value for verticalTabs/,
          "verticalTabs.set rejects with an invalid value."
        );
    
        browser.test.sendMessage("done");
      
  },
  async function part_2() {
        await browser.test.assertRejects(
          browser.browserSettings.closeTabsByDoubleClick.set({ value: true }),
          /android is not a supported platform for the closeTabsByDoubleClick setting/,
          "closeTabsByDoubleClick.set rejects on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.closeTabsByDoubleClick.get({}),
          /android is not a supported platform for the closeTabsByDoubleClick setting/,
          "closeTabsByDoubleClick.get rejects on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.closeTabsByDoubleClick.clear({}),
          /android is not a supported platform for the closeTabsByDoubleClick setting/,
          "closeTabsByDoubleClick.clear rejects on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.set({ value: "bad" }),
          /bad is not a valid value for verticalTabs/,
          "verticalTabs.set rejects with an invalid value on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.set({ value: true }),
          /android is not a supported platform for verticalTabs/,
          "verticalTabs.set rejects on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.get({}),
          /android is not a supported platform for verticalTabs/,
          "verticalTabs.get rejects on Android."
        );
    
        await browser.test.assertRejects(
          browser.browserSettings.verticalTabs.clear({}),
          /android is not a supported platform for verticalTabs/,
          "verticalTabs.clear rejects on Android."
        );
    
        browser.test.sendMessage("done");
      
  },
  async function part_3() {
          browser.runtime.onUpdateAvailable.addListener(async details => {
            if (details) {
              await browser.browserSettings.webNotificationsDisabled.set({
                value: true,
              });
              if (details.version) {
                // This should be the version of the pending update.
                browser.test.assertEq("2.0", details.version, "correct version");
                browser.test.notifyPass("delay");
              }
            } else {
              browser.test.fail("no details object passed");
            }
          });
          browser.test.sendMessage("ready");
        
  },
]);
