#!/usr/bin/env python3
"""
apm_peer_integration.py
Integration module for Python Tkinter GUI
"""

import requests
import threading
import time
from typing import List, Dict, Optional, Callable
from dataclasses import dataclass


@dataclass
class Peer:
    """Peer information"""
    id: str
    name: str
    ip: str
    port: int
    last_seen_ago: float
    status: str
    
    @classmethod
    def from_dict(cls, data: dict) -> 'Peer':
        return cls(
            id=data['id'],
            name=data['name'],
            ip=data['ip'],
            port=data['port'],
            last_seen_ago=data['last_seen_ago'],
            status=data['status']
        )


class PeerDiscoveryClient:
    """Client for peer discovery service"""
    
    def __init__(self, api_base: str = "http://localhost:8080"):
        self.api_base = api_base
        self.monitoring = False
        self.monitor_thread = None
        self.session = requests.Session()
        self.session.timeout = 5
        
    def get_peers(self, include_offline: bool = False) -> List[Peer]:
        """Fetch all peers"""
        try:
            params = {}
            if include_offline:
                params['include_offline'] = 'true'
            
            response = self.session.get(
                f"{self.api_base}/api/peers",
                params=params
            )
            response.raise_for_status()
            
            data = response.json()
            return [Peer.from_dict(p) for p in data.get('peers', [])]
            
        except Exception as e:
            print(f"Error fetching peers: {e}")
            return []
    
    def get_peer(self, peer_id: str) -> Optional[Peer]:
        """Get specific peer by ID"""
        try:
            response = self.session.get(f"{self.api_base}/api/peers/{peer_id}")
            response.raise_for_status()
            
            data = response.json()
            return Peer.from_dict(data['peer'])
            
        except Exception as e:
            print(f"Error fetching peer {peer_id}: {e}")
            return None
    
    def update_status(self, status: str) -> bool:
        """Update local status"""
        try:
            response = self.session.post(
                f"{self.api_base}/api/status",
                json={'status': status}
            )
            response.raise_for_status()
            return True
            
        except Exception as e:
            print(f"Error updating status: {e}")
            return False
    
    def get_session_status(self, session_id: str) -> Optional[Dict]:
        """Get call session status"""
        try:
            response = self.session.get(f"{self.api_base}/api/session/{session_id}")
            response.raise_for_status()
            
            data = response.json()
            return data.get('session')
            
        except Exception as e:
            print(f"Error fetching session {session_id}: {e}")
            return None
    
    def start_monitoring(self, callback: Callable[[List[Peer]], None], 
                        interval: float = 3.0):
        """Start monitoring peers with callback"""
        if self.monitoring:
            return
        
        self.monitoring = True
        
        def monitor_loop():
            while self.monitoring:
                peers = self.get_peers()
                if callback:
                    callback(peers)
                time.sleep(interval)
        
        self.monitor_thread = threading.Thread(target=monitor_loop, daemon=True)
        self.monitor_thread.start()
    
    def stop_monitoring(self):
        """Stop monitoring"""
        self.monitoring = False
        if self.monitor_thread and self.monitor_thread.is_alive():
            self.monitor_thread.join(timeout=2)
    
    def ping(self) -> bool:
        """Check if service is reachable"""
        try:
            response = self.session.get(f"{self.api_base}/api/peers", timeout=2)
            return response.status_code == 200
        except Exception:
            return False


