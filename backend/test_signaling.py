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


@pytest.mark.parametrize("token", [None, "wrong-key"])
def test_active_endpoint_enforces_configured_token(monkeypatch, token):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", "bedrock-ws-key")
    with TestClient(app) as client, client.websocket_connect("/ws") as ws:
        message = {"type": "join", "roomId": "room", "peerId": "peer"}
        if token is not None:
            message["token"] = token
        ws.send_json(message)
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


@pytest.mark.parametrize("session_id", ["session-123", "room"])
def test_join_relay_retry_and_disconnect_contract(monkeypatch, session_id):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", None)
    with TestClient(app) as client:
        with client.websocket_connect("/ws") as first:
            first.send_json({"type": "join", "roomId": "room", "peerId": "first"})
            assert first.receive_json()["type"] == "joined"
            with client.websocket_connect("/ws") as second:
                second.send_json({"type": "join", "roomId": "room", "peerId": "second"})
                assert second.receive_json()["type"] == "joined"
                assert first.receive_json() == {"type": "peer_joined", "peerId": "second"}
                second.send_json({"type": "end", "sessionId": session_id, "fromPeerId": "forged"})
                second.send_json({"type": "ping"})
                assert second.receive_json() == {"type": "pong"}
                # A receiver ping bounds the assertion if a relay is dropped.
                first.send_json({"type": "ping"})
                assert first.receive_json() == {"type": "end", "sessionId": session_id, "fromPeerId": "second"}
                assert first.receive_json() == {"type": "pong"}
                second.send_json({"type": "join", "roomId": "room", "peerId": "second"})
                assert second.receive_json()["type"] == "joined"
            assert first.receive_json() == {"type": "peer_left", "peerId": "second"}
    assert not protocol.hub.rooms
    assert not protocol.hub.peers


@pytest.mark.parametrize("message_type", ["call", "offer", "answer", "accept", "end", "ice"])
@pytest.mark.parametrize("supply_room", [False, True])
def test_lobby_call_session_is_independent_of_routing_room(monkeypatch, message_type, supply_room):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", "lobby-test-key")
    with TestClient(app) as client, client.websocket_connect("/ws") as sender:
        sender.send_json({"type": "join", "roomId": "apm-lobby", "peerId": "sender", "token": "lobby-test-key"})
        assert sender.receive_json()["type"] == "joined"
        with client.websocket_connect("/ws") as receiver, client.websocket_connect("/ws") as outsider:
            receiver.send_json({"type": "join", "roomId": "apm-lobby", "peerId": "receiver", "token": "lobby-test-key"})
            assert receiver.receive_json()["type"] == "joined"
            assert sender.receive_json() == {"type": "peer_joined", "peerId": "receiver"}
            outsider.send_json({"type": "join", "roomId": "private-room", "peerId": "outsider", "token": "lobby-test-key"})
            assert outsider.receive_json()["type"] == "joined"

            message = {"type": message_type, "sessionId": "session-123", "toPeerId": "receiver", "fromPeerId": "forged"}
            if supply_room:
                message["roomId"] = "apm-lobby"
            sender.send_json(message)
            sender.send_json({"type": "ping"})
            assert sender.receive_json() == {"type": "pong"}
            receiver.send_json({"type": "ping"})
            assert receiver.receive_json() == {**message, "fromPeerId": "sender"}
            assert receiver.receive_json() == {"type": "pong"}
            outsider.send_json({"type": "ping"})
            assert outsider.receive_json() == {"type": "pong"}
    assert not protocol.hub.rooms
    assert not protocol.hub.peers
    assert not protocol.hub.last_seen


@pytest.mark.parametrize("session_id", [None, False, 123, 1.5, [], {}, "", " \t\n", "\u2003"])
def test_malformed_session_identifier_is_not_relayed(monkeypatch, session_id):
    monkeypatch.setattr(protocol, "EXPECTED_TOKEN", None)
    with TestClient(app) as client, client.websocket_connect("/ws") as sender:
        sender.send_json({"type": "join", "roomId": "apm-lobby", "peerId": "sender"})
        assert sender.receive_json()["type"] == "joined"
        with client.websocket_connect("/ws") as receiver:
            receiver.send_json({"type": "join", "roomId": "apm-lobby", "peerId": "receiver"})
            assert receiver.receive_json()["type"] == "joined"
            assert sender.receive_json()["type"] == "peer_joined"
            sender.send_json({"type": "call", "sessionId": session_id, "toPeerId": "receiver"})
            sender.send_json({"type": "ping"})
            assert sender.receive_json() == {"type": "pong"}
            receiver.send_json({"type": "ping"})
            assert receiver.receive_json() == {"type": "pong"}
    assert not protocol.hub.last_seen


def test_session_metadata_cannot_select_another_room_or_targeted_delivery():
    async def run():
        hub = protocol.SignalingHub()
        sender, receiver, bystander, outsider, unjoined = [AsyncMock() for _ in range(5)]
        hub.rooms = {"apm-lobby": {sender, receiver, bystander}, "private-room": {outsider}}
        hub.peers = {sender: "sender", receiver: "receiver", bystander: "bystander", outsider: "outsider"}
        # Even a correlation ID named after another room routes only by membership.
        message = {"type": "call", "sessionId": "private-room", "toPeerId": "outsider", "fromPeerId": "forged"}
        await hub.relay(sender, message)
        expected = {**message, "fromPeerId": "sender"}
        assert json.loads(receiver.send_text.call_args.args[0]) == expected
        assert json.loads(bystander.send_text.call_args.args[0]) == expected
        sender.send_text.assert_not_called()
        outsider.send_text.assert_not_called()
        assert message["fromPeerId"] == "forged"
        receiver.send_text.reset_mock()
        bystander.send_text.reset_mock()
        await hub.relay(sender, {**message, "roomId": "private-room"})
        await hub.relay(unjoined, {**message, "roomId": "apm-lobby"})
        receiver.send_text.assert_not_called()
        bystander.send_text.assert_not_called()
        outsider.send_text.assert_not_called()
    asyncio.run(run())
