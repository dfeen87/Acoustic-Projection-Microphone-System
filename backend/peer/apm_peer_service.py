#!/usr/bin/env python3
"""
apm_peer_service.py - APM Peer Discovery & Heartbeat Service
Handles peer discovery, status monitoring, and heartbeat management

Usage:
    python3 apm_peer_service.py
    
API Endpoints:
    http://localhost:8080/api/peers
    http://localhost:8080/api/peers/{peer_id}
    http://localhost:8080/api/status
    http://localhost:8080/api/session/{session_id}
"""

import asyncio
import socket
import json
import time
from dataclasses import dataclass, asdict
from typing import Dict, Set, Optional
from datetime import datetime, timedelta
import logging

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)


@dataclass
class Peer:
    """Represents a peer in the network"""
    id: str
    name: str
    ip: str
    port: int
    last_seen: float
    status: str  # online, away, offline
    capabilities: Dict[str, bool]
    
    def to_dict(self):
        return {
            **asdict(self),
            'last_seen_ago': time.time() - self.last_seen
        }


class PeerDiscoveryService:
    """Handles peer discovery via UDP broadcast"""
    
    def __init__(self, port=5060, broadcast_port=5061):
        self.port = port
        self.broadcast_port = broadcast_port
        self.peers: Dict[str, Peer] = {}
        self.local_peer: Optional[Peer] = None
        self.running = False
        self.broadcast_interval = 5  # seconds
        self.heartbeat_interval = 5  # seconds - Fixed: was missing
        self.heartbeat_timeout = 15  # seconds
        self.cleanup_interval = 10  # seconds
        
    async def start(self, local_name: str, capabilities: Dict[str, bool]):
        """Start the peer discovery service"""
        # Initialize local peer
        local_ip = self._get_local_ip()
        self.local_peer = Peer(
            id=f"peer-{local_ip.replace('.', '-')}",
            name=local_name,
            ip=local_ip,
            port=self.port,
            last_seen=time.time(),
            status='online',
            capabilities=capabilities
        )
        
        self.running = True
        logger.info(f"Starting peer discovery service on {local_ip}:{self.port}")
        
        # Start tasks
        await asyncio.gather(
            self._broadcast_presence(),
            self._listen_for_peers(),
            self._cleanup_stale_peers(),
            self._heartbeat_monitor()
        )
    
    def _get_local_ip(self) -> str:
        """Get local IP address"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(("8.8.8.8", 80))
            ip = s.getsockname()[0]
            s.close()
            return ip
        except Exception as e:
            logger.warning(f"Failed to get local IP: {e}")
            return "127.0.0.1"
    
    async def _broadcast_presence(self):
        """Broadcast presence to network"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        
        while self.running:
            try:
                message = {
                    'type': 'peer_announce',
                    'peer': asdict(self.local_peer),
                    'timestamp': time.time()
                }
                
                data = json.dumps(message).encode('utf-8')
                sock.sendto(data, ('<broadcast>', self.broadcast_port))
                logger.debug(f"Broadcast presence: {self.local_peer.name}")
                
            except Exception as e:
                logger.error(f"Broadcast error: {e}")
            
            await asyncio.sleep(self.broadcast_interval)
        
        sock.close()
    
    async def _listen_for_peers(self):
        """Listen for peer announcements"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind(('', self.broadcast_port))
        sock.setblocking(False)
        
        logger.info(f"Listening for peers on port {self.broadcast_port}")
        
        while self.running:
            try:
                data, addr = await asyncio.get_event_loop().sock_recvfrom(sock, 4096)
                message = json.loads(data.decode('utf-8'))
                
                if message['type'] == 'peer_announce':
                    peer_data = message['peer']
                    
                    # Don't add ourselves
                    if peer_data['id'] == self.local_peer.id:
                        continue
                    
                    peer = Peer(**peer_data)
                    peer.last_seen = time.time()
                    
                    # New peer discovered
                    if peer.id not in self.peers:
                        logger.info(f"Discovered new peer: {peer.name} ({peer.ip})")
                    
                    self.peers[peer.id] = peer
                    
                elif message['type'] == 'heartbeat':
                    peer_id = message['peer_id']
                    if peer_id in self.peers:
                        self.peers[peer_id].last_seen = time.time()
                        self.peers[peer_id].status = message.get('status', 'online')
                
            except asyncio.CancelledError:
                break
            except BlockingIOError:
                await asyncio.sleep(0.1)
            except Exception as e:
                logger.error(f"Listen error: {e}")
                await asyncio.sleep(0.1)
        
        sock.close()
    
    async def _cleanup_stale_peers(self):
        """Remove peers that haven't been seen recently"""
        while self.running:
            await asyncio.sleep(self.cleanup_interval)
            
            current_time = time.time()
            stale_peers = []
            
            for peer_id, peer in self.peers.items():
                time_since_seen = current_time - peer.last_seen
                
                if time_since_seen > self.heartbeat_timeout * 2:
                    # Mark as offline
                    if peer.status != 'offline':
                        logger.info(f"Peer {peer.name} went offline")
                        peer.status = 'offline'
                        stale_peers.append(peer_id)
                elif time_since_seen > self.heartbeat_timeout:
                    # Mark as away
                    if peer.status == 'online':
                        logger.info(f"Peer {peer.name} is away")
                        peer.status = 'away'
            
            # Remove offline peers after extended period
            for peer_id in stale_peers:
                if current_time - self.peers[peer_id].last_seen > 60:
                    logger.info(f"Removing stale peer: {self.peers[peer_id].name}")
                    del self.peers[peer_id]
    
    async def _heartbeat_monitor(self):
        """Monitor and send heartbeats"""
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        
        while self.running:
            try:
                # Send lightweight heartbeat
                message = {
                    'type': 'heartbeat',
                    'peer_id': self.local_peer.id,
                    'status': self.local_peer.status,
                    'timestamp': time.time()
                }
                
                data = json.dumps(message).encode('utf-8')
                sock.sendto(data, ('<broadcast>', self.broadcast_port))
                
            except Exception as e:
                logger.error(f"Heartbeat error: {e}")
            
            await asyncio.sleep(self.heartbeat_interval)
        
        sock.close()
    
    def get_peers(self, include_offline=False) -> list:
        """Get list of discovered peers"""
        peers = []
        for peer in self.peers.values():
            if include_offline or peer.status != 'offline':
                peers.append(peer.to_dict())
        return peers
    
    def get_peer(self, peer_id: str) -> Optional[Dict]:
        """Get specific peer by ID"""
        peer = self.peers.get(peer_id)
        return peer.to_dict() if peer else None
    
    def update_local_status(self, status: str):
        """Update local peer status"""
        if self.local_peer:
            self.local_peer.status = status
            self.local_peer.last_seen = time.time()
            logger.info(f"Local status updated to: {status}")
    
    async def stop(self):
        """Stop the service"""
        logger.info("Stopping peer discovery service")
        self.running = False


