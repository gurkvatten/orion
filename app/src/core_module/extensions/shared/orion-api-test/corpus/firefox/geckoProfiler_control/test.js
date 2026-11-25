"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          const runningListener = isRunning => {
            if (isRunning) {
              browser.test.sendMessage("started");
            } else {
              browser.test.sendMessage("stopped");
            }
          };
    
          browser.test.onMessage.addListener(async (message, data) => {
            let result;
            switch (message) {
              case "start":
                result = await browser.geckoProfiler.start({
                  bufferSize: 10000,
                  windowLength: 20,
                  interval: 0.5,
                  features: ["js"],
                  threads: ["GeckoMain"],
                });
                browser.test.assertEq(undefined, result, "start returns nothing.");
                break;
              case "stop":
                result = await browser.geckoProfiler.stop();
                browser.test.assertEq(undefined, result, "stop returns nothing.");
                break;
              case "pause":
                result = await browser.geckoProfiler.pause();
                browser.test.assertEq(undefined, result, "pause returns nothing.");
                browser.test.sendMessage("paused");
                break;
              case "resume":
                result = await browser.geckoProfiler.resume();
                browser.test.assertEq(undefined, result, "resume returns nothing.");
                browser.test.sendMessage("resumed");
                break;
              case "test profile":
                result = await browser.geckoProfiler.getProfile();
                browser.test.assertTrue(
                  "libs" in result,
                  "The profile contains libs."
                );
                browser.test.assertTrue(
                  "meta" in result,
                  "The profile contains meta."
                );
                browser.test.assertTrue(
                  "threads" in result,
                  "The profile contains threads."
                );
                browser.test.assertTrue(
                  result.threads.some(t => t.name == "GeckoMain"),
                  "The profile contains a GeckoMain thread."
                );
                browser.test.sendMessage("tested profile");
                break;
              case "test dump to file":
                try {
                  await browser.geckoProfiler.dumpProfileToFile(data.fileName);
                  browser.test.sendMessage("tested dump to file", {});
                } catch (e) {
                  browser.test.sendMessage("tested dump to file", {
                    error: e.message,
                  });
                }
                break;
              case "test profile as array buffer":
                let arrayBuffer =
                  await browser.geckoProfiler.getProfileAsArrayBuffer();
                browser.test.assertTrue(
                  arrayBuffer.byteLength >= 2,
                  "The profile array buffer contains data."
                );
                let textDecoder = new TextDecoder();
                let profile = JSON.parse(textDecoder.decode(arrayBuffer));
                browser.test.assertTrue(
                  "libs" in profile,
                  "The profile contains libs."
                );
                browser.test.assertTrue(
                  "meta" in profile,
                  "The profile contains meta."
                );
                browser.test.assertTrue(
                  "threads" in profile,
                  "The profile contains threads."
                );
                browser.test.assertTrue(
                  profile.threads.some(t => t.name == "GeckoMain"),
                  "The profile contains a GeckoMain thread."
                );
                browser.test.sendMessage("tested profile as array buffer");
                break;
              case "remove runningListener":
                browser.geckoProfiler.onRunning.removeListener(runningListener);
                browser.test.sendMessage("removed runningListener");
                break;
            }
          });
    
          browser.test.sendMessage("ready");
    
          browser.geckoProfiler.onRunning.addListener(runningListener);
        
  },
]);
