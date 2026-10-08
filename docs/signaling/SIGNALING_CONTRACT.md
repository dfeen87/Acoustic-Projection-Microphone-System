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
- A nonblank identifier for the connection's signaling space.
- A socket is bound to one room/peer identity by `join`.
- Relays use actual membership. An optional supplied `roomId` must match the
  joined room; another room is never selected through caller payloads.

### Call session

- `sessionId`: nonblank string when supplied.
- Correlates call-control/WebRTC messages; it does not grant room access or
  establish call ownership.
- It may differ from `roomId`: `frontend/ui/APMDashboard.jsx` joins `apm-lobby`
  and generates independent `session-<timestamp>` IDs. The April 2026 persistent
  lobby implementation superseded the older room-equals-session convention.
- A per-call room using the same ID is still valid, but is not required. The
  server does not maintain a one-active-call-per-room state machine.

### Relay boundary

- Only joined sockets can relay. The sender's joined room owns routing.
- `fromPeerId` always comes from server membership, replacing a caller value
  without mutating the caller's object.
- Delivery remains broadcast within that room, excluding the sender.
  `toPeerId` is a client filtering hint; it does not enable delivery across rooms
  or provide private server-side targeting.
- Null, numeric, Boolean, collection, empty and whitespace-only session IDs are
  dropped as invalid relay metadata. Omitting the field retains existing behavior.
- External messages remain strict JSON text: duplicate keys, nonfinite numbers,
  malformed Unicode and unsupported nesting are rejected by the shared decoder.
  Invalid protocol requests close with code 1008; valid JSON with a rejected
  room/session relay does not create a message in any room.
- Only the server emits `peer_joined`, `peer_left`, `joined` and `pong`.

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
```

When `SIGNALING_WS_TOKEN` is configured, `join` also requires the matching
`token`. Unconfigured shared-token mode and heartbeat/disconnect behavior are
unchanged.

### Lobby call example

After two peers join `apm-lobby`, this message is relayed within that lobby with
the sender's server-derived `fromPeerId`:

```json
{
  "type": "call",
  "sessionId": "session-123",
  "toPeerId": "other-peer"
}
```

`offer`, `answer`, `accept`, `end` and `ice` use the same independent call
correlation. Session/recipient handling belongs to the client; room membership
and shared tokens do not provide per-call or multi-tenant authorization.
