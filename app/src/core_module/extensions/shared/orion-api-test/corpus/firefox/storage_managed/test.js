"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          await browser.test.assertRejects(
            browser.storage.managed.set({ a: 1 }),
            /storage.managed is read-only/,
            "browser.storage.managed.set() rejects because it's read only"
          );
    
          await browser.test.assertRejects(
            browser.storage.managed.remove("str"),
            /storage.managed is read-only/,
            "browser.storage.managed.remove() rejects because it's read only"
          );
    
          await browser.test.assertRejects(
            browser.storage.managed.clear(),
            /storage.managed is read-only/,
            "browser.storage.managed.clear() rejects because it's read only"
          );
    
          browser.test.sendMessage(
            "results",
            await Promise.all([
              browser.storage.managed.get(),
              browser.storage.managed.get("str"),
              browser.storage.managed.get(["null", "obj"]),
              browser.storage.managed.get({ str: "a", num: 2 }),
            ])
          );
        
  },
  async function part_2() {
          await browser.test.assertRejects(
            browser.storage.managed.get({ a: 1 }),
            /Managed storage manifest not found/,
            "browser.storage.managed.get() rejects when without manifest"
          );
    
          browser.test.notifyPass();
        
  },
  async function part_3() {
          const dummyListener = () => {};
          browser.storage.managed.onChanged.addListener(dummyListener);
          browser.test.assertTrue(
            browser.storage.managed.onChanged.hasListener(dummyListener),
            "addListener works according to hasListener"
          );
          browser.storage.managed.onChanged.removeListener(dummyListener);
    
          // We should get a warning for each registration.
          browser.storage.managed.onChanged.addListener(() => {});
          browser.storage.managed.onChanged.addListener(() => {});
          browser.storage.managed.onChanged.addListener(() => {});
    
          // Invoke the storage.managed API to make sure that we have made a
          // round trip to the parent process and back. This is because event
          // registration is async but we cannot await (bug 1300234).
          await browser.test.assertRejects(
            browser.storage.managed.get({ a: 1 }),
            /Managed storage manifest not found/,
            "browser.storage.managed.get() rejects when without manifest"
          );
    
          browser.test.notifyPass();
        
  },
]);
