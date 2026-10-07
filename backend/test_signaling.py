import asyncio
import json
from unittest.mock import AsyncMock

import pytest
from fastapi.testclient import TestClient
from fastapi import WebSocketDisconnect

from backend.signaling import ws_signaling as protocol
from backend.signaling.signaling import app


def test_unjoined_socket_cannot_inject_into_room():
    async def run():
        hub = protocol.SignalingHub()
        member, attacker = AsyncMock(), AsyncMock()
        hub.rooms["private"] = {member}
        message = {"type": "end", "roomId": "private", "fromPeerId": "forged"}
        await hub.relay(attacker, message)
        member.send_text.assert_not_called()
        assert message["fromPeerId"] == "forged"  # Caller-owned payload is not mutated.
    asyncio.run(run())


def test_broadcast_uses_snapshot_when_membership_changes():
    async def run():
        hub = protocol.SignalingHub()
        member = AsyncMock()
        hub.rooms["room"] = {member}
        member.send_text.side_effect = lambda _: hub.rooms["room"].clear()
        await hub._broadcast("room", {"type": "end"})
    asyncio.run(run())


def test_active_endpoint_enforces_configured_token(monkeypatch):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", "bedrock-ws-key")
    with TestClient(app) as client, client.websocket_connect("/ws") as ws:
        ws.send_json({"type": "join", "roomId": "room", "peerId": "peer"})
        with pytest.raises(WebSocketDisconnect) as exc:
            ws.receive_json()
        assert exc.value.code == 1008


@pytest.mark.parametrize("message", [[], None, {"type": "join", "roomId": "", "peerId": "x"},
                                    {"type": "join", "roomId": ["room"], "peerId": "x"}])
def test_malformed_message_closes_without_membership_leak(message, monkeypatch):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", None)
    with TestClient(app) as client, client.websocket_connect("/ws") as ws:
        ws.send_text(json.dumps(message))
        with pytest.raises(WebSocketDisconnect) as exc:
            ws.receive_json()
        assert exc.value.code == 1008
    assert not protocol.hub.rooms
    assert not protocol.hub.peers
    assert not protocol.hub.last_seen


def test_join_relay_retry_and_disconnect_contract(monkeypatch):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", None)
    with TestClient(app) as client:
        with client.websocket_connect("/ws") as first:
            first.send_json({"type": "join", "roomId": "room", "peerId": "first"})
            assert first.receive_json()["type"] == "joined"
            with client.websocket_connect("/ws") as second:
                second.send_json({"type": "join", "roomId": "room", "peerId": "second"})
                assert second.receive_json()["type"] == "joined"
                assert first.receive_json() == {"type": "peer_joined", "peerId": "second"}
                second.send_json({"type": "end", "sessionId": "room", "fromPeerId": "forged"})
                assert first.receive_json()["fromPeerId"] == "second"
                second.send_json({"type": "join", "roomId": "room", "peerId": "second"})
                assert second.receive_json()["type"] == "joined"
            assert first.receive_json() == {"type": "peer_left", "peerId": "second"}
    assert not protocol.hub.rooms
    assert not protocol.hub.peers
