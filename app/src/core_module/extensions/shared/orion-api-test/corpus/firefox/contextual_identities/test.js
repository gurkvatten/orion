"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.assertTrue(
          !browser.contextualIdentities,
          "contextualIdentities API is not available when the contextualIdentities permission is not required"
        );
        browser.test.notifyPass("contextualIdentities_without_permission");
      
  },
  async function part_2() {
        function createOneTimeListener(type) {
          return new Promise((resolve, reject) => {
            try {
              browser.test.assertTrue(
                type in browser.contextualIdentities,
                `Found API object browser.contextualIdentities.${type}`
              );
              const listener = change => {
                browser.test.assertTrue(
                  "contextualIdentity" in change,
                  `Found identity in change`
                );
                browser.contextualIdentities[type].removeListener(listener);
                resolve(change);
              };
              browser.contextualIdentities[type].addListener(listener);
            } catch (e) {
              reject(e);
            }
          });
        }
    
        function assertExpected(expected, container) {
          // Number of keys that are added by the APIs
          const createdCount = 2;
          for (let key of Object.keys(container)) {
            browser.test.assertTrue(key in expected, `found property ${key}`);
            browser.test.assertEq(
              expected[key],
              container[key],
              `property value for ${key} is correct`
            );
          }
          const hexMatch = /^#[0-9a-f]{6}$/;
          browser.test.assertTrue(
            hexMatch.test(expected.colorCode),
            "Color code property was expected Hex shape"
          );
          const iconMatch = /^resource:\/\/usercontext-content\/[a-z]+[.]svg$/;
          browser.test.assertTrue(
            iconMatch.test(expected.iconUrl),
            "Icon url property was expected shape"
          );
          browser.test.assertEq(
            Object.keys(expected).length,
            Object.keys(container).length + createdCount,
            "all expected properties found"
          );
        }
    
        let onCreatePromise = createOneTimeListener("onCreated");
    
        let containerObj = { name: "foobar", color: "red", icon: "circle" };
        let ci = await browser.contextualIdentities.create(containerObj);
        browser.test.assertTrue(!!ci, "We have an identity");
        const onCreateListenerResponse = await onCreatePromise;
        const cookieStoreId = ci.cookieStoreId;
        assertExpected(
          onCreateListenerResponse.contextualIdentity,
          Object.assign(containerObj, { cookieStoreId })
        );
    
        let onUpdatedPromise = createOneTimeListener("onUpdated");
        let updateContainerObj = { name: "testing", color: "blue", icon: "dollar" };
        ci = await browser.contextualIdentities.update(
          cookieStoreId,
          updateContainerObj
        );
        browser.test.assertTrue(!!ci, "We have an update identity");
        const onUpdatedListenerResponse = await onUpdatedPromise;
        assertExpected(
          onUpdatedListenerResponse.contextualIdentity,
          Object.assign(updateContainerObj, { cookieStoreId })
        );
    
        let onRemovePromise = createOneTimeListener("onRemoved");
        ci = await browser.contextualIdentities.remove(
          updateContainerObj.cookieStoreId
        );
        browser.test.assertTrue(!!ci, "We have an remove identity");
        const onRemoveListenerResponse = await onRemovePromise;
        assertExpected(
          onRemoveListenerResponse.contextualIdentity,
          Object.assign(updateContainerObj, { cookieStoreId })
        );
    
        browser.test.notifyPass("contextualIdentities_events");
      
  },
  async function part_3() {
        let ci;
        await browser.test.assertRejects(
          browser.contextualIdentities.get("foobar"),
          "Invalid contextual identity: foobar",
          "API should reject here"
        );
        await browser.test.assertRejects(
          browser.contextualIdentities.update("foobar", { name: "testing" }),
          "Invalid contextual identity: foobar",
          "API should reject for unknown updates"
        );
        await browser.test.assertRejects(
          browser.contextualIdentities.remove("foobar"),
          "Invalid contextual identity: foobar",
          "API should reject for removing unknown containers"
        );
    
        ci = await browser.contextualIdentities.get("firefox-container-1");
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertTrue("name" in ci, "We have an identity.name");
        browser.test.assertTrue("color" in ci, "We have an identity.color");
        browser.test.assertTrue("icon" in ci, "We have an identity.icon");
        browser.test.assertEq("Personal", ci.name, "identity.name is correct");
        browser.test.assertEq(
          "firefox-container-1",
          ci.cookieStoreId,
          "identity.cookieStoreId is correct"
        );
    
        function listenForMessage(messageName, stateChangeBool) {
          return new Promise(resolve => {
            browser.test.onMessage.addListener(function listener(msg) {
              browser.test.log(`Got message from background: ${msg}`);
              if (msg === messageName + "-response") {
                browser.test.onMessage.removeListener(listener);
                resolve();
              }
            });
            browser.test.log(
              `Sending message to background: ${messageName} ${stateChangeBool}`
            );
            browser.test.sendMessage(messageName, stateChangeBool);
          });
        }
    
        await listenForMessage("containers-state-change", false);
    
        browser.test.assertRejects(
          browser.contextualIdentities.query({}),
          "Contextual identities are currently disabled",
          "Throws when containers are disabled"
        );
    
        await listenForMessage("containers-state-change", true);
    
        let cis = await browser.contextualIdentities.query({});
        browser.test.assertEq(
          4,
          cis.length,
          "by default we should have 4 containers"
        );
    
        cis = await browser.contextualIdentities.query({ name: "Personal" });
        browser.test.assertEq(
          1,
          cis.length,
          "by default we should have 1 container called Personal"
        );
    
        cis = await browser.contextualIdentities.query({ name: "foobar" });
        browser.test.assertEq(
          0,
          cis.length,
          "by default we should have 0 container called foobar"
        );
    
        ci = await browser.contextualIdentities.create({
          name: "foobar",
          color: "red",
          icon: "gift",
        });
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertEq("foobar", ci.name, "identity.name is correct");
        browser.test.assertEq("red", ci.color, "identity.color is correct");
        browser.test.assertEq("gift", ci.icon, "identity.icon is correct");
        browser.test.assertTrue(
          !!ci.cookieStoreId,
          "identity.cookieStoreId is correct"
        );
    
        browser.test.assertRejects(
          browser.contextualIdentities.create({
            name: "foobar",
            color: "red",
            icon: "firefox",
          }),
          "Invalid icon firefox for container",
          "Create container called with an invalid icon"
        );
    
        browser.test.assertRejects(
          browser.contextualIdentities.create({
            name: "foobar",
            color: "firefox-orange",
            icon: "gift",
          }),
          "Invalid color name firefox-orange for container",
          "Create container called with an invalid color"
        );
    
        cis = await browser.contextualIdentities.query({});
        browser.test.assertEq(
          5,
          cis.length,
          "we should still have have 5 containers"
        );
    
        ci = await browser.contextualIdentities.get(ci.cookieStoreId);
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertEq("foobar", ci.name, "identity.name is correct");
        browser.test.assertEq("red", ci.color, "identity.color is correct");
        browser.test.assertEq("gift", ci.icon, "identity.icon is correct");
    
        browser.test.assertRejects(
          browser.contextualIdentities.update(ci.cookieStoreId, {
            name: "foobar",
            color: "red",
            icon: "firefox",
          }),
          "Invalid icon firefox for container",
          "Create container called with an invalid icon"
        );
    
        browser.test.assertRejects(
          browser.contextualIdentities.update(ci.cookieStoreId, {
            name: "foobar",
            color: "firefox-orange",
            icon: "gift",
          }),
          "Invalid color name firefox-orange for container",
          "Create container called with an invalid color"
        );
    
        cis = await browser.contextualIdentities.query({});
        browser.test.assertEq(5, cis.length, "now we have 5 identities");
    
        ci = await browser.contextualIdentities.update(ci.cookieStoreId, {
          name: "barfoo",
          color: "blue",
          icon: "cart",
        });
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
        browser.test.assertEq("blue", ci.color, "identity.color is correct");
        browser.test.assertEq("cart", ci.icon, "identity.icon is correct");
    
        ci = await browser.contextualIdentities.get(ci.cookieStoreId);
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
        browser.test.assertEq("blue", ci.color, "identity.color is correct");
        browser.test.assertEq("cart", ci.icon, "identity.icon is correct");
    
        ci = await browser.contextualIdentities.remove(ci.cookieStoreId);
        browser.test.assertTrue(!!ci, "We have an identity");
        browser.test.assertEq("barfoo", ci.name, "identity.name is correct");
        browser.test.assertEq("blue", ci.color, "identity.color is correct");
        browser.test.assertEq("cart", ci.icon, "identity.icon is correct");
    
        cis = await browser.contextualIdentities.query({});
        browser.test.assertEq(4, cis.length, "we are back to 4 identities");
    
        browser.test.notifyPass("contextualIdentities");
      
  },
  async function part_4() {
        let ci = await browser.contextualIdentities.get("firefox-container-1");
        browser.test.assertTrue(!!ci, "We have an identity");
    
        browser.test.notifyPass("contextualIdentities");
      
  },
  async function part_5() {
        let extensionInfo = await browser.management.getSelf();
        if (extensionInfo.version == "1.0.0") {
          const containers = await browser.contextualIdentities.query({});
          browser.test.assertEq(
            containers.length,
            4,
            "We still have the original containers"
          );
          await browser.contextualIdentities.create({
            name: "foobar",
            color: "red",
            icon: "circle",
          });
        }
        const containers = await browser.contextualIdentities.query({});
        browser.test.assertEq(containers.length, 5, "We have a new container");
        if (extensionInfo.version == "1.1.0") {
          await browser.contextualIdentities.remove(containers[4].cookieStoreId);
        }
        browser.test.notifyPass("contextualIdentities");
      
  },
  async function part_6() {
            browser.contextualIdentities.onCreated.addListener(() => {
              browser.test.sendMessage("created");
            });
            browser.contextualIdentities.onUpdated.addListener(() => {});
            browser.contextualIdentities.onRemoved.addListener(() => {
              browser.test.sendMessage("removed");
            });
            browser.test.sendMessage("ready");
          
  },
]);
