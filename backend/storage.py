import sqlite3
import time
import uuid
import threading
from typing import Dict, List, Optional


class Storage:
    def __init__(self, db_path: str = "backend/data.sqlite") -> None:
        self.db_path = db_path
        self._db_lock = threading.Lock()  # Add lock for thread safety
        self._init_db()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self) -> None:
        with self._db_lock:  # Protect database initialization
            with self._connect() as conn:
                conn.execute(
                    """
                    CREATE TABLE IF NOT EXISTS peers (
                        id TEXT PRIMARY KEY,
                        name TEXT NOT NULL,
                        ip TEXT NOT NULL,
                        status TEXT NOT NULL,
                        last_seen REAL NOT NULL
                    )
                    """
                )
                conn.execute(
                    """
                    CREATE TABLE IF NOT EXISTS sessions (
                        id TEXT PRIMARY KEY,
                        peer_id TEXT NOT NULL,
                        status TEXT NOT NULL,
                        created_at REAL NOT NULL,
                        updated_at REAL NOT NULL
                    )
                    """
                )
                conn.execute(
                    """
                    CREATE TABLE IF NOT EXISTS metadata (
                        key TEXT PRIMARY KEY,
                        value TEXT NOT NULL
                    )
                    """
                )
                conn.commit()

    def get_metadata(self, key: str) -> Optional[str]:
        with self._connect() as conn:
            row = conn.execute(
                "SELECT value FROM metadata WHERE key = ?", (key,)
            ).fetchone()
            return row["value"] if row else None

    def set_metadata(self, key: str, value: str) -> None:
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute(
                    "INSERT OR REPLACE INTO metadata (key, value) VALUES (?, ?)",
                    (key, value),
                )
                conn.commit()

    def count_peers(self) -> int:
        with self._connect() as conn:
            row = conn.execute("SELECT COUNT(*) AS count FROM peers").fetchone()
            return int(row["count"]) if row else 0

    def list_peers(self) -> List[Dict[str, object]]:
        with self._connect() as conn:
            rows = conn.execute("SELECT * FROM peers").fetchall()
            return [dict(row) for row in rows]

    def get_peer(self, peer_id: str) -> Optional[Dict[str, object]]:
        with self._connect() as conn:
            row = conn.execute(
                "SELECT * FROM peers WHERE id = ?", (peer_id,)
            ).fetchone()
            return dict(row) if row else None

    def upsert_peer(self, peer: Dict[str, object]) -> None:
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute(
                    """
                    INSERT OR REPLACE INTO peers (id, name, ip, status, last_seen)
                    VALUES (?, ?, ?, ?, ?)
                    """,
                    (
                        peer["id"],
                        peer["name"],
                        peer["ip"],
                        peer["status"],
                        peer["last_seen"],
                    ),
                )
                conn.commit()

    def update_peer_status(self, peer_id: str, status: str, last_seen: float) -> None:
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute(
                    "UPDATE peers SET status = ?, last_seen = ? WHERE id = ?",
                    (status, last_seen, peer_id),
                )
                conn.commit()

    def ensure_local_peer(self) -> Dict[str, object]:
        local_id = self.get_metadata("local_peer_id")
        if local_id:
            peer = self.get_peer(local_id)
            if peer:
                return peer

        local_id = "local-" + uuid.uuid4().hex[:8]
        peer = {
            "id": local_id,
            "name": "You",
            "ip": "127.0.0.1",
            "status": "online",
            "last_seen": time.time(),
        }
        self.upsert_peer(peer)
        self.set_metadata("local_peer_id", local_id)
        return peer

    def ensure_seed_peers(self) -> None:
        if self.count_peers() == 0:
            self.ensure_local_peer()
            now = time.time()
            starter = [
                {
                    "id": "peer-" + uuid.uuid4().hex[:6],
                    "name": "Alice Cooper",
                    "ip": "192.168.1.101",
                    "status": "online",
                    "last_seen": now,
                },
                {
                    "id": "peer-" + uuid.uuid4().hex[:6],
                    "name": "Bob Martinez",
                    "ip": "192.168.1.102",
                    "status": "online",
                    "last_seen": now,
                },
                {
                    "id": "peer-" + uuid.uuid4().hex[:6],
                    "name": "Carol Zhang",
                    "ip": "192.168.1.103",
                    "status": "away",
                    "last_seen": now,
                },
            ]
            for peer in starter:
                self.upsert_peer(peer)
        else:
            self.ensure_local_peer()

    def create_session(self, peer_id: str) -> Dict[str, object]:
        now = time.time()
        session = {
            "id": "session-" + uuid.uuid4().hex[:10],
            "peer_id": peer_id,
            "status": "calling",
            "created_at": now,
            "updated_at": now,
        }
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute(
                    """
                    INSERT INTO sessions (id, peer_id, status, created_at, updated_at)
                    VALUES (?, ?, ?, ?, ?)
                    """,
                    (
                        session["id"],
                        session["peer_id"],
                        session["status"],
                        session["created_at"],
                        session["updated_at"],
                    ),
                )
                conn.commit()
        return session

    def get_session(self, session_id: str) -> Optional[Dict[str, object]]:
        with self._connect() as conn:
            row = conn.execute(
                "SELECT * FROM sessions WHERE id = ?", (session_id,)
            ).fetchone()
            return dict(row) if row else None

    def update_session_status(self, session_id: str, status: str, updated_at: float) -> None:
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute(
                    "UPDATE sessions SET status = ?, updated_at = ? WHERE id = ?",
                    (status, updated_at, session_id),
                )
                conn.commit()

    def mark_stale_sessions(self, timeout_seconds: float, now: Optional[float] = None) -> int:
        if now is None:
            now = time.time()
        threshold = now - timeout_seconds
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                cursor = conn.execute(
                    """
                    UPDATE sessions
                    SET status = ?, updated_at = ?
                    WHERE status IN ('calling', 'ringing') AND updated_at <= ?
                    """,
                    ("timeout", now, threshold),
                )
                conn.commit()
                return cursor.rowcount

    def purge_sessions(self, older_than_seconds: float, now: Optional[float] = None) -> int:
        if now is None:
            now = time.time()
        threshold = now - older_than_seconds
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                cursor = conn.execute(
                    """
                    DELETE FROM sessions
                    WHERE status IN ('ended', 'timeout') AND updated_at <= ?
                    """,
                    (threshold,),
                )
                conn.commit()
                return cursor.rowcount
