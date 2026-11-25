"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let events = [];
          browser.storage.onChanged.addListener((_, area) => {
            events.push(area);
          });
          browser.test.onMessage.addListener(_msg => {
            browser.test.sendMessage("bg-events", events.join());
          });
          browser.runtime.onMessage.addListener(async _msg => {
            await browser.storage.local.set({ foo: "local" });
            await browser.storage.session.set({ foo: "session" });
            await browser.storage.sync.set({ foo: "sync" });
            browser.test.sendMessage("done");
          });
        
  },
  async function part_2() {
        let before = await browser.storage.session.get();
    
        browser.storage.session.set({ count: (before.count ?? 0) + 1 });
    
        // Roundtrip the data through the parent process.
        let after = await browser.storage.session.get();
    
        browser.test.sendMessage("data", { before, after });
      
  },
]);
