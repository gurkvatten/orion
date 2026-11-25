"use strict";

// Bridge error-path tests: native calls that cannot be handled must report
// the failure instead of leaving the caller hanging forever. In the Firefox
// flavor (promise convention) that means the returned promise REJECTS with an
// Error; in the Chrome flavor (callback convention) the callback must still be
// invoked, with runtime.lastError set for its duration — `call()` from
// lib/api.js turns that into a rejection so the same assertions apply.
// Covers the orion-linux fixes: generated not-implemented stubs reject, and
// dispatch failures (bad argument types) reject with the dispatch error.
//
// Every call is wrapped in a timeout so a regression (promise never settled,
// callback never invoked) fails loudly instead of hanging the whole test run.
//
// runtime.getPlatformInfo is the "known unimplemented" probe: it exists in
// both flavors and has no native implementation, so the generated stub
// rejects with "... is not implemented". Swap it when it gets implemented.

function withTimeout(promise, ms, label) {
    return Promise.race([
        promise,
        new Promise((_, reject) =>
            setTimeout(
                () => reject(new Error(`TIMEOUT: ${label} did not settle within ${ms}ms (promise never resolved or rejected)`)),
                ms
            )),
    ]);
}

// `subject` is a promise or a thunk returning one. Argument validation errors
// are thrown synchronously by the bridge (as in Chrome and Firefox), so pass a
// thunk when the call itself may throw: the async wrapper turns the throw into
// a rejection and the same assertions apply.
async function assertRejects(subject, pattern, label) {
    const promise = typeof subject === "function" ? (async () => subject())() : subject;
    let value;
    try {
        value = await withTimeout(promise, 3000, label);
    } catch (e) {
        if (e instanceof Error && e.message.startsWith("TIMEOUT:"))
            throw e;
        assert(
            e instanceof Error,
            `${label}: rejection value should be an Error, got ${typeof e} (${JSON.stringify(e)})`
        );
        if (pattern && !pattern.test(e.message)) {
            throw new Error(
                `${label}: rejection message ${JSON.stringify(e.message)} does not match ${pattern}`
            );
        }
        return e;
    }
    throw new Error(`${label}: expected rejection, but resolved with ${JSON.stringify(value)}`);
}

TestRunner.suite("errors", (test) => {
    test("unimplemented API rejects (runtime.getPlatformInfo)", async () => {
        await assertRejects(
            () => call(api.runtime, "getPlatformInfo"),
            /not implemented/,
            "runtime.getPlatformInfo"
        );
    });

    test("invalid enum value rejects with a type error (storage.local.setAccessLevel)", async () => {
        // The bridge checks enum members against the IDL before dispatch, so
        // an unknown access level never reaches the native side. (The call
        // itself is allowed from extension pages -- they are trusted
        // contexts; only content scripts are refused.)
        await assertRejects(
            () => call(api.storage.local, "setAccessLevel", { accessLevel: "NOT_A_LEVEL" }),
            /overload|while calling|Type ?Error/i,
            "storage.local.setAccessLevel with an unknown accessLevel"
        );
    });

    test("argument type mismatch rejects with a type error", async () => {
        // The bridge validates dictionary members against the IDL before
        // dispatch (no overload matches a string delayInMinutes); an older
        // bridge let it through and the native side rejected instead.
        await assertRejects(
            () => call(api.alarms, "create", "bad-type-alarm", { delayInMinutes: "not-a-number" }),
            /overload|while calling|Type ?Error/i,
            "alarms.create with string delayInMinutes"
        );
    });

    test("service still works after a rejection", async () => {
        await assertRejects(() => call(api.runtime, "getPlatformInfo"), null, "runtime.getPlatformInfo");
        await call(api.alarms, "clearAll");
        await call(api.alarms, "create", "post-error-alarm", { delayInMinutes: 60 });
        const alarm = await call(api.alarms, "get", "post-error-alarm");
        assertEqual(alarm.name, "post-error-alarm");
        await call(api.alarms, "clearAll");
    });

    test("callback convention: runtime.lastError set during the callback, cleared after (Chrome flavor)", async () => {
        if (!OrionApi.isChrome) return; // the Firefox flavor uses the promise convention
        const seen = await withTimeout(new Promise((resolve) => {
            chrome.runtime.getPlatformInfo(() => {
                const err = chrome.runtime.lastError;
                resolve(err ? { message: err.message } : null);
            });
        }), 3000, "chrome.runtime.getPlatformInfo callback");
        assert(seen !== null, "runtime.lastError should be set inside the callback");
        assertType(seen.message, "string", "runtime.lastError.message");
        assert(/not implemented/.test(seen.message), `unexpected lastError message: ${seen.message}`);
        assert(chrome.runtime.lastError == null, "runtime.lastError should be cleared after the callback");
    });

    test("promise convention: rejection does not leave runtime.lastError set (Firefox flavor)", async () => {
        if (OrionApi.isChrome) return; // the Chrome flavor uses the callback convention
        await assertRejects(browser.runtime.getPlatformInfo(), /not implemented/, "runtime.getPlatformInfo");
        assert(browser.runtime.lastError == null, "runtime.lastError should not be set by a rejected promise");
    });
});
