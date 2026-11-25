"use strict";

// notifications — automated suite.
//
// Deliberately never calls create(): that would put a real toast on the
// desktop during every test run. This suite covers the API surface and the
// calls that have no visible effect; notifications_manual.html (linked from
// the popup) exercises create / update / clear and the click callbacks
// interactively.
//
// Flavor notes: update(), getPermissionLevel() and onButtonClicked are
// Chrome-only in the IDL ([Tags=Chrome]); onShown is Firefox-only.

TestRunner.suite("notifications", (test) => {
    const settle = (promise, label) => Promise.race([
        promise,
        new Promise((_, reject) => setTimeout(
            () => reject(new Error(`TIMEOUT: ${label} did not settle within 3000ms`)), 3000)),
    ]);

    test("namespace exists (manifest permission 'notifications')", async () => {
        assert(api.notifications, "notifications should be defined on the flavor's root");
        for (const fn of ["create", "clear", "getAll"])
            assertType(api.notifications[fn], "function", `notifications.${fn}`);
        for (const ev of ["onClicked", "onClosed"]) {
            assert(api.notifications[ev], `notifications.${ev} should be defined`);
            assertType(api.notifications[ev].addListener, "function", `notifications.${ev}.addListener`);
            assertType(api.notifications[ev].removeListener, "function", `notifications.${ev}.removeListener`);
        }
    });

    test("the other root also exposes the namespace", async () => {
        // Orion installs both roots in every flavor; the suites use the
        // flavor-native one, this only checks the mirror is present.
        const other = OrionApi.isChrome
            ? (typeof browser !== "undefined" ? browser : undefined)
            : (typeof chrome !== "undefined" ? chrome : undefined);
        const name = OrionApi.isChrome ? "browser" : "chrome";
        assert(other && other.notifications, `${name}.notifications should be defined`);
        assertType(other.notifications.create, "function", `${name}.notifications.create`);
        assertType(other.notifications.clear, "function", `${name}.notifications.clear`);
    });

    test("getAll resolves an id -> true map", async () => {
        const all = await settle(call(api.notifications, "getAll"), "getAll");
        assert(all !== null && typeof all === "object" && !Array.isArray(all),
            `expected a plain object, got ${JSON.stringify(all)}`);
        for (const [id, value] of Object.entries(all))
            assertEqual(value, true, `getAll()[${JSON.stringify(id)}]`);
    });

    test("clear of an unknown id resolves false", async () => {
        const ok = await settle(call(api.notifications, "clear", "orion-test-does-not-exist"), "clear");
        assertEqual(ok, false);
    });

    test("update of an unknown id resolves false (Chrome API)", async () => {
        if (typeof api.notifications.update !== "function") return; // not in the Firefox flavor
        const ok = await settle(
            call(api.notifications, "update", "orion-test-does-not-exist", { title: "unused" }), "update");
        assertEqual(ok, false);
    });

    test("getPermissionLevel resolves 'granted' (Chrome API)", async () => {
        if (typeof api.notifications.getPermissionLevel !== "function") return;
        const level = await settle(call(api.notifications, "getPermissionLevel"), "getPermissionLevel");
        assertEqual(level, "granted");
    });

    test("listeners can be added and removed without a notification", async () => {
        const listener = () => {};
        api.notifications.onClicked.addListener(listener);
        api.notifications.onClosed.addListener(listener);
        api.notifications.onClicked.removeListener(listener);
        api.notifications.onClosed.removeListener(listener);
    });

    test("flavor-specific members match the build", async () => {
        const n = api.notifications;
        if (OrionApi.isChrome) {
            assertType(n.update, "function", "chrome flavor: update()");
            assertType(n.getPermissionLevel, "function", "chrome flavor: getPermissionLevel()");
            assert(n.onButtonClicked, "chrome flavor: onButtonClicked should be defined");
        } else {
            assert(n.onShown, "firefox flavor: onShown should be defined");
        }
    });
});
