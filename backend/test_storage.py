import tempfile
import time
import unittest

from backend.storage import Storage


class StorageTestCase(unittest.TestCase):
    def test_session_timeout_flow(self) -> None:
        with tempfile.NamedTemporaryFile() as tmp:
            storage = Storage(db_path=tmp.name)
            storage.ensure_local_peer()
            peer_id = "peer-test"
            storage.upsert_peer(
                {
                    "id": peer_id,
                    "name": "Test Peer",
                    "ip": "10.0.0.1",
                    "status": "online",
                    "last_seen": time.time(),
                }
            )

            session = storage.create_session(peer_id)
            fetched = storage.get_session(session["id"])
            self.assertIsNotNone(fetched)
            self.assertEqual(fetched["status"], "calling")

            now = session["updated_at"] + 100
            updated = storage.mark_stale_sessions(timeout_seconds=30, now=now)
            self.assertEqual(updated, 1)

            timed_out = storage.get_session(session["id"])
            self.assertIsNotNone(timed_out)
            self.assertEqual(timed_out["status"], "timeout")

    def test_add_peer(self):
        """Test that add_peer persists a peer with expected fields."""
        with tempfile.NamedTemporaryFile() as tmp:
            storage = Storage(db_path=tmp.name)
            peer = storage.add_peer("Test Peer", "192.168.1.100")
            assert peer["name"] == "Test Peer"
            assert peer["ip"] == "192.168.1.100"
            assert peer["status"] == "online"
            assert peer["id"].startswith("peer-")
            fetched = storage.get_peer(peer["id"])
            assert fetched is not None
            assert fetched["name"] == "Test Peer"

    def test_delete_peer(self):
        """Test that delete_peer removes a peer."""
        with tempfile.NamedTemporaryFile() as tmp:
            storage = Storage(db_path=tmp.name)
            peer = storage.add_peer("To Delete", "10.0.0.1")
            storage.delete_peer(peer["id"])
            assert storage.get_peer(peer["id"]) is None

    def test_delete_missing_peer(self):
        """Test that deleting a non-existent peer doesn't raise."""
        with tempfile.NamedTemporaryFile() as tmp:
            storage = Storage(db_path=tmp.name)
            storage.delete_peer("peer-nonexistent")


if __name__ == "__main__":
    unittest.main()
