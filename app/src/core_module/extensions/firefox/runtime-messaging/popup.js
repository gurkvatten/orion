const output = document.querySelector("#console-output");
const openPorts = new Set();

const levelForMethod = {
  debug: "debug",
  info: "info",
  warn: "warn",
  error: "error"
};

function formatArgument(value) {
  if (typeof value === "string") {
    return value;
  }

  if (value && typeof value === "object" && (value.stack || value.message)) {
    const message = value.message || String(value);
    const stack = value.stack || "";
    return stack.includes(message) ? stack : `${message}\n${stack}`;
  }

  try {
    const serialized = JSON.stringify(value, null, 2);
    return serialized === undefined ? String(value) : serialized;
  } catch (error) {
    return String(value);
  }
}

function appendConsoleLine(method, args) {
  const level = levelForMethod[method] || "log";
  const line = document.createElement("div");
  const time = document.createElement("span");
  const label = document.createElement("span");
  const message = document.createElement("span");

  line.className = `console-line console-${level}`;
  time.className = "console-time";
  label.className = "console-level";
  message.className = "console-message";

  time.textContent = new Date().toLocaleTimeString();
  label.textContent = method;
  message.textContent = args.map(formatArgument).join(" ");

  line.append(time, label, message);
  output.append(line);
  window.scrollTo(0, document.documentElement.scrollHeight);
}

function hookConsole() {
  const messageMethods = ["debug", "log", "info", "warn", "error"];

  for (const method of messageMethods) {
    if (typeof console[method] !== "function") {
      continue;
    }

    const nativeMethod = console[method].bind(console);
    console[method] = (...args) => {
      nativeMethod(...args);
      appendConsoleLine(method, args);
    };
  }
}

function requestId(prefix) {
  return `${prefix}-${Date.now()}-${Math.random().toString(16).slice(2)}`;
}

function runSafely(method, operation) {
  try {
    operation();
  } catch (error) {
    console.error(`[popup] browser.runtime.${method} threw`, error);
  }
}

function expectVoid(method, result) {
  if (result !== undefined) {
    console.error(`[popup] ${method} must return undefined`, result);
  }
}

function makeMessage(method, prefix) {
  return {
    target: "background",
    type: "send-message-request",
    method,
    requestId: requestId(prefix)
  };
}

function sendMessageWithPromise(method, args) {
  console.log(`[popup] browser.runtime.${method}`, args);
  const result = browser.runtime.sendMessage(...args);

  if (!result || typeof result.then !== "function") {
    console.error(`[popup] browser.runtime.${method} must return a Promise`, result);
    return;
  }

  result.then((response) => {
    console.info(`[popup] browser.runtime.${method} response`, response);
  }, (error) => {
    console.error(`[popup] browser.runtime.${method} rejected`, error);
  });
}

function exercisePort(method, port, expectedName) {
  if (!port || typeof port.postMessage !== "function" || typeof port.disconnect !== "function") {
    console.error(`[popup] browser.runtime.${method} did not return a Port`, port);
    return;
  }

  if (port.name !== expectedName) {
    console.error(`[popup] browser.runtime.${method} returned an incorrect Port.name`, {
      actual: port.name,
      expected: expectedName
    });
  }

  const id = requestId("port");
  openPorts.add(port);
  console.log(`[popup] browser.runtime.${method}`, {
    name: port.name,
    requestId: id
  });

  port.onMessage.addListener((message) => {
    console.info(`[popup] Port.onMessage (${port.name})`, message);

    if (message.type === "port-response" && message.requestId === id) {
      console.debug(`[popup] Port.disconnect (${port.name})`);
      expectVoid("Port.disconnect()", port.disconnect());
    }
  });

  port.onDisconnect.addListener((disconnectedPort) => {
    openPorts.delete(port);

    if (disconnectedPort !== port) {
      console.error(`[popup] Port.onDisconnect (${port.name}) received an incorrect Port`, disconnectedPort);
    }

    if (!disconnectedPort) {
      return;
    }

    if (disconnectedPort.error) {
      console.error(`[popup] Port.onDisconnect (${port.name})`, disconnectedPort.error.message);
      return;
    }

    console.warn(`[popup] Port.onDisconnect (${port.name})`);
  });

  expectVoid(`Port.postMessage() from ${method}`, port.postMessage({
    target: "background",
    type: "port-request",
    method,
    requestId: id
  }));
}

function runSendMessageTests() {
  runSafely("sendMessage(message)", () => {
    const message = makeMessage("sendMessage(message)", "promise-internal");
    sendMessageWithPromise("sendMessage(message)", [message]);
  });

  runSafely("sendMessage(message, options)", () => {
    const message = makeMessage("sendMessage(message, options)", "promise-options");
    sendMessageWithPromise("sendMessage(message, options)", [
      message,
      {}
    ]);
  });

  runSafely("sendMessage(extensionId, message)", () => {
    const message = makeMessage("sendMessage(extensionId, message)", "promise-external");
    sendMessageWithPromise("sendMessage(extensionId, message)", [
      browser.runtime.id,
      message
    ]);
  });

  runSafely("sendMessage(extensionId, message, options)", () => {
    const message = makeMessage(
      "sendMessage(extensionId, message, options)",
      "promise-external-options"
    );
    sendMessageWithPromise("sendMessage(extensionId, message, options)", [
      browser.runtime.id,
      message,
      {}
    ]);
  });
}

function runConnectTests() {
  runSafely("connect()", () => {
    exercisePort("connect()", browser.runtime.connect(), "");
  });
  runSafely("connect(connectInfo)", () => {
    exercisePort("connect(connectInfo)", browser.runtime.connect({
      name: "connect-info"
    }), "connect-info");
  });
  runSafely("connect(extensionId)", () => {
    exercisePort("connect(extensionId)", browser.runtime.connect(browser.runtime.id), "");
  });
  runSafely("connect(extensionId, connectInfo)", () => {
    exercisePort("connect(extensionId, connectInfo)", browser.runtime.connect(browser.runtime.id, {
      name: "connect-extension-id"
    }), "connect-extension-id");
  });
}

function runTests() {
  console.info("[popup] running browser.runtime messaging tests", {
    extensionId: browser.runtime.id
  });
  runSendMessageTests();
  runConnectTests();
}

hookConsole();
console.log("[popup] URI", window.location.href);

window.addEventListener("error", (event) => {
  console.error("[popup] uncaught error", event.error || event.message);
});

window.addEventListener("unhandledrejection", (event) => {
  console.error("[popup] unhandled promise rejection", event.reason);
});

browser.runtime.onMessage.addListener((message, sender) => {
  console.info("[popup] browser.runtime.onMessage", {
    message,
    sender: {
      id: sender.id,
      url: sender.url
    }
  });
});

document.querySelector("#run-tests").addEventListener("click", runTests);
document.querySelector("#clear-console").addEventListener("click", () => {
  output.replaceChildren();
});

runTests();
