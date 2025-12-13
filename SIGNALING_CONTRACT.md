# APM WebSocket Signaling Contract (v1)

This document defines the authoritative signaling protocol for the
Acoustic Projection Microphone (APM) system.

This contract is intentionally minimal and stable.
All frontend and backend signaling logic MUST conform to this document.

---

## Transport

- Protocol: WebSocket
- Endpoint: `/ws`
- Encoding: JSON
- One WebSocket connection per peer

---

## Core Concepts

### Peer
A uniquely identified client participating in signaling.

- `peerId`: string (globally unique per client instance)

### Room
A logical signaling space.

- `roomId`: string
- **Rule:** `roomId === sessionId`
- One active call per room

---

## Connection Lifecycle

1. Client opens WebSocket to `/ws`
2. Client sends `join`
3. Server responds with `joined`
4. Server broadcasts presence events
5. Clients exchange call-control messages
6. Client sends `end` or disconnects

---

## Message Types

### join
Sent by client immediately after WebSocket opens.

```json
{
  "type": "join",
  "roomId": "string",
  "peerId": "string"
}