class HeartbeatManager:
    """Manages heartbeat for active call sessions"""
    
    def __init__(self):
        self.active_sessions: Dict[str, dict] = {}
        self.heartbeat_interval = 5  # seconds
        self.timeout_threshold = 15  # seconds
        
    def start_session(self, session_id: str, peer_ip: str, peer_port: int):
        """Start heartbeat monitoring for a session"""
        self.active_sessions[session_id] = {
            'peer_ip': peer_ip,
            'peer_port': peer_port,
            'last_heartbeat': time.time(),
            'status': 'active',
            'rtt': 0  # Round-trip time
        }
        logger.info(f"Started heartbeat for session {session_id}")
    
    async def monitor_session(self, session_id: str, on_timeout_callback):
        """Monitor heartbeat for a specific session"""
        if session_id not in self.active_sessions:
            return
        
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        session = self.active_sessions[session_id]
        
        while session_id in self.active_sessions:
            try:
                # Send heartbeat ping
                message = {
                    'type': 'session_heartbeat',
                    'session_id': session_id,
                    'timestamp': time.time()
                }
                
                data = json.dumps(message).encode('utf-8')
                send_time = time.time()
                
                sock.sendto(data, (session['peer_ip'], session['peer_port']))
                
                # Wait for response (with timeout)
                sock.settimeout(2.0)
                try:
                    response, _ = sock.recvfrom(1024)
                    recv_time = time.time()
                    
                    session['last_heartbeat'] = recv_time
                    session['rtt'] = (recv_time - send_time) * 1000  # ms
                    session['status'] = 'active'
                    
                except socket.timeout:
                    # Check if we've exceeded timeout threshold
                    if time.time() - session['last_heartbeat'] > self.timeout_threshold:
                        logger.warning(f"Session {session_id} timeout!")
                        session['status'] = 'timeout'
                        if on_timeout_callback:
                            await on_timeout_callback(session_id)
                
            except Exception as e:
                logger.error(f"Heartbeat error for session {session_id}: {e}")
            
            await asyncio.sleep(self.heartbeat_interval)
        
        sock.close()
    
    def get_session_status(self, session_id: str) -> Optional[dict]:
        """Get status of a session"""
        return self.active_sessions.get(session_id)
    
    def end_session(self, session_id: str):
        """Stop heartbeat monitoring for a session"""
        if session_id in self.active_sessions:
            del self.active_sessions[session_id]
            logger.info(f"Ended heartbeat for session {session_id}")


