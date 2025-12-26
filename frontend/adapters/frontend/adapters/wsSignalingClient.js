const getDefaultConfig = () => {
  const globalConfig = globalThis?.APM_WS_CONFIG ?? {};
  return {
    url: globalConfig.url ?? globalThis?.APM_WS_URL ?? "ws://localhost:8080/ws",
    token: globalConfig.token ?? globalThis?.APM_WS_TOKEN ?? "",
    pingIntervalMs: globalConfig.pingIntervalMs ?? 10000,
    reconnectBaseMs: globalConfig.reconnectBaseMs ?? 1000,
    reconnectMaxMs: globalConfig.reconnectMaxMs ?? 10000,
  };
};

export class WSSignalingClient {
  constructor(config = {}) {
    const configObj = typeof config === "string" ? { url: config } : config;
    this.config = { ...getDefaultConfig(), ...configObj };
    this.ws = null;
    this.handlers = {};
    this.pingInterval = null;
    this.reconnectTimer = null;
    this.reconnectAttempts = 0;
    this.shouldReconnect = true;
    this.lastConnectParams = null;
  }

  connect({ roomId, peerId }) {
    this.lastConnectParams = { roomId, peerId };
    this.shouldReconnect = true;
    this._connectInternal();
  }

  _connectInternal() {
    if (!this.lastConnectParams) return;
    this.ws = new WebSocket(this.config.url);

    this.ws.onopen = () => {
      this.reconnectAttempts = 0;
      console.info("WebSocket connected.");
      this.send({
        type: "join",
        roomId: this.lastConnectParams.roomId,
        peerId: this.lastConnectParams.peerId,
        token: this.config.token,
      });
      this._startPing();
    };

    this.ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      if (msg.type === "pong") {
        return;
      }
      const handler = this.handlers[msg.type];
      if (handler) handler(msg);
    };

    this.ws.onerror = (event) => {
      console.warn("WebSocket error.", event);
    };

    this.ws.onclose = (event) => {
      this._stopPing();
      console.warn(`WebSocket closed. code=${event.code} reason=${event.reason}`);
      if (this.handlers.close) {
        this.handlers.close(event);
      }
      if (this.shouldReconnect) {
        this._scheduleReconnect();
      }
    };
  }

  _startPing() {
    this._stopPing();
    this.pingInterval = setInterval(() => {
      this.send({ type: "ping" });
    }, this.config.pingIntervalMs);
  }

  _stopPing() {
    if (this.pingInterval) {
      clearInterval(this.pingInterval);
      this.pingInterval = null;
    }
  }

  _scheduleReconnect() {
    if (this.reconnectTimer) return;
    const delay = Math.min(
      this.config.reconnectBaseMs * 2 ** this.reconnectAttempts,
      this.config.reconnectMaxMs
    );
    this.reconnectAttempts += 1;
    console.info(`Reconnecting WebSocket in ${delay}ms.`);
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this._connectInternal();
    }, delay);
  }

  on(type, handler) {
    this.handlers[type] = handler;
  }

  send(message) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(JSON.stringify(message));
    }
  }

  disconnect() {
    this.shouldReconnect = false;
    if (this.reconnectTimer) {
      clearTimeout(this.reconnectTimer);
      this.reconnectTimer = null;
    }
    this._stopPing();
    this.ws?.close();
  }
}
