import asyncio
import math
from concurrent.futures import ThreadPoolExecutor
from unittest.mock import patch

import pytest
from fastapi.testclient import TestClient

from backend import app as api
from backend import telemetry
from backend.storage import Storage


@pytest.fixture
def client(tmp_path, monkeypatch):
    storage = Storage(str(tmp_path / "control.sqlite"))
    monkeypatch.setattr(api, "Storage", lambda: storage)
    monkeypatch.setattr(api, "_API_KEY", "")
    api.session_translations.clear()
    with TestClient(api.app) as test_client:
        yield test_client
    api.session_translations.clear()


def test_page_load_cannot_authorize_api(client, monkeypatch):
    monkeypatch.setattr(api, "_API_KEY", "bedrock-test-key")
    assert client.get("/").status_code == 200
    assert client.get("/api/peers").status_code == 401
    assert client.get("/api/config").json() == {"auth_enabled": True}
    assert client.get("/api/config/private").status_code == 401
    assert client.get("/api/peers", headers={"X-APM-API-Key": "bedrock-test-key"}).status_code == 200


def test_terminal_session_cannot_be_resurrected(client):
    peer = client.post("/api/peers", json={"name": "Remote", "ip": "127.0.0.2"}).json()["peer"]
    with patch.object(api.urllib_request, "urlopen", side_effect=api.urllib_error.URLError("offline")):
        session = client.post("/api/session", params={"peer_id": peer["id"]}).json()["session"]
    ended = client.post(f"/api/session/{session['id']}/end").json()["session"]
    assert client.post(f"/api/session/{session['id']}/accept").status_code == 409
    assert client.get(f"/api/session/{session['id']}").json()["session"] == ended
    assert client.post(f"/api/session/{session['id']}/end").json()["session"] == ended


def test_node_identity_cannot_be_deleted(client):
    local_id = api.app.state.local_peer_id
    assert client.delete(f"/api/peers/{local_id}").status_code == 403
    assert client.get(f"/api/peers/{local_id}").status_code == 200


def test_missing_peer_cannot_create_orphaned_session(tmp_path):
    store = Storage(str(tmp_path / "missing.sqlite"))
    with pytest.raises(ValueError):
        store.create_session("missing", session_id="orphan")
    assert store.get_session("orphan") is None


@pytest.mark.parametrize("payload", [[], {"translated_text": None, "success": True},
                                      {"translated_text": "hola", "success": "false"},
                                      {"translated_text": "hola", "success": False}])
def test_malformed_translation_is_not_success(client, payload):
    import json
    from subprocess import CompletedProcess
    with patch.object(api.subprocess, "run", return_value=CompletedProcess([], 0, json.dumps(payload), "")):
        response = client.post("/api/translate", json={"text": "hello"})
    assert response.status_code == 502


def test_local_peer_identity_is_atomic_across_storage_instances(tmp_path):
    path = str(tmp_path / "identity.sqlite")
    stores = [Storage(path) for _ in range(8)]
    with ThreadPoolExecutor(max_workers=8) as pool:
        peers = list(pool.map(lambda store: store.ensure_local_peer(), stores))
    assert len({peer["id"] for peer in peers}) == 1
    assert stores[0].count_peers() == 1
    assert stores[0].get_metadata("local_peer_id") == peers[0]["id"]


@pytest.mark.parametrize("bad_time", [math.nan, math.inf, -1])
def test_invalid_session_timestamp_preserves_state(tmp_path, bad_time):
    store = Storage(str(tmp_path / "time.sqlite"))
    local = store.ensure_local_peer()
    session = store.create_session(local["id"])
    with pytest.raises(ValueError):
        store.update_session_status(session["id"], "connected", bad_time)
    assert store.get_session(session["id"]) == session


