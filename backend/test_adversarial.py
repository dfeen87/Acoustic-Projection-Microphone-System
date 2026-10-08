"""Behavioral regressions from the V11 adversarial second pass."""
import asyncio
from unittest.mock import AsyncMock

import pytest
from fastapi import WebSocketDisconnect
from fastapi.testclient import TestClient

from backend import app as api
from backend.signaling import ws_signaling as protocol
from backend.signaling.signaling import app as signaling_app
from backend.storage import Storage


@pytest.mark.parametrize("invalid_configuration", [False, True])
def test_telemetry_constructor_failure_does_not_start_background_tasks(tmp_path, monkeypatch, invalid_configuration):
    from backend import telemetry
    storage = Storage(str(tmp_path / "constructor.sqlite"))
    monkeypatch.setattr(api, "Storage", lambda: storage)
    monkeypatch.delattr(api.app.state, "telemetry_client", raising=False)
    if invalid_configuration:
        monkeypatch.setenv("APM_TELEMETRY_PORT", "65536")
        expected_error = ValueError
    else:
        def unavailable_client():
            raise RuntimeError("telemetry construction failed")
        monkeypatch.setattr(telemetry, "TelemetryClient", unavailable_client)
        expected_error = RuntimeError

    async def run():
        existing_tasks = asyncio.all_tasks()
        try:
            with pytest.raises(expected_error):
                async with api.lifespan(api.app):
                    pytest.fail("Invalid dependency must prevent startup")
            assert not (asyncio.all_tasks() - existing_tasks)
        finally:
            # Clean up a leak exposed by the pre-fix test, without masking it.
            pending = asyncio.all_tasks() - existing_tasks
            if pending:
                api.app.state.stop_event.set()
                await asyncio.gather(*pending, return_exceptions=True)
    asyncio.run(run())


@pytest.mark.parametrize("startup_failure", [False, True])
def test_lifespan_exception_releases_housekeeping_and_telemetry(tmp_path, monkeypatch, startup_failure):
    storage = Storage(str(tmp_path / "lifecycle.sqlite"))
    monkeypatch.setattr(api, "Storage", lambda: storage)
    telemetry = AsyncMock()
    if startup_failure:
        telemetry.start.side_effect = RuntimeError("telemetry startup failed")
    monkeypatch.setattr(api.app.state, "telemetry_client", telemetry, raising=False)

    async def run():
        try:
            with pytest.raises(RuntimeError):
                async with api.lifespan(api.app):
                    raise RuntimeError("application failed")
            assert api.app.state.stop_event.is_set()
            assert api.app.state.housekeeper_task.done()
            telemetry.stop.assert_awaited_once()
        finally:
            # The deliberately failing pre-fix test must not itself leak a task.
            api.app.state.stop_event.set()
            await api.app.state.housekeeper_task
    asyncio.run(run())


@pytest.mark.parametrize("message_type", ["peer_joined", "peer_left", "joined", "pong"])
def test_room_member_cannot_forge_server_presence_or_acknowledgement(message_type):
    async def run():
        hub = protocol.SignalingHub()
        attacker, victim = AsyncMock(), AsyncMock()
        hub.peers[attacker] = "attacker"
        hub.peers[victim] = "victim"
        hub.rooms["room"] = {attacker, victim}
        await hub.relay(attacker, {"type": message_type, "roomId": "room", "peerId": "victim"})
        victim.send_text.assert_not_called()
    asyncio.run(run())


@pytest.mark.parametrize("constant", ["NaN", "Infinity", "-Infinity", "1e999"])
def test_non_json_numeric_evidence_closes_signaling_connection(constant, monkeypatch):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", None)
    with TestClient(signaling_app) as client, client.websocket_connect("/ws") as socket:
        socket.send_text('{"type":"ping","evidence":' + constant + '}')
        with pytest.raises(WebSocketDisconnect) as exc:
            socket.receive_json()
        assert exc.value.code == 1008
    assert not protocol.hub.last_seen


def test_binary_signaling_frame_is_rejected_without_server_exception():
    with TestClient(signaling_app) as client, client.websocket_connect("/ws") as socket:
        socket.send_bytes(b'{"type":"ping"}')
        with pytest.raises(WebSocketDisconnect) as exc:
            socket.receive_json()
        assert exc.value.code == 1008
    assert not protocol.hub.last_seen


