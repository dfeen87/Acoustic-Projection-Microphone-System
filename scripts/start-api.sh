#!/bin/bash
# APM REST API Server Launcher
# Convenience script to start the FastAPI backend with global node access

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "$PROJECT_ROOT"

# Check if Python is available
if ! command -v python3 &> /dev/null; then
    echo "Error: python3 is not installed"
    exit 1
fi

# Check if uvicorn is available
if ! python3 -c "import uvicorn" 2>/dev/null; then
    echo "Installing backend dependencies..."
    pip3 install -r backend/requirements.txt
fi

# Parse arguments
HOST="${APM_API_HOST:-0.0.0.0}"
PORT="${APM_API_PORT:-8080}"
RELOAD=""

for arg in "$@"; do
    case $arg in
        --reload)
            RELOAD="--reload"
            ;;
        --host=*)
            HOST="${arg#*=}"
            ;;
        --port=*)
            PORT="${arg#*=}"
            ;;
    esac
done

echo "Starting APM REST API Server..."
echo "Host: $HOST"
echo "Port: $PORT"
echo ""

python3 backend/main.py --host "$HOST" --port "$PORT" $RELOAD
