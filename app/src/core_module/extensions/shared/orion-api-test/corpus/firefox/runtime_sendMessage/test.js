"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "respond-now") {
            respond(msg);
          } else if (msg == "respond-soon") {
            setTimeout(() => {
              respond(msg);
            }, 0);
            return true;
          } else if (msg == "respond-promise") {
            return Promise.resolve(msg);
          } else if (msg == "respond-promise-false") {
            return Promise.resolve(false);
          } else if (msg == "respond-false") {
            // return false means that respond() is not expected to be called.
            setTimeout(() => respond("should be ignored"));
            return false;
          } else if (msg == "respond-never") {
            return undefined;
          } else if (msg == "respond-error") {
            return Promise.reject(new Error(msg));
          } else if (msg == "throw-error") {
            throw new Error(msg);
          } else if (msg === "respond-uncloneable") {
            return Promise.resolve(window);
          } else if (msg === "reject-uncloneable") {
            return Promise.reject(window);
          } else if (msg == "reject-undefined") {
            return Promise.reject();
          } else if (msg == "throw-undefined") {
            throw undefined; // eslint-disable-line no-throw-literal
          }
        });
    
        browser.runtime.onMessage.addListener((msg, sender, respond) => {
          if (msg == "respond-now") {
            respond("hello");
          } else if (msg == "respond-now-2") {
            respond(msg);
          }
        });
    
        browser.runtime.onMessage.addListener(msg => {
          if (msg == "respond-now") {
            // If a response from another listener is received first, this
            // exception should be ignored.  Test fails if it is not.
    
            // All this is of course stupid, but some extensions depend on it.
            msg.blah.this.throws();
          }
        });
    
        let childFrame = document.createElement("iframe");
        childFrame.src = "extensionpage.html";
        document.body.appendChild(childFrame);
      
  },
  async function part_2() {
        browser.runtime.onMessage.addListener(msg => {
          // eslint-disable-next-line mozilla/use-isInstance -- this function runs in an extension
          browser.test.assertTrue(msg.blob instanceof Blob, "Message is a blob");
          return Promise.resolve(msg);
        });
    
        let childFrame = document.createElement("iframe");
        childFrame.src = "extensionpage.html";
        document.body.appendChild(childFrame);
      
  },
  async function part_3() {
        let savedResolve, savedRespond;
    
        browser.runtime.onMessage.addListener((msg, _, respond) => {
          browser.test.log(`Got request: ${msg}`);
          switch (msg) {
            case "ping":
              respond("pong");
              return;
    
            case "promise-save":
              return new Promise(resolve => {
                savedResolve = resolve;
              });
            case "promise-resolve":
              savedResolve("saved-resolve");
              return;
            case "promise-never":
              return new Promise(() => {});
    
            case "callback-save":
              savedRespond = respond;
              return true;
            case "callback-call":
              savedRespond("saved-respond");
              return;
            case "callback-never":
              return true;
          }
        });
    
        const frame = document.createElement("iframe");
        frame.src = "page.html";
        document.body.appendChild(frame);
      
  },
  async function part_4() {
          browser.runtime.onMessage.addListener((msg, _, respond) => {
            browser.test.log(`Background got request: ${msg}`);
    
            switch (msg) {
              case "ask-bg-fast":
                respond("bg-respond");
                return true;
    
              case "ask-bg-slow":
                return new Promise(r => setTimeout(() => r("bg-promise")), 1000);
            }
          });
          browser.test.sendMessage("bg-ready");
        
  },
]);
