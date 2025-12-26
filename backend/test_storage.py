import tempfile
import time
import unittest

from storage import Storage


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


if __name__ == "__main__":
    unittest.main()
