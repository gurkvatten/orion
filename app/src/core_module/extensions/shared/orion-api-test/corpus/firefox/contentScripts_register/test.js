"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        let cssCode = `
          #registered-extension-text-style {
            background-color: blue;
          }
        `;
    
        const matches = ["http://localhost/*/file_sample_registered_styles.html"];
    
        browser.test.assertThrows(
          () => {
            browser.contentScripts.register({
              matches,
              unknownParam: "unexpected property",
            });
          },
          /Unexpected property "unknownParam"/,
          "contentScripts.register throws on unexpected properties"
        );
    
        let fileScript = await browser.contentScripts.register({
          css: [{ file: "registered_ext_style.css" }],
          matches,
          runAt: "document_start",
        });
    
        let textScript = await browser.contentScripts.register({
          css: [{ code: cssCode }],
          matches,
          runAt: "document_start",
        });
    
        browser.test.onMessage.addListener(async msg => {
          switch (msg) {
            case "unregister-text":
              await textScript.unregister().catch(err => {
                browser.test.fail(
                  `Unexpected exception while unregistering text style: ${err}`
                );
              });
    
              await browser.test.assertRejects(
                textScript.unregister(),
                /Content script already unregistered/,
                "Got the expected rejection on calling script.unregister() multiple times"
              );
    
              browser.test.sendMessage("unregister-text:done");
              break;
            case "unregister-file":
              await fileScript.unregister().catch(err => {
                browser.test.fail(
                  `Unexpected exception while unregistering url style: ${err}`
                );
              });
    
              await browser.test.assertRejects(
                fileScript.unregister(),
                /Content script already unregistered/,
                "Got the expected rejection on calling script.unregister() multiple times"
              );
    
              browser.test.sendMessage("unregister-file:done");
              break;
            default:
              browser.test.fail(`Unexpected test message received: ${msg}`);
          }
        });
    
        browser.test.sendMessage("background_ready");
      
  },
  async function part_2() {
        const frame = document.createElement("iframe");
        frame.setAttribute("src", "/background-frame.html");
    
        document.body.appendChild(frame);
    
        browser.test.onMessage.addListener(msg => {
          switch (msg) {
            case "unload-frame":
              frame.remove();
              browser.test.sendMessage("unload-frame:done");
              break;
            default:
              browser.test.fail(`Unexpected test message received: ${msg}`);
          }
        });
    
        browser.test.sendMessage("background_ready");
      
  },
  async function part_3() {
        browser.runtime.onMessage.addListener(
          ([msg, expectedStates, readyState]) => {
            if (msg == "chrome-namespace-ok") {
              browser.test.sendMessage(msg);
              return;
            }
    
            browser.test.assertEq("script-run", msg, "message type is correct");
            browser.test.assertTrue(
              expectedStates.includes(readyState),
              `readyState "${readyState}" is one of [${expectedStates}]`
            );
            browser.test.sendMessage("script-run-" + expectedStates[0]);
          }
        );
    
        // Raise an exception when the content script cannot be registered
        // because the extension has no permission to access the specified origin.
    
        await browser.test.assertRejects(
          browser.contentScripts.register({
            matches: ["http://*/*"],
            js: [
              {
                code: 'browser.test.fail("content script with wrong matches should not run")',
              },
            ],
          }),
          /Permission denied to register a content script for/,
          "The reject contains the expected error message"
        );
    
        // Register a content script from a JS code string.
    
        function textScriptCodeStart() {
          browser.runtime.sendMessage([
            "script-run",
            ["loading"],
            document.readyState,
          ]);
        }
        function textScriptCodeEnd() {
          browser.runtime.sendMessage([
            "script-run",
            ["interactive", "complete"],
            document.readyState,
          ]);
        }
        function textScriptCodeIdle() {
          browser.runtime.sendMessage([
            "script-run",
            ["complete"],
            document.readyState,
          ]);
        }
    
        // Register content scripts from both extension URLs and plain JS code strings.
    
        const content_scripts = [
          // Plain JS code strings.
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ code: `(${textScriptCodeStart})()` }],
            runAt: "document_start",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ code: `(${textScriptCodeEnd})()` }],
            runAt: "document_end",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ code: `(${textScriptCodeIdle})()` }],
            runAt: "document_idle",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ code: `(${textScriptCodeIdle})()` }],
            runAt: "document_idle",
            cookieStoreId: "firefox-container-1",
          },
          // Extension URLs.
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ file: "content_script_start.js" }],
            runAt: "document_start",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ file: "content_script_end.js" }],
            runAt: "document_end",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ file: "content_script_idle.js" }],
            runAt: "document_idle",
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ file: "content_script.js" }],
            // "runAt" is not specified here to ensure that it defaults to document_idle when missing.
          },
          {
            matches: ["http://localhost/*/file_sample.html"],
            js: [{ file: "content_script_idle.js" }],
            runAt: "document_idle",
            cookieStoreId: "firefox-container-1",
          },
        ];
    
        const expectedAPIs = ["unregister"];
    
        for (const scriptOptions of content_scripts) {
          const script = await browser.contentScripts.register(scriptOptions);
          const actualAPIs = Object.keys(script);
    
          browser.test.assertEq(
            JSON.stringify(expectedAPIs),
            JSON.stringify(actualAPIs),
            `Got a script API object for ${scriptOptions.js[0]}`
          );
        }
    
        browser.test.sendMessage("background-ready");
      
  },
  async function part_4() {
        await browser.contentScripts.register({
          js: [{ file: "content_script.js" }],
          css: [{ file: "content_style.css" }],
          matches: ["http://localhost/*"],
          excludeMatches: ["http://localhost/exclude/*"],
          excludeGlobs: ["*_exclude.html"],
          includeGlobs: ["*_include.html"],
          allFrames: true,
          matchAboutBlank: true,
          runAt: "document_start",
          world: "MAIN",
        });
    
        browser.test.sendMessage("background-ready", window.location.origin);
      
  },
  async function part_5() {
        await browser.contentScripts.register({
          js: [{ file: "cs.js" }],
          matches: ["http://localhost/*"],
          matchOriginAsFallback: true,
        });
        browser.test.sendMessage("ready");
      
  },
]);
