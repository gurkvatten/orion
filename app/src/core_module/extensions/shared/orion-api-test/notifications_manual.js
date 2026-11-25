"use strict";

// Manual notifications test page: builds NotificationOptions from the form,
// calls notifications.* on the flavor's root (browser.* in the Firefox flavor,
// chrome.* with callbacks in the Chrome flavor; see lib/api.js) and logs every
// result and every event the API raises, so clicking a toast (or its buttons)
// on the desktop can be matched against the callbacks the extension receives.

document.addEventListener("DOMContentLoaded", () => {
    const env = window.OrionEnv;
    const notifications = api.notifications;

    document.getElementById("env").innerHTML =
        `flavor <code>${env.flavor}</code> · scheme <code>${env.scheme}</code> · ` +
        `manifest v<code>${env.manifestVersion}</code>`;

    const logBody = document.querySelector("#log tbody");
    const lastIdEl = document.getElementById("last-id");
    let lastId = "";

    function log(kind, what, data) {
        const tr = document.createElement("tr");
        tr.className = kind;
        const when = document.createElement("td");
        when.className = "when";
        when.textContent = new Date().toLocaleTimeString([], { hour12: false }) +
            "." + String(new Date().getMilliseconds()).padStart(3, "0");
        const kindTd = document.createElement("td");
        kindTd.className = "kind";
        kindTd.textContent = what;
        const details = document.createElement("td");
        details.className = "what";
        details.textContent = data === undefined ? "" :
            (typeof data === "string" ? data : JSON.stringify(data, null, 1));
        tr.append(when, kindTd, details);
        logBody.insertBefore(tr, logBody.firstChild);
    }

    if (!notifications) {
        log("error", `${OrionApi.isChrome ? "chrome" : "browser"}.notifications`, "not available — is the 'notifications' permission in the manifest?");
        document.querySelectorAll(".actions button").forEach((b) => (b.disabled = true));
        return;
    }

    // ----- events -----------------------------------------------------------
    for (const name of ["onShown", "onClicked", "onButtonClicked", "onClosed"]) {
        const event = notifications[name];
        if (!event || typeof event.addListener !== "function") {
            log("call", name, `not exposed in the ${env.flavor} flavor`);
            continue;
        }
        event.addListener((...args) => log("event", name, args));
    }

    // ----- option building --------------------------------------------------
    // A generated swatch: exercises the data: URL path of iconUrl (the core
    // writes it to a cache file before the host sees it).
    function swatchDataUrl() {
        const canvas = document.createElement("canvas");
        canvas.width = canvas.height = 64;
        const ctx = canvas.getContext("2d");
        ctx.fillStyle = "#1d3557";
        ctx.beginPath();
        ctx.roundRect(4, 4, 56, 56, 12);
        ctx.fill();
        ctx.fillStyle = "#f1faee";
        ctx.font = "bold 16px system-ui, sans-serif";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.fillText("data:", 32, 33);
        return canvas.toDataURL("image/png");
    }

    function bannerDataUrl() {
        const canvas = document.createElement("canvas");
        canvas.width = 364;
        canvas.height = 180;
        const ctx = canvas.getContext("2d");
        const gradient = ctx.createLinearGradient(0, 0, 364, 180);
        gradient.addColorStop(0, "#1d3557");
        gradient.addColorStop(1, "#457b9d");
        ctx.fillStyle = gradient;
        ctx.fillRect(0, 0, 364, 180);
        ctx.fillStyle = "#f1faee";
        ctx.font = "bold 28px system-ui, sans-serif";
        ctx.fillText("image notification", 40, 100);
        return canvas.toDataURL("image/png");
    }

    const value = (id) => document.getElementById(id).value;
    const checked = (id) => document.getElementById(id).checked;

    function buildOptions() {
        const type = value("opt-type");
        const options = { type, title: value("opt-title"), message: value("opt-message") };
        // "package": a path inside the extension (resolved by the core from the
        // package); "data": an inline image; "none": no iconUrl at all.
        const icon = value("opt-icon");
        if (icon === "package") options.iconUrl = "icons/orion-64.png";
        else if (icon === "data") options.iconUrl = swatchDataUrl();
        if (value("opt-context")) options.contextMessage = value("opt-context");
        const buttonCount = Number(value("opt-buttons"));
        if (buttonCount > 0) {
            options.buttons = [{ title: "Open" }, { title: "Dismiss" }].slice(0, buttonCount);
        }
        if (type === "image") options.imageUrl = bannerDataUrl();
        if (type === "list") {
            options.items = [
                { title: "First", message: "one" },
                { title: "Second", message: "two" },
                { title: "Third", message: "three" },
            ];
        }
        if (type === "progress") options.progress = Number(value("opt-progress"));
        const priority = Number(value("opt-priority"));
        if (priority !== 0) options.priority = priority;
        if (checked("opt-require")) options.requireInteraction = true;
        if (checked("opt-silent")) options.silent = true;
        if (checked("opt-eventtime")) options.eventTime = Date.now();
        return options;
    }

    function summarize(options) {
        // Keep the log readable: data: URLs are long.
        const copy = { ...options };
        for (const key of ["iconUrl", "imageUrl"])
            if (typeof copy[key] === "string" && copy[key].startsWith("data:"))
                copy[key] = `data:… (${copy[key].length} chars)`;
        return copy;
    }

    function setLastId(id) {
        lastId = id || "";
        lastIdEl.textContent = lastId || "—";
    }

    async function logged(label, fn) {
        try {
            const result = await fn();
            log("call", label, result);
            return result;
        } catch (e) {
            log("error", label + " rejected", (e && e.message) || String(e));
            return undefined;
        }
    }

    // ----- actions ----------------------------------------------------------
    document.getElementById("btn-create").addEventListener("click", async () => {
        const options = buildOptions();
        const requestedId = value("opt-id").trim();
        log("call", "create(...)", { id: requestedId || "(generated)", options: summarize(options) });
        const id = await logged("create → id", () =>
            requestedId ? call(notifications, "create", requestedId, options) : call(notifications, "create", options));
        if (typeof id === "string") setLastId(id);
    });

    const updateButton = document.getElementById("btn-update");
    if (typeof notifications.update !== "function") {
        updateButton.disabled = true;
        updateButton.title = `update() is not exposed in the ${env.flavor} flavor`;
    }
    updateButton.addEventListener("click", async () => {
        if (!lastId) return log("error", "update", "create a notification first");
        const options = buildOptions();
        log("call", `update(${JSON.stringify(lastId)}, ...)`, summarize(options));
        await logged("update → ok", () => call(notifications, "update", lastId, options));
    });

    document.getElementById("btn-clear").addEventListener("click", async () => {
        if (!lastId) return log("error", "clear", "create a notification first");
        await logged(`clear(${JSON.stringify(lastId)}) → ok`, () => call(notifications, "clear", lastId));
    });

    document.getElementById("btn-getall").addEventListener("click", async () => {
        await logged("getAll()", () => call(notifications, "getAll"));
    });

    const permissionButton = document.getElementById("btn-permission");
    if (typeof notifications.getPermissionLevel !== "function") {
        permissionButton.disabled = true;
        permissionButton.title = `getPermissionLevel() is not exposed in the ${env.flavor} flavor`;
    }
    permissionButton.addEventListener("click", async () => {
        await logged("getPermissionLevel()", () => call(notifications, "getPermissionLevel"));
    });

    document.getElementById("btn-clearlog").addEventListener("click", () => {
        logBody.textContent = "";
    });

    log("call", "ready", `listening for notification events (${env.flavor} flavor, ${OrionApi.isChrome ? "chrome.* callbacks" : "browser.* promises"})`);
});
