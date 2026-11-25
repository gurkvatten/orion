"use strict";

// Extracted from mozilla-central toolkit/components/extensions/test/
// xpcshell (Mozilla Public License 2.0). Each part is the background()
// body of one loadExtension() call in the upstream test file, sequenced
// under the corpus harness. Harness handshakes (sendMessage/awaitMessage)
// have no peer here and will fail — that failure is the TODO signal.

browser.test.runTests([
  async function part_1() {
      browser.runtime.getPlatformInfo(info => {
        let validOSs = ["mac", "win", "android", "cros", "linux", "openbsd"];
        let validArchs = [
          "aarch64",
          "arm",
          "ppc64",
          "s390x",
          "sparc64",
          "x86-32",
          "x86-64",
        ];
    
        browser.test.assertTrue(validOSs.includes(info.os), "OS is valid");
        browser.test.assertTrue(
          validArchs.includes(info.arch),
          "Architecture is valid"
        );
        browser.test.notifyPass("runtime.getPlatformInfo");
      });
  },
]);
