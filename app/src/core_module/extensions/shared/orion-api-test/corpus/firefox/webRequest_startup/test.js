"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("got-request");
            },
            { urls: ["http://example.com/data/file_sample.html"] }
          );
          browser.test.sendMessage("ready");
        
  },
  async function part_2() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("got-request");
            },
            { urls: ["http://example.com/data/file_sample.html"] }
          );
        
  },
  async function part_3() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.fail("Listener should not have been called");
            },
            { urls: ["http://test1.example.com/*"] },
            ["blocking"]
          );
        
  },
  async function part_4() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("got-request");
            },
            { urls: ["http://example.com/data/file_sample.html"] },
            ["blocking"]
          );
        
  },
  async function part_5() {
            browser.runtime.onUpdateAvailable.addListener(() => {
              browser.test.sendMessage("postponed");
            });
    
            browser.webRequest.onBeforeRequest.addListener(
              () => {
                browser.test.sendMessage("got-request");
              },
              { urls: ["http://example.com/data/file_sample.html"] },
              ["blocking"]
            );
          
  },
  async function part_6() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("got-request");
            },
            { urls: ["http://example.com/data/file_sample.html"] },
            ["blocking"]
          );
          browser.webRequest.onSendHeaders.addListener(
            () => {
              browser.test.sendMessage("got-sendheaders");
            },
            { urls: ["http://example.com/data/file_sample.html"] }
          );
          // Force a staged updated.
          browser.runtime.onUpdateAvailable.addListener(async details => {
            if (details && details.version) {
              // This should be the version of the pending update.
              browser.test.assertEq("2.0", details.version, "correct version");
              browser.test.sendMessage("delay");
            }
          });
        
  },
  async function part_7() {
          browser.test.sendMessage("loaded");
        
  },
  async function part_8() {
          browser.webRequest.onBeforeRequest.addListener(
            () => {
              browser.test.sendMessage("got-request");
            },
            { urls: ["http://example.com/data/file_sample.html"] },
            ["blocking"]
          );
          // Force a staged updated.
          browser.runtime.onUpdateAvailable.addListener(async details => {
            if (details && details.version) {
              // This should be the version of the pending update.
              browser.test.assertEq("2.0", details.version, "correct version");
              browser.test.sendMessage("delay");
            }
          });
        
  },
]);
