# APM REST API - Global Node Access

This directory contains the FastAPI-based REST API server for the Acoustic Projection Microphone System. The API provides endpoints for peer discovery, session management, and system status monitoring across all nodes in the network.

## Features

- 🌐 **Global Node Access** - Accessible from any node in the network (binds to 0.0.0.0)
- 🔒 **Localhost Mode** - Can be restricted to localhost for security
- 📊 **Health Monitoring** - Built-in health check endpoints
- 🔄 **Auto-Reload** - Development mode with automatic code reloading
- 📝 **API Documentation** - Interactive Swagger/OpenAPI docs at `/docs`

## Quick Start

### Using Shell Scripts (Recommended)

**Linux/Mac:**
```bash
./scripts/start-api.sh                  # Start with global access (0.0.0.0:8080)
APM_API_PORT=9000 ./scripts/start-api.sh    # Custom port
./scripts/start-api.sh --reload         # Development mode
```

**Windows:**
```batch
scripts\start-api.bat                   # Start with global access (0.0.0.0:8080)
set APM_API_PORT=9000 && scripts\start-api.bat   # Custom port
scripts\start-api.bat --reload          # Development mode
```

### Using Python Directly

```bash
# Install dependencies first
pip install -r backend/requirements.txt

# Start with global access
python3 backend/main.py

# Start with custom configuration
python3 backend/main.py --host 0.0.0.0 --port 8080 --reload

# Localhost only
python3 backend/main.py --host 127.0.0.1 --port 8080
```

## Configuration

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `APM_API_HOST` | `0.0.0.0` | Host to bind to |
| `APM_API_PORT` | `8080` | Port to bind to |

### Command-Line Arguments

```bash
python3 backend/main.py --help
```

| Argument | Default | Description |
|----------|---------|-------------|
| `--host` | `0.0.0.0` | Host to bind to (use 127.0.0.1 for localhost only) |
| `--port` | `8080` | Port to bind to |
| `--reload` | `false` | Enable auto-reload for development |
| `--log-level` | `INFO` | Logging level (DEBUG, INFO, WARNING, ERROR) |

## API Endpoints

### Health & Status

- **GET `/health`** - Health check endpoint
  ```json
  {"ok": true}
  ```

### Peer Management

- **GET `/api/peers`** - List all discovered peers
  ```json
  {
    "peers": [
      {
        "id": "peer-abc123",
        "name": "Alice Cooper",
        "ip": "192.168.1.101",
        "status": "online",
        "last_seen_ago": 2.5
      }
    ]
  }
  ```

- **GET `/api/peers/{peer_id}`** - Get details of a specific peer
  ```json
  {
    "peer": {
      "id": "peer-abc123",
      "name": "Alice Cooper",
      "ip": "192.168.1.101",
      "status": "online"
    }
  }
  ```

- **POST `/api/status`** - Update local peer status
  ```json
  // Request
  {"status": "online"}  // or "away", "busy"
  
  // Response
  {"ok": true, "peer": {"id": "local-xyz", "status": "online"}}
  ```

### Session Management

- **POST `/api/session`** - Create a new session
  ```
  POST /api/session?peer_id=peer-abc123
  ```
  ```json
  {
    "session": {
      "id": "session-1234567890",
      "peer_id": "peer-abc123",
      "status": "calling",
      "created_at": 1645123456.789,
      "updated_at": 1645123456.789
    }
  }
  ```

- **GET `/api/session/{session_id}`** - Get session status
  ```json
  {
    "session": {
      "id": "session-1234567890",
      "peer_id": "peer-abc123",
      "status": "connected",
      "created_at": 1645123456.789,
      "updated_at": 1645123460.123
    }
  }
  ```

- **POST `/api/session/{session_id}/accept`** - Accept a session
  ```json
  {"ok": true, "session": {...}}
  ```

- **POST `/api/session/{session_id}/end`** - End a session
  ```json
  {"ok": true, "session": {...}}
  ```

