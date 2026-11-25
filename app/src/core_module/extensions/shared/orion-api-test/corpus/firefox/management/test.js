"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const permObj = { permissions: ["management"] };
    
        let hasPerm = await browser.permissions.contains(permObj);
        browser.test.assertTrue(!hasPerm, "does not have management permission");
        browser.test.assertTrue(
          !!browser.management,
          "management namespace exists"
        );
        // These require permission
        let requires_permission = [
          "getAll",
          "get",
          "install",
          "setEnabled",
          "onDisabled",
          "onEnabled",
          "onInstalled",
          "onUninstalled",
        ];
    
        async function testAvailable() {
          // These are always available regardless of permission.
          for (let fn of ["getSelf", "uninstallSelf"]) {
            browser.test.assertTrue(
              !!browser.management[fn],
              `management.${fn} exists`
            );
          }
    
          let hasPerm = await browser.permissions.contains(permObj);
          for (let fn of requires_permission) {
            browser.test.assertEq(
              hasPerm,
              !!browser.management[fn],
              `management.${fn} does not exist`
            );
          }
        }
    
        await testAvailable();
    
        browser.test.onMessage.addListener(async () => {
          browser.test.log("test with permission");
    
          // get permission
          await browser.permissions.request(permObj);
          let hasPerm = await browser.permissions.contains(permObj);
          browser.test.assertTrue(
            hasPerm,
            "management permission.request accepted"
          );
          await testAvailable();
    
          browser.management.onInstalled.addListener(() => {
            browser.test.fail("onInstalled listener invoked");
          });
    
          browser.test.log("test without permission");
          // remove permission
          await browser.permissions.remove(permObj);
          hasPerm = await browser.permissions.contains(permObj);
          browser.test.assertFalse(
            hasPerm,
            "management permission.request removed"
          );
          await testAvailable();
    
          browser.test.sendMessage("done");
        });
    
        browser.test.sendMessage("started");
      
  },
  async function part_2() {
        browser.test.onMessage.addListener(async (msg, id) => {
          let addon = await browser.management.get(id);
          browser.test.sendMessage("addon", addon);
        });
    
        let addons = await browser.management.getAll();
        browser.test.assertEq(
          2,
          addons.length,
          "management.getAll returned correct number of add-ons."
        );
        browser.test.sendMessage("addons", addons);
      
  },
  async function part_3() {
            browser.management.onInstalled.addListener(details => {
              browser.test.sendMessage("onInstalled", details);
            });
            browser.management.onUninstalled.addListener(details => {
              browser.test.sendMessage("onUninstalled", details);
            });
            browser.management.onEnabled.addListener(() => {
              browser.test.sendMessage("onEnabled");
            });
            browser.management.onDisabled.addListener(() => {
              browser.test.sendMessage("onDisabled");
            });
          
  },
]);
