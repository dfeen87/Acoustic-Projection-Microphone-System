import sqlite3
import time
import uuid
import threading
import math
import os
from contextlib import contextmanager
from typing import Dict, List, Optional


class Storage:
    def __init__(self, db_path: Optional[str] = None) -> None:
        self.db_path = db_path if db_path is not None else os.environ.get("APM_DB_PATH", "backend/data.sqlite")
        self._db_lock = threading.Lock()  # Add lock for thread safety
        self._init_db()

    @contextmanager
    def _connect(self):
        conn = sqlite3.connect(self.db_path, check_same_thread=False)
        conn.row_factory = sqlite3.Row
        try:
            conn.execute("PRAGMA journal_mode=WAL")
            with conn:
                yield conn
        finally:
            conn.close()

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

    def add_peer(self, name: str, ip: str) -> Dict[str, object]:
        peer = {
            "id": "peer-" + uuid.uuid4().hex[:6],
            "name": name,
            "ip": ip,
            "status": "online",
            "last_seen": time.time()
        }
        self.upsert_peer(peer)
        return peer

    def delete_peer(self, peer_id: str) -> None:
        with self._db_lock:
            with self._connect() as conn:
                conn.execute("DELETE FROM peers WHERE id = ?", (peer_id,))
                conn.commit()

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
        # The transaction protects identity creation across Storage instances
        # and processes, not just threads sharing this object's lock.
        with self._db_lock, self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            row = conn.execute(
                "SELECT peers.* FROM peers JOIN metadata ON peers.id = metadata.value "
                "WHERE metadata.key = 'local_peer_id'"
            ).fetchone()
            if row:
                return dict(row)
            local_id = "local-" + uuid.uuid4().hex[:8]
            peer = {"id": local_id, "name": "You", "ip": "127.0.0.1",
                    "status": "online", "last_seen": time.time()}
            conn.execute("INSERT INTO peers VALUES (?, ?, ?, ?, ?)", tuple(peer.values()))
            conn.execute("INSERT OR REPLACE INTO metadata VALUES ('local_peer_id', ?)", (local_id,))
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

    def create_session(
        self,
        peer_id: str,
        *,
        status: str = "calling",
        session_id: Optional[str] = None,
    ) -> Dict[str, object]:
        if status not in {"calling", "ringing"}:
            raise ValueError("New sessions must be calling or ringing")
        now = time.time()
        session = {
            "id": session_id or ("session-" + uuid.uuid4().hex[:10]),
            "peer_id": peer_id,
            "status": status,
            "created_at": now,
            "updated_at": now,
        }
        with self._db_lock:  # Protect write operation
            with self._connect() as conn:
                conn.execute("BEGIN IMMEDIATE")
                if not conn.execute("SELECT 1 FROM peers WHERE id = ?", (peer_id,)).fetchone():
                    raise ValueError("Peer not found")
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

    def register_incoming_session(self, local_id: str, session_id: str,
                                  caller: Dict[str, object]) -> Dict[str, object]:
        with self._db_lock, self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            existing = conn.execute("SELECT * FROM sessions WHERE id = ?", (session_id,)).fetchone()
            if existing:
                if existing["peer_id"] != local_id:
                    raise ValueError("Session ID belongs to a different peer")
                return dict(existing)
            now = time.time()
            conn.execute(
                "INSERT OR IGNORE INTO peers (id, name, ip, status, last_seen) VALUES (?, ?, ?, ?, ?)",
                (caller["id"], caller["name"], caller["ip"], "online", now),
            )
            session = {"id": session_id, "peer_id": local_id, "status": "ringing",
                       "created_at": now, "updated_at": now}
            conn.execute("INSERT INTO sessions VALUES (?, ?, ?, ?, ?)", tuple(session.values()))
            return session

    def get_session(self, session_id: str) -> Optional[Dict[str, object]]:
        with self._connect() as conn:
            row = conn.execute(
                "SELECT * FROM sessions WHERE id = ?", (session_id,)
            ).fetchone()
            return dict(row) if row else None

    def get_latest_session_for_peer(
        self, peer_id: str, statuses: List[str]
    ) -> Optional[Dict[str, object]]:
        if not statuses:
            return None
        placeholders = ",".join("?" for _ in statuses)
        with self._connect() as conn:
            row = conn.execute(
                f"""
                SELECT *
                FROM sessions
                WHERE peer_id = ? AND status IN ({placeholders})
                ORDER BY updated_at DESC
                LIMIT 1
                """,
                (peer_id, *statuses),
            ).fetchone()
            return dict(row) if row else None

    def update_session_status(self, session_id: str, status: str, updated_at: float) -> Optional[Dict[str, object]]:
        transitions = {
            "calling": {"ringing", "connected", "ended", "timeout"},
            "ringing": {"connected", "ended", "timeout"},
            "connected": {"ended"},
            "ended": set(), "timeout": set(),
        }
        if isinstance(updated_at, bool) or not math.isfinite(updated_at) or updated_at < 0:
            raise ValueError("Session timestamp must be finite and nonnegative")
        if status not in transitions:
            raise ValueError("Unknown session status")
        with self._db_lock, self._connect() as conn:
            conn.execute("BEGIN IMMEDIATE")
            row = conn.execute("SELECT * FROM sessions WHERE id = ?", (session_id,)).fetchone()
            if not row:
                return None
            session = dict(row)
            if session["status"] == status:
                return session  # Retries do not extend lifetime or rewrite history.
            if status not in transitions.get(session["status"], set()):
                raise ValueError("Invalid session transition")
            if updated_at < session["updated_at"]:
                raise ValueError("Stale session transition")
            conn.execute("UPDATE sessions SET status = ?, updated_at = ? WHERE id = ?",
                         (status, updated_at, session_id))
            session.update(status=status, updated_at=updated_at)
            return session

    def mark_stale_sessions(self, timeout_seconds: float, now: Optional[float] = None) -> int:
        if now is None:
            now = time.time()
        if not math.isfinite(timeout_seconds) or timeout_seconds < 0 or not math.isfinite(now) or now < 0:
            raise ValueError("Invalid session timeout")
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
        if not math.isfinite(older_than_seconds) or older_than_seconds < 0 or not math.isfinite(now) or now < 0:
            raise ValueError("Invalid session retention")
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
