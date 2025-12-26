import asyncio
import time
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from backend.storage import Storage

app = FastAPI(title="APM FastAPI Backend", version="0.1.0")

# If you use Vite proxy, CORS is less critical, but keep it anyway.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

SESSION_TIMEOUT_SECONDS = 30
SESSION_CLEANUP_INTERVAL_SECONDS = 5
SESSION_PURGE_AFTER_SECONDS = 10 * 60

# -----------------------------
# Models
# -----------------------------
class StatusUpdate(BaseModel):
    status: str  # "online" | "away" | "busy"

class Peer(BaseModel):
    id: str
    name: str
    ip: str
    status: str
    last_seen: float

class Session(BaseModel):
    id: str
    peer_id: str
    status: str  # "calling" | "ringing" | "connected" | "ended" | "timeout"
    created_at: float
    updated_at: float


async def session_housekeeper(storage: Storage, stop_event: asyncio.Event) -> None:
    while not stop_event.is_set():
        now = time.time()
        storage.mark_stale_sessions(SESSION_TIMEOUT_SECONDS, now=now)
        storage.purge_sessions(SESSION_PURGE_AFTER_SECONDS, now=now)
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=SESSION_CLEANUP_INTERVAL_SECONDS)
        except asyncio.TimeoutError:
            continue


@app.on_event("startup")
async def startup() -> None:
    storage = Storage()
    storage.ensure_seed_peers()
    local_peer = storage.ensure_local_peer()
    app.state.storage = storage
    app.state.local_peer_id = local_peer["id"]
    app.state.stop_event = asyncio.Event()
    app.state.housekeeper_task = asyncio.create_task(
        session_housekeeper(storage, app.state.stop_event)
    )


@app.on_event("shutdown")
async def shutdown() -> None:
    stop_event = app.state.stop_event
    stop_event.set()
    await app.state.housekeeper_task


# -----------------------------
# Endpoints your system expects
# -----------------------------
@app.get("/health")
def health():
    return {"ok": True}

@app.get("/api/peers")
def list_peers():
    storage: Storage = app.state.storage
    now = time.time()
    peers_out = []
    for peer in storage.list_peers():
        peers_out.append(
            {
                "id": peer["id"],
                "name": peer["name"],
                "ip": peer["ip"],
                "status": peer["status"],
                "last_seen_ago": max(0.0, now - peer["last_seen"]),
            }
        )
    return {"peers": peers_out}

@app.post("/api/status")
def update_status(body: StatusUpdate):
    storage: Storage = app.state.storage
    local_id = app.state.local_peer_id
    peer = storage.get_peer(local_id)
    if not peer:
        raise HTTPException(500, "Local peer missing")
    now = time.time()
    storage.update_peer_status(local_id, body.status, now)
    return {"ok": True, "peer": {"id": local_id, "status": body.status}}

@app.get("/api/peers/{peer_id}")
def peer_details(peer_id: str):
    storage: Storage = app.state.storage
    peer = storage.get_peer(peer_id)
    if not peer:
        raise HTTPException(404, "Peer not found")
    return {
        "peer": {
            "id": peer["id"],
            "name": peer["name"],
            "ip": peer["ip"],
            "status": peer["status"],
        }
    }

# Sessions (your hook monitors /api/session/{id})
@app.post("/api/session")
def create_session(peer_id: str):
    storage: Storage = app.state.storage
    if not storage.get_peer(peer_id):
        raise HTTPException(404, "Peer not found")
    session = storage.create_session(peer_id)
    return {"session": Session(**session).model_dump()}

@app.get("/api/session/{session_id}")
def get_session(session_id: str):
    storage: Storage = app.state.storage
    session = storage.get_session(session_id)
    if not session:
        raise HTTPException(404, "Session not found")
    return {"session": Session(**session).model_dump()}

@app.post("/api/session/{session_id}/accept")
def accept_session(session_id: str):
    storage: Storage = app.state.storage
    session = storage.get_session(session_id)
    if not session:
        raise HTTPException(404, "Session not found")
    now = time.time()
    storage.update_session_status(session_id, "connected", now)
    session["status"] = "connected"
    session["updated_at"] = now
    return {"ok": True, "session": Session(**session).model_dump()}

@app.post("/api/session/{session_id}/end")
def end_session(session_id: str):
    storage: Storage = app.state.storage
    session = storage.get_session(session_id)
    if not session:
        raise HTTPException(404, "Session not found")
    now = time.time()
    storage.update_session_status(session_id, "ended", now)
    session["status"] = "ended"
    session["updated_at"] = now
    return {"ok": True, "session": Session(**session).model_dump()}