# REST API Server (using aiohttp)
class PeerAPIServer:
    """HTTP API for peer management"""
    
    def __init__(self, discovery_service: PeerDiscoveryService, 
                 heartbeat_manager: HeartbeatManager, port=8080):
        self.discovery = discovery_service
        self.heartbeat = heartbeat_manager
        self.port = port
        self.app = None
    
    async def start(self):
        """Start the API server"""
        try:
            from aiohttp import web
            
            self.app = web.Application()
            self.app.router.add_get('/api/peers', self.get_peers)
            self.app.router.add_get('/api/peers/{peer_id}', self.get_peer)
            self.app.router.add_post('/api/status', self.update_status)
            self.app.router.add_get('/api/session/{session_id}', self.get_session)
            
            runner = web.AppRunner(self.app)
            await runner.setup()
            site = web.TCPSite(runner, '0.0.0.0', self.port)
            await site.start()
            
            logger.info(f"API server started on port {self.port}")
            
        except ImportError:
            logger.warning("aiohttp not installed, API server disabled")
    
    async def get_peers(self, request):
        """GET /api/peers - Get all peers"""
        from aiohttp import web
        include_offline = request.query.get('include_offline', 'false') == 'true'
        peers = self.discovery.get_peers(include_offline)
        return web.json_response({'peers': peers})
    
    async def get_peer(self, request):
        """GET /api/peers/{peer_id} - Get specific peer"""
        from aiohttp import web
        peer_id = request.match_info['peer_id']
        peer = self.discovery.get_peer(peer_id)
        if peer:
            return web.json_response({'peer': peer})
        return web.json_response({'error': 'Peer not found'}, status=404)
    
    async def update_status(self, request):
        """POST /api/status - Update local status"""
        from aiohttp import web
        data = await request.json()
        status = data.get('status')
        if status in ['online', 'away', 'busy']:
            self.discovery.update_local_status(status)
            return web.json_response({'success': True})
        return web.json_response({'error': 'Invalid status'}, status=400)
    
    async def get_session(self, request):
        """GET /api/session/{session_id} - Get session status"""
        from aiohttp import web
        session_id = request.match_info['session_id']
        session = self.heartbeat.get_session_status(session_id)
        if session:
            return web.json_response({'session': session})
        return web.json_response({'error': 'Session not found'}, status=404)


async def main():
    """Main entry point"""
    # Initialize services
    discovery = PeerDiscoveryService(port=5060, broadcast_port=5061)
    heartbeat = HeartbeatManager()
    api_server = PeerAPIServer(discovery, heartbeat, port=8080)
    
    # Start all services
    try:
        await asyncio.gather(
            discovery.start(
                local_name=socket.gethostname(),
                capabilities={
                    'encryption': True,
                    'translation': True,
                    'audio': True
                }
            ),
            api_server.start()
        )
    except KeyboardInterrupt:
        logger.info("Shutting down...")
        await discovery.stop()


if __name__ == "__main__":
    asyncio.run(main())
