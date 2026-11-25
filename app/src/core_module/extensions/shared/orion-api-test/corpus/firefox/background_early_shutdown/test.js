"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.sendMessage("background_startup_observed");
        
  },
  async function part_2() {
          browser.test.sendMessage("background_starting");
        
  },
  async function part_3() {
          window.onload = () => {
            browser.test.sendMessage("background_has_fully_loaded");
          };
        
  },
  async function part_4() {
          const domreadyPromise = new Promise(resolve => {
            window.addEventListener("load", resolve, { once: true });
          });
          browser.permissions.onAdded.addListener(() => {
            browser.test.log("permissions.onAdded has fired");
            // Wait for DOMContentLoaded to have fired before notifying the test.
            // This guarantees that backgroundState is "running" instead of
            // potentially "starting".
            domreadyPromise.then(() => {
              browser.test.sendMessage("event_fired");
            });
          });
        
  },
  async function part_5() {
            window.addEventListener(
              "load",
              () => {
                browser.test.sendMessage("persistentbg_started");
              },
              { once: true }
            );
          
  },
]);
