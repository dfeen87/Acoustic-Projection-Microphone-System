#!/usr/bin/env python3
"""
APM REST API Server - Main Entry Point

Starts the FastAPI backend with global node access, allowing
all nodes in the network to access the REST API endpoints.

Usage:
    python3 backend/main.py                    # Default: 0.0.0.0:8080
    python3 backend/main.py --port 9000        # Custom port
    python3 backend/main.py --host 127.0.0.1   # Localhost only
    python3 backend/main.py --reload           # Development mode with auto-reload

Endpoints:
    GET  /health                      - Health check
    GET  /api/peers                   - List all peers
    GET  /api/peers/{peer_id}         - Get peer details
    POST /api/status                  - Update local peer status
    POST /api/session                 - Create new session
    GET  /api/session/{session_id}    - Get session status
    POST /api/session/{session_id}/accept  - Accept session
    POST /api/session/{session_id}/end     - End session
"""

import argparse
import logging
import os
import sys
from pathlib import Path

# Ensure backend module is importable
sys.path.insert(0, str(Path(__file__).parent.parent))

import backend.app as backend_app
from backend.telemetry import TelemetryClient

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%dT%H:%M:%S'
)
logger = logging.getLogger(__name__)


def _resolve_default_port() -> int:
    for env_key in ("PORT", "APM_API_PORT"):
        raw_value = os.environ.get(env_key)
        if not raw_value:
            continue
        try:
            port = int(raw_value)
        except ValueError:
            logger.warning("Ignoring invalid %s value: %s", env_key, raw_value)
            continue
        if 1 <= port <= 65535:
            return port
        logger.warning("Ignoring out-of-range %s value: %s", env_key, raw_value)
    return 8080


def main():
    parser = argparse.ArgumentParser(
        description='APM REST API Server - Global Node Access',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument(
        '--host',
        default='0.0.0.0',
        help='Host to bind to (default: 0.0.0.0 for global access)'
    )
    parser.add_argument(
        '--port',
        type=int,
        default=_resolve_default_port(),
        help='Port to bind to (default: PORT env var, else APM_API_PORT env var, else 8080)'
    )
    parser.add_argument(
        '--reload',
        action='store_true',
        help='Enable auto-reload for development'
    )
    parser.add_argument(
        '--log-level',
        choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
        default='INFO',
        help='Logging level (default: INFO)'
    )
    
    args = parser.parse_args()

    # Validate port range
    if not (1 <= args.port <= 65535):
        logger.error(f"Invalid port {args.port}: must be between 1 and 65535")
        sys.exit(1)

    # Update logging level
    logging.getLogger().setLevel(getattr(logging, args.log_level))
    
    # Log startup information
    logger.info("=" * 60)
    logger.info("APM REST API Server - Starting")
    logger.info("=" * 60)
    bind_host = "0.0.0.0"
    logger.info(f"Host: {bind_host}")
    logger.info(f"Port: {args.port}")
    logger.info(f"Reload: {args.reload}")
    logger.info(f"Log Level: {args.log_level}")
    
    logger.info("🌐 Global node access enabled - API accessible from network")
    
    logger.info("=" * 60)
    logger.info("")
    logger.info("Available endpoints:")
    logger.info(f"  http://<your-ip>:{args.port}/health")
    logger.info(f"  http://<your-ip>:{args.port}/api/peers")
    logger.info(f"  http://<your-ip>:{args.port}/docs (API docs)")
    logger.info("")
    
    try:
        import uvicorn
        backend_app.app.state.telemetry_client = TelemetryClient()
        uvicorn.run(
            "backend.app:app",
            host=bind_host,
            port=args.port,
            reload=args.reload,
            log_level=args.log_level.lower(),
            access_log=True
        )
    except ImportError:
        logger.error("uvicorn is not installed. Install with: pip install uvicorn[standard]")
        sys.exit(1)
    except KeyboardInterrupt:
        logger.info("\nShutting down gracefully...")
        sys.exit(0)
    except Exception as e:
        logger.error(f"Failed to start server: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