## Interactive API Documentation

Once the server is running, visit:

- **Swagger UI**: http://localhost:8080/docs
- **ReDoc**: http://localhost:8080/redoc

## Global Node Access

By default, the API server binds to `0.0.0.0`, making it accessible from any node in the network. This is essential for:

- **Peer Discovery**: Other nodes need to query this node's peer list
- **Session Management**: Remote nodes need to initiate and manage sessions
- **Health Monitoring**: Network-wide health checks

### Security Considerations

When enabling global node access:

1. **Firewall**: Ensure your firewall allows traffic on the configured port (default: 8080)
2. **Network**: Only expose the API on trusted networks
3. **Localhost Mode**: For local-only testing, use `--host 127.0.0.1`

Example for localhost-only access:
```bash
python3 backend/main.py --host 127.0.0.1 --port 8080
```

## Testing the API

### Using curl

```bash
# Health check
curl http://localhost:8080/health

# List peers
curl http://localhost:8080/api/peers

# Get specific peer
curl http://localhost:8080/api/peers/peer-abc123

# Update status
curl -X POST http://localhost:8080/api/status \
  -H "Content-Type: application/json" \
  -d '{"status": "online"}'

# Create session
curl -X POST "http://localhost:8080/api/session?peer_id=peer-abc123"
```

### Using Python requests

```python
import requests

# Health check
response = requests.get("http://localhost:8080/health")
print(response.json())

# List peers
response = requests.get("http://localhost:8080/api/peers")
print(response.json())

# Update status
response = requests.post(
    "http://localhost:8080/api/status",
    json={"status": "online"}
)
print(response.json())
```

## Development

### Running in Development Mode

With auto-reload enabled, the server will automatically restart when code changes:

```bash
python3 backend/main.py --reload --log-level DEBUG
```

### Running Tests

```bash
# Run backend tests
python3 -m pytest backend/test_storage.py

# Run integration tests (requires server to be running)
cd launcher && npm test
```

## Integration with APM System

The REST API integrates with the full APM system:

1. **Production Launcher**: `launcher/apm_launcher.js` starts the complete system including the REST API
2. **Peer Discovery**: `backend/peer/apm_peer_service.py` uses UDP broadcast for peer discovery
3. **WebSocket Signaling**: `backend/signaling/signaling.py` provides real-time communication

To start the complete system:
```bash
./scripts/start-apm.sh
```

## Troubleshooting

### Port Already in Use

```
Error: [Errno 48] Address already in use
```

**Solution**: Change the port or kill the process using the port
```bash
# Find process on port 8080
lsof -i :8080  # Linux/Mac
netstat -ano | findstr :8080  # Windows

# Use different port
python3 backend/main.py --port 8081
```

### Cannot Connect from Other Nodes

**Check host binding**:
```bash
# Should be 0.0.0.0 for global access, not 127.0.0.1
python3 backend/main.py --host 0.0.0.0
```

**Check firewall**:
```bash
# Linux: Allow port 8080
sudo ufw allow 8080

# Check if port is open
netstat -an | grep 8080
```

### Dependencies Not Installed

```
ModuleNotFoundError: No module named 'uvicorn'
```

**Solution**:
```bash
pip install -r backend/requirements.txt
```

## Architecture

The REST API follows the architecture guidelines in `/docs/extension_points.md`:

- **Non-blocking**: API operations don't block DSP processing
- **Graceful degradation**: Network failures don't crash the system
- **Clear boundaries**: API layer is isolated from DSP core
- **Explicit failure modes**: All errors return proper HTTP status codes

## License

MIT License - Copyright (c) 2025 Don Michael Feeney Jr.

## See Also

- [Extension Points](../docs/extension_points.md) - Architecture guidelines
- [Signaling Contract](../docs/signaling/SIGNALING_CONTRACT.md) - WebSocket protocol
- [Main README](../README.md) - Full system documentation
