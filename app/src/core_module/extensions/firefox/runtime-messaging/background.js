const runtime = browser.runtime;

function expectVoid(method, result) {
  if (result !== undefined) {
    console.error(`[background] ${method} must return undefined`, result);
  }
}

self.addEventListener("error", (event) => {
  console.error("[background] uncaught error", event.error || event.message);
});

self.addEventListener("unhandledrejection", (event) => {
  console.error("[background] unhandled promise rejection", event.reason);
});

console.log("[background] Firefox Runtime Messaging Test loaded", {
  extensionId: runtime.id
});

runtime.onMessage.addListener((message, sender) => {
  console.info("[background] browser.runtime.onMessage", message);
  console.debug("[background] message sender", {
    id: sender.id,
    url: sender.url
  });

  if (!message || message.target !== "background") {
    console.warn("[background] ignored message for another target", message);
    return undefined;
  }

  const response = {
    type: "send-message-response",
    requestId: message.requestId,
    method: message.method,
    from: "background",
    receivedAt: new Date().toISOString()
  };

  console.log("[background] returning a Promise response", response);

  console.debug("[background] sending a follow-up message to the popup");
  const delivery = runtime.sendMessage({
    target: "popup",
    type: "background-notification",
    requestId: message.requestId,
    from: "background"
  });

  if (!delivery || typeof delivery.then !== "function") {
    console.error("[background] browser.runtime.sendMessage(message) must return a Promise", delivery);
  } else {
    delivery.then(() => {
      console.info("[background] follow-up message delivered");
    }, (error) => {
      console.error("[background] follow-up message rejected", error);
    });
  }

  return Promise.resolve(response);
});

runtime.onConnect.addListener((port) => {
  console.info("[background] browser.runtime.onConnect", {
    name: port.name,
    sender: port.sender
  });

  port.onMessage.addListener((message) => {
    console.log(`[background] port.onMessage (${port.name})`, message);

    expectVoid("Port.postMessage() response", port.postMessage({
      type: "port-response",
      requestId: message.requestId,
      method: message.method,
      portName: port.name,
      from: "background",
      receivedAt: new Date().toISOString()
    }));
  });

  port.onDisconnect.addListener((disconnectedPort) => {
    if (disconnectedPort !== port) {
      console.error(`[background] Port.onDisconnect (${port.name}) received an incorrect Port`, disconnectedPort);
    }

    if (!disconnectedPort) {
      return;
    }

    if (disconnectedPort.error) {
      console.error(`[background] port disconnected with error (${port.name})`, disconnectedPort.error.message);
      return;
    }

    console.warn(`[background] port disconnected (${port.name})`);
  });

  console.debug(`[background] sending initial port message (${port.name})`);
  expectVoid("Port.postMessage() connected", port.postMessage({
    type: "port-connected",
    portName: port.name,
    from: "background"
  }));
});
