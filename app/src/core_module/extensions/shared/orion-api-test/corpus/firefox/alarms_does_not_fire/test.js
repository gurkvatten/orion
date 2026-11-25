"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let ALARM_NAME = "test_ext_alarms";
    
        browser.alarms.onAlarm.addListener(() => {
          browser.test.fail("cleared alarm does not fire");
          browser.test.notifyFail("alarm-cleared");
        });
        browser.alarms.create(ALARM_NAME, { when: Date.now() + 1000 });
    
        let wasCleared = await browser.alarms.clear(ALARM_NAME);
        browser.test.assertTrue(wasCleared, "alarm was cleared");
    
        await new Promise(resolve => setTimeout(resolve, 2000));
    
        browser.test.notifyPass("alarm-cleared");
      
  },
]);
