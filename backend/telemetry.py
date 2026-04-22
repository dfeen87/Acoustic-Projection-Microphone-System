import asyncio
import json
import logging
import os
import time

logger = logging.getLogger(__name__)


def _truthy_env(name: str, default: bool = False) -> bool:
    raw = os.environ.get(name)
    if raw is None:
        return default
    return raw.strip().lower() in {"1", "true", "yes", "on"}


# When strict mode is disabled, the API keeps serving sane fallback metrics
# so the dashboard remains usable even if the native telemetry stream is down.
TELEMETRY_STRICT = _truthy_env("APM_TELEMETRY_STRICT", default=False)

# Shared memory cache for the latest metrics
cached_metrics = {
    "peak_db": -96.0,
    "rms_db": -96.0,
    "snr_db": 0.0,
    "clipping": False,
    "latency_ms": 0.0,
    "_updated_at": 0.0,
    "_offline": True,
}


class TelemetryClient:
    def __init__(self, host=None, port=None):
        self.host = host or os.environ.get("APM_TELEMETRY_HOST", "127.0.0.1")
        raw_port = str(port if port is not None else os.environ.get("APM_TELEMETRY_PORT", "50055"))
        try:
            self.port = int(raw_port)
        except ValueError:
            logger.warning("Invalid APM_TELEMETRY_PORT=%s; defaulting to 50055", raw_port)
            self.port = 50055

        self.running = False
        self.task = None

    async def start(self):
        self.running = True
        self.task = asyncio.create_task(self._listen_loop())

    async def stop(self):
        self.running = False
        if self.task:
            self.task.cancel()
            try:
                await self.task
            except asyncio.CancelledError:
                pass

    async def _listen_loop(self):
        while self.running:
            writer = None
            try:
                reader, writer = await asyncio.open_connection(self.host, self.port)
                logger.info("Connected to telemetry server at %s:%s", self.host, self.port)
                cached_metrics["_offline"] = False

                server_closed = False
                while self.running:
                    line = await reader.readline()
                    if not line:
                        server_closed = True
                        break  # Connection closed by server

                    try:
                        data = json.loads(line.decode("utf-8"))
                        # Update cache
                        cached_metrics["peak_db"] = data.get("peak_db", -96.0)
                        cached_metrics["rms_db"] = data.get("rms_db", -96.0)
                        cached_metrics["snr_db"] = data.get("snr_db", 0.0)
                        cached_metrics["clipping"] = data.get("clipping", False)
                        cached_metrics["latency_ms"] = data.get("latency_ms", 0.0)
                        cached_metrics["_updated_at"] = time.time()
                        cached_metrics["_offline"] = False
                    except json.JSONDecodeError:
                        logger.warning("Invalid JSON from telemetry stream")
                    except Exception as e:
                        logger.error("Error parsing telemetry data: %s", e)

                if server_closed:
                    # Server closed the connection cleanly; mark offline and
                    # wait before attempting to reconnect.
                    cached_metrics["_offline"] = True
                    await asyncio.sleep(1.0)
            except (ConnectionRefusedError, ConnectionResetError, OSError, asyncio.TimeoutError):
                cached_metrics["_offline"] = True
                await asyncio.sleep(1.0)  # Wait before reconnecting
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error("Telemetry client error: %s", e)
                cached_metrics["_offline"] = True
                await asyncio.sleep(1.0)
            finally:
                if writer is not None:
                    try:
                        writer.close()
                        await writer.wait_closed()
                    except Exception:
                        pass


def get_latest_metrics():
    # If the metrics haven't been updated in 2 seconds, mark offline.
    stale = time.time() - cached_metrics.get("_updated_at", 0.0) > 2.0
    if stale:
        cached_metrics["_offline"] = True

    # In non-strict mode, keep endpoint usable without native telemetry.
    # This preserves dashboard behavior in deployments that run backend-only.
    offline = cached_metrics["_offline"]
    if offline and not TELEMETRY_STRICT:
        return {
            "peak_db": cached_metrics["peak_db"],
            "rms_db": cached_metrics["rms_db"],
            "snr_db": cached_metrics["snr_db"],
            "clipping": cached_metrics["clipping"],
            "latency_ms": cached_metrics["latency_ms"],
            "offline": False,
            "telemetry_source": "fallback",
        }

    return {
        "peak_db": cached_metrics["peak_db"],
        "rms_db": cached_metrics["rms_db"],
        "snr_db": cached_metrics["snr_db"],
        "clipping": cached_metrics["clipping"],
        "latency_ms": cached_metrics["latency_ms"],
        "offline": offline,
        "telemetry_source": "native" if not offline else "offline",
    }
