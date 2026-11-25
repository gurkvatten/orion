"use strict";

TestRunner.suite("storage.local", (test) => {
    test("set and get single value", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { key1: "value1" });
        const result = await call(api.storage.local, "get", "key1");
        assertEqual(result.key1, "value1");
    });

    test("set and get multiple values", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { a: 1, b: "two", c: true });
        const result = await call(api.storage.local, "get", ["a", "b", "c"]);
        assertEqual(result.a, 1);
        assertEqual(result.b, "two");
        assertEqual(result.c, true);
    });

    test("get with default values for missing key", async () => {
        await call(api.storage.local, "clear");
        const result = await call(api.storage.local, "get", { missing: "default_val" });
        assertEqual(result.missing, "default_val");
    });

    test("get null returns all", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { x: 10, y: 20 });
        const result = await call(api.storage.local, "get", null);
        assertEqual(result.x, 10);
        assertEqual(result.y, 20);
    });

    test("getKeys", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { alpha: 1, beta: 2 });
        const keys = await call(api.storage.local, "getKeys");
        assert(Array.isArray(keys), "getKeys should return an array");
        assertEqual(keys.length, 2, "should have 2 keys");
        assertContains(keys, "alpha");
        assertContains(keys, "beta");
    });

    test("remove single key", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { keep: 1, remove_me: 2 });
        await call(api.storage.local, "remove", "remove_me");
        const result = await call(api.storage.local, "get", ["keep", "remove_me"]);
        assertEqual(result.keep, 1);
        assertEqual(result.remove_me, undefined, "removed key");
    });

    test("remove array of keys", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { a: 1, b: 2, c: 3 });
        await call(api.storage.local, "remove", ["a", "b"]);
        const result = await call(api.storage.local, "get", ["a", "b", "c"]);
        assertEqual(result.a, undefined);
        assertEqual(result.b, undefined);
        assertEqual(result.c, 3);
    });

    test("clear removes everything", async () => {
        await call(api.storage.local, "set", { temp1: "a", temp2: "b" });
        await call(api.storage.local, "clear");
        const keys = await call(api.storage.local, "getKeys");
        assertEqual(keys.length, 0, "should have 0 keys after clear");
    });

    test("overwrite existing key", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.local, "set", { key: "old" });
        await call(api.storage.local, "set", { key: "new" });
        const result = await call(api.storage.local, "get", "key");
        assertEqual(result.key, "new");
    });

    test("stores object values", async () => {
        await call(api.storage.local, "clear");
        const obj = { nested: { deep: true }, arr: [1, 2, 3] };
        await call(api.storage.local, "set", { complex: obj });
        const result = await call(api.storage.local, "get", "complex");
        assertEqual(JSON.stringify(result.complex), JSON.stringify(obj));
    });
});

TestRunner.suite("storage.session", (test) => {
    test("set and get", async () => {
        await call(api.storage.session, "clear");
        await call(api.storage.session, "set", { sess_key: "sess_val" });
        const result = await call(api.storage.session, "get", "sess_key");
        assertEqual(result.sess_key, "sess_val");
    });

    test("clear", async () => {
        await call(api.storage.session, "set", { tmp: 42 });
        await call(api.storage.session, "clear");
        const keys = await call(api.storage.session, "getKeys");
        assertEqual(keys.length, 0);
    });

    test("session and local are independent", async () => {
        await call(api.storage.local, "clear");
        await call(api.storage.session, "clear");
        await call(api.storage.local, "set", { shared_name: "local_val" });
        await call(api.storage.session, "set", { shared_name: "session_val" });
        const localResult = await call(api.storage.local, "get", "shared_name");
        const sessionResult = await call(api.storage.session, "get", "shared_name");
        assertEqual(localResult.shared_name, "local_val");
        assertEqual(sessionResult.shared_name, "session_val");
    });
});
