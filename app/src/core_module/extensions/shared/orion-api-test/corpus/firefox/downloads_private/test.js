"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          function promiseEvent(eventTarget, accept) {
            return new Promise(resolve => {
              eventTarget.addListener(function listener(data) {
                if (accept && !accept(data)) {
                  return;
                }
                eventTarget.removeListener(listener);
                resolve(data);
              });
            });
          }
          let startTestPromise = promiseEvent(browser.test.onMessage);
          let removeTestPromise = promiseEvent(
            browser.test.onMessage,
            msg => msg == "remove"
          );
          let onCreatedPromise = promiseEvent(browser.downloads.onCreated);
          let onDonePromise = promiseEvent(
            browser.downloads.onChanged,
            delta => delta.state && delta.state.current === "complete"
          );
    
          browser.test.sendMessage("ready");
          let { url, filename } = await startTestPromise;
    
          browser.test.log("Starting private download");
          let downloadId = await browser.downloads.download({
            url,
            filename,
            incognito: true,
          });
          browser.test.sendMessage("downloadId", downloadId);
    
          browser.test.log("Waiting for downloads.onCreated");
          let createdItem = await onCreatedPromise;
    
          browser.test.log("Waiting for completion notification");
          await onDonePromise;
    
          // test_ext_downloads_download.js already tests whether the file exists
          // in the file system. Here we will only verify that the  downloads API
          // behaves in a meaningful way.
    
          let [downloadItem] = await browser.downloads.search({ id: downloadId });
          browser.test.assertEq(url, createdItem.url, "onCreated url should match");
          browser.test.assertEq(url, downloadItem.url, "download url should match");
          browser.test.assertTrue(
            createdItem.incognito,
            "created download should be private"
          );
          browser.test.assertTrue(
            downloadItem.incognito,
            "stored download should be private"
          );
    
          await removeTestPromise;
          browser.test.log("Removing downloaded file");
          browser.test.assertTrue(downloadItem.exists, "downloaded file exists");
          await browser.downloads.removeFile(downloadId);
    
          // Disabled because the assertion fails - https://bugzil.la/1381031
          // let [downloadItem2] = await browser.downloads.search({id: downloadId});
          // browser.test.assertFalse(downloadItem2.exists, "file should be deleted");
    
          browser.test.log("Erasing private download from history");
          let erasePromise = promiseEvent(browser.downloads.onErased);
          await browser.downloads.erase({ id: downloadId });
          browser.test.assertEq(
            downloadId,
            await erasePromise,
            "onErased should be fired for the erased private download"
          );
    
          browser.test.notifyPass("private download test done");
        
  },
  async function part_2() {
          browser.downloads.onCreated.addListener(() => {
            browser.test.fail("download-onCreated");
          });
          browser.downloads.onChanged.addListener(() => {
            browser.test.fail("download-onChanged");
          });
          browser.downloads.onErased.addListener(() => {
            browser.test.fail("download-onErased");
          });
          browser.test.onMessage.addListener(async (msg, data) => {
            if (msg == "download") {
              let { url, filename, downloadId } = data;
              await browser.test.assertRejects(
                browser.downloads.download({
                  url,
                  filename,
                  incognito: true,
                }),
                /private browsing access not allowed/,
                "cannot download using incognito without permission."
              );
    
              let downloads = await browser.downloads.search({ id: downloadId });
              browser.test.assertEq(
                downloads.length,
                0,
                "cannot search for incognito downloads"
              );
              let erasing = await browser.downloads.erase({ id: downloadId });
              browser.test.assertEq(
                erasing.length,
                0,
                "cannot erase incognito download"
              );
    
              await browser.test.assertRejects(
                browser.downloads.removeFile(downloadId),
                /Invalid download id/,
                "cannot remove incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.pause(downloadId),
                /Invalid download id/,
                "cannot pause incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.resume(downloadId),
                /Invalid download id/,
                "cannot resume incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.cancel(downloadId),
                /Invalid download id/,
                "cannot cancel incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.removeFile(downloadId),
                /Invalid download id/,
                "cannot remove incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.show(downloadId),
                /Invalid download id/,
                "cannot show incognito download"
              );
              await browser.test.assertRejects(
                browser.downloads.getFileIcon(downloadId),
                /Invalid download id/,
                "cannot show incognito download"
              );
            }
            if (msg == "download.open") {
              let { downloadId } = data;
              await browser.test.assertRejects(
                browser.downloads.open(downloadId),
                /Invalid download id/,
                "cannot open incognito download"
              );
            }
            browser.test.sendMessage("continue");
          });
        
  },
]);
