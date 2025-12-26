from fastapi import WebSocket, WebSocketDisconnect
from typing import Dict, Optional, Set
import asyncio
import json
import logging
import os
import time


logger = logging.getLogger(__name__)

EXPECTED_TOKEN = os.getenv("SIGNALING_WS_TOKEN")
HEARTBEAT_INTERVAL_SEC = float(os.getenv("SIGNALING_WS_HEARTBEAT_INTERVAL_SEC", "10"))
STALE_TIMEOUT_SEC = float(os.getenv("SIGNALING_WS_STALE_TIMEOUT_SEC", "30"))


class SignalingHub:
    """
    In-memory WebSocket signaling hub.
    Safe for prototypes. Docker-safe.
    """

    def __init__(self):
        # room_id -> set of WebSocket connections
        self.rooms: Dict[str, Set[WebSocket]] = {}
        # websocket -> peer_id
        self.peers: Dict[WebSocket, str] = {}
        # websocket -> last seen time
        self.last_seen: Dict[WebSocket, float] = {}
        self._heartbeat_task: Optional[asyncio.Task] = None

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.last_seen[websocket] = time.monotonic()
        self._ensure_heartbeat_task()

    async def disconnect(self, websocket: WebSocket):
        peer_id = self.peers.get(websocket)

        for room_id, sockets in self.rooms.items():
            if websocket in sockets:
                sockets.remove(websocket)
                await self._broadcast(
                    room_id,
                    {
                        "type": "peer_left",
                        "peerId": peer_id,
                    },
                    exclude=websocket,
                )

        self.peers.pop(websocket, None)
        self.last_seen.pop(websocket, None)

    async def join_room(self, websocket: WebSocket, room_id: str, peer_id: str):
        if room_id not in self.rooms:
            self.rooms[room_id] = set()

        self.rooms[room_id].add(websocket)
        self.peers[websocket] = peer_id

        # Notify others in room
        await self._broadcast(
            room_id,
            {
                "type": "peer_joined",
                "peerId": peer_id,
            },
            exclude=websocket,
        )

        # Acknowledge join
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

    def touch(self, websocket: WebSocket):
        self.last_seen[websocket] = time.monotonic()

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
            self.last_seen.pop(ws, None)

    def _ensure_heartbeat_task(self):
        if self._heartbeat_task and not self._heartbeat_task.done():
            return
        self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())

    async def _heartbeat_loop(self):
        while True:
            await asyncio.sleep(HEARTBEAT_INTERVAL_SEC)
            now = time.monotonic()
            stale = [
                ws
                for ws, seen_at in list(self.last_seen.items())
                if now - seen_at > STALE_TIMEOUT_SEC
            ]
            for ws in stale:
                peer_id = self.peers.get(ws)
                logger.info(
                    "Disconnecting stale websocket peer_id=%s last_seen=%.2fs ago",
                    peer_id,
                    now - self.last_seen.get(ws, now),
                )
                try:
                    await ws.close(code=1001, reason="stale connection")
                except Exception:
                    pass
                await self.disconnect(ws)


# Singleton hub instance
hub = SignalingHub()


async def signaling_ws(websocket: WebSocket):
    """
    WebSocket entrypoint.
    This is what FastAPI will mount later.
    """
    await hub.connect(websocket)

    try:
        while True:
            raw = await websocket.receive_text()
            msg = json.loads(raw)
            hub.touch(websocket)

            msg_type = msg.get("type")

            if msg_type == "join":
                token = msg.get("token")
                if EXPECTED_TOKEN:
                    if not token:
                        logger.warning("Missing token for join request.")
                        await websocket.close(code=1008, reason="missing token")
                        return
                    if token != EXPECTED_TOKEN:
                        logger.warning("Invalid token for join request.")
                        await websocket.close(code=1008, reason="invalid token")
                        return
                await hub.join_room(
                    websocket,
                    room_id=msg.get("roomId"),
                    peer_id=msg.get("peerId"),
                )
            elif msg_type == "ping":
                await websocket.send_text(json.dumps({"type": "pong"}))
            else:
                await hub.relay(websocket, msg)

    except WebSocketDisconnect:
        logger.info("WebSocket disconnected.")
        await hub.disconnect(websocket)