@pytest.mark.parametrize("payload", [
    '{"success":false,"success":true,"translated_text":"hola"}',
    '{"success":true,"translated_text":null,"translated_text":"hola"}',
])
def test_contradictory_translation_evidence_is_rejected(payload, tmp_path, monkeypatch):
    from subprocess import CompletedProcess
    monkeypatch.setattr(api, "Storage", lambda: Storage(str(tmp_path / "translation.sqlite")))
    monkeypatch.setattr(api, "_API_KEY", "")
    monkeypatch.setattr(api.subprocess, "run", lambda *args, **kwargs: CompletedProcess([], 0, payload, ""))
    with TestClient(api.app) as client:
        response = client.post("/api/translate", json={"text": "hello"})
        assert response.status_code == 502


@pytest.mark.parametrize("payload", [
    '{"success":true,"translated_text":"\\ud800"}',
    '{"success":true,"translated_text":"\\udfff"}',
    '{"success":true,"translated_text":"hola","metadata":{"\\ud800":true}}',
    '{"success":true,"translated_text":"hola","metadata":["\\udfff"]}',
])
def test_bridge_unpaired_surrogates_are_rejected(payload, tmp_path, monkeypatch):
    from subprocess import CompletedProcess
    monkeypatch.setattr(api, "Storage", lambda: Storage(str(tmp_path / "surrogates.sqlite")))
    monkeypatch.setattr(api, "_API_KEY", "")
    monkeypatch.setattr(api.subprocess, "run", lambda *args, **kwargs: CompletedProcess([], 0, payload, ""))
    with TestClient(api.app) as client:
        assert client.post("/api/translate", json={"text": "hello"}).status_code == 502


def test_valid_bridge_surrogate_pairs_remain_accepted(tmp_path, monkeypatch):
    from subprocess import CompletedProcess
    payload = '{"success":true,"translated_text":"\\ud83d\\ude00","metadata":{"\\ud83d\\ude00":["\\u3053\\u3093\\u306b\\u3061\\u306f"]}}'
    monkeypatch.setattr(api, "Storage", lambda: Storage(str(tmp_path / "unicode.sqlite")))
    monkeypatch.setattr(api, "_API_KEY", "")
    monkeypatch.setattr(api.subprocess, "run", lambda *args, **kwargs: CompletedProcess([], 0, payload, ""))
    with TestClient(api.app) as client:
        response = client.post("/api/translate", json={"text": "hello"})
        assert response.status_code == 200
        assert response.json()["translated_text"] == "\U0001f600"


@pytest.mark.parametrize("payload", [
    '{"type":"join","roomId":"room","peerId":"\\ud800"}',
    '{"type":"ping","metadata":{"\\udfff":true}}',
])
def test_signaling_unpaired_surrogates_are_rejected_without_membership_leak(payload):
    with TestClient(signaling_app) as client, client.websocket_connect("/ws") as socket:
        socket.send_text(payload)
        with pytest.raises(WebSocketDisconnect) as exc:
            socket.receive_json()
        assert exc.value.code == 1008
    assert not protocol.hub.peers
    assert not protocol.hub.last_seen


def test_bridge_invalid_utf8_output_is_rejected(tmp_path, monkeypatch):
    monkeypatch.setattr(api, "Storage", lambda: Storage(str(tmp_path / "decode.sqlite")))
    monkeypatch.setattr(api, "_API_KEY", "")

    def undecodable_output(*args, **kwargs):
        raise UnicodeDecodeError("utf-8", b"\xff", 0, 1, "invalid start byte")

    monkeypatch.setattr(api.subprocess, "run", undecodable_output)
    with TestClient(api.app) as client:
        assert client.post("/api/translate", json={"text": "hello"}).status_code == 502


def test_bridge_unsupported_json_nesting_is_rejected(tmp_path, monkeypatch):
    from subprocess import CompletedProcess
    nesting = "[" * 10000 + "0" + "]" * 10000
    payload = '{"success":true,"translated_text":"hola","evidence":' + nesting + '}'
    monkeypatch.setattr(api, "Storage", lambda: Storage(str(tmp_path / "nesting.sqlite")))
    monkeypatch.setattr(api, "_API_KEY", "")
    monkeypatch.setattr(api.subprocess, "run", lambda *args, **kwargs: CompletedProcess([], 0, payload, ""))
    with TestClient(api.app) as client:
        assert client.post("/api/translate", json={"text": "hello"}).status_code == 502


