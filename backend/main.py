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
import sys
from pathlib import Path

# Ensure backend module is importable
sys.path.insert(0, str(Path(__file__).parent.parent))

logging.basicConfig(
    level=logging.INFO,
    format='[%(asctime)s] [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%dT%H:%M:%S'
)
logger = logging.getLogger(__name__)


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
        default=8080,
        help='Port to bind to (default: 8080)'
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
    
    # Update logging level
    logging.getLogger().setLevel(getattr(logging, args.log_level))
    
    # Log startup information
    logger.info("=" * 60)
    logger.info("APM REST API Server - Starting")
    logger.info("=" * 60)
    logger.info(f"Host: {args.host}")
    logger.info(f"Port: {args.port}")
    logger.info(f"Reload: {args.reload}")
    logger.info(f"Log Level: {args.log_level}")
    
    if args.host == '0.0.0.0':
        logger.info("🌐 Global node access enabled - API accessible from network")
    else:
        logger.info(f"🔒 API accessible only from {args.host}")
    
    logger.info("=" * 60)
    logger.info("")
    logger.info("Available endpoints:")
    logger.info(f"  http://{args.host if args.host != '0.0.0.0' else '<your-ip>'}:{args.port}/health")
    logger.info(f"  http://{args.host if args.host != '0.0.0.0' else '<your-ip>'}:{args.port}/api/peers")
    logger.info(f"  http://{args.host if args.host != '0.0.0.0' else '<your-ip>'}:{args.port}/docs (API docs)")
    logger.info("")
    
    try:
        import uvicorn
        uvicorn.run(
            "backend.app:app",
            host=args.host,
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
