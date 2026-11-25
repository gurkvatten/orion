"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let count = 0;
    
        browser.alarms.onAlarm.addListener(async alarm => {
          browser.test.assertEq(
            "replaced alarm",
            alarm.name,
            "Expected last alarm"
          );
          browser.test.assertEq(
            0,
            count++,
            "duplicate named alarm replaced existing alarm"
          );
          let results = await browser.alarms.getAll();
    
          // "replaced alarm" is expected to be replaced with a non-repeating
          // alarm, so it should not appear in the list of alarms.
          browser.test.assertEq(1, results.length, "exactly one alarms exists");
          browser.test.assertEq(
            "unrelated alarm",
            results[0].name,
            "remaining alarm has the expected name"
          );
    
          browser.test.notifyPass("alarm-duplicate");
        });
    
        // Alarm that is so far in the future that it is never triggered.
        browser.alarms.create("unrelated alarm", { delayInMinutes: 60 });
        // Alarm that repeats.
        browser.alarms.create("replaced alarm", {
          delayInMinutes: 1 / 60,
          periodInMinutes: 1 / 60,
        });
        // Before the repeating alarm is triggered, it is immediately replaced with
        // a non-repeating alarm.
        browser.alarms.create("replaced alarm", { delayInMinutes: 3 / 60 });
      
  },
]);
