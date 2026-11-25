"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        function backgroundFetch(url) {
          return new Promise((resolve, reject) => {
            let xhr = new XMLHttpRequest();
            xhr.overrideMimeType("text/plain");
            xhr.open("GET", url);
            xhr.onload = () => {
              resolve(xhr.responseText);
            };
            xhr.onerror = reject;
            xhr.send();
          });
        }
    
        Promise.all([
          backgroundFetch("foo.css"),
          backgroundFetch("bar.CsS?x#y"),
          backgroundFetch("foo.txt"),
        ]).then(results => {
          browser.test.assertEq(
            "body { max-width: 42px; }",
            results[0],
            "CSS file localized"
          );
          browser.test.assertEq(
            "body { max-width: 42px; }",
            results[1],
            "CSS file localized"
          );
    
          browser.test.assertEq(
            "body { __MSG_foo__; }",
            results[2],
            "Text file not localized"
          );
    
          browser.test.notifyPass("i18n-css");
        });
    
        browser.test.sendMessage("ready", browser.runtime.getURL("/"));
      
  },
]);
