"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.onMessage.addListener(async msg => {
            if (msg === "check_permissions") {
              browser.test.assertEq(
                await browser.permissions.contains({ permissions: ["trialML"] }),
                false,
                "Optional-only permission not granted when in permissions[]."
              );
              browser.test.assertTrue(
                browser.webNavigation,
                "Other permission unaffected by refusal of optional-only permission"
              );
              browser.test.sendMessage("check_permissions:done");
              return;
            }
            if (msg === "request") {
              await browser.test.assertRejects(
                browser.permissions.request({ permissions: ["trialML"] }),
                "Cannot request permission trialML since it was not declared in optional_permissions",
                "Cannot request permission not listed in optional_permissions"
              );
              browser.test.sendMessage("request:done");
              return;
            }
            browser.test.fail(`Unexpected message: ${msg}`);
          });
        
  },
  async function part_2() {
          browser.runtime.onInstalled.addListener(async () => {
            browser.test.assertEq(
              await browser.permissions.contains({ permissions: ["trialML"] }),
              false,
              "Optional-only permission not granted at first."
            );
            browser.test.sendMessage("installed");
          });
          browser.test.onMessage.addListener(async msg => {
            browser.test.assertEq("request", msg);
            let granted = await browser.permissions.request({
              permissions: ["trialML"],
            });
            browser.test.sendMessage("request:result", granted);
          });
        
  },
  async function part_3() {
            const ERROR_ONLY_ONE_PERM_ALLOWED =
              "Cannot request permission trialML with another permission";
    
            async function testPermissionsRequest(permissions) {
              return new Promise(resolve => {
                browser.test.withHandlingUserInput(() =>
                  resolve(browser.permissions.request(permissions))
                );
              });
            }
    
            await browser.test.assertRejects(
              testPermissionsRequest({ permissions: ["webNavigation", "trialML"] }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject regular permission + optional-only permission"
            );
    
            await browser.test.assertRejects(
              testPermissionsRequest({ permissions: ["trialML", "cookies"] }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject optional-only permission + permission without warning"
            );
    
            await browser.test.assertRejects(
              testPermissionsRequest({ permissions: ["trialML", "webNavigation"] }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject optional-only permissions + regular permission"
            );
    
            await browser.test.assertRejects(
              testPermissionsRequest({ permissions: ["trialML", "userScripts"] }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject optional-only permissions (trialML, userScripts)"
            );
    
            // The UI logic (isUserScriptsRequest in ExtensionsUI.sys.mjs) assumes
            // that "userScripts" is the only permission if present in a permission
            // request. That is the case because "userScripts" is an
            // OptionalOnlyPermission, which the following check confirms:
            await browser.test.assertRejects(
              testPermissionsRequest({ permissions: ["userScripts", "trialML"] }),
              // Note: this is comparable to the ["trialML", "userScripts"] test
              // above, but the error message differs because the message includes
              // the first optional-only permission, "userScripts" in this case.
              "Cannot request permission userScripts with another permission",
              "Should reject optional-only permissions (userScripts, trialML)"
            );
    
            await browser.test.assertRejects(
              testPermissionsRequest({
                permissions: ["trialML"],
                origins: ["https://example.com/*"],
              }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject optional-only permission + host permission"
            );
    
            await browser.test.assertRejects(
              // In theory we could detect that the same permission is listed twice
              // and recognize that it will only generate one permission warning.
              // Our implementation does not deduplicate, and there is no reason
              // for supporting it, so we just reject it.
              testPermissionsRequest({
                permissions: ["trialML", "trialML"],
                origins: ["https://example.com/*"],
              }),
              ERROR_ONLY_ONE_PERM_ALLOWED,
              "Should reject optional-only permission if it is listed twice"
            );
    
            browser.test.sendMessage("done");
          
  },
  async function part_4() {
            async function testPermissionsRequest(permissions) {
              return new Promise(resolve => {
                browser.test.withHandlingUserInput(() =>
                  resolve(browser.permissions.request(permissions))
                );
              });
            }
    
            await browser.test.assertRejects(
              testPermissionsRequest({
                permissions: ["trialML"],
                data_collection: ["technicalAndInteraction"],
              }),
              "Cannot request permission trialML with another permission",
              "Should reject data collection permission + optional-only permission"
            );
    
            await browser.test.assertRejects(
              testPermissionsRequest({
                permissions: ["userScripts"],
                data_collection: ["technicalAndInteraction"],
              }),
              "Cannot request permission userScripts with another permission",
              "Should reject data collection permission + optional-only permission"
            );
    
            browser.test.sendMessage("done");
          
  },
]);
