import asyncio
import json
import os
import secrets
import subprocess
import sys
import threading
import time
from contextlib import asynccontextmanager
from pathlib import Path
from typing import Literal
from urllib import error as urllib_error
from urllib import request as urllib_request

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

_API_KEY = os.environ.get("APM_API_KEY", "").strip()
_EXEMPT_PREFIXES = ("/health", "/docs", "/openapi", "/redoc", "/api/config")

# On Render (and other cloud platforms) HTTPS is always used, so the Secure
# flag can be set on session cookies.  Locally we skip it so the cookie still
# works over plain HTTP during development.
_IS_PRODUCTION = bool(os.environ.get("RENDER", ""))

# When API-key auth is active, generate a single-use random session token on
# startup.  The browser receives this opaque token (not the real API key) in
# an HTTP-only cookie so the secret key itself is never transmitted to the
# client.  The token is valid for the lifetime of the process; a restart
# issues a new one and any stored cookies are automatically invalidated.
_SESSION_TOKEN = secrets.token_hex(32) if _API_KEY else ""

# Cookie lifetime: 7 days.  A fresh token is issued on every server restart,
# so the practical window of exposure is bounded by the deploy cadence.
_SESSION_COOKIE_MAX_AGE = 7 * 24 * 60 * 60

SESSION_TIMEOUT_SECONDS = 30
SESSION_CLEANUP_INTERVAL_SECONDS = 5
SESSION_PURGE_AFTER_SECONDS = 10 * 60


def _extract_request_ip(request: Request) -> str:
    """Best-effort client IP extraction (proxy-aware)."""
    forwarded_for = request.headers.get("x-forwarded-for", "")
    if forwarded_for:
        first_hop = forwarded_for.split(",", 1)[0].strip()
        if first_hop:
            return first_hop

    real_ip = request.headers.get("x-real-ip", "").strip()
    if real_ip:
        return real_ip

    if request.client and request.client.host:
        return request.client.host
    return "127.0.0.1"


def _ensure_request_peer(request: Request) -> dict:
    """Ensure each web client IP has a stable peer identity in storage."""
    storage: Storage = app.state.storage
    now = time.time()
    client_ip = _extract_request_ip(request)
    peer_id = f"web-{client_ip.replace('.', '-').replace(':', '-')}"
    peer = storage.get_peer(peer_id)
    if not peer:
        peer = {
            "id": peer_id,
            "name": f"Web {client_ip}",
            "ip": client_ip,
            "status": "online",
            "last_seen": now,
        }
        storage.upsert_peer(peer)
        return peer

    if peer["ip"] != client_ip or peer["status"] == "offline":
        peer["ip"] = client_ip
        peer["status"] = "online"
    peer["last_seen"] = now
    storage.upsert_peer(peer)
    return peer


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
    path = request.url.path

    # Let CORS preflight and explicit exempt routes pass through untouched.
    if request.method == "OPTIONS" or path.startswith(_EXEMPT_PREFIXES):
        return await call_next(request)

    if _API_KEY and path.startswith("/api"):
        provided = request.headers.get("X-APM-API-Key", "").strip()
        session_cookie = request.cookies.get("apm_session", "").strip()
        # Accept either the raw API key in the header (existing behaviour) or
        # the opaque session token that the backend sets as an HTTP-only cookie.
        if provided != _API_KEY and session_cookie != _SESSION_TOKEN:
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


class IncomingSessionCreate(BaseModel):
    session_id: str
    caller_peer_id: str
    caller_name: str
    caller_ip: str

class ProfileCreate(BaseModel):
    name: str

class CalibrationStart(BaseModel):
    action: Literal["start", "cancel", "advance"]

class TranslateRequest(BaseModel):
    text: str
    source_lang: str = "en"
    target_lang: str = "es"

class TranslateResponse(BaseModel):
    transcribed_text: str
    translated_text: str
    source_language: str
    target_language: str
    success: bool

class SessionTranslationCreate(BaseModel):
    sender_peer_id: str
    source_language: str
    target_language: str
    original_text: str
    translated_text: str
    timestamp_ms: float | None = None

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

