"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async isPrivileged => {
            const worker = new Worker("worker.js");
            worker.isPrivileged = isPrivileged;
            worker.onmessage = function (e) {
              const msg = `${
                this.isPrivileged
                  ? "privileged addon can"
                  : "non-privileged addon can't"
              } instantiate a SharedArrayBuffer
              in a worker`;
              if (e.data === this.isPrivileged) {
                browser.test.succeed(msg);
              } else {
                browser.test.fail(msg);
              }
              browser.test.sendMessage("test-sab-worker:done");
            };
          });
        
  },
  async function part_2() {
          browser.test.onMessage.addListener(async isPrivileged => {
            let succeed = null;
            try {
              new SharedArrayBuffer(1);
              succeed = true;
            } catch (e) {
              succeed = false;
            } finally {
              const msg = `${
                isPrivileged ? "privileged addon can" : "non-privileged addon can't"
              } instantiate a SharedArrayBuffer
              in the main thread`;
              if (succeed === isPrivileged) {
                browser.test.succeed(msg);
              } else {
                browser.test.fail(msg);
              }
              browser.test.sendMessage("test-sab-content:done");
            }
          });
        
  },
]);
