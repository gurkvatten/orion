"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let manifest = browser.runtime.getManifest();
        let permObj = { permissions: manifest.optional_permissions, origins: [] };
    
        function verifyPermissions(enabled) {
          for (let perm of manifest.optional_permissions) {
            browser.test.assertEq(
              enabled,
              !!browser[perm],
              `${perm} API is ${
                enabled ? "injected" : "removed"
              } after permission request`
            );
          }
        }
    
        browser.permissions.onAdded.addListener(details => {
          browser.test.assertEq(
            JSON.stringify(details.permissions),
            JSON.stringify(manifest.optional_permissions),
            "expected permissions added"
          );
          verifyPermissions(true);
          browser.test.sendMessage("added");
        });
    
        browser.permissions.onRemoved.addListener(details => {
          browser.test.assertEq(
            JSON.stringify(details.permissions),
            JSON.stringify(manifest.optional_permissions),
            "expected permissions removed"
          );
          verifyPermissions(false);
          browser.test.sendMessage("removed");
        });
    
        browser.test.onMessage.addListener((msg, enabled) => {
          if (msg === "request") {
            browser.permissions.request(permObj);
          } else if (msg === "verify_access") {
            verifyPermissions(enabled);
            browser.test.sendMessage("verified");
          } else if (msg === "revoke") {
            browser.permissions.remove(permObj);
          }
        });
      
  },
  async function part_2() {
        const permObj = { permissions: ["geolocation"] };
        browser.test.onMessage.addListener(async msg => {
          if (msg === "request") {
            await browser.permissions.request(permObj);
          } else if (msg === "remove") {
            await browser.permissions.remove(permObj);
          }
          let result = await browser.permissions.contains(permObj);
          browser.test.sendMessage("done", result);
        });
      
  },
  async function part_3() {
        const permObj = { permissions: ["browserSettings"] };
        browser.test.onMessage.addListener(async msg => {
          if (msg === "request") {
            await browser.permissions.request(permObj);
            await browser.browserSettings.cacheEnabled.set({ value: false });
          } else if (msg === "remove") {
            await browser.permissions.remove(permObj);
          }
          browser.test.sendMessage("done");
        });
      
  },
  async function part_4() {
        const permObj = { permissions: ["privacy"] };
        browser.test.onMessage.addListener(async msg => {
          if (msg === "request") {
            await browser.permissions.request(permObj);
            await browser.privacy.websites.trackingProtectionMode.set({
              value: "always",
            });
          } else if (msg === "remove") {
            await browser.permissions.remove(permObj);
          }
          browser.test.sendMessage("done");
        });
      
  },
  async function part_5() {
            browser.permissions.onAdded.addListener(details => {
              browser.test.sendMessage("added", details);
            });
    
            browser.permissions.onRemoved.addListener(details => {
              browser.test.sendMessage("removed", details);
            });
          
  },
  async function part_6() {
          browser.test.onMessage.addListener(async () => {
            browser.test.log("Calling permissions.request().");
            await browser.permissions.request({ permissions: ["browserSettings"] });
            browser.test.log("permissions.request() resolved.");
            browser.test.sendMessage("done");
          });
        
  },
  async function part_7() {
        browser.test.onMessage.addListener(async msg => {
          browser.test.assertEq("getAll", msg, "expected correct message");
          const permissions = await browser.permissions.getAll();
          browser.test.sendMessage("all", permissions);
        });
    
        browser.permissions.onAdded.addListener(details => {
          browser.test.sendMessage("added", details);
        });
    
        browser.permissions.onRemoved.addListener(details => {
          browser.test.sendMessage("removed", details);
        });
    
        browser.test.sendMessage("ready");
      
  },
  async function part_8() {
        browser.test.onMessage.addListener(async msg => {
          browser.test.assertEq("getAll", msg, "expected correct message");
          const permissions = await browser.permissions.getAll();
          browser.test.sendMessage("all", permissions);
        });
    
        browser.test.sendMessage("ready");
      
  },
  async function part_9() {
        browser.test.onMessage.addListener(async msg => {
          if (msg === "request-good") {
            await browser.permissions.request({
              data_collection: ["technicalAndInteraction"],
            });
    
            const permissions = await browser.permissions.getAll();
            browser.test.sendMessage(`${msg}:done`, permissions);
            return;
          }
    
          if (msg === "request-invalid") {
            try {
              browser.permissions.request({
                data_collection: ["invalid-permission"],
              });
              browser.test.fail("expected error");
            } catch (err) {
              browser.test.assertTrue(
                /Error processing data_collection.0: Value "invalid-permission" must either:/.test(
                  err.message
                ),
                "expected error"
              );
            }
            browser.test.sendMessage(`${msg}:done`);
            return;
          }
    
          if (msg === "request-bad") {
            await browser.test.assertRejects(
              browser.permissions.request({
                data_collection: ["healthInfo"],
              }),
              /Cannot request data collection permission healthInfo since it was not declared in data_collection_permissions.optional/,
              "Expected rejection"
            );
            browser.test.sendMessage(`${msg}:done`);
            return;
          }
    
          browser.test.fail(`Got unexpected msg "${msg}"`);
        });
    
        browser.test.sendMessage("ready");
      
  },
  async function part_10() {
        browser.test.onMessage.addListener(async (msg, arg) => {
          if (msg === "contains") {
            const result = await browser.permissions.contains(arg);
            browser.test.sendMessage(`${msg}:done`, result);
            return;
          }
    
          if (msg === "request") {
            await browser.permissions.request(arg);
            browser.test.sendMessage(`${msg}:done`);
            return;
          }
    
          if (msg === "remove") {
            await browser.permissions.remove(arg);
            browser.test.sendMessage(`${msg}:done`);
            return;
          }
    
          browser.test.fail(`Got unexpected msg "${msg}"`);
        });
    
        browser.test.sendMessage("ready");
      
  },
]);
