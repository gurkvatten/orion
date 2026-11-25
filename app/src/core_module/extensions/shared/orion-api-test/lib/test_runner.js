"use strict";

const TestRunner = {
    _suites: [],

    suite(name, fn) {
        const suite = { name, tests: [] };
        fn((testName, testFn) => suite.tests.push({ name: testName, fn: testFn }));
        this._suites.push(suite);
    },

    async run() {
        const results = [];
        for (const suite of this._suites) {
            for (const test of suite.tests) {
                const entry = { suite: suite.name, name: test.name };
                const start = performance.now();
                try {
                    await test.fn();
                    entry.pass = true;
                } catch (e) {
                    entry.pass = false;
                    entry.error = e.message || String(e);
                }
                entry.ms = Math.round(performance.now() - start);
                results.push(entry);
            }
        }
        return results;
    }
};

function assert(condition, msg) {
    if (!condition) throw new Error(msg || "Assertion failed");
}

function assertEqual(actual, expected, msg) {
    if (actual !== expected) {
        const prefix = msg ? msg + ": " : "";
        throw new Error(
            `${prefix}expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`
        );
    }
}

function assertType(value, type, msg) {
    if (typeof value !== type) {
        const prefix = msg ? msg + ": " : "";
        throw new Error(
            `${prefix}expected type ${type}, got ${typeof value} (${JSON.stringify(value)})`
        );
    }
}

function assertContains(arr, item, msg) {
    if (!Array.isArray(arr) || !arr.includes(item)) {
        const prefix = msg ? msg + ": " : "";
        throw new Error(
            `${prefix}expected ${JSON.stringify(arr)} to contain ${JSON.stringify(item)}`
        );
    }
}
