# APM Signaling Contract (v1)

## Transport
- WebSocket: /ws

## Rooms

- `roomId` identifies the socket's joined signaling space, such as `apm-lobby`.
- `sessionId` correlates a call; it may differ from `roomId`. The dashboard
  keeps its lobby connection while generating independent `session-<timestamp>` IDs.
- Joining binds a socket to one room and peer identity. A relay's optional
  `roomId` must match that membership; omitting it uses the joined room.
- Supplied `sessionId` values must be nonblank strings, using the same identifier
  policy as joins. Invalid session IDs are dropped without relay; they cannot
  select a different room. Per-call rooms remain an optional client convention.
- Relays broadcast only within the joined room. `fromPeerId` is server-derived;
  `toPeerId` remains a client filtering hint, not a server delivery ACL.
- Configured join tokens, strict JSON validation, heartbeat/disconnect cleanup
  and server-owned presence/acknowledgement messages remain enforced.

The hub owns room membership, not call-session authorization or state. This
distinction preserves lobby-based call/offer/answer/accept/end/ICE signaling
without granting cross-room access through call metadata.

## Messages

### join
{
  type: "join",
  roomId: string,
  peerId: string
}

### peer_joined
{
  type: "peer_joined",
  peerId: string
}

### peer_left
{
  type: "peer_left",
  peerId: string
}

### call
{
  type: "call",
  sessionId: string
}

### accept
{
  type: "accept",
  sessionId: string
}

### end
{
  type: "end",
  sessionId: string
}
