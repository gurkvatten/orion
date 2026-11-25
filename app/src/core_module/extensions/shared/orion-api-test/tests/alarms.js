"use strict";

TestRunner.suite("alarms", (test) => {
    test("create and get alarm", async () => {
        await call(api.alarms, "clearAll");
        await call(api.alarms, "create", "test-alarm", { delayInMinutes: 60 });
        const alarm = await call(api.alarms, "get", "test-alarm");
        assert(alarm !== null && alarm !== undefined, "alarm should exist");
        assertEqual(alarm.name, "test-alarm");
    });

    test("getAll returns created alarms", async () => {
        await call(api.alarms, "clearAll");
        await call(api.alarms, "create", "alarm-a", { delayInMinutes: 60 });
        await call(api.alarms, "create", "alarm-b", { delayInMinutes: 120 });
        const all = await call(api.alarms, "getAll");
        assert(Array.isArray(all), "getAll should return an array");
        assertEqual(all.length, 2, "should have 2 alarms");
        const names = all.map(a => a.name).sort();
        assertEqual(names[0], "alarm-a");
        assertEqual(names[1], "alarm-b");
    });

    test("get nonexistent alarm returns undefined/null", async () => {
        await call(api.alarms, "clearAll");
        const alarm = await call(api.alarms, "get", "does-not-exist");
        assert(
            alarm === null || alarm === undefined,
            `expected null/undefined, got ${JSON.stringify(alarm)}`
        );
    });

    test("clear specific alarm", async () => {
        await call(api.alarms, "clearAll");
        await call(api.alarms, "create", "keep-me", { delayInMinutes: 60 });
        await call(api.alarms, "create", "remove-me", { delayInMinutes: 60 });
        const cleared = await call(api.alarms, "clear", "remove-me");
        assert(cleared === true || cleared === undefined, "clear should succeed");
        const all = await call(api.alarms, "getAll");
        assertEqual(all.length, 1, "one alarm remaining");
        assertEqual(all[0].name, "keep-me");
    });

    test("clearAll removes everything", async () => {
        await call(api.alarms, "create", "temp1", { delayInMinutes: 60 });
        await call(api.alarms, "create", "temp2", { delayInMinutes: 60 });
        await call(api.alarms, "clearAll");
        const all = await call(api.alarms, "getAll");
        assertEqual(all.length, 0, "should be empty after clearAll");
    });

    test("alarm has scheduledTime", async () => {
        await call(api.alarms, "clearAll");
        const before = Date.now();
        await call(api.alarms, "create", "timed", { delayInMinutes: 1 });
        const alarm = await call(api.alarms, "get", "timed");
        assert(alarm !== null && alarm !== undefined, "alarm should exist");
        assertType(alarm.scheduledTime, "number", "scheduledTime");
        assert(alarm.scheduledTime >= before, "scheduledTime should be in the future");
    });

    test("create with periodInMinutes", async () => {
        await call(api.alarms, "clearAll");
        await call(api.alarms, "create", "periodic", {
            delayInMinutes: 1,
            periodInMinutes: 5,
        });
        const alarm = await call(api.alarms, "get", "periodic");
        assert(alarm !== null && alarm !== undefined, "alarm should exist");
        assertEqual(alarm.name, "periodic");
    });
});