def test_signaling_unsupported_json_nesting_is_rejected_without_server_exception():
    nesting = "[" * 10000 + "0" + "]" * 10000
    with TestClient(signaling_app) as client, client.websocket_connect("/ws") as socket:
        socket.send_text('{"type":"ping","evidence":' + nesting + '}')
        with pytest.raises(WebSocketDisconnect) as exc:
            socket.receive_json()
        assert exc.value.code == 1008
    assert not protocol.hub.last_seen


def test_incoming_registration_with_missing_local_peer_rolls_back_caller_and_session(tmp_path):
    storage = Storage(str(tmp_path / "incoming.sqlite"))
    with pytest.raises(ValueError):
        storage.register_incoming_session("missing-local", "incoming-orphan", {
            "id": "new-caller", "name": "Caller", "ip": "127.0.0.2",
        })
    assert storage.get_session("incoming-orphan") is None
    assert storage.get_peer("new-caller") is None


def test_peer_id_collision_does_not_replace_existing_identity(tmp_path, monkeypatch):
    from types import SimpleNamespace
    from backend import storage as persistence
    storage = Storage(str(tmp_path / "collision.sqlite"))
    original = storage.add_peer("Original", "127.0.0.2")
    suffix = original["id"].removeprefix("peer-")
    monkeypatch.setattr(persistence.uuid, "uuid4", lambda: SimpleNamespace(hex=suffix))
    with pytest.raises(ValueError):
        storage.add_peer("Replacement", "127.0.0.3")
    assert storage.get_peer(original["id"]) == original
    assert storage.count_peers() == 1


def test_peer_id_collision_retries_new_identity_without_changing_existing_peer(tmp_path, monkeypatch):
    from types import SimpleNamespace
    from backend import storage as persistence
    storage = Storage(str(tmp_path / "retry.sqlite"))
    original = storage.add_peer("Original", "127.0.0.2")
    candidates = iter([original["id"].removeprefix("peer-"), "retry2"])
    monkeypatch.setattr(persistence.uuid, "uuid4", lambda: SimpleNamespace(hex=next(candidates)))
    added = storage.add_peer("New", "127.0.0.3")
    assert added["id"] != original["id"]
    assert storage.get_peer(added["id"]) == added
    assert storage.get_peer(original["id"]) == original


def test_ambiguous_telemetry_does_not_refresh_last_valid_evidence(monkeypatch):
    from backend import telemetry
    original = {"peak_db": -30.0, "rms_db": -40.0, "snr_db": 10.0,
                "clipping": False, "latency_ms": 3.0,
                "_updated_at": 42.0, "_offline": True}
    monkeypatch.setattr(telemetry, "cached_metrics", original.copy())

    async def run():
        reader = asyncio.StreamReader()
        reader.feed_data(b'{"peak_db":null,"peak_db":-1,"rms_db":-2,"snr_db":1,"clipping":false,"latency_ms":5}\n')
        reader.feed_eof()
        client = telemetry.TelemetryClient()

        class Writer:
            def close(self):
                client.running = False

            async def wait_closed(self):
                pass

        async def open_connection(*args):
            return reader, Writer()

        monkeypatch.setattr(asyncio, "open_connection", open_connection)
        client.running = True
        await client._listen_loop()

    asyncio.run(run())
    assert telemetry.cached_metrics == original


def test_stale_telemetry_cannot_remain_online_after_malformed_or_absent_evidence(monkeypatch):
    from backend import telemetry
    monkeypatch.setattr(telemetry.time, "monotonic", lambda: 100.0)
    monkeypatch.setattr(telemetry, "cached_metrics", {
        "peak_db": -12.0, "rms_db": -24.0, "snr_db": 12.0,
        "clipping": False, "latency_ms": 4.0,
        "_updated_at": 97.0, "_offline": False,
    })
    monkeypatch.setattr(telemetry, "TELEMETRY_STRICT", True)
    metrics = telemetry.get_latest_metrics()
    assert metrics["offline"] is True
    assert metrics["telemetry_source"] == "offline"


@pytest.mark.parametrize("cursor", ["nan", "inf", "-inf"])
def test_translation_poll_cursor_must_be_finite(cursor, tmp_path, monkeypatch):
    storage = Storage(str(tmp_path / "cursor.sqlite"))
    monkeypatch.setattr(api, "Storage", lambda: storage)
    monkeypatch.setattr(api, "_API_KEY", "")
    with TestClient(api.app) as client:
        session = storage.create_session(api.app.state.local_peer_id)
        response = client.get(f"/api/session/{session['id']}/translations", params={"since_ms": cursor})
        assert response.status_code == 422
