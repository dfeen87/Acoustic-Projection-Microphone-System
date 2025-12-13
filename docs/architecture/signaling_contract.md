# APM Signaling Contract (v1)

## Transport
- WebSocket: /ws

## Rooms
- roomId === sessionId
- One active call per room

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