session_translations: dict[str, list[dict[str, object]]] = {}
session_translations_lock = threading.Lock()

# -----------------------------
# Endpoints your system expects
# -----------------------------
@app.get("/health")
def health():
    return {"ok": True}

@app.get("/api/config")
def get_config():
    """Return server-side configuration flags consumed by the frontend.

    This endpoint is intentionally exempt from API-key authentication so the
    UI can query it before any credentials are known.  It never exposes the
    actual key – only a boolean indicating whether authentication is active.
    """
    return {"auth_enabled": bool(_API_KEY)}

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

@app.post("/api/translate")
def translate_text(body: TranslateRequest):
    text = body.text.strip()
    if not text:
        raise HTTPException(400, "Text is required")
    if body.source_lang == body.target_lang:
        return TranslateResponse(
            transcribed_text=text,
            translated_text=text,
            source_language=body.source_lang,
            target_language=body.target_lang,
            success=True,
        ).model_dump()

    script_path = Path(__file__).resolve().parent.parent / "scripts" / "translation_bridge.py"
    if not script_path.exists():
        raise HTTPException(500, "Translation bridge script not found")

    cmd = [
        sys.executable,
        str(script_path),
        "--text",
        text,
        "--source",
        body.source_lang,
        "--target",
        body.target_lang,
        "--json",
    ]
    if os.environ.get("APM_OFFLINE") == "1":
        cmd.append("--offline")

    try:
        completed = subprocess.run(
            cmd,
            check=True,
            capture_output=True,
            text=True,
            timeout=45,
        )
    except subprocess.TimeoutExpired as exc:
        raise HTTPException(504, "Translation timed out") from exc
    except subprocess.CalledProcessError as exc:
        stderr = (exc.stderr or "").strip()
        detail = stderr.splitlines()[-1] if stderr else "Translation failed"
        raise HTTPException(500, detail) from exc

    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise HTTPException(500, "Translation output parsing failed") from exc

    translated_text = str(payload.get("translated_text", "")).strip()
    if not translated_text:
        raise HTTPException(500, "Translation produced empty output")

    return TranslateResponse(
        transcribed_text=text,
        translated_text=translated_text,
        source_language=body.source_lang,
        target_language=body.target_lang,
        success=bool(payload.get("success", True)),
    ).model_dump()

@app.post("/api/session/{session_id}/translation")
def publish_session_translation(session_id: str, body: SessionTranslationCreate):
    storage: Storage = app.state.storage
    if not storage.get_session(session_id):
        raise HTTPException(404, "Session not found")

    original_text = body.original_text.strip()
    translated_text = body.translated_text.strip()
    if not original_text or not translated_text:
        raise HTTPException(400, "Both original_text and translated_text are required")

    message = {
        "session_id": session_id,
        "sender_peer_id": body.sender_peer_id,
        "source_language": body.source_language,
        "target_language": body.target_language,
        "original_text": original_text,
        "translated_text": translated_text,
        "timestamp_ms": body.timestamp_ms or (time.time() * 1000.0),
    }

    with session_translations_lock:
        messages = session_translations.setdefault(session_id, [])
        messages.append(message)
        if len(messages) > 200:
            del messages[:-200]

    return {"ok": True}

@app.get("/api/session/{session_id}/translations")
def get_session_translations(session_id: str, since_ms: float = 0):
    storage: Storage = app.state.storage
    if not storage.get_session(session_id):
        raise HTTPException(404, "Session not found")

    with session_translations_lock:
        messages = list(session_translations.get(session_id, []))

    filtered = [msg for msg in messages if float(msg.get("timestamp_ms", 0)) > since_ms]
    return {"translations": filtered}

@app.get("/api/peers")
def list_peers(request: Request):
    storage: Storage = app.state.storage
    _ensure_request_peer(request)
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
def remove_peer(peer_id: str, request: Request):
    local_peer = _ensure_request_peer(request)
    if peer_id == local_peer["id"]:
        raise HTTPException(403, "Cannot delete the local peer")
    storage: Storage = app.state.storage
    if not storage.get_peer(peer_id):
        raise HTTPException(404, "Peer not found")
    storage.delete_peer(peer_id)
    return {"ok": True}

