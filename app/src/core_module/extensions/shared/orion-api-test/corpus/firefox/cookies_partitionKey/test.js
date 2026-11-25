"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      let portsByDomain = new Map();
    
      async function getDocumentCookies(port) {
        return new Promise(resolve => {
          port.onMessage.addListener(function listener(cookieString) {
            port.onMessage.removeListener(listener);
            resolve(cookieString);
          });
          port.postMessage("get_cookies");
        });
      }
    
      // Stringify cookie identifier for comparisons in assertions.
      function stringifyCookie(cookie) {
        if (!cookie) {
          return "COOKIE MISSING";
        }
        let domain = cookie.domain;
        if (!domain) {
          // The return value of `cookies.remove` has a URL instead of a domain.
          domain = new URL(cookie.url).hostname;
        }
        return `${cookie.name} domain=${domain} firstPartyDomain=${
          cookie.firstPartyDomain
        } partitionKey=${JSON.stringify(cookie.partitionKey)}`;
      }
      function stringifyCookies(cookies) {
        return cookies.map(stringifyCookie).sort().join(" , ");
      }
    
      // detailsIn may have partitionKey and firstPartyDomain attributes.
      // expectedOut has partitionKey and firstPartyDomain attributes.
      async function runTestCase({ domain, detailsIn, expectedOut }) {
        const port = portsByDomain.get(domain);
        browser.test.assertTrue(port, `Got port to document for ${domain}`);
    
        let allCookies = await browser.cookies.getAll({
          domain,
          firstPartyDomain: null,
          partitionKey: {},
        });
    
        let allCookiesWithFPD = await browser.cookies.getAll({
          domain,
          ...detailsIn,
        });
        browser.test.assertEq(
          stringifyCookies(allCookies),
          stringifyCookies(allCookiesWithFPD),
          "cookies.getAll returns consistent results"
        );
    
        for (let [key, expectedValue] of Object.entries(expectedOut)) {
          expectedValue = JSON.stringify(expectedValue);
          browser.test.assertTrue(
            allCookies.every(c => JSON.stringify(c[key]) === expectedValue),
            `All ${allCookies.length} cookies have ${key}=${expectedValue}`
          );
        }
    
        // delete_me: get, remove, get.
        const cookieToDelete = {
          url: `http://${domain}/`,
          name: "delete_me",
          ...detailsIn,
        };
        const deletedCookie = {
          ...cookieToDelete,
          ...expectedOut,
        };
        browser.test.assertEq(
          stringifyCookie(deletedCookie),
          stringifyCookie(await browser.cookies.get(cookieToDelete)),
          "delete_me cookie exists before removal"
        );
        browser.test.assertEq(
          stringifyCookie(deletedCookie),
          stringifyCookie(await browser.cookies.remove(cookieToDelete)),
          "delete_me cookie has been removed by cookies.remove"
        );
        browser.test.assertEq(
          null,
          await browser.cookies.get(cookieToDelete),
          "delete_me cookie does not exist any more"
        );
    
        // edit_me: set, retrieve via document.cookie
        const cookieToEdit = {
          url: `http://${domain}/`,
          name: "edit_me",
          ...detailsIn,
        };
        const editedCookie = await browser.cookies.set({
          ...cookieToEdit,
          value: `new_value_${domain}`,
        });
        browser.test.assertEq(
          stringifyCookie({ ...cookieToEdit, ...expectedOut }),
          stringifyCookie(editedCookie),
          "edit_me cookie updated"
        );
        browser.test.assertEq(
          await getDocumentCookies(port),
          `edit_me=new_value_${domain}`,
          "Expected cookies after removing and editing a cookie"
        );
    
        // edit_me: get, remove, getAll.
        browser.test.assertEq(
          stringifyCookie(editedCookie),
          stringifyCookie(await browser.cookies.get(cookieToEdit)),
          "edit_me cookie still exists"
        );
        await browser.cookies.remove(cookieToEdit);
        let allCookiesAtEnd = await browser.cookies.getAll({
          domain,
          firstPartyDomain: null,
          partitionKey: {},
        });
        browser.test.assertEq(
          "[]",
          JSON.stringify(allCookiesAtEnd),
          "No cookies left"
        );
      }
    
      let resolveTestReady;
      let testReadyPromise = new Promise(resolve => {
        resolveTestReady = resolve;
      });
    
      browser.test.onMessage.addListener(async (msg, testCase) => {
        await testReadyPromise;
        browser.test.assertEq("runTest", msg, `Starting: ${testCase.description}`);
        try {
          await runTestCase(testCase);
        } catch (e) {
          browser.test.fail(`Unexpected error: ${e} :: ${e.stack}`);
        }
        browser.test.sendMessage("runTest_done");
      });
    
      // cookie-checker-contentscript.js will connect.
      browser.runtime.onConnect.addListener(port => {
        portsByDomain.set(port.name, port);
        browser.test.log(`Got port #${portsByDomain.size} ${port.name}`);
        if (portsByDomain.size === 2) {
          // The top document and the embedded frame has loaded and the
          // content script that we use to read cookies is connected.
          // The test can now start.
          resolveTestReady();
        }
      });
  },
  async function part_2() {
          const url = "http://example.com/";
          const name = "dfpi_invalid_partitionKey_dummy_name";
          const value = "1";
    
          // Shorthands to minimize boilerplate.
          const set = d => browser.cookies.set({ url, name, value, ...d });
          const remove = d => browser.cookies.remove({ url, name, ...d });
          const get = d => browser.cookies.get({ url, name, ...d });
          const getAll = d => browser.cookies.getAll(d);
    
          await browser.test.assertRejects(
            set({ partitionKey: { topLevelSite: "example.net" } }),
            /Invalid value for 'partitionKey' attribute/,
            "partitionKey must be a URL, not a domain"
          );
          await browser.test.assertRejects(
            set({ partitionKey: { topLevelSite: "chrome://foo" } }),
            /Invalid value for 'partitionKey' attribute/,
            "partitionKey cannot be the chrome:-scheme (canonicalization fails)"
          );
          await browser.test.assertRejects(
            set({ partitionKey: { topLevelSite: "chrome://foo/foo/foo" } }),
            /Invalid value for 'partitionKey' attribute/,
            "partitionKey cannot be the chrome:-scheme (canonicalization passes)"
          );
          await browser.test.assertRejects(
            set({ partitionKey: { topLevelSite: "http://[]:" } }),
            /Invalid value for 'partitionKey' attribute/,
            "partitionKey must be a valid URL"
          );
    
          browser.test.assertThrows(
            () => get({ partitionKey: "" }),
            /Error processing partitionKey: Expected object instead of ""/,
            "cookies.get should reject invalid partitionKey (string)"
          );
          browser.test.assertThrows(
            () => get({ partitionKey: { topLevelSite: "http://x", badkey: 0 } }),
            /Error processing partitionKey: Unexpected property "badkey"/,
            "cookies.get should reject unsupported keys in partitionKey"
          );
          await browser.test.assertRejects(
            remove({ partitionKey: { topLevelSite: "invalid" } }),
            /Invalid value for 'partitionKey' attribute/,
            "cookies.remove should reject invalid partitionKey.topLevelSite"
          );
          await browser.test.assertRejects(
            get({ partitionKey: { topLevelSite: "invalid" } }),
            /Invalid value for 'partitionKey' attribute/,
            "cookies.get should reject invalid partitionKey.topLevelSite"
          );
          await browser.test.assertRejects(
            getAll({ partitionKey: { topLevelSite: "invalid" } }),
            /Invalid value for 'partitionKey' attribute/,
            "cookies.getAll should reject invalid partitionKey.topLevelSite"
          );
    
          // firstPartyDomain and partitionKey are mutually exclusive, because
          // FPI and dFPI are mutually exclusive.
          await browser.test.assertRejects(
            set({ firstPartyDomain: "example.net", partitionKey: {} }),
            /Partitioned cookies cannot have a 'firstPartyDomain' attribute./,
            "partitionKey and firstPartyDomain cannot both be non-empty"
          );
    
          // On Nightly, dFPI is enabled by default. We have to disable it first,
          // before we can enable FPI. Otherwise we would get error:
          // Can't enable firstPartyIsolate when cookieBehavior is 'reject_trackers_and_partition_foreign'
          await browser.privacy.websites.cookieConfig.set({
            value: { behavior: "reject_trackers" },
          });
          await browser.privacy.websites.firstPartyIsolate.set({
            value: true,
          });
    
          // FPI and dFPI are mutually exclusive. FPI is documented to require the
          // firstPartyDomain attribute, let's verify that, despite it being
          // technically possible to support both attributes.
          for (let cookiesMethod of [get, getAll, remove, set]) {
            await browser.test.assertRejects(
              cookiesMethod({ partitionKey: { topLevelSite: url } }),
              /First-Party Isolation is enabled, but the required 'firstPartyDomain' attribute was not set./,
              `cookies.${cookiesMethod.name} requires firstPartyDomain when FPI is enabled`
            );
          }
    
          // The pref changes above (to dFPI/FPI) via the browser.privacy API  will
          // be undone when the extension unloads.
    
          browser.test.sendMessage("test_done");
        
  },
  async function part_3() {
          let cookie = await browser.cookies.set({
            url: "http://example.com/",
            name: "moz_ext_party",
            value: "1",
            // moz-extension: URL is passed here, in an attempt to mark the cookie
            // as part of the "moz-extension:"-partition. Below we will expect ""
            // because the dFPI implementation treats "moz-extension" as
            // unpartitioned, see
            // https://searchfox.org/mozilla-central/rev/ac7da6c7306d86e2f86a302ce1e170ad54b3c1fe/caps/OriginAttributes.cpp#79-82
            partitionKey: { topLevelSite: browser.runtime.getURL("/") },
          });
          browser.test.assertEq(
            null,
            cookie.partitionKey,
            "Cookies in moz-extension:-URL are unpartitioned"
          );
          let deletedCookie = await browser.cookies.remove({
            url: "http://example.com/",
            name: "moz_ext_party",
            partitionKey: { topLevelSite: "moz-extension://ignoreme" },
          });
          browser.test.assertEq(
            null,
            deletedCookie.partitionKey,
            "moz-extension:-partition key is treated as unpartitioned"
          );
          browser.test.sendMessage("test_done");
        
  },
  async function part_4() {
          // Retrieve all cookies, partitioned and unpartitioned. We expect only
          // unpartitioned cookies at first because the top frame and the child
          // frame have the same origin.
          let initialCookies = await browser.cookies.getAll({ partitionKey: {} });
          browser.test.assertEq(
            "delete_me=frame,edit_me=frame",
            initialCookies.map(c => `${c.name}=${c.value}`).join(),
            "Same-site frames are in unpartitioned storage; /frame overwrites /top"
          );
          browser.test.assertTrue(
            await browser.cookies.remove({
              url: "https://third.example.net/",
              name: "delete_me",
            }),
            "Removed unpartitioned cookie"
          );
          browser.test.assertEq(
            "[null,null]",
            JSON.stringify(initialCookies.map(c => c.partitionKey)),
            "Cookies in same-site/same-origin frames are not partitioned"
          );
    
          // We only have one unpartitioned cookie (edit_cookie) left.
    
          // Add new cookie whose partitionKey is same-site relative to url.
          let newCookie = await browser.cookies.set({
            url: "http://third.example.net/",
            name: "edit_me",
            value: "url_is_partitionKey_eTLD+2",
            partitionKey: { topLevelSite: "http://third.example.net" },
          });
          browser.test.assertEq(
            "http://example.net",
            newCookie.partitionKey.topLevelSite,
            "Created cookie with partitionKey=url; eTLD+2 is normalized as eTLD+1"
          );
    
          browser.test.assertTrue(
            await browser.cookies.remove({
              url: "http://third.example.net/",
              name: "edit_me",
              partitionKey: {},
            }),
            "Removed unpartitioned cookie when partitionKey: {} is used"
          );
    
          browser.test.assertEq(
            null,
            await browser.cookies.remove({
              url: "http://third.example.net/",
              name: "edit_me",
              partitionKey: {},
            }),
            "No more unpartitioned cookies to remove"
          );
    
          browser.test.assertTrue(
            await browser.cookies.remove({
              url: "http://third.example.net/",
              name: "edit_me",
              partitionKey: { topLevelSite: "http://example.net" },
            }),
            "Removed partitioned cookie when partitionKey is passed"
          );
    
          browser.test.sendMessage("test_done");
        
  },
  async function part_5() {
          const url = "http://third.example.net";
          const name = "test_url_is_identical_to_partitionKey";
          const partitionKey = { topLevelSite: "http://example.com" };
          const firstPartyDomain = "example.net";
    
          // Create non-partitioned cookie, create partitioned cookie.
          await browser.cookies.set({ url, name, value: "no_partition" });
          await browser.cookies.set({ url, name, value: "fpd", firstPartyDomain });
          await browser.cookies.set({ url, name, partitionKey, value: "party" });
          // partitionKey + firstPartyDomain was tested in dfpi_invalid_partitionKey
    
          async function getAllValues(details) {
            let cookies = await browser.cookies.getAll(details);
            let values = cookies.map(c => c.value);
            return values.sort().join(); // Serialize for use with assertEq.
          }
    
          browser.test.assertEq(
            "no_partition",
            await getAllValues({}),
            "getAll() returns unpartitioned by default"
          );
    
          browser.test.assertEq(
            "no_partition,party",
            await getAllValues({ partitionKey: {} }),
            "getAll() with partitionKey: {} returns all cookies"
          );
    
          browser.test.assertEq(
            "party",
            await getAllValues({ partitionKey }),
            "getAll() with specific partitionKey returns partitionKey cookies only"
          );
    
          browser.test.assertEq(
            "",
            await getAllValues({ partitionKey: { topLevelSite: url } }),
            "getAll() with partitionKey set to cookie URL does not match anything"
          );
    
          browser.test.assertEq(
            "",
            await getAllValues({ partitionKey, firstPartyDomain }),
            "getAll() with non-empty partitionKey and firstPartyDomain does not match anything"
          );
          browser.test.assertEq(
            "fpd",
            await getAllValues({ partitionKey: {}, firstPartyDomain }),
            "getAll() with empty partitionKey and firstPartyDomain matches fpd"
          );
    
          browser.test.assertEq(
            "fpd,no_partition,party",
            await getAllValues({ partitionKey: {}, firstPartyDomain: null }),
            "getAll() with empty partitionKey and firstPartyDomain:null matches everything"
          );
    
          await browser.cookies.remove({ url, name });
          await browser.cookies.remove({ url, name, firstPartyDomain });
          await browser.cookies.remove({ url, name, partitionKey });
    
          browser.test.sendMessage("test_done");
        
  },
  async function part_6() {
          async function getAllValues(details) {
            let cookies = await browser.cookies.getAll(details);
            let values = cookies.map(c => c.value);
            return values.sort().join(); // Serialize for use with assertEq.
          }
          const urls = ["http://first.example.com", "http://third.example.net"];
          const name = "test_getAll_partitionKey_hasCrossSiteAncestor";
          const partitionKeyUnspecified = { topLevelSite: "http://example.com" };
          const partitionKeySameSite = {
            topLevelSite: "http://example.com",
            hasCrossSiteAncestor: false,
          };
          const partitionKeyForeign = {
            topLevelSite: "http://example.com",
            hasCrossSiteAncestor: true,
          };
          try {
            for (let url of urls) {
              await browser.cookies.set({ url, name, value: "no_partition" });
              if (url == "http://first.example.com") {
                await browser.cookies.set({
                  url,
                  name,
                  value: "partition1",
                  partitionKey: partitionKeySameSite,
                });
              } else {
                await browser.test.assertRejects(
                  browser.cookies.set({
                    url,
                    name,
                    value: "partition1",
                    partitionKey: partitionKeySameSite,
                  }),
                  /Invalid value for 'partitionKey' attribute/,
                  // When topLevelSite and url are cross-site, hasCrossSiteAncestor cannot be false
                  "cookies.get should reject invalid partitionKey.topLevelSite"
                );
              }
              await browser.cookies.set({
                url,
                name,
                value: "partition3",
                partitionKey: partitionKeyForeign,
              });
              let cookie = await browser.cookies.set({
                url,
                name,
                value: "partitionX",
                partitionKey: partitionKeyUnspecified,
              });
    
              if (url == "http://first.example.com") {
                browser.test.assertDeepEq(
                  partitionKeySameSite,
                  cookie.partitionKey,
                  "partitionKey.hasCrossSiteAncestor computed as false"
                );
                browser.test.assertEq(
                  "no_partition,partition3,partitionX",
                  await getAllValues({ partitionKey: {} }),
                  "getAll() with empty partitionKey gets all cookies"
                );
                browser.test.assertEq(
                  "partition3,partitionX",
                  await getAllValues({ partitionKey: partitionKeyUnspecified }),
                  "getAll() with partitionKey with undefined hasCrossSiteAncestor gets all partitioned cookies"
                );
                browser.test.assertEq(
                  "partitionX",
                  await getAllValues({ partitionKey: partitionKeySameSite }),
                  "getAll() with partitionKey with false hasCrossSiteAncestor gets cookie stored with undefined hasCrossSiteAncestor"
                );
                browser.test.assertEq(
                  "partition3",
                  await getAllValues({ partitionKey: partitionKeyForeign }),
                  "getAll() with partitionKey with true hasCrossSiteAncestor gets only cookie stored with true hasCrossSiteAncestor"
                );
              } else {
                browser.test.assertDeepEq(
                  partitionKeyForeign,
                  cookie.partitionKey,
                  "partitionKey.hasCrossSiteAncestor computed as true"
                );
                browser.test.assertEq(
                  "no_partition,partitionX",
                  await getAllValues({ partitionKey: {} }),
                  "getAll() with empty partitionKey gets all cookies"
                );
                browser.test.assertEq(
                  "partitionX",
                  await getAllValues({ partitionKey: partitionKeyUnspecified }),
                  "getAll() with partitionKey with undefined hasCrossSiteAncestor gets all partitioned cookies"
                );
                browser.test.assertEq(
                  "",
                  await getAllValues({ partitionKey: partitionKeySameSite }),
                  "getAll() with partitionKey with false hasCrossSiteAncestor gets no cookies"
                );
                browser.test.assertEq(
                  "partitionX",
                  await getAllValues({ partitionKey: partitionKeyForeign }),
                  "getAll() with partitionKey with true hasCrossSiteAncestor gets cookie stored with undefined hasCrossSiteAncestor"
                );
              }
    
              let removedCookie = await browser.cookies.remove({ url, name });
              browser.test.assertTrue(
                removedCookie,
                "remove() without partitionKey removes unpartitioned cookie"
              );
              removedCookie = await browser.cookies.remove({ url, name });
              browser.test.assertDeepEq(
                null,
                removedCookie,
                "remove() without partitionKey does not remove partitioned cookies"
              );
              removedCookie = await browser.cookies.remove({
                url,
                name,
                partitionKey: partitionKeyUnspecified,
              });
              browser.test.assertTrue(
                removedCookie,
                "remove() with partitionKey and unspecified ancestry bit removes the cookie set with that parameter"
              );
              if (url == "http://first.example.com") {
                browser.test.assertDeepEq(
                  partitionKeySameSite,
                  removedCookie?.partitionKey,
                  "remove() with partitionKey and unspecified ancestry bit removes the correct partitioned cookie"
                );
                removedCookie = await browser.cookies.remove({
                  url,
                  name,
                  partitionKey: partitionKeySameSite,
                });
                browser.test.assertDeepEq(
                  null,
                  removedCookie,
                  "remove() with same site partition key is the same as the unspecified ancentry for this url"
                );
                removedCookie = await browser.cookies.remove({
                  url,
                  name,
                  partitionKey: partitionKeyForeign,
                });
                browser.test.assertTrue(
                  removedCookie,
                  "remove() with partitionKey and foreign ancestry bit removes the right cookie"
                );
              } else {
                browser.test.assertDeepEq(
                  partitionKeyForeign,
                  removedCookie?.partitionKey,
                  "remove() with partitionKey and unspecified ancestry bit removes the correct partitioned cookie"
                );
                await browser.test.assertRejects(
                  browser.cookies.remove({
                    url,
                    name,
                    partitionKey: partitionKeySameSite,
                  }),
                  /Invalid value for 'partitionKey' attribute/,
                  // When topLevelSite and url are cross-site, hasCrossSiteAncestor cannot be false
                  "cookies.get should reject invalid partitionKey.topLevelSite"
                );
                removedCookie = await browser.cookies.remove({
                  url,
                  name,
                  partitionKey: partitionKeyForeign,
                });
                browser.test.assertDeepEq(
                  null,
                  removedCookie,
                  "remove() with foreign partition key is the same as the unspecified ancentry for this url"
                );
              }
            }
          } catch (e) {
            browser.test.fail("Unexpected error: " + e);
          }
    
          browser.test.sendMessage("test_done");
        
  },
]);
