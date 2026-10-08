from fastapi import WebSocket, WebSocketDisconnect
from typing import Dict, Optional, Set
import asyncio
import json
import logging
import os
import time
import hmac
import math
from backend import json_contract


logger = logging.getLogger(__name__)

EXPECTED_TOKEN = os.getenv("SIGNALING_WS_TOKEN")
HEARTBEAT_INTERVAL_SEC = float(os.getenv("SIGNALING_WS_HEARTBEAT_INTERVAL_SEC", "10"))
STALE_TIMEOUT_SEC = float(os.getenv("SIGNALING_WS_STALE_TIMEOUT_SEC", "30"))
if (not math.isfinite(HEARTBEAT_INTERVAL_SEC) or HEARTBEAT_INTERVAL_SEC <= 0
        or not math.isfinite(STALE_TIMEOUT_SEC) or STALE_TIMEOUT_SEC <= 0):
    raise ValueError("Signaling heartbeat and stale timeout must be finite and positive")


def _identifier(value):
    return isinstance(value, str) and bool(value.strip())



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
        peer_id = self.peers.pop(websocket, None)
        self.last_seen.pop(websocket, None)
        departed = []
        for room_id, sockets in list(self.rooms.items()):
            if websocket in sockets:
                sockets.discard(websocket)
                departed.append(room_id)
            if not sockets:
                self.rooms.pop(room_id, None)
        for room_id in departed:
            await self._broadcast(room_id, {"type": "peer_left", "peerId": peer_id})
        if not self.last_seen and self._heartbeat_task:
            task, self._heartbeat_task = self._heartbeat_task, None
            if task is not asyncio.current_task():
                task.cancel()
                try:
                    await task
                except asyncio.CancelledError:
                    pass

    async def join_room(self, websocket: WebSocket, room_id: str, peer_id: str):
        if not _identifier(room_id) or not _identifier(peer_id):
            raise ValueError("Invalid room or peer identity")
        current_peer = self.peers.get(websocket)
        if current_peer is not None:
            if current_peer != peer_id or websocket not in self.rooms.get(room_id, set()):
                raise ValueError("Connection already joined another identity or room")
        elif peer_id in self.peers.values():
            raise ValueError("Peer identity is already connected")
        else:
            self.rooms.setdefault(room_id, set()).add(websocket)
            self.peers[websocket] = peer_id
            await self._broadcast(room_id, {"type": "peer_joined", "peerId": peer_id}, exclude=websocket)
        await websocket.send_text(json.dumps({"type": "joined", "roomId": room_id, "peerId": peer_id}))

    async def relay(self, websocket: WebSocket, message: dict):
        if not isinstance(message, dict) or websocket not in self.peers:
            return
        # Presence and acknowledgements are emitted only by this hub. A member
        # cannot impersonate server state changes by supplying another peerId.
        if message.get("type") in {"peer_joined", "peer_left", "joined", "pong"}:
            return
        joined_room = next((room for room, sockets in self.rooms.items() if websocket in sockets), None)
        room_id = message.get("roomId", joined_room)
        if room_id != joined_room or joined_room is None:
            return
        if "sessionId" in message and message["sessionId"] != joined_room:
            return
        # Membership owns provenance; caller data never supplies sender identity.
        payload = {**message, "fromPeerId": self.peers[websocket]}
        await self._broadcast(joined_room, payload, exclude=websocket)

    def touch(self, websocket: WebSocket):
        self.last_seen[websocket] = time.monotonic()

    async def _broadcast(self, room_id: str, message: dict, exclude=None):
        dead = []

        for ws in tuple(self.rooms.get(room_id, ())):
            if ws is exclude:
                continue
            try:
                await ws.send_text(json.dumps(message))
            except Exception:
                dead.append(ws)

        for ws in dead:
            await self.disconnect(ws)

    def _ensure_heartbeat_task(self):
        if self._heartbeat_task and not self._heartbeat_task.done():
            return
        self._heartbeat_task = asyncio.create_task(self._heartbeat_loop())

    async def _heartbeat_loop(self):
        while self.last_seen:
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
            event = await websocket.receive()
            if event["type"] == "websocket.disconnect":
                raise WebSocketDisconnect(event.get("code", 1000))
            raw = event.get("text")
            if not isinstance(raw, str):
                raise ValueError("Signaling requires JSON text frames")
            msg = json_contract.loads(raw)
            if not isinstance(msg, dict) or not _identifier(msg.get("type")):
                raise ValueError("Invalid signaling message")
            hub.touch(websocket)
            msg_type = msg["type"]
            if msg_type == "join":
                token = msg.get("token")
                if EXPECTED_TOKEN and (not isinstance(token, str) or not hmac.compare_digest(
                        token.encode("utf-8"), EXPECTED_TOKEN.encode("utf-8"))):
                    raise ValueError("Invalid signaling token")
                await hub.join_room(websocket, room_id=msg.get("roomId"), peer_id=msg.get("peerId"))
            elif msg_type == "ping":
                await websocket.send_text(json.dumps({"type": "pong"}))
            else:
                await hub.relay(websocket, msg)
    except WebSocketDisconnect:
        logger.info("WebSocket disconnected.")
    except (ValueError, TypeError, UnicodeError):
        await websocket.close(code=1008, reason="invalid signaling request")
    finally:
        await hub.disconnect(websocket)
