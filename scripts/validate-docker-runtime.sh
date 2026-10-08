#!/usr/bin/env bash
# Validate the shipped API container with isolated credentials and durable data.
set -euo pipefail
python3 - "${1:-apm-system:11.0.0}" <<'PY'
import json
import pathlib
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import uuid

image = sys.argv[1]
container = "apm-runtime-validation-" + uuid.uuid4().hex
fixture_key = "apm-runtime-test-only-key"
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))

def docker(*args):
    return subprocess.check_output(["docker", *args], text=True).strip()

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

with tempfile.TemporaryDirectory(prefix="apm-runtime-validation-") as scratch:
    data = pathlib.Path(scratch) / "data"
    data.mkdir()
    def start():
        docker("run", "--detach", "--name", container,
               "--publish", "127.0.0.1::8080",
               "--mount", f"type=bind,src={data},dst=/data",
               "--env", "APM_DB_PATH=/data/apm.sqlite",
               "--env", f"APM_API_KEY={fixture_key}", image)
        address = docker("port", container, "8080/tcp")
        require(address.startswith("127.0.0.1:"), "Container port must bind loopback")
        base = "http://" + address
        for _ in range(100):
            try:
                with opener.open(base + "/health", timeout=0.5) as response:
                    if response.status == 200:
                        return base
            except (urllib.error.URLError, OSError):
                pass
            time.sleep(0.1)
        raise RuntimeError("Container API did not become live")

    def request(path, *, authorized=False, body=None):
        headers = {"X-APM-API-Key": fixture_key} if authorized else {}
        if body is not None:
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(base + path, headers=headers,
            data=json.dumps(body).encode() if body is not None else None)
        try:
            response = opener.open(req, timeout=5)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            raw = response.read()
            payload = json.loads(raw) if raw and "application/json" in response.headers.get("Content-Type", "") else None
            return response.status, payload, response.headers

    try:
        base = start()
        require(request("/health")[:2] == (200, {"ok": True}), "API liveness failed")
        require(request("/api/config")[:2] == (200, {"auth_enabled": True}), "API key configuration failed")
        require(request("/api/metrics")[0] == 401, "Protected API allowed missing credentials")
        require(request("/")[2].get("Set-Cookie") is None, "Page issued an authorization cookie")
        require(request("/api/metrics")[0] == 401, "Page access granted protected API access")
        status, metrics, _ = request("/api/metrics", authorized=True)
        require(status == 200 and metrics["offline"] is True, "Native telemetry must remain offline")
        require(metrics["telemetry_source"] == "fallback", "Fallback metrics were presented as native evidence")
        require(docker("exec", container, "/app/apm_backend", "--version") == "11.0.0", "Native version mismatch")
        docker("exec", container, "python3", "-c",
               "import sys; from pathlib import Path; root=Path('/app'); "
               "sys.exit(1 if list(root.rglob('.env')) or list(root.rglob('.env.*')) else 0)")
        status, added, _ = request("/api/peers", authorized=True,
            body={"name": "Container persistence fixture", "ip": "127.0.0.8"})
        require(status == 200, "Persisted peer creation failed")
        peer_id = added["peer"]["id"]
        docker("stop", "--time", "5", container)
        docker("rm", container)
        base = start()
        status, peers, _ = request("/api/peers", authorized=True)
        require(status == 200 and any(peer["id"] == peer_id for peer in peers["peers"]), "Peer did not survive restart")
        require(request("/api/peers")[0] == 401, "Restart lost authorization configuration")
        require((data / "apm.sqlite").is_file(), "SQLite state was not persisted in the mounted directory")
        print("Docker runtime passed: liveness/config, authorization, truthful offline metrics, native version and restart persistence.")
    finally:
        subprocess.run(["docker", "rm", "--force", container],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
PY
