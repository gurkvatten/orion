"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let port;
        browser.test.onMessage.addListener(async (what, payload) => {
          if (what == "request") {
            await browser.permissions.request({ permissions: ["nativeMessaging"] });
            // connectNative requires permission
            port = browser.runtime.connectNative("echo");
            port.onMessage.addListener(msg => {
              browser.test.sendMessage("message", msg);
            });
            browser.test.sendMessage("ready");
          } else if (what == "send") {
            if (payload._json) {
              let json = payload._json;
              payload.toJSON = () => json;
              delete payload._json;
            }
            port.postMessage(payload);
          }
        });
      
  },
  async function part_2() {
        let port = browser.runtime.connectNative(appname);
        let MSG = "test";
        port.onMessage.addListener(msg => {
          browser.test.assertEq(MSG, msg, "Got expected message back");
          browser.test.sendMessage("done");
        });
        port.postMessage(MSG);
      
  },
  async function part_3() {
          browser.test.onMessage.addListener(async (appname, expectedError) => {
            await browser.test.assertRejects(
              browser.runtime.sendNativeMessage(appname, "dummymsg"),
              expectedError,
              "Expected sendNativeMessage error"
            );
            browser.test.sendMessage("done");
          });
        
  },
  async function part_4() {
        let MSG = { test: "hello world" };
    
        // Check error handling
        await browser.test.assertRejects(
          browser.runtime.sendNativeMessage("nonexistent", MSG),
          "No such native application nonexistent",
          "sendNativeMessage() to a nonexistent app failed"
        );
    
        // Check regular message exchange
        let reply = await browser.runtime.sendNativeMessage("echo", MSG);
    
        let expected = JSON.stringify(MSG);
        let received = JSON.stringify(reply);
        browser.test.assertEq(expected, received, "Received echoed native message");
    
        browser.test.sendMessage("finished");
      
  },
  async function part_5() {
        let port = browser.runtime.connectNative("echo");
        port.onMessage.addListener((msg, msgPort) => {
          browser.test.assertEq(
            port,
            msgPort,
            "onMessage handler should receive the port as the second argument"
          );
          browser.test.sendMessage("message", msg);
        });
        port.onDisconnect.addListener(() => {
          browser.test.fail("onDisconnect should not be called for disconnect()");
        });
        browser.test.onMessage.addListener((what, payload) => {
          if (what == "send") {
            if (payload._json) {
              let json = payload._json;
              payload.toJSON = () => json;
              delete payload._json;
            }
            port.postMessage(payload);
          } else if (what == "disconnect") {
            try {
              port.disconnect();
              browser.test.assertThrows(
                () => port.postMessage("void"),
                "Attempt to postMessage on disconnected port"
              );
              browser.test.sendMessage("disconnect-result", { success: true });
            } catch (err) {
              browser.test.sendMessage("disconnect-result", {
                success: false,
                errmsg: err.message,
              });
            }
          }
        });
        browser.test.sendMessage("ready");
      
  },
  async function part_6() {
        const PAYLOAD = "0123456789A";
        let port = browser.runtime.connectNative("echo");
        try {
          port.postMessage(PAYLOAD);
          browser.test.sendMessage("result", null);
        } catch (ex) {
          browser.test.sendMessage("result", ex.message);
        }
      
  },
  async function part_7() {
        const PAYLOAD = "0123456789A";
        let port = browser.runtime.connectNative("echo");
        port.onDisconnect.addListener(msgPort => {
          browser.test.assertEq(
            port,
            msgPort,
            "onDisconnect handler should receive the port as the first argument"
          );
          browser.test.assertEq(
            "Native application tried to send a message of 13 bytes, which exceeds the limit of 10 bytes.",
            port.error && port.error.message
          );
          browser.test.sendMessage("result", "disconnected");
        });
        port.onMessage.addListener(() => {
          browser.test.sendMessage("result", "message");
        });
        port.postMessage(PAYLOAD);
      
  },
  async function part_8() {
        browser.test.assertEq(
          chrome.runtime.connectNative,
          undefined,
          "chrome.runtime.connectNative does not exist without nativeMessaging permission"
        );
        browser.test.assertEq(
          browser.runtime.connectNative,
          undefined,
          "browser.runtime.connectNative does not exist without nativeMessaging permission"
        );
        browser.test.assertEq(
          chrome.runtime.sendNativeMessage,
          undefined,
          "chrome.runtime.sendNativeMessage does not exist without nativeMessaging permission"
        );
        browser.test.assertEq(
          browser.runtime.sendNativeMessage,
          undefined,
          "browser.runtime.sendNativeMessage does not exist without nativeMessaging permission"
        );
        browser.test.sendMessage("finished");
      
  },
  async function part_9() {
        let port = browser.runtime.connectNative("info");
        port.onMessage.addListener(msg => {
          browser.test.sendMessage("result", msg);
        });
      
  },
  async function part_10() {
        let port = browser.runtime.connectNative("stderr");
        port.onDisconnect.addListener(msgPort => {
          browser.test.assertEq(
            port,
            msgPort,
            "onDisconnect handler should receive the port as the first argument"
          );
          browser.test.assertEq(
            null,
            port.error,
            "Normal application exit is not an error"
          );
          browser.test.sendMessage("finished");
        });
      
  },
  async function part_11() {
        function once() {
          return new Promise(resolve => {
            let MSG = "hello";
            let port = browser.runtime.connectNative("echo");
    
            port.onMessage.addListener(msg => {
              browser.test.assertEq(MSG, msg, "Got expected message back");
              port.disconnect();
              resolve();
            });
            port.postMessage(MSG);
          });
        }
    
        await once();
        await once();
        browser.test.notifyPass("multiple-connect");
      
  },
  async function part_12() {
          browser.runtime.onSuspend.addListener(() => {
            browser.test.sendMessage("bgpage:suspending");
          });
    
          let port;
          browser.test.onMessage.addListener(async (msg, ...args) => {
            switch (msg) {
              case "request-permission": {
                await browser.permissions.request({
                  permissions: ["nativeMessaging"],
                });
                break;
              }
              case "delayedecho-sendmessage": {
                browser.runtime
                  .sendNativeMessage("delayedecho", args[0])
                  .then(msg =>
                    browser.test.sendMessage(
                      `delayedecho-sendmessage:got-reply`,
                      msg
                    )
                  );
                break;
              }
              case "connectNative": {
                if (port) {
                  browser.test.fail(`Unexpected already connected NativeApp port`);
                } else {
                  port = browser.runtime.connectNative("echo");
                }
                break;
              }
              case "disconnectNative": {
                if (!port) {
                  browser.test.fail(`Unexpected undefined NativeApp port`);
                }
                port?.disconnect();
                break;
              }
              default:
                browser.test.fail(`Got an unexpected test message: ${msg}`);
            }
    
            browser.test.sendMessage(`${msg}:done`);
          });
          browser.test.sendMessage("bg:ready");
        
  },
]);
