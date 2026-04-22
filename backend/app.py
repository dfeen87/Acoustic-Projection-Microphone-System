import asyncio
import os
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Literal

from fastapi import FastAPI, HTTPException, Request, Response
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, field_validator

from backend.storage import Storage
from backend.telemetry import get_latest_metrics

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

    telemetry_client = getattr(app.state, "telemetry_client", None)
    if telemetry_client is None:
        from backend.telemetry import TelemetryClient
        telemetry_client = TelemetryClient()
        app.state.telemetry_client = telemetry_client
    await telemetry_client.start()

    yield
    # ---------- shutdown ----------
    app.state.stop_event.set()
    await app.state.housekeeper_task
    await telemetry_client.stop()


app = FastAPI(title="APM FastAPI Backend", version="8.1.0", lifespan=lifespan)

_CORS_ORIGINS_ENV = os.environ.get("APM_CORS_ORIGINS", "")
_CORS_ORIGINS = [origin.strip() for origin in _CORS_ORIGINS_ENV.split(",") if origin.strip()]
if not _CORS_ORIGINS:
    _CORS_ORIGINS = ["http://localhost:5173"]

app.add_middleware(
    CORSMiddleware,
    allow_origins=_CORS_ORIGINS,
    allow_credentials=True,
    allow_methods=["GET", "POST", "DELETE", "OPTIONS"],
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

class PeerCreate(BaseModel):
    name: str
    ip: str

    @field_validator("ip")
    @classmethod
    def validate_ip(cls, v):
        parts = v.split(".")
        if len(parts) != 4:
            raise ValueError("Invalid IPv4 address")
        for part in parts:
            if not part.isdigit() or not 0 <= int(part) <= 255:
                raise ValueError("Invalid IPv4 address")
        return v

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

class ProfileCreate(BaseModel):
    name: str

class CalibrationStart(BaseModel):
    action: Literal["start", "cancel", "advance"]

# -----------------------------
# Mock State for New Features
# -----------------------------
mock_diagnostics = {
    "ok": True,
    "message": "OK",
    "input_device_ok": True,
    "sample_rate_ok": True,
    "channel_mapping_ok": True
}

mock_metrics = {
    "peak_db": -12.5,
    "rms_db": -24.0,
    "snr_db": 15.2,
    "clipping": False,
    "latency_ms": 4.2
}

mock_profiles = [
    {"name": "Default", "feedback_suppression_enabled": True},
    {"name": "Lecture Hall", "feedback_suppression_enabled": True},
    {"name": "Studio", "feedback_suppression_enabled": False}
]

mock_calibration_state = {
    "step": "Idle",
    "progress": 0.0,
    "result": None
}

# -----------------------------
# Endpoints your system expects
# -----------------------------
@app.get("/health")
def health():
    return {"ok": True}

# -----------------------------
# New V7.0.0 Endpoints
# -----------------------------
@app.get("/api/health/diagnostics")
def get_diagnostics():
    return mock_diagnostics

@app.get("/api/metrics")
def get_metrics():
    return get_latest_metrics()

@app.get("/api/profiles")
def list_profiles():
    return {"profiles": mock_profiles, "active": "Default"}

@app.post("/api/profiles")
def create_profile(profile: ProfileCreate):
    mock_profiles.append({"name": profile.name, "feedback_suppression_enabled": True})
    return {"ok": True, "profile": profile.name}

@app.get("/api/calibration")
def get_calibration_status():
    return mock_calibration_state

@app.post("/api/calibration")
def control_calibration(action: CalibrationStart):
    if action.action == "start":
        mock_calibration_state["step"] = "MeasureNoiseFloor"
        mock_calibration_state["progress"] = 0.0
    elif action.action == "cancel":
        mock_calibration_state["step"] = "Idle"
        mock_calibration_state["progress"] = 0.0
    elif action.action == "advance":
        if mock_calibration_state["step"] == "MeasureNoiseFloor":
            mock_calibration_state["step"] = "MeasureGain"
            mock_calibration_state["progress"] = 0.33
        elif mock_calibration_state["step"] == "MeasureGain":
            mock_calibration_state["step"] = "MeasureLatency"
            mock_calibration_state["progress"] = 0.66
        elif mock_calibration_state["step"] == "MeasureLatency":
            mock_calibration_state["step"] = "Complete"
            mock_calibration_state["progress"] = 1.0
            mock_calibration_state["result"] = {
                "rms_noise_floor_db": -65.0,
                "peak_noise_floor_db": -50.0,
                "recommended_input_gain": 0.8,
                "estimated_latency_ms": 45.0,
                "valid": True
            }
    return mock_calibration_state

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

@app.post("/api/peers")
def add_peer(body: PeerCreate):
    storage: Storage = app.state.storage
    peer = storage.add_peer(body.name, body.ip)
    return {"ok": True, "peer": peer}

@app.delete("/api/peers/{peer_id}")
def remove_peer(peer_id: str):
    if peer_id == app.state.local_peer_id:
        raise HTTPException(403, "Cannot delete the local peer")
    storage: Storage = app.state.storage
    if not storage.get_peer(peer_id):
        raise HTTPException(404, "Peer not found")
    storage.delete_peer(peer_id)
    return {"ok": True}

@app.get("/api/status")
def get_status():
    storage: Storage = app.state.storage
    local_id = app.state.local_peer_id
    peer = storage.get_peer(local_id)
    if not peer:
        raise HTTPException(500, "Local peer missing")
    return {"status": peer["status"], "peer_id": local_id}

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


_PROJECT_ROOT = Path(__file__).resolve().parents[1]
_UI_DIST_DIR = _PROJECT_ROOT / "ui" / "dist"
_UI_ASSETS_DIR = _UI_DIST_DIR / "assets"
if _UI_ASSETS_DIR.is_dir():
    app.mount("/assets", StaticFiles(directory=_UI_ASSETS_DIR), name="assets")


@app.get("/{full_path:path}")
def serve_frontend(request: Request, full_path: str):
    if request.url.path.startswith(("/api", "/health")):
        raise HTTPException(status_code=404, detail="Not Found")
    index_path = _UI_DIST_DIR / "index.html"
    if not index_path.is_file():
        raise HTTPException(status_code=404, detail="Frontend build not found")
    return FileResponse(index_path)
