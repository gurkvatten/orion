"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const data = await browser.storage.local.get();
          browser.test.assertEq(
            Object.keys(data).length,
            0,
            "Expect the storage.local store to be empty"
          );
          browser.test.sendMessage("test-stored-data:done");
        
  },
  async function part_2() {
          const storedData = await browser.storage.local.get();
          browser.test.assertEq(
            "test_value",
            storedData.test_key_string,
            "Got the expected data after the storage.local data migration"
          );
          browser.test.sendMessage("storage-local-data-migrated");
        
  },
  async function part_3() {
        const storedData = await browser.storage.local.get();
    
        browser.test.assertEq(
          "test_value1",
          storedData.test_key_string,
          "Got the expected data after the storage.local data migration"
        );
        browser.test.assertEq(
          1000,
          storedData.test_key_number,
          "Got the expected data after the storage.local data migration"
        );
        browser.test.assertEq(
          true,
          storedData.test_nested_data.nested_key,
          "Got the expected data after the storage.local data migration"
        );
    
        browser.test.sendMessage("storage-local-data-migrated");
      
  },
  async function part_4() {
        const storedData = await browser.storage.local.get("test_key_string");
    
        browser.test.assertEq(
          "test_value",
          storedData.test_key_string,
          "Got the expected data after the storage.local data migration"
        );
    
        browser.test.sendMessage("storage-local-data-migrated");
      
  },
  async function part_5() {
        const storedData = await browser.storage.local.get();
    
        browser.test.assertEq(
          Object.keys(storedData).length,
          0,
          "No data should be found on invalid data migration"
        );
    
        await browser.storage.local.set({
          test_key_string_on_IDBBackend: "expected-value",
        });
    
        browser.test.sendMessage("storage-local-data-migrated-and-set");
      
  },
  async function part_6() {
        await browser.storage.local.set({
          test_key_string_on_JSONFileBackend: "expected-value",
        });
        browser.test.sendMessage("storage-local-data-migrated-and-set");
      
  },
  async function part_7() {
          browser.test.onMessage.addListener(async (msg, dataSize) => {
            if (msg !== "verify-stored-data") {
              return;
            }
            const res = await browser.storage.local.get();
            browser.test.assertEq(
              res.data && res.data.length,
              dataSize,
              "Got the expected data"
            );
            browser.test.sendMessage("verify-stored-data:done");
          });
    
          browser.test.sendMessage("bg-page:ready");
        
  },
]);
