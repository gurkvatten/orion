"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        const ALARM_NAME = "test_ext_alarms";
        let count = 0;
        let timer;
    
        browser.alarms.onAlarm.addListener(alarm => {
          browser.test.assertEq(
            alarm.name,
            ALARM_NAME,
            "alarm has the expected name"
          );
          if (count++ === 3) {
            clearTimeout(timer);
            browser.alarms.clear(ALARM_NAME).then(wasCleared => {
              browser.test.assertTrue(wasCleared, "alarm was cleared");
    
              browser.test.notifyPass("alarm-periodic");
            });
          }
        });
    
        browser.alarms.create(ALARM_NAME, { periodInMinutes: 0.02 });
    
        timer = setTimeout(async () => {
          browser.test.fail("alarm fired expected number of times");
    
          let wasCleared = await browser.alarms.clear(ALARM_NAME);
          browser.test.assertTrue(wasCleared, "alarm was cleared");
    
          browser.test.notifyFail("alarm-periodic");
        }, 30000);
      
  },
]);