# Enhanced APMControlPanel with peer discovery integration
class EnhancedAPMControlPanel:
    """Enhanced control panel with real peer discovery"""
    
    def __init__(self, root):
        # ... (keep existing __init__ code)
        
        # Add peer discovery client
        self.peer_client = PeerDiscoveryClient("http://localhost:8080")
        self.discovered_peers = []
        
        # Check service connection on startup
        self.check_peer_service()
        
        # Start monitoring peers
        self.start_peer_monitoring()
    
    def check_peer_service(self):
        """Check if peer discovery service is running"""
        if self.peer_client.ping():
            self.add_translation("System", "✓ Peer discovery service connected")
            self.backend_connected = True
            self.status_label.config(
                text="● Connected",
                fg=self.accent_green
            )
        else:
            self.add_translation("System", "✗ Peer discovery service offline")
            self.backend_connected = False
            self.status_label.config(
                text="● Disconnected",
                fg=self.accent_red
            )
    
    def start_peer_monitoring(self):
        """Start monitoring discovered peers"""
        def on_peers_update(peers: List[Peer]):
            self.discovered_peers = peers
            # Update UI on main thread
            self.root.after(0, self.update_peers_display, peers)
        
        self.peer_client.start_monitoring(on_peers_update, interval=3.0)
    
    def update_peers_display(self, peers: List[Peer]):
        """Update the peer list display"""
        # This would update your peer list UI
        # For example, refreshing a listbox or treeview
        pass
    
    def on_status_change(self, new_status: str):
        """Handle status change"""
        if self.peer_client.update_status(new_status):
            self.add_translation("System", f"Status changed to: {new_status}")
        else:
            self.add_translation("System", "Failed to update status")
    
    def initiate_call_with_peer(self, peer: Peer):
        """Start a call with discovered peer"""
        self.call_active = True
        self.call_status_label.config(text=f"Calling {peer.name}...")
        
        # Create session and start monitoring
        session_id = f"session-{int(time.time())}"
        
        def monitor_session():
            while self.call_active:
                status = self.peer_client.get_session_status(session_id)
                if status:
                    if status['status'] == 'timeout':
                        self.root.after(0, self.on_call_timeout, peer)
                        break
                    
                    # Update RTT display
                    rtt = status.get('rtt', 0)
                    self.root.after(0, self.update_call_quality, rtt)
                
                time.sleep(5)
        
        threading.Thread(target=monitor_session, daemon=True).start()
    
    def on_call_timeout(self, peer: Peer):
        """Handle call timeout"""
        self.add_translation("System", f"⚠ Connection lost with {peer.name}")
        self.call_active = False
        self.call_btn.config(
            text="📞 Start Call",
            bg=self.accent_green
        )
    
    def update_call_quality(self, rtt: float):
        """Update call quality indicator based on RTT"""
        if rtt < 50:
            quality = "Excellent"
            color = self.accent_green
        elif rtt < 100:
            quality = "Good"
            color = self.accent_purple
        elif rtt < 200:
            quality = "Fair"
            color = "#f59e0b"
        else:
            quality = "Poor"
            color = self.accent_red
        
        # Update quality label if it exists
        # self.quality_label.config(text=f"Quality: {quality} ({rtt:.1f}ms)", fg=color)
    
    def cleanup(self):
        """Cleanup on exit"""
        self.peer_client.stop_monitoring()


# Example standalone test
if __name__ == "__main__":
    import tkinter as tk
    
    print("Testing peer discovery client...")
    
    client = PeerDiscoveryClient()
    
    # Check connection
    if client.ping():
        print("✓ Connected to peer service")
    else:
        print("✗ Peer service not available")
        exit(1)
    
    # Get peers
    peers = client.get_peers()
    print(f"\nDiscovered {len(peers)} peers:")
    for peer in peers:
        print(f"  - {peer.name} ({peer.ip}) - {peer.status} - {peer.last_seen_ago:.1f}s ago")
    
    # Update status
    if client.update_status("online"):
        print("\n✓ Status updated to online")
    
    # Monitor peers for 10 seconds
    print("\nMonitoring peers for 10 seconds...")
    
    def on_update(peers):
        print(f"[{time.strftime('%H:%M:%S')}] Active peers: {len(peers)}")
    
    client.start_monitoring(on_update, interval=2.0)
    time.sleep(10)
    client.stop_monitoring()
    
    print("\nTest complete!")