def test_stale_and_invalid_transition_preserve_state(tmp_path):
    store = Storage(str(tmp_path / "state.sqlite"))
    peer = store.ensure_local_peer()
    session = store.create_session(peer["id"])
    with pytest.raises(ValueError):
        store.update_session_status(session["id"], "connected", session["updated_at"] - 1)
    with pytest.raises(ValueError):
        store.update_session_status(session["id"], "unknown", session["updated_at"] + 1)
    assert store.get_session(session["id"]) == session
    store.update_session_status(session["id"], "connected", session["updated_at"] + 1)
    assert store.mark_stale_sessions(0, now=session["updated_at"] + 10) == 0


def test_incoming_retry_is_atomic(client):
    payload = {"session_id": "incoming-bedrock", "caller_peer_id": "remote-bedrock",
               "caller_name": "Remote", "caller_ip": "127.0.0.2"}
    with ThreadPoolExecutor(max_workers=8) as pool:
        responses = list(pool.map(lambda _: client.post("/api/session/incoming", json=payload), range(8)))
    assert all(response.status_code == 200 for response in responses)
    assert len({response.json()["session"]["created_at"] for response in responses}) == 1


def test_incoming_node_session_is_visible_to_dashboard(client):
    payload = {"session_id": "incoming-visible", "caller_peer_id": "remote-visible",
               "caller_name": "Remote", "caller_ip": "127.0.0.2"}
    assert client.post("/api/session/incoming", json=payload).status_code == 200
    response = client.get("/api/session/incoming")
    assert response.json()["session"]["id"] == payload["session_id"]


def test_untrusted_forwarded_header_does_not_change_identity(client):
    actual = client.get("/api/status").json()["peer_id"]
    spoofed = client.get("/api/status", headers={"X-Forwarded-For": "192.0.2.123",
                                                "X-Real-IP": "192.0.2.124"}).json()["peer_id"]
    assert spoofed == actual


@pytest.mark.parametrize("ip", ["127.0.0.01", "١٢٧.0.0.1", "127.0.0.1:80"])
def test_peer_address_must_be_canonical_ipv4(client, ip):
    before = len(client.get("/api/peers").json()["peers"])
    assert client.post("/api/peers", json={"name": "invalid", "ip": ip}).status_code == 422
    assert len(client.get("/api/peers").json()["peers"]) == before


@pytest.mark.parametrize("bad_payload", [
    [], {"peak_db": 0}, {"peak_db": math.nan}, {"latency_ms": -1},
    {"clipping": "false"}, {"rms_db": True}, {"snr_db": math.inf},
])
def test_invalid_telemetry_preserves_last_valid_record(bad_payload, monkeypatch):
    valid = {"peak_db": -12.0, "rms_db": -24.0, "snr_db": 10.0,
             "clipping": False, "latency_ms": 4.0}
    initial = {**valid, "_updated_at": 0.0, "_offline": True}
    monkeypatch.setattr(telemetry, "cached_metrics", initial.copy())

    async def run():
        import json
        reader = asyncio.StreamReader()
        reader.feed_data((json.dumps(valid) + "\n").encode())
        malformed = bad_payload if isinstance(bad_payload, list) or len(bad_payload) == 1 and "peak_db" in bad_payload else {**valid, **bad_payload}
        reader.feed_data((json.dumps(malformed) + "\n").encode())
        reader.feed_eof()
        client = telemetry.TelemetryClient()

        class Writer:
            def close(self):
                client.running = False
            async def wait_closed(self):
                pass

        with patch.object(asyncio, "open_connection", return_value=(reader, Writer())):
            client.running = True
            await client._listen_loop()
    asyncio.run(run())
    assert {key: telemetry.cached_metrics[key] for key in valid} == valid


def test_fallback_telemetry_remains_identifiably_offline(monkeypatch):
    monkeypatch.setattr(telemetry, "TELEMETRY_STRICT", False)
    monkeypatch.setitem(telemetry.cached_metrics, "_updated_at", 0.0)
    metrics = telemetry.get_latest_metrics()
    assert metrics["offline"] is True
    assert metrics["telemetry_source"] == "fallback"
