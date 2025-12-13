from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from collections import defaultdict
import json

app = FastAPI()

# NOTE:
# This file ALREADY mounts /ws via @app.websocket("/ws")
# We do NOT use app.add_api_websocket_route here to avoid double-registration.
# This is the active WebSocket signaling endpoint.

# room_id -> set of websockets
rooms = defaultdict(set)

# ws -> (room_id, peer_id)
who = {}

async def broadcast(room_id: str, message: dict, exclude: WebSocket | None = None):
    dead = []
    for ws in rooms[room_id]:
        if ws is exclude:
            continue
        try:
            await ws.send_text(json.dumps(message))
        except Exception:
            dead.append(ws)
    for ws in dead:
        rooms[room_id].discard(ws)

@app.websocket("/ws")
async def ws_signaling(ws: WebSocket):
    await ws.accept()
    try:
        while True:
            raw = await ws.receive_text()
            msg = json.loads(raw)
            t = msg.get("type")

            # join room
            if t == "join":
                room_id = msg["roomId"]
                peer_id = msg["peerId"]
                who[ws] = (room_id, peer_id)
                rooms[room_id].add(ws)

                # announce to others
                await broadcast(
                    room_id,
                    {"type": "peer_joined", "peerId": peer_id},
                    exclude=ws,
                )

                await ws.send_text(
                    json.dumps(
                        {
                            "type": "joined",
                            "roomId": room_id,
                            "peerId": peer_id,
                        }
                    )
                )
                continue

            # relay messages to room
            room_id, peer_id = who.get(ws, (None, None))
            if not room_id:
                await ws.send_text(
                    json.dumps(
                        {
                            "type": "error",
                            "message": "not joined",
                        }
                    )
                )
                continue

            msg["fromPeerId"] = peer_id
            await broadcast(room_id, msg, exclude=ws)

    except WebSocketDisconnect:
        room_id, peer_id = who.get(ws, (None, None))
        if room_id:
            rooms[room_id].discard(ws)
            await broadcast(
                room_id,
                {"type": "peer_left", "peerId": peer_id},
                exclude=None,
            )
        who.pop(ws, None)
