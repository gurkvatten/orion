"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let register1 = true,
            register2 = true;
          if (localStorage.getItem("skip1")) {
            register1 = false;
          }
          if (localStorage.getItem("skip2")) {
            register2 = false;
          }
    
          let listener1 = arg => browser.test.sendMessage("listener1", arg);
          let listener2 = arg => browser.test.sendMessage("listener2", arg);
          let listener3 = arg => browser.test.sendMessage("listener3", arg);
    
          if (register1) {
            browser.startupBlocking.onEvent1.addListener(listener1, "listener1");
          }
          if (register2) {
            browser.startupBlocking.onEvent1.addListener(listener2, "listener2");
            browser.startupBlocking.onEvent2.addListener(listener3, "listener3");
          }
    
          browser.test.onMessage.addListener(msg => {
            if (msg == "unregister2") {
              browser.startupBlocking.onEvent2.removeListener(listener3);
              localStorage.setItem("skip2", true);
            } else if (msg == "unregister1") {
              localStorage.setItem("skip1", true);
              browser.test.sendMessage("unregistered");
            }
          });
    
          browser.test.sendMessage("ready");
        
  },
  async function part_2() {
          let listener = arg => browser.test.sendMessage("triggered", arg);
          browser.startupBlocking.onEvent1.addListener(listener, "triggered");
          browser.test.sendMessage("bg_started");
        
  },
  async function part_3() {
            let listener = arg => browser.test.sendMessage("triggered", arg);
            browser.startupBlocking.onEvent1.addListener(listener, "triggered");
            let listenerNs = arg => browser.test.sendMessage("triggered-et2", arg);
            browser.nonStartupBlocking.onEvent1.addListener(
              listenerNs,
              "triggered-et2"
            );
            browser.test.onMessage.addListener(() => {
              let listener = arg => browser.test.sendMessage("triggered2", arg);
              browser.startupBlocking.onEvent2.addListener(listener, "triggered2");
              browser.test.sendMessage("async-registered-listener");
            });
            browser.test.sendMessage("bg_started");
          
  },
  async function part_4() {
          let listeners = {
            startupBlocking: arg =>
              browser.test.sendMessage("triggered-startupBlocking", arg),
            nonStartupBlocking: arg =>
              browser.test.sendMessage("triggered-nonStartupBlocking", arg),
          };
    
          browser.startupBlocking.onEvent1.addListener(listeners.startupBlocking);
          browser.nonStartupBlocking.onEvent1.addListener(
            listeners.nonStartupBlocking
          );
    
          if (localStorage.getItem("sync-unregister-listeners")) {
            browser.test.log("Removing listeners synchronously");
            browser.startupBlocking.onEvent1.removeListener(
              listeners.startupBlocking
            );
            browser.nonStartupBlocking.onEvent1.removeListener(
              listeners.nonStartupBlocking
            );
          }
    
          browser.test.onMessage.addListener((msg, ...args) => {
            switch (msg) {
              case "async-unregister-listener": {
                const apiNs = args[0];
                browser[apiNs].onEvent1.removeListener(listeners[apiNs]);
                break;
              }
              case "sync-unregister-listeners-next": {
                localStorage.setItem("sync-unregister-listeners", true);
                break;
              }
              default:
                browser.test.fail(`Got unexpected test message: ${msg}`);
            }
            browser.test.sendMessage(`${msg}:done`);
          });
          browser.test.sendMessage("bg_started");
        
  },
  async function part_5() {
          // Listen for options being set so a restart will have them.
          browser.test.onMessage.addListener(async (message, options) => {
            if (message == "set-options") {
              await browser.startupBlocking.testOptions(options);
              browser.test.sendMessage("set-options:done");
            }
          });
          let listener = arg => browser.test.sendMessage("triggered", arg);
          browser.startupBlocking.onEvent1.addListener(listener, "triggered");
          let listener2 = arg => browser.test.sendMessage("triggered", arg);
          browser.startupBlocking.onEvent2.addListener(listener2, "triggered");
          browser.test.sendMessage("bg_started");
        
  },
  async function part_6() {
          let listener = arg => browser.test.sendMessage("triggered", arg);
          browser.startupBlocking.onEvent1.addListener(listener, "triggered");
          browser.test.sendMessage(
            "bg_started",
            browser.runtime.getURL("extpage.html")
          );
        
  },
  async function part_7() {
          const eventParams = [
            { fromCustomParam1: "value1" },
            ["fromCustomParam2"],
          ];
          const otherEventParams = [
            { fromCustomParam1: "value2" },
            ["fromCustomParam2Other"],
          ];
          browser.nonStartupBlocking.onEvent3.addListener(
            function listener1(arg) {
              browser.test.log("listener1 called on nonStartupBlocking.onEvent3");
              browser.test.sendMessage("listener1", arg);
            },
            ...eventParams
          );
          browser.nonStartupBlocking.onEvent3.addListener(
            function listener2(arg) {
              browser.test.log("listener2 called on nonStartupBlocking.onEvent3");
              browser.test.sendMessage("listener2", arg);
            },
            ...eventParams
          );
          browser.nonStartupBlocking.onEvent3.addListener(
            function listener3(arg) {
              browser.test.log("listener3 called on nonStartupBlocking.onEvent3");
              browser.test.sendMessage("listener3", arg);
            },
            ...otherEventParams
          );
          browser.test.sendMessage("ready");
        
  },
]);
