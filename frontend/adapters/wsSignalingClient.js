export class WSSignalingClient {
  constructor(url = "ws://localhost:8080/ws") {
    this.url = url;
    this.ws = null;
    this.handlers = {};
  }

  connect({ roomId, peerId }) {
    this.ws = new WebSocket(this.url);

    this.ws.onopen = () => {
      this.send({
        type: "join",
        roomId,
        peerId,
      });
    };

    this.ws.onmessage = (event) => {
      const msg = JSON.parse(event.data);
      const handler = this.handlers[msg.type];
      if (handler) handler(msg);
    };

    this.ws.onclose = () => {
      if (this.handlers.close) {
        this.handlers.close();
      }
    };
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
    this.ws?.close();
  }
}
