"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        try {
          const BASE_URL = `http://example.com/dummy`;
    
          const cache = await window.caches.open("test-cache-api");
          browser.test.assertTrue(
            await window.caches.has("test-cache-api"),
            "CacheStorage.has should resolve to true"
          );
    
          // Test that adding and requesting cached http urls
          // works as well.
          await cache.add(BASE_URL);
          browser.test.assertEq(
            "test_ext_cache_api.js",
            await cache.match(BASE_URL).then(res => res?.text()),
            "Got the expected content from the cached http url"
          );
    
          // Test that the http urls that the cache API is allowed
          // to fetch and cache are limited by the host permissions
          // associated to the extensions (same as when the extension
          // for fetch from those urls using fetch or XHR).
          await browser.test.assertRejects(
            cache.add(`http://anotherdomain.com/dummy`),
            "NetworkError when attempting to fetch resource.",
            "Got the expected rejection of requesting an http not allowed by host permissions"
          );
    
          // Test that deleting the cache storage works as expected.
          browser.test.assertTrue(
            await window.caches.delete("test-cache-api"),
            "Cache deleted successfully"
          );
          browser.test.assertTrue(
            !(await window.caches.has("test-cache-api")),
            "CacheStorage.has should resolve to false"
          );
        } catch (err) {
          browser.test.fail(`Unexpected error on using Cache API: ${err}`);
          throw err;
        } finally {
          browser.test.sendMessage("test-cache-api-allowed");
        }
      
  },
  async function part_2() {
        try {
          const BASE_URL = `http://example.com/dummy`;
    
          const cache = await window.caches.open("test-cache-api");
          browser.test.assertTrue(
            await window.caches.has("test-cache-api"),
            "CacheStorage.has should resolve to true"
          );
    
          // Test that adding and requesting cached http urls
          // works as well.
          await cache.add(BASE_URL);
          browser.test.assertEq(
            "test_ext_cache_api.js",
            await cache.match(BASE_URL).then(res => res?.text()),
            "Got the expected content from the cached http url"
          );
        } catch (err) {
          browser.test.fail(`Unexpected error on using Cache API: ${err}`);
          throw err;
        } finally {
          browser.test.sendMessage("cache-storage-created");
        }
      
  },
]);
