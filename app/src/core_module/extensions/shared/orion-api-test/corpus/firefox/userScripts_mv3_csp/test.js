"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          // This function will be serialized and executed as a user script.
          function testEvalInWorld(worldId) {
            try {
              // eslint-disable-next-line no-eval
              eval("// If blocked by CSP, then this will throw");
              window.wrappedJSObject.res.evalOk.push(worldId);
            } catch {
              window.wrappedJSObject.res.evalBlocked.push(worldId);
            }
          }
          browser.test.onMessage.addListener(async (msg, args) => {
            if (msg === "grantUserScriptsPermission") {
              await browser.permissions.request({ permissions: ["userScripts"] });
              // To allow testEvalCheckerWithUserScripts() to know for certain
              // whether the user scripts have executed, trigger a notification
              // after all other user scripts have run.
              await browser.userScripts.register([
                {
                  id: "evalChecker_done_marker",
                  matches: ["*://example.com/evalChecker"],
                  js: [{ code: "userScriptsExecutionDone.resolve();" }],
                  // document_idle is guaranteed to run after document_end above.
                  runAt: "document_idle",
                  world: "MAIN",
                },
              ]);
            } else if (msg === "registerUserScriptForWorldId") {
              const worldId = args;
              await browser.userScripts.register([
                {
                  id: `script for world ${worldId}.`,
                  matches: ["*://example.com/evalChecker"],
                  js: [{ code: `(${testEvalInWorld})("${worldId}")` }],
                  runAt: "document_end",
                  worldId: worldId,
                },
              ]);
            } else if (msg === "configureWorld") {
              await browser.userScripts.configureWorld(args);
            } else if (msg === "resetWorldConfiguration") {
              await browser.userScripts.resetWorldConfiguration(args);
            } else if (msg === "getWorldConfigurations") {
              let res = await browser.userScripts.getWorldConfigurations();
              for (let properties of res) {
                // We are only interested in the worldId / csp properties, so drop
                // all other properties so we can keep the assertions simple.
                delete properties.messaging;
              }
              browser.test.sendMessage("getWorldConfigurations:done", res);
              return;
            } else {
              browser.test.fail(`Unexpected message: ${msg}`);
            }
            browser.test.sendMessage(`${msg}:done`);
          });
        
  },
]);
