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
    console.error(`[popup] chrome.runtime.${method} threw`, error);
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

function sendMessageWithCallback(method, args) {
  console.log(`[popup] chrome.runtime.${method}`, args);
  const result = chrome.runtime.sendMessage(...args, (response) => {
    if (chrome.runtime.lastError) {
      console.error(`[popup] chrome.runtime.${method} failed`, chrome.runtime.lastError.message);
      return;
    }

    console.info(`[popup] chrome.runtime.${method} response`, response);
  });
  expectVoid(`chrome.runtime.${method}`, result);
}

function sendMessageWithPromise(method, args) {
  console.log(`[popup] chrome.runtime.${method}`, args);
  const result = chrome.runtime.sendMessage(...args);

  if (!result || typeof result.then !== "function") {
    console.error(`[popup] chrome.runtime.${method} must return a Promise`, result);
    return;
  }

  result.then((response) => {
    console.info(`[popup] chrome.runtime.${method} response`, response);
  }, (error) => {
    console.error(`[popup] chrome.runtime.${method} rejected`, error);
  });
}

function exercisePort(method, port, expectedName) {
  if (!port || typeof port.postMessage !== "function" || typeof port.disconnect !== "function") {
    console.error(`[popup] chrome.runtime.${method} did not return a Port`, port);
    return;
  }

  if (port.name !== expectedName) {
    console.error(`[popup] chrome.runtime.${method} returned an incorrect Port.name`, {
      actual: port.name,
      expected: expectedName
    });
  }

  const id = requestId("port");
  openPorts.add(port);
  console.log(`[popup] chrome.runtime.${method}`, {
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

  port.onDisconnect.addListener(() => {
    openPorts.delete(port);

    if (chrome.runtime.lastError) {
      console.error(`[popup] Port.onDisconnect (${port.name})`, chrome.runtime.lastError.message);
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
  runSafely("sendMessage(message, callback)", () => {
    const message = makeMessage("sendMessage(message, callback)", "callback-internal");
    sendMessageWithCallback("sendMessage(message, callback)", [message]);
  });

  runSafely("sendMessage(message, options, callback)", () => {
    const message = makeMessage("sendMessage(message, options, callback)", "callback-options");
    sendMessageWithCallback("sendMessage(message, options, callback)", [
      message,
      { includeTlsChannelId: false }
    ]);
  });

  runSafely("sendMessage(extensionId, message, callback)", () => {
    const message = makeMessage("sendMessage(extensionId, message, callback)", "callback-external");
    sendMessageWithCallback("sendMessage(extensionId, message, callback)", [
      chrome.runtime.id,
      message
    ]);
  });

  runSafely("sendMessage(extensionId, message, options, callback)", () => {
    const message = makeMessage(
      "sendMessage(extensionId, message, options, callback)",
      "callback-external-options"
    );
    sendMessageWithCallback("sendMessage(extensionId, message, options, callback)", [
      chrome.runtime.id,
      message,
      { includeTlsChannelId: false }
    ]);
  });

  runSafely("sendMessage(message)", () => {
    const message = makeMessage("sendMessage(message)", "promise-internal");
    sendMessageWithPromise("sendMessage(message)", [message]);
  });

  runSafely("sendMessage(message, options)", () => {
    const message = makeMessage("sendMessage(message, options)", "promise-options");
    sendMessageWithPromise("sendMessage(message, options)", [
      message,
      { includeTlsChannelId: false }
    ]);
  });

  runSafely("sendMessage(extensionId, message)", () => {
    const message = makeMessage("sendMessage(extensionId, message)", "promise-external");
    sendMessageWithPromise("sendMessage(extensionId, message)", [
      chrome.runtime.id,
      message
    ]);
  });

  runSafely("sendMessage(extensionId, message, options)", () => {
    const message = makeMessage(
      "sendMessage(extensionId, message, options)",
      "promise-external-options"
    );
    sendMessageWithPromise("sendMessage(extensionId, message, options)", [
      chrome.runtime.id,
      message,
      { includeTlsChannelId: false }
    ]);
  });
}

function runConnectTests() {
  runSafely("connect()", () => {
    exercisePort("connect()", chrome.runtime.connect(), "");
  });
  runSafely("connect(connectInfo)", () => {
    exercisePort("connect(connectInfo)", chrome.runtime.connect({
      name: "connect-info"
    }), "connect-info");
  });
  runSafely("connect(extensionId)", () => {
    exercisePort("connect(extensionId)", chrome.runtime.connect(chrome.runtime.id), "");
  });
  runSafely("connect(extensionId, connectInfo)", () => {
    exercisePort("connect(extensionId, connectInfo)", chrome.runtime.connect(chrome.runtime.id, {
      name: "connect-extension-id"
    }), "connect-extension-id");
  });
}

function runTests() {
  console.info("[popup] running chrome.runtime messaging tests", {
    extensionId: chrome.runtime.id
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

chrome.runtime.onMessage.addListener((message, sender) => {
  console.info("[popup] chrome.runtime.onMessage", {
    message,
    sender: {
      id: sender.id,
      url: sender.url
    }
  });
  return false;
});

document.querySelector("#run-tests").addEventListener("click", runTests);
document.querySelector("#clear-console").addEventListener("click", () => {
  output.replaceChildren();
});

runTests();
