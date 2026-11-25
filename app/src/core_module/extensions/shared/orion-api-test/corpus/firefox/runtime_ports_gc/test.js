"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let obj1 = {};
          let obj2 = {};
          let witness1 = browser.gcHelper.registerWitness(obj1);
          let witness2 = browser.gcHelper.registerWitness(obj2);
          obj1 = null;
          await browser.gcHelper.forceGarbageCollect();
          browser.test.assertTrue(
            browser.gcHelper.isGarbageCollected(witness1),
            "obj1 should have been garbage-collected"
          );
          browser.test.assertFalse(
            browser.gcHelper.isGarbageCollected(witness2),
            "obj2 should not have been garbage-collected"
          );
    
          browser.test.sendMessage("done");
        
  },
  async function part_2() {
          let witnessPortSender;
          let witnessPortReceiver;
    
          browser.runtime.onConnect.addListener(port => {
            browser.test.assertEq("daName", port.name, "expected port");
            witnessPortReceiver = browser.gcHelper.registerWitness(port);
            port.disconnect();
          });
    
          // runtime.connect() only triggers onConnect for different contexts,
          // so create a frame to have a different context.
          // A blank frame in a moz-extension:-document will have access to the
          // extension APIs.
          let frameWindow = await new Promise(resolve => {
            let f = document.createElement("iframe");
            f.onload = () => resolve(f.contentWindow);
            document.body.append(f);
          });
          await new Promise(resolve => {
            let port = frameWindow.browser.runtime.connect({ name: "daName" });
            witnessPortSender = browser.gcHelper.registerWitness(port);
            port.onDisconnect.addListener(() => resolve());
          });
    
          await browser.gcHelper.forceGarbageCollect();
    
          browser.test.assertTrue(
            browser.gcHelper.isGarbageCollected(witnessPortSender),
            "runtime.connect() port should have been garbage-collected"
          );
          browser.test.assertTrue(
            browser.gcHelper.isGarbageCollected(witnessPortReceiver),
            "runtime.onConnect port should have been garbage-collected"
          );
    
          browser.test.sendMessage("done");
        
  },
]);
