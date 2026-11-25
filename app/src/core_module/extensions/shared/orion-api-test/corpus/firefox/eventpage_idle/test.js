"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
            () => {
              browser.test.sendMessage("allowPopupsForUserEvents");
            }
          );
          browser.runtime.onSuspend.addListener(async () => {
            let setting =
              await browser.browserSettings.allowPopupsForUserEvents.get({});
            browser.test.sendMessage("suspended", setting);
          });
        
  },
  async function part_2() {
            let start = Date.now();
    
            browser.runtime.onSuspend.addListener(() => {
              browser.test.sendMessage("done", Date.now() - start);
            });
    
            browser.runtime.getBrowserInfo();
            // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
            setTimeout(() => browser.runtime.getBrowserInfo(), 50);
          
  },
  async function part_3() {
            browser.test.log("background script start");
            browser.runtime.onSuspend.addListener(() => {
              browser.test.sendMessage("suspended");
            });
            browser.test.sendMessage("ready");
          
  },
  async function part_4() {
          browser.test.onMessage.addListener(async () => {
            await browser.runtime.getBrowserInfo();
            browser.test.sendMessage("done");
          });
          browser.test.sendMessage("ready");
        
  },
  async function part_5() {
            browser.runtime.onSuspend.addListener(() => {
              // return a promise that never resolves
              return new Promise(() => {});
            });
          
  },
  async function part_6() {
            browser.runtime.onSuspend.addListener(() => {
              // Raise an error to test error handling in onSuspend
              return Promise.reject("testing reject");
            });
          
  },
  async function part_7() {
            let resolveSuspend;
            browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
              () => {
                browser.test.sendMessage("allowPopupsForUserEvents");
              }
            );
            browser.runtime.onSuspend.addListener(() => {
              browser.test.sendMessage("suspending");
              return new Promise(resolve => {
                resolveSuspend = resolve;
              });
            });
            browser.runtime.onSuspendCanceled.addListener(() => {
              browser.test.sendMessage("suspendCanceled");
            });
            browser.test.onMessage.addListener(() => {
              resolveSuspend();
            });
          
  },
  async function part_8() {
          browser.runtime.onSuspend.addListener(() => {
            browser.test.fail(
              `runtime.onSuspend listener should have not been called`
            );
          });
    
          // Call an API method implemented in the parent process (to be sure runtime.onSuspend
          // listener is going to be fully registered from a parent process perspective by the
          // time we will send the "bg-ready" test message).
          await browser.runtime.getBrowserInfo();
    
          browser.test.sendMessage("bg-ready");
        
  },
  async function part_9() {
          browser.test.sendMessage("bg-ready");
        
  },
  async function part_10() {
          let idx = 0;
          browser.browserSettings.allowPopupsForUserEvents.onChange.addListener(
            async () => {
              const currIdx = idx++;
              await new Promise((resolve, reject) => {
                browser.test.onMessage.addListener(msg => {
                  switch (`${msg}-${currIdx}`) {
                    case "unblock-promise-0":
                      resolve();
                      browser.test.sendMessage("allowPopupsForUserEvents:resolved");
                      break;
                    case "unblock-promise-1":
                      reject(new Error("expected-test-rejection"));
                      browser.test.sendMessage("allowPopupsForUserEvents:rejected");
                      break;
                    default:
                      browser.test.fail(`Unexpected test message: ${msg}`);
                  }
                });
                browser.test.sendMessage("allowPopupsForUserEvents:awaiting");
              });
            }
          );
    
          browser.runtime.onSuspend.addListener(() => {
            // Raise an error to test error handling in onSuspend
            return browser.test.sendMessage("runtime-on-suspend");
          });
    
          browser.test.sendMessage("bg-script-ready");
        
  },
  async function part_11() {
          browser.test.sendMessage("wakey");
        
  },
]);
