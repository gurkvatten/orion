"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.idle.queryState(20).then(
          status => {
            browser.test.assertEq("active", status, "Idle status is active");
            browser.test.notifyPass("idle");
          },
          err => {
            browser.test.fail(`Error: ${err} :: ${err.stack}`);
            browser.test.notifyFail("idle");
          }
        );
      
  },
  async function part_2() {
        browser.idle.queryState(15).then(
          status => {
            browser.test.assertEq("idle", status, "Idle status is idle");
            browser.test.notifyPass("idle");
          },
          err => {
            browser.test.fail(`Error: ${err} :: ${err.stack}`);
            browser.test.notifyFail("idle");
          }
        );
      
  },
  async function part_3() {
        browser.idle.setDetectionInterval(99);
        browser.test.sendMessage("detectionIntervalSet");
      
  },
  async function part_4() {
        browser.idle.setDetectionInterval(99);
        browser.idle.onStateChanged.addListener(newState => {
          browser.test.assertEq(
            "idle",
            newState,
            "listener fired with the expected state"
          );
          browser.test.sendMessage("listenerFired");
        });
        browser.test.sendMessage("listenerAdded");
      
  },
  async function part_5() {
        browser.idle.onStateChanged.addListener(newState => {
          browser.test.assertEq(
            "idle",
            newState,
            "listener fired with the expected state"
          );
          browser.test.sendMessage("listenerFired");
        });
        browser.idle.setDetectionInterval(99);
        browser.test.sendMessage("detectionIntervalSet");
      
  },
  async function part_6() {
        browser.idle.onStateChanged.addListener(newState => {
          browser.test.assertEq(
            "active",
            newState,
            "listener fired with the expected state"
          );
          browser.test.sendMessage("listenerFired");
        });
        browser.test.sendMessage("listenerAdded");
      
  },
  async function part_7() {
            browser.idle.setDetectionInterval(99);
            browser.idle.onStateChanged.addListener(newState => {
              browser.test.assertEq(
                "active",
                newState,
                "listener fired with the expected state"
              );
              browser.test.sendMessage("listenerFired");
            });
          
  },
]);
