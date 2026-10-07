"""Behavioral API tests with an isolated database and real application lifespan."""
import tempfile
from pathlib import Path
from unittest.mock import patch

import pytest
from fastapi.testclient import TestClient

from backend import app as api
from backend.storage import Storage


@pytest.fixture
def client(tmp_path):
    with patch.object(api, "Storage", lambda: Storage(str(tmp_path / "api.sqlite"))):
        with TestClient(api.app) as test_client:
            yield test_client


def test_health(client):
    response = client.get("/health")
    assert response.status_code == 200
    assert response.json() == {"ok": True}


def test_peers(client):
    response = client.get("/api/peers")
    assert response.status_code == 200
    assert isinstance(response.json()["peers"], list)
    assert response.json()["peers"]


def test_status_update(client):
    response = client.post("/api/status", json={"status": "online"})
    assert response.status_code == 200
    assert response.json()["ok"] is True
    assert response.json()["peer"]["status"] == "online"
    assert client.post("/api/status", json={"status": "unknown"}).status_code == 422


def test_session_create(client):
    # A web peer is local to this instance and requires no network notification.
    peer_id = client.get("/api/status").json()["peer_id"]
    response = client.post("/api/session", params={"peer_id": peer_id})
    assert response.status_code == 200
    session = response.json()["session"]
    assert session["status"] == "calling"
    assert session["peer_id"] == peer_id
    assert client.get(f"/api/session/{session['id']}").json()["session"] == session
    assert client.post("/api/session", params={"peer_id": "missing"}).status_code == 404


def main():
    with tempfile.TemporaryDirectory() as directory, patch.object(
        api, "Storage", lambda: Storage(str(Path(directory) / "api.sqlite"))
    ), TestClient(api.app) as test_client:
        for test in (test_health, test_peers, test_status_update, test_session_create):
            test(test_client)
    print("4 API smoke tests passed")


if __name__ == "__main__":
    main()
