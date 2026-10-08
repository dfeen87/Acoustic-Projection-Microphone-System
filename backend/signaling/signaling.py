"""Standalone signaling service, using the shared authenticated protocol."""
from fastapi import FastAPI
from backend.signaling.ws_signaling import signaling_ws

app = FastAPI()
app.add_api_websocket_route("/ws", signaling_ws)
