import asyncio
import os
import time
from contextlib import asynccontextmanager
from typing import Literal

from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel

from backend.storage import Storage

# ---------------------------------------------------------------------------
# API key authentication middleware (opt-in via APM_API_KEY env variable).
# If APM_API_KEY is not set, authentication is skipped (backward compatible
# for trusted networks).  When set, all /api/* routes require the header
# X-APM-API-Key to match the configured secret.  /health and /docs are
# always exempt.
#
# NOTE: For production rate limiting, add `slowapi` (pip install slowapi) and
# attach a Limiter instance to the app.
# ---------------------------------------------------------------------------

_API_KEY = os.environ.get("APM_API_KEY", "")
_EXEMPT_PREFIXES = ("/health", "/docs", "/openapi", "/redoc")

SESSION_TIMEOUT_SECONDS = 30
SESSION_CLEANUP_INTERVAL_SECONDS = 5
SESSION_PURGE_AFTER_SECONDS = 10 * 60


async def session_housekeeper(storage: Storage, stop_event: asyncio.Event) -> None:
    while not stop_event.is_set():
        now = time.time()
        storage.mark_stale_sessions(SESSION_TIMEOUT_SECONDS, now=now)
        storage.purge_sessions(SESSION_PURGE_AFTER_SECONDS, now=now)
        try:
            await asyncio.wait_for(stop_event.wait(), timeout=SESSION_CLEANUP_INTERVAL_SECONDS)
        except asyncio.TimeoutError:
            continue


@asynccontextmanager
async def lifespan(app: FastAPI):
    # ---------- startup ----------
    storage = Storage()
    storage.ensure_seed_peers()
    local_peer = storage.ensure_local_peer()
    app.state.storage = storage
    app.state.local_peer_id = local_peer["id"]
    app.state.stop_event = asyncio.Event()
    app.state.housekeeper_task = asyncio.create_task(
        session_housekeeper(storage, app.state.stop_event)
    )
    yield
    # ---------- shutdown ----------
    app.state.stop_event.set()
    await app.state.housekeeper_task


app = FastAPI(title="APM FastAPI Backend", version="4.1.0", lifespan=lifespan)

# If you use Vite proxy, CORS is less critical, but keep it anyway.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["GET", "POST", "OPTIONS"],
    allow_headers=["Content-Type", "X-APM-API-Key"],
)


@app.middleware("http")
async def api_key_middleware(request: Request, call_next) -> Response:
    """Validate X-APM-API-Key header for /api/* endpoints when APM_API_KEY is set."""
    if _API_KEY and request.url.path.startswith("/api"):
        provided = request.headers.get("X-APM-API-Key", "")
        if provided != _API_KEY:
            return Response(content='{"detail":"Unauthorized"}', status_code=401,
                            media_type="application/json")
    return await call_next(request)


# -----------------------------
# Models
# -----------------------------
class StatusUpdate(BaseModel):
    status: Literal["online", "away", "busy"]

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
