"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
          let perms = await browser.permissions.getAll();
          browser.test.sendMessage("permissions", perms);
        
  },
  async function part_2() {
        browser.test.onMessage.addListener(async (method, arg) => {
          if (method == "getAll") {
            let perms = await browser.permissions.getAll();
            let url = browser.runtime.getURL("*");
            perms.origins = perms.origins.filter(i => i != url);
            browser.test.sendMessage("getAll.result", perms);
          } else if (method == "contains") {
            let result = await browser.permissions.contains(arg);
            browser.test.sendMessage("contains.result", result);
          } else if (method == "request") {
            try {
              let result = await browser.permissions.request(arg);
              browser.test.sendMessage("request.result", {
                status: "success",
                result,
              });
            } catch (err) {
              browser.test.sendMessage("request.result", {
                status: "error",
                message: err.message,
              });
            }
          } else if (method == "remove") {
            let result = await browser.permissions.remove(arg);
            browser.test.sendMessage("remove.result", result);
          }
        });
      
  },
  async function part_3() {
        browser.test.onMessage.addListener(async perms => {
          await browser.permissions.request(perms);
          browser.test.sendMessage("requested");
        });
    
        let all = await browser.permissions.getAll();
        let url = browser.runtime.getURL("*");
        all.origins = all.origins.filter(i => i != url);
        browser.test.sendMessage("perms", all);
      
  },
  async function part_4() {
          browser.test.sendMessage("ready", browser.runtime.getURL("page.html"));
        
  },
  async function part_5() {
          browser.test.onMessage.addListener(async () => {
            let before = !!browser.tabs.captureVisibleTab;
            let granted = await browser.permissions.request({
              origins: ["<all_urls>"],
            });
            let after = !!browser.tabs.captureVisibleTab;
    
            browser.test.sendMessage("results", [before, granted, after]);
          });
        
  },
  async function part_6() {
        browser.test.onMessage.addListener(async (msg, arg) => {
          if (msg == "request") {
            try {
              let result = await browser.permissions.request(arg);
              browser.test.sendMessage("result", result);
            } catch (e) {
              browser.test.sendMessage("result", e.message);
            }
          }
          if (msg === "getAll") {
            let result = await browser.permissions.getAll(arg);
            browser.test.sendMessage("granted", result);
          }
        });
      
  },
  async function part_7() {
        browser.test.onMessage.addListener(async (msg, arg) => {
          if (msg == "request") {
            let result = await browser.permissions.request(arg);
            browser.test.sendMessage("result", result);
          }
          if (msg === "getAll") {
            let result = await browser.permissions.getAll(arg);
            browser.test.sendMessage("granted", result);
          }
        });
      
  },
  async function part_8() {
        browser.test.onMessage.addListener(async (method, arg) => {
          try {
            if (method == "getAll") {
              let perms = await browser.permissions.getAll();
              browser.test.sendMessage("getAll.result", perms);
            } else if (method == "contains") {
              let result = await browser.permissions.contains(arg);
              browser.test.sendMessage("contains.result", {
                status: "success",
                result,
              });
            } else if (method == "request") {
              let result = await browser.permissions.request(arg);
              browser.test.sendMessage("request.result", {
                status: "success",
                result,
              });
            } else if (method == "remove") {
              let result = await browser.permissions.remove(arg);
              browser.test.sendMessage("remove.result", result);
            }
          } catch (err) {
            browser.test.sendMessage(`${method}.result`, {
              status: "error",
              message: err.message,
            });
          }
        });
      
  },
  async function part_9() {
          browser.test.onMessage.addListener(async () => {
            let result = await browser.permissions.request({
              permissions: [],
              origins: ["<all_urls>"],
            });
            browser.test.sendMessage("result", result);
          });
          browser.permissions.onAdded.addListener(async permissions => {
            browser.test.sendMessage("onAdded", permissions);
          });
          browser.test.sendMessage("ready");
        
  },
]);
