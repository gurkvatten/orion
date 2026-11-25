(function() {
    function deepEq(a, b) {
        if (a === b) return true;
        if (typeof a !== typeof b) return false;
        if (a && b && typeof a === "object") {
            if (Array.isArray(a) !== Array.isArray(b)) return false;
            const ak = Object.keys(a);
            const bk = Object.keys(b);
            if (ak.length !== bk.length) return false;
            return ak.every((k) => deepEq(a[k], b[k]));
        }
        return false;
    }

    function fmt(v) {
        try { return JSON.stringify(v); } catch (e) { return String(v); }
    }

    function runtimeApi() {
        return (typeof browser !== "undefined" && browser.runtime) ? browser : chrome;
    }

    function lastError() {
        try { return runtimeApi().runtime.lastError; } catch (e) { return undefined; }
    }

    const implementations = window.__webextensions_v2_implementations__ =
        window.__webextensions_v2_implementations__ || {};

    // --- Result collection ---
    // Shared across the browser.test and chrome.test v2 projections.

    const results = [];
    let suiteName = "corpus";
    let onResult = null;
    let resolveSession;
    const session = new Promise((res) => { resolveSession = res; });
    let sessionDone = false;

    function finishSession() {
        if (sessionDone) return;
        sessionDone = true;
        if (typeof window !== "undefined") window.__testResults = results;
        resolveSession(results);
    }

    function pushResult(r) {
        results.push(r);
        if (onResult) {
            try { onResult(r); } catch (e) {}
        }
    }

    if (typeof window !== "undefined") {
        window.TestHarness = {
            session,
            results,
            setSuite(name) { suiteName = name; },
            set onResult(fn) { onResult = fn; },
            recordUncaught(message, stack) {
                if (current && !current.done) settle(current, false, message, stack);
                else pushResult({ suite: suiteName, name: "(load error)", pass: false,
                                  error: message, stack: stack || "", ms: 0 });
                finishSession();
            },
            finish: finishSession,
        };
    }

    // --- Test state (shared) ---

    let current = null;
    let inRunTests = false;

    function settle(c, pass, error, stack) {
        if (!c || c.done) return;
        c.done = true;
        pushResult({
            suite: suiteName,
            name: c.name,
            pass,
            error: error || "",
            stack: stack || "",
            ms: Math.round(performance.now() - c.start),
        });
        if (c.resolve) c.resolve();
    }

    function onCallbackFired(c) {
        if (!c || c.done) return;
        c.pending--;
        if (c.pending <= 0) settle(c, true, null);
    }

    // --- V2 JavaScript implementations ---

    const test = {};
    let invokeNative = null;

    function consume(operation) {
        if (operation && typeof operation.catch === "function") {
            operation.catch((error) => console.error("browser.test native call failed", error));
        }
    }

    function nativeResult(binding, message) {
        if (!invokeNative) throw new Error("browser.test native bridge is unavailable");
        const args = message === undefined ? {} : { message: String(message) };
        consume(invokeNative(binding, args));
    }

    const nativeNotifyPass = (message) => nativeResult("__notifyPass", message);
    const nativeNotifyFail = (message) => nativeResult("__notifyFail", message);

        test.log = (message) => {
            if (!invokeNative) throw new Error("browser.test native bridge is unavailable");
            consume(invokeNative("__log", { message: String(message) }));
        };

        test.succeed = (msg) => {
            settle(current, true, null);
            nativeNotifyPass(msg);
        };

        test.fail = (msg) => {
            settle(current, false, msg || "fail()");
            nativeNotifyFail(msg || "fail()");
        };

        test.notifyPass = (msg) => {
            if (current && !current.done) settle(current, true, null);
            else if (!inRunTests) pushResult({ suite: suiteName, name: "(script)", pass: true, error: "", ms: 0 });
            if (!inRunTests) finishSession();
            nativeNotifyPass(msg);
        };

        test.notifyFail = (msg) => {
            if (current && !current.done) settle(current, false, msg || "notifyFail()");
            else if (!inRunTests) pushResult({ suite: suiteName, name: "(script)", pass: false, error: msg || "notifyFail()", ms: 0 });
            if (!inRunTests) finishSession();
            nativeNotifyFail(msg || "notifyFail()");
        };

        // --- Assertions ---

        test.assertEq = (expected, actual, message) => {
            if (!deepEq(expected, actual)) {
                throw new Error(
                    `${message ? message + ": " : ""}assertEq: expected ${fmt(expected)}, got ${fmt(actual)}`
                );
            }
        };

        test.assertDeepEq = test.assertEq;

        test.checkDeepEq = (a, b) => deepEq(a, b);

        test.assertTrue = (cond, message) => {
            if (cond !== true && !cond) {
                throw new Error(`${message ? message + ": " : ""}assertTrue: got ${fmt(cond)}`);
            }
        };

        test.assertFalse = (cond, message) => {
            if (cond) {
                throw new Error(`${message ? message + ": " : ""}assertFalse: got ${fmt(cond)}`);
            }
        };

        test.assertNe = (expected, actual, message) => {
            if (deepEq(expected, actual)) {
                throw new Error(`${message ? message + ": " : ""}assertNe: both were ${fmt(actual)}`);
            }
        };

        test.assertNoLastError = () => {
            const e = lastError();
            if (e) throw new Error(`unexpected lastError: ${e.message}`);
        };

        test.assertLastError = (expected) => {
            const e = lastError();
            if (!e) throw new Error(`expected lastError ${fmt(expected)}, but none was set`);
            if (e.message !== expected)
                throw new Error(`expected lastError ${fmt(expected)}, got ${fmt(e.message)}`);
        };

        test.assertThrows = (fn, ...rest) => {
            let self;
            let args = [];
            let expected;
            for (const a of rest) {
                if (a instanceof RegExp || typeof a === "string") expected = a;
                else if (Array.isArray(a)) args = a;
                else if (a && typeof a === "object") self = a;
            }
            let threw = false;
            let message = "";
            try {
                fn.apply(self, args);
            } catch (e) {
                threw = true;
                message = (e && e.message) || String(e);
            }
            if (!threw) throw new Error("assertThrows: function did not throw");
            if (expected instanceof RegExp && !expected.test(message)) {
                throw new Error(`assertThrows: message ${fmt(message)} did not match ${expected}`);
            }
            if (typeof expected === "string" && message !== expected) {
                throw new Error(`assertThrows: expected ${fmt(expected)}, got ${fmt(message)}`);
            }
        };

        test.assertRejects = async (promise, expected, message) => {
            let rejected = false;
            let msg = "";
            try {
                await promise;
            } catch (e) {
                rejected = true;
                msg = (e && e.message) || String(e);
            }
            if (!rejected)
                throw new Error(`${message ? message + ": " : ""}assertRejects: promise did not reject`);
            if (expected instanceof RegExp && !expected.test(msg)) {
                throw new Error(
                    `${message ? message + ": " : ""}assertRejects: message ${fmt(msg)} did not match ${expected}`
                );
            }
            if (typeof expected === "string" && msg !== expected) {
                throw new Error(
                    `${message ? message + ": " : ""}assertRejects: expected ${fmt(expected)}, got ${fmt(msg)}`
                );
            }
        };

        test.assertResolves = async (promise, message) => {
            try {
                await promise;
            } catch (e) {
                throw new Error(
                    `${message ? message + ": " : ""}assertResolves: promise rejected: ${(e && e.message) || e}`
                );
            }
        };

        test.assertSafe = (fn, message) => {
            try {
                return fn();
            } catch (e) {
                throw new Error(
                    `${message ? message + ": " : ""}assertSafe: function threw: ${(e && e.message) || e}`
                );
            }
        };

        test.assertSafeResolve = async (fn, message) => {
            let result;
            try {
                result = fn();
            } catch (e) {
                throw new Error(
                    `${message ? message + ": " : ""}assertSafeResolve: function threw: ${(e && e.message) || e}`
                );
            }
            try {
                await result;
            } catch (e) {
                throw new Error(
                    `${message ? message + ": " : ""}assertSafeResolve: promise rejected: ${(e && e.message) || e}`
                );
            }
        };

        // --- Callback wrappers ---

        test.callbackPass = (fn) => {
            const c = current;
            if (c) c.pending++;
            return function (...a) {
                try {
                    const e = lastError();
                    if (e) throw new Error(`unexpected lastError: ${e.message}`);
                    if (fn) fn.apply(this, a);
                } catch (err) {
                    settle(c, false, (err && err.message) || String(err), err && err.stack);
                    return;
                }
                onCallbackFired(c);
            };
        };

        test.callbackFail = (expectedError, fn) => {
            const c = current;
            if (c) c.pending++;
            return function (...a) {
                try {
                    const e = lastError();
                    if (!e) throw new Error(`expected lastError ${fmt(expectedError)}, but none was set`);
                    if (e.message !== expectedError)
                        throw new Error(`expected lastError ${fmt(expectedError)}, got ${fmt(e.message)}`);
                    if (fn) fn.apply(this, a);
                } catch (err) {
                    settle(c, false, (err && err.message) || String(err), err && err.stack);
                    return;
                }
                onCallbackFired(c);
            };
        };

        test.listenOnce = (event, fn) => {
            const c = current;
            if (c) c.pending++;
            const listener = function (...a) {
                try { event.removeListener(listener); } catch (e) {}
                try {
                    if (fn) fn.apply(this, a);
                } catch (err) {
                    settle(c, false, (err && err.message) || String(err), err && err.stack);
                    return;
                }
                onCallbackFired(c);
            };
            event.addListener(listener);
        };

        // --- Runner ---

        test.runTests = async (tests) => {
            if (!tests || tests.length === 0) {
                pushResult({
                    suite: suiteName,
                    name: "(no tests ran)",
                    pass: false,
                    error: "runTests received an empty list — suite selects tests via getConfig().customArg or a native harness",
                    ms: 0,
                });
                finishSession();
                return;
            }
            let index = 0;
            inRunTests = true;
            for (const t of tests) {
                index++;
                const name = t.name || `test #${index}`;
                current = { name, pending: 0, done: false, start: performance.now(), resolve: null };
                const done = new Promise((res) => { current.resolve = res; });
                const c = current;
                const watchdog = setTimeout(() => {
                    settle(c, false, "test timed out after 8s");
                    nativeNotifyFail(`${name}: timed out after 8s`);
                }, 8000);
                try {
                    const ret = t();
                    if (ret && typeof ret.then === "function") {
                        ret.catch(() => {});
                        await Promise.race([ret, done]);
                    }
                } catch (err) {
                    settle(c, false, (err && err.message) || String(err), err && err.stack);
                }
                if (!c.done && c.pending <= 0) {
                    settle(c, true, null);
                }
                await done;
                clearTimeout(watchdog);
            }
            inRunTests = false;
            finishSession();
        };

        test.addTest = (fn) => {
            return new Promise((resolve, reject) => {
                const c = { name: fn.name || "addTest", pending: 0, done: false, start: performance.now(), resolve: null };
                const prev = current;
                current = c;
                const done = new Promise((res) => { c.resolve = res; });
                const watchdog = setTimeout(() => {
                    settle(c, false, "test timed out after 8s");
                    reject(new Error(`${c.name}: timed out after 8s`));
                }, 8000);
                try {
                    const ret = fn();
                    if (ret && typeof ret.then === "function") {
                        ret.then(() => {
                            if (!c.done) { settle(c, true, null); resolve(); }
                        }).catch((err) => {
                            settle(c, false, (err && err.message) || String(err));
                            reject(err);
                        });
                    } else {
                        if (!c.done && c.pending <= 0) { settle(c, true, null); resolve(); }
                    }
                } catch (err) {
                    settle(c, false, (err && err.message) || String(err));
                    reject(err);
                }
                done.then(() => { clearTimeout(watchdog); current = prev; });
            });
        };

        // --- Stubs ---

        test.getConfig = (cb) => { cb({ testServer: { port: 0 }, customArg: "" }); };
        test.testCallback = () => {};

    const argumentsByImplementation = {
        notifyPass: ["message"],
        notifyFail: ["message"],
        log: ["message"],
        runTests: ["tests"],
        addTest: ["test"],
        succeed: ["message"],
        fail: ["message"],
        assertEq: ["expected", "actual", "message"],
        assertNe: ["expected", "actual", "message"],
        assertTrue: ["condition", "message"],
        assertFalse: ["condition", "message"],
        assertDeepEq: ["expected", "actual", "message"],
        assertThrows: ["test", "argument1", "argument2", "argument3"],
        assertRejects: ["promise", "expected", "message"],
        assertResolves: ["promise", "message"],
        assertSafe: ["test", "message"],
        assertSafeResolve: ["test", "message"],
        assertNoLastError: [],
        assertLastError: ["expected"],
        callbackPass: ["callback"],
        callbackFail: ["expectedError", "callback"],
        listenOnce: ["event", "callback"],
        getConfig: ["callback"],
        testCallback: ["callback"],
        checkDeepEq: ["expected", "actual"],
    };

    for (const [name, argumentNames] of Object.entries(argumentsByImplementation)) {
        implementations[`Test.${name}`] = (context) => {
            invokeNative = context.invokeNative;
            return test[name](...argumentNames.map((argument) => context.args[argument]));
        };
    }
})();