@app.get("/api/status")
def get_status(request: Request):
    local_peer = _ensure_request_peer(request)
    return {"status": local_peer["status"], "peer_id": local_peer["id"]}

@app.post("/api/status")
def update_status(body: StatusUpdate, request: Request):
    storage: Storage = app.state.storage
    local_peer = _ensure_request_peer(request)
    now = time.time()
    storage.update_peer_status(local_peer["id"], body.status, now)
    return {"ok": True, "peer": {"id": local_peer["id"], "status": body.status}}

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
def create_session(peer_id: str, request: Request):
    storage: Storage = app.state.storage
    peer = storage.get_peer(peer_id)
    if not peer:
        raise HTTPException(404, "Peer not found")
    session = storage.create_session(peer_id)
    local_peer = _ensure_request_peer(request)

    # Best-effort cross-instance signaling: notify the remote peer backend
    # by IP so it can create a matching incoming session for its local user.
    if local_peer and not str(peer["id"]).startswith("web-"):
        notify_payload = {
            "session_id": session["id"],
            "caller_peer_id": local_peer["id"],
            "caller_name": local_peer["name"],
            "caller_ip": local_peer["ip"],
        }
        target_url = f"http://{peer['ip']}:8080/api/session/incoming"
        headers = {"Content-Type": "application/json"}
        if _API_KEY:
            headers["X-APM-API-Key"] = _API_KEY
        req = urllib_request.Request(
            target_url,
            data=json.dumps(notify_payload).encode("utf-8"),
            headers=headers,
            method="POST",
        )
        try:
            with urllib_request.urlopen(req, timeout=2):
                pass
        except (urllib_error.URLError, TimeoutError):
            # Keep local UX functional even when the remote instance
            # is unavailable. Caller side can still timeout gracefully.
            pass

    return {"session": Session(**session).model_dump()}

@app.get("/api/session/incoming")
def get_incoming_session(request: Request):
    storage: Storage = app.state.storage
    local_id = _ensure_request_peer(request)["id"]
    session = storage.get_latest_session_for_peer(local_id, ["calling", "ringing"])
    if not session:
        return {"session": None}
    if session["status"] == "calling":
        now = time.time()
        storage.update_session_status(session["id"], "ringing", now)
        session["status"] = "ringing"
        session["updated_at"] = now
    return {"session": Session(**session).model_dump()}

@app.post("/api/session/incoming")
def register_incoming_session(body: IncomingSessionCreate):
    storage: Storage = app.state.storage
    local_id = app.state.local_peer_id
    existing = storage.get_session(body.session_id)
    if existing:
        return {"session": Session(**existing).model_dump()}

    remote_peer = storage.get_peer(body.caller_peer_id)
    now = time.time()
    if not remote_peer:
        storage.upsert_peer(
            {
                "id": body.caller_peer_id,
                "name": body.caller_name,
                "ip": body.caller_ip,
                "status": "online",
                "last_seen": now,
            }
        )
    session = storage.create_session(
        local_id, status="ringing", session_id=body.session_id
    )
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
    with session_translations_lock:
        session_translations.pop(session_id, None)
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
    response = FileResponse(index_path)
    if _API_KEY:
        # Set an HTTP-only session cookie so every browser that loads the UI
        # is automatically authenticated without users needing to paste the
        # API key manually.  The cookie value is an opaque random token (not
        # the API key itself) generated at server startup.  Properties:
        #   • httponly  – JavaScript cannot read it (XSS-safe)
        #   • samesite=strict – not sent on cross-site requests (CSRF-safe)
        #   • secure    – only transmitted over HTTPS (set in production)
        #   • path=/    – accompanies every request to this origin
        #   • max_age   – 7-day lifetime; refreshed on each page load
        response.set_cookie(
            key="apm_session",
            value=_SESSION_TOKEN,
            httponly=True,
            samesite="strict",
            secure=_IS_PRODUCTION,
            path="/",
            max_age=_SESSION_COOKIE_MAX_AGE,
        )
    return response
