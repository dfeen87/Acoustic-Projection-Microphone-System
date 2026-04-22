import asyncio
import json
import logging
import time

logger = logging.getLogger(__name__)

# Shared memory cache for the latest metrics
cached_metrics = {
    "peak_db": -96.0,
    "rms_db": -96.0,
    "snr_db": 0.0,
    "clipping": False,
    "latency_ms": 0.0,
    "_updated_at": 0.0,
    "_offline": True
}

class TelemetryClient:
    def __init__(self, host='127.0.0.1', port=50055):
        self.host = host
        self.port = port
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
                logger.info(f"Connected to telemetry server at {self.host}:{self.port}")
                cached_metrics["_offline"] = False

                server_closed = False
                while self.running:
                    line = await reader.readline()
                    if not line:
                        server_closed = True
                        break # Connection closed by server

                    try:
                        data = json.loads(line.decode('utf-8'))
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
                        logger.error(f"Error parsing telemetry data: {e}")

                if server_closed:
                    # Server closed the connection cleanly; mark offline and
                    # wait before attempting to reconnect.
                    cached_metrics["_offline"] = True
                    await asyncio.sleep(1.0)
            except (ConnectionRefusedError, ConnectionResetError, OSError, asyncio.TimeoutError):
                cached_metrics["_offline"] = True
                await asyncio.sleep(1.0) # Wait before reconnecting
            except asyncio.CancelledError:
                break
            except Exception as e:
                logger.error(f"Telemetry client error: {e}")
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
    # If the metrics haven't been updated in 2 seconds, mark offline
    if time.time() - cached_metrics.get("_updated_at", 0.0) > 2.0:
        cached_metrics["_offline"] = True

    return {
        "peak_db": cached_metrics["peak_db"],
        "rms_db": cached_metrics["rms_db"],
        "snr_db": cached_metrics["snr_db"],
        "clipping": cached_metrics["clipping"],
        "latency_ms": cached_metrics["latency_ms"],
        "offline": cached_metrics["_offline"]
    }
