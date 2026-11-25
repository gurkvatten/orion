"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          browser.test.assertTrue(
            !browser.alarms,
            "alarm API is not available when the alarm permission is not required"
          );
          browser.test.notifyPass("alarms_permission");
        
  },
  async function part_2() {
        let ALARM_NAME = "test_ext_alarms";
    
        browser.alarms.create(ALARM_NAME, { when: Date.now() + 2000000 });
    
        let wasCleared = await browser.alarms.clear(ALARM_NAME + "1");
        browser.test.assertFalse(wasCleared, "alarm was not cleared");
    
        let alarms = await browser.alarms.getAll();
        browser.test.assertEq(1, alarms.length, "alarm was not removed");
        browser.test.notifyPass("alarm-clear");
      
  },
  async function part_3() {
        browser.alarms.create({ when: Date.now() + 2000000 });
    
        let alarm = await browser.alarms.get();
        browser.test.assertEq("", alarm.name, "expected alarm returned");
    
        let wasCleared = await browser.alarms.clear();
        browser.test.assertTrue(wasCleared, "alarm was cleared");
    
        let alarms = await browser.alarms.getAll();
        browser.test.assertEq(0, alarms.length, "alarm was removed");
    
        browser.test.notifyPass("alarm-single-arg");
      
  },
  async function part_4() {
        browser.alarms.create(alarmName, { when: Date.now() + 2000000 });
    
        let alarm = await browser.alarms.get();
        browser.test.assertTrue(alarm, "got an alarm");
        browser.test.assertEq("", alarm.name, "expected alarm returned");
    
        let wasCleared = await browser.alarms.clear();
        browser.test.assertTrue(wasCleared, "alarm was cleared");
    
        let alarms = await browser.alarms.getAll();
        browser.test.assertEq(0, alarms.length, "alarm was removed");
    
        browser.test.notifyPass("alarm-test-done");
      
  },
  async function part_5() {
        const ALARM_NAME = "test_alarm";
    
        let suffixes = [0, 1, 2];
    
        for (let suffix of suffixes) {
          browser.alarms.create(ALARM_NAME + suffix, {
            when: Date.now() + (suffix + 1) * 10000,
          });
        }
    
        let alarms = await browser.alarms.getAll();
        browser.test.assertEq(
          suffixes.length,
          alarms.length,
          "expected number of alarms were found"
        );
        alarms.forEach((alarm, index) => {
          browser.test.assertEq(
            ALARM_NAME + index,
            alarm.name,
            "alarm has the expected name"
          );
        });
    
        for (let suffix of suffixes) {
          let alarm = await browser.alarms.get(ALARM_NAME + suffix);
          browser.test.assertEq(
            ALARM_NAME + suffix,
            alarm.name,
            "alarm has the expected name"
          );
          browser.test.sendMessage(`get-${suffix}`);
        }
    
        let wasCleared = await browser.alarms.clear(ALARM_NAME + suffixes[0]);
        browser.test.assertTrue(wasCleared, "alarm was cleared");
    
        alarms = await browser.alarms.getAll();
        browser.test.assertEq(2, alarms.length, "alarm was removed");
    
        let alarm = await browser.alarms.get(ALARM_NAME + suffixes[0]);
        browser.test.assertEq(undefined, alarm, "non-existent alarm is undefined");
        browser.test.sendMessage(`get-invalid`);
    
        wasCleared = await browser.alarms.clearAll();
        browser.test.assertTrue(wasCleared, "alarms were cleared");
    
        alarms = await browser.alarms.getAll();
        browser.test.assertEq(0, alarms.length, "no alarms exist");
        browser.test.sendMessage("clearAll");
        browser.test.sendMessage("clear");
        browser.test.sendMessage("getAll");
      
  },
  async function part_6() {
        let ALARM_NAME = "test_ext_alarms";
        let timer;
    
        browser.alarms.onAlarm.addListener(alarm => {
          browser.test.assertEq(
            ALARM_NAME,
            alarm.name,
            "alarm has the expected name"
          );
          clearTimeout(timer);
          browser.test.sendMessage("alarms-create-with-options");
        });
    
        browser.alarms.create(ALARM_NAME, createOptions);
    
        timer = setTimeout(async () => {
          browser.test.fail("alarm fired within expected time");
          let wasCleared = await browser.alarms.clear(ALARM_NAME);
          browser.test.assertTrue(wasCleared, "alarm was cleared");
          browser.test.sendMessage("alarms-create-with-options");
        }, 10000);
      
  },
  async function part_7() {
        const ALARM_NAME = "test_ext_alarms";
        const when = Date.now() + 2000000;
    
        // test callback return
        browser.alarms.create(ALARM_NAME, { when }, result => {
          browser.test.assertEq(undefined, result, "no callback params");
          browser.test.sendMessage("callback done");
        });
    
        // test promise return
        browser.alarms.create(ALARM_NAME + 1, { when }).then(result => {
          browser.test.assertEq(undefined, result, "no promise params");
          browser.test.sendMessage("promise done");
        });
      
  },
]);
