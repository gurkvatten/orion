"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const dbName = "broken-blob";
        const dbStore = "blob-store";
        const dbVersion = 1;
        const blobContent = "Hello World!";
    
        let db = await new Promise((resolve, reject) => {
          let dbOpen = indexedDB.open(dbName, dbVersion);
          dbOpen.onerror = event => {
            browser.test.fail(`Error opening the DB: ${event.target.error}`);
            browser.test.notifyFail("test-completed");
            reject();
          };
          dbOpen.onsuccess = event => {
            resolve(event.target.result);
          };
          dbOpen.onupgradeneeded = event => {
            let dbobj = event.target.result;
            dbobj.onerror = error => {
              browser.test.fail(`Error updating the DB: ${error.target.error}`);
              browser.test.notifyFail("test-completed");
              reject();
            };
            dbobj.createObjectStore(dbStore);
          };
        });
    
        async function save(blob) {
          let txn = db.transaction([dbStore], "readwrite");
          let store = txn.objectStore(dbStore);
          let req = store.put(blob, "key");
    
          return new Promise((resolve, reject) => {
            req.onsuccess = () => {
              resolve();
            };
            req.onerror = event => {
              browser.test.fail(
                `Error saving the blob into the DB: ${event.target.error}`
              );
              browser.test.notifyFail("test-completed");
              reject();
            };
          });
        }
    
        async function load() {
          let txn = db.transaction([dbStore], "readonly");
          let store = txn.objectStore(dbStore);
          let req = store.getAll();
    
          return new Promise((resolve, reject) => {
            req.onsuccess = () => resolve(req.result);
            req.onerror = () => reject(req.error);
          })
            .then(loadDetails => {
              let blobs = [];
              loadDetails.forEach(details => {
                blobs.push(details);
              });
              return blobs[0];
            })
            .catch(err => {
              browser.test.fail(
                `Error loading the blob from the DB: ${err} :: ${err.stack}`
              );
              browser.test.notifyFail("test-completed");
            });
        }
    
        browser.test.log("Blob creation");
        await save(new Blob([blobContent]));
        let blob = await load();
    
        db.close();
    
        browser.runtime.onMessage.addListener(([msg, what]) => {
          browser.test.log("Message received from content: " + msg);
          if (msg == "script-ready") {
            return Promise.resolve({ blob });
          }
    
          if (msg == "script-value") {
            browser.test.assertEq(blobContent, what, "blob content matches");
            browser.test.notifyPass("test-completed");
            return;
          }
    
          browser.test.fail(`Unexpected test message received: ${msg}`);
        });
    
        browser.test.sendMessage("bg-ready");
      
  },
]);
