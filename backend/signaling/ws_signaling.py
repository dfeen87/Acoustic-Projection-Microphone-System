from fastapi import WebSocket, WebSocketDisconnect
from typing import Dict, Set
import json
import asyncio

class SignalingHub:
    """
    In-memory WebSocket signaling hub.
    Prototype-safe. Docker-safe.
    """

    def __init__(self):
        # room_id -> set(WebSocket)
        self.rooms: Dict[str, Set[WebSocket]] = {}
        # websocket -> peer_id
        self.peers: Dict[WebSocket, str] = {}

    async def connect(self, websocket: WebSocket):
        await websocket.accept()

    async def disconnect(self, websocket: WebSocket):
        peer_id = self.peers.get(websocket)
        for room, sockets in self.rooms.items():
            if websocket in sockets:
                sockets.remove(websocket)
                await self._broadcast(
                    room,
                    {
                        "type": "peer_left",
                        "peerId": peer_id,
                    },
                    exclude=websocket,
                )
        self.peers.pop(websocket, None)

    async def join_room(self, websocket: WebSocket, room_id: str, peer_id: str):
        if room_id not in self.rooms:
            self.rooms[room_id] = set()

        self.rooms[room_id].add(websocket)
        self.peers[websocket] = peer_id

        await self._broadcast(
            room_id,
            {
                "type": "peer_joined",
                "peerId": peer_id,
            },
            exclude=websocket,
        )

        await websocket.send_text(
            json.dumps(
                {
                    "type": "joined",
                    "roomId": room_id,
                    "peerId": peer_id,
                }
            )
        )

    async def relay(self, websocket: WebSocket, message: dict):
        room_id = message.get("roomId")
        if not room_id or room_id not in self.rooms:
            return

        peer_id = self.peers.get(websocket)
        message["fromPeerId"] = peer_id

        await self._broadcast(room_id, message, exclude=websocket)

    async def _broadcast(self, room_id: str, message: dict, exclude=None):
        dead = []
        for ws in self.rooms.get(room_id, []):
            if ws is exclude:
                continue
            try:
                await ws.send_text(json.dumps(message))
            except Exception:
                dead.append(ws)

        for ws in dead:
            self.rooms[room_id].discard(ws)
            self.peers.pop(ws, None)


hub = SignalingHub()


async def signaling_ws(websocket: WebSocket):
    """
    WebSocket entrypoint.
    """
    await hub.connect(websocket)

    try:
        while True:
            raw = await websocket.receive_text()
            msg = json.loads(raw)

            msg_type = msg.get("type")

            if msg_type == "join":
                await hub.join_room(
                    websocket,
                    room_id=msg.get("roomId"),
                    peer_id=msg.get("peerId"),
                )
            else:
                await hub.relay(websocket, msg)

    except WebSocketDisconnect:
        await hub.disconnect(websocket)
