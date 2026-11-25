"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        if (shouldHaveExperiments) {
          await testFooExperiment();
        } else {
          await testFooFailExperiment();
        }
    
        browser.test.notifyPass("background.experiments.foo");
      
  },
  async function part_2() {
        await testFooExperiment();
    
        browser.test.assertEq(
          "object",
          typeof browser.experiments.crunk,
          "typeof browser.experiments.crunk"
        );
    
        browser.test.assertEq(
          "function",
          typeof browser.experiments.crunk.child,
          "typeof browser.experiments.crunk.child"
        );
    
        browser.test.assertEq(
          "function",
          typeof browser.experiments.crunk.parent,
          "typeof browser.experiments.crunk.parent"
        );
    
        browser.test.assertEq(
          "crunk-child",
          browser.experiments.crunk.child(),
          "crunk.child()"
        );
    
        browser.test.assertEq(
          "crunk-parent",
          await browser.experiments.crunk.parent(),
          "await crunk.parent()"
        );
    
        browser.test.notifyPass("background.experiments.crunk");
      
  },
]);
