from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import Dict, List, Optional
import time
import uuid

app = FastAPI(title="APM FastAPI Backend", version="0.1.0")

# If you use Vite proxy, CORS is less critical, but keep it anyway.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

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

# -----------------------------
# In-memory state (prototype)
# -----------------------------
PEERS: Dict[str, Peer] = {}
SESSIONS: Dict[str, Session] = {}

LOCAL_ID = "local-" + uuid.uuid4().hex[:8]
PEERS[LOCAL_ID] = Peer(
    id=LOCAL_ID,
    name="You",
    ip="127.0.0.1",
    status="online",
    last_seen=time.time(),
)

# some starter peers so UI is not empty
def seed_peer(name: str, ip: str, status: str):
    pid = "peer-" + uuid.uuid4().hex[:6]
    PEERS[pid] = Peer(id=pid, name=name, ip=ip, status=status, last_seen=time.time())

seed_peer("Alice Cooper", "192.168.1.101", "online")
seed_peer("Bob Martinez", "192.168.1.102", "online")
seed_peer("Carol Zhang", "192.168.1.103", "away")

# -----------------------------
# Endpoints your system expects
# -----------------------------
@app.get("/health")
def health():
    return {"ok": True}

@app.get("/api/peers")
def list_peers():
    now = time.time()
    peers_out = []
    for p in PEERS.values():
        peers_out.append({
            "id": p.id,
            "name": p.name,
            "ip": p.ip,
            "status": p.status,
            "last_seen_ago": max(0.0, now - p.last_seen),
        })
    return {"peers": peers_out}

@app.post("/api/status")
def update_status(body: StatusUpdate):
    # update local peer
    p = PEERS.get(LOCAL_ID)
    if not p:
        raise HTTPException(500, "Local peer missing")
    p.status = body.status
    p.last_seen = time.time()
    PEERS[LOCAL_ID] = p
    return {"ok": True, "peer": {"id": p.id, "status": p.status}}

@app.get("/api/peers/{peer_id}")
def peer_details(peer_id: str):
    p = PEERS.get(peer_id)
    if not p:
        raise HTTPException(404, "Peer not found")
    return {"peer": {"id": p.id, "name": p.name, "ip": p.ip, "status": p.status}}

# Sessions (your hook monitors /api/session/{id})
@app.post("/api/session")
def create_session(peer_id: str):
    if peer_id not in PEERS:
        raise HTTPException(404, "Peer not found")
    sid = "session-" + uuid.uuid4().hex[:10]
    now = time.time()
    SESSIONS[sid] = Session(
        id=sid, peer_id=peer_id, status="calling", created_at=now, updated_at=now
    )
    return {"session": SESSIONS[sid].model_dump()}

@app.get("/api/session/{session_id}")
def get_session(session_id: str):
    s = SESSIONS.get(session_id)
    if not s:
        raise HTTPException(404, "Session not found")

    # simple prototype timeout logic
    now = time.time()
    if s.status in ("calling", "ringing") and (now - s.updated_at) > 30:
        s.status = "timeout"
        s.updated_at = now
        SESSIONS[session_id] = s

    return {"session": s.model_dump()}

@app.post("/api/session/{session_id}/accept")
def accept_session(session_id: str):
    s = SESSIONS.get(session_id)
    if not s:
        raise HTTPException(404, "Session not found")
    s.status = "connected"
    s.updated_at = time.time()
    SESSIONS[session_id] = s
    return {"ok": True, "session": s.model_dump()}

@app.post("/api/session/{session_id}/end")
def end_session(session_id: str):
    s = SESSIONS.get(session_id)
    if not s:
        raise HTTPException(404, "Session not found")
    s.status = "ended"
    s.updated_at = time.time()
    SESSIONS[session_id] = s
    return {"ok": True, "session": s.model_dump()}
