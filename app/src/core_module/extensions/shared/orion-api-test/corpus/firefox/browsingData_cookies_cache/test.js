"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.test.onMessage.addListener(async msg => {
          if (msg == "removeCache") {
            await browser.browsingData.removeCache({});
          } else {
            await browser.browsingData.remove({}, { cache: true });
          }
          browser.test.sendMessage("cacheRemoved");
        });
      
  },
  async function part_2() {
        browser.test.onMessage.addListener(async (msg, options) => {
          if (msg == "removeCookies") {
            await browser.browsingData.removeCookies(options);
          } else {
            await browser.browsingData.remove(options, { cookies: true });
          }
          browser.test.sendMessage("cookiesRemoved");
        });
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(async options => {
          await browser.browsingData.remove(options, {
            cache: true,
            cookies: true,
          });
          browser.test.sendMessage("cacheAndCookiesRemoved");
        });
      
  },
]);
