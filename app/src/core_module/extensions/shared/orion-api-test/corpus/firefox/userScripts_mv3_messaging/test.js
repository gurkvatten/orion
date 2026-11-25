"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.userScripts.configureWorld({ messaging: true });
          await browser.userScripts.register([
            {
              id: "error checker",
              matches: ["*://example.com/dummy"],
              js: [{ file: "userscript.js" }],
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_2() {
          // Set up message listeners. The user script will send multiple messages,
          // and ultimately send a message will all responses received so far.
          // NOTE: To make sure that the functionality is independent of other
          // messaging APIs, we only register runtime.onUserScriptMessage here,
          // and no other extension messaging APIs.
          browser.runtime.onUserScriptMessage.addListener(
            (msg, sender, sendResponse) => {
              // worldId defaults to "" when not specified in userScripts.register
              // and userScripts.configureWorld. That default value should appear
              // here as sender.userScriptWorldId (also an empty string).
              browser.test.assertEq(
                "",
                sender.userScriptWorldId,
                `Expected userScriptWorldId in onUserScriptMessage for: ${msg}`
              );
              switch (msg) {
                case "expectPromiseResult":
                  return Promise.resolve("Promise");
                case "expectSendResponse":
                  sendResponse("sendResponse");
                  return;
                case "expectSendResponseAsync":
                  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
                  setTimeout(() => sendResponse("sendResponseAsync"), 50);
                  return true;
                case "expectDefaultResponse":
                  return;
                default:
                  browser.test.assertDeepEq(
                    ["Promise", "sendResponse", "sendResponseAsync", undefined],
                    msg,
                    "All sendMessage calls got the expected response"
                  );
                  browser.test.sendMessage("testRuntimeSendMessage:done");
              }
            }
          );
          browser.runtime.onInstalled.addListener(async () => {
            await browser.userScripts.configureWorld({ messaging: true });
            await browser.userScripts.register([
              {
                id: "messaging checker",
                matches: ["*://example.com/dummy"],
                js: [{ file: "userscript.js" }],
              },
            ]);
            browser.test.sendMessage("registered");
          });
        
  },
  async function part_3() {
          let msgCount = 0;
          browser.runtime.onUserScriptMessage.addListener(async (msg, sender) => {
            ++msgCount;
            browser.test.assertEq(
              "non_default_worldId",
              sender.userScriptWorldId,
              "Expected userScriptWorldId in onUserScriptMessage"
            );
            if (msgCount === 1) {
              browser.test.assertEq("initial_message", msg, "Initial message");
              browser.test.log("Calling configureWorld with messaging=false");
              await browser.userScripts.configureWorld({
                worldId: "non_default_worldId",
                messaging: false,
              });
              return;
            }
            if (msgCount === 2) {
              browser.test.assertEq("after_messaging_false", msg, "Second message");
              return;
            }
            // After reload.
            browser.test.fail(`Unexpected onUserScriptMessage ${msgCount}: ${msg}`);
          });
          browser.runtime.onMessage.addListener((msg, sender) => {
            browser.test.assertFalse(
              "userScriptWorldId" in sender,
              "No userScriptWorldId in runtime.onMessage"
            );
            browser.test.assertEq(
              2,
              msgCount,
              "Should reach reportResultViaContentScript after reloading page"
            );
            browser.test.assertEq(
              "Result:ReferenceError: browser is not defined",
              msg,
              "Expected (error) message after reload when messaging=false"
            );
            browser.test.sendMessage("done");
          });
          browser.runtime.onMessageExternal.addListener(msg => {
            browser.test.fail(`Unexpected message: ${msg}`);
          });
          await browser.userScripts.configureWorld({ messaging: true });
          await browser.userScripts.register([
            {
              id: "Test effect of configureWorld(messaging=false) and reload",
              matches: ["*://example.com/dummy"],
              js: [{ file: "userscript.js" }],
              worldId: "non_default_worldId",
            },
          ]);
          browser.test.sendMessage("registered");
        
  },
  async function part_4() {
          browser.runtime.onUserScriptConnect.addListener(port => {
            browser.test.assertEq(
              "connect_world",
              port.sender.userScriptWorldId,
              `Expected userScriptWorldId in onUserScriptConnect: ${port.name}`
            );
            if (port.name === "first_port") {
              port.onMessage.addListener(msg => {
                browser.test.assertDeepEq(
                  { messageBack: { hi: 1 } },
                  msg,
                  "port.onMessage triggered from user script"
                );
                port.disconnect();
                // ^ should trigger port.onDisconnect in the user script, which
                // will signal back the status by connecting again to second_port.
              });
              port.onDisconnect.addListener(() => {
                // We should not expect a disconnect, because we call
                // port.disconnect() from this end.
                browser.test.fail(`Unexpected port.onDisconnect: ${port.error}`);
              });
              port.postMessage({ hi: 1 });
              return;
            }
            if (port.name === "second_port") {
              port.onMessage.addListener(msg => {
                browser.test.assertDeepEq(
                  { errorFromFirstPort: null },
                  msg,
                  "When we disconnect first_port, other port.error should be null"
                );
                browser.test.sendMessage("port.onMessage:done");
              });
              port.onDisconnect.addListener(() => {
                browser.test.assertDeepEq(
                  null,
                  port.error,
                  "Our port.error when other side (user script) disconnects"
                );
                browser.test.sendMessage("port.onDisconnect:done");
              });
              return;
            }
            browser.test.fail(`Unexpected port: ${port.name}`);
          });
          browser.runtime.onInstalled.addListener(async () => {
            await browser.userScripts.configureWorld({ messaging: true });
            await browser.userScripts.register([
              {
                id: "messaging checker",
                matches: ["*://example.com/dummy"],
                js: [{ file: "userscript.js" }],
                worldId: "connect_world",
              },
            ]);
            browser.test.sendMessage("registered");
          });
        
  },
  async function part_5() {
            browser.runtime.onUserScriptConnect.addListener(port => {
              browser.test.assertEq("", port.name, "Got default port.name");
              browser.test.assertEq(
                "",
                port.sender.userScriptWorldId,
                "Got default userScriptWorldId"
              );
              port.onDisconnect.addListener(() => {
                browser.test.assertDeepEq(
                  null,
                  port.error,
                  "Closing port due to a navigation is not an error"
                );
                browser.test.sendMessage("done");
              });
              port.postMessage("bye");
            });
            await browser.userScripts.configureWorld({ messaging: true });
            await browser.userScripts.register([
              {
                id: "messaging checker",
                matches: ["*://example.com/dummy"],
                js: [{ file: "userscript.js" }],
              },
            ]);
            browser.test.sendMessage("registered");
          
  },
]);
