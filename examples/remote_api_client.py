#!/usr/bin/env python3
"""
Example: Using APM REST API from Remote Node

This example demonstrates how to access the APM REST API
from another node in the network with global node access enabled.
"""

import requests
import json

# Replace with the IP address of the node running the APM REST API
# When running locally for testing, use 127.0.0.1
# When accessing from another node, use the actual IP (e.g., 192.168.1.100)
API_HOST = "127.0.0.1"  # Change to actual IP for remote access
API_PORT = 8080

BASE_URL = f"http://{API_HOST}:{API_PORT}"

def discover_peers():
    """Discover all peers in the network"""
    print(f"Discovering peers from {BASE_URL}...")
    
    try:
        response = requests.get(f"{BASE_URL}/api/peers", timeout=5)
        response.raise_for_status()
        
        data = response.json()
        peers = data.get("peers", [])
        
        print(f"\nFound {len(peers)} peer(s):")
        for peer in peers:
            print(f"  - {peer['name']} ({peer['ip']}) - {peer['status']}")
            print(f"    Last seen: {peer['last_seen_ago']:.1f} seconds ago")
        
        return peers
        
    except requests.exceptions.RequestException as e:
        print(f"Error connecting to API: {e}")
        return []

def get_peer_details(peer_id):
    """Get details of a specific peer"""
    print(f"\nGetting details for peer: {peer_id}")
    
    try:
        response = requests.get(f"{BASE_URL}/api/peers/{peer_id}", timeout=5)
        response.raise_for_status()
        
        data = response.json()
        peer = data.get("peer")
        
        print(f"Peer details:")
        print(f"  ID: {peer['id']}")
        print(f"  Name: {peer['name']}")
        print(f"  IP: {peer['ip']}")
        print(f"  Status: {peer['status']}")
        
        return peer
        
    except requests.exceptions.RequestException as e:
        print(f"Error getting peer details: {e}")
        return None

def initiate_session(peer_id):
    """Initiate a session with a peer"""
    print(f"\nInitiating session with peer: {peer_id}")
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/session",
            params={"peer_id": peer_id},
            timeout=5
        )
        response.raise_for_status()
        
        data = response.json()
        session = data.get("session")
        
        print(f"Session created:")
        print(f"  Session ID: {session['id']}")
        print(f"  Status: {session['status']}")
        print(f"  Peer ID: {session['peer_id']}")
        
        return session
        
    except requests.exceptions.RequestException as e:
        print(f"Error creating session: {e}")
        return None

def update_status(status):
    """Update local peer status"""
    print(f"\nUpdating status to: {status}")
    
    try:
        response = requests.post(
            f"{BASE_URL}/api/status",
            json={"status": status},
            timeout=5
        )
        response.raise_for_status()
        
        data = response.json()
        print(f"Status updated: {data}")
        
        return data
        
    except requests.exceptions.RequestException as e:
        print(f"Error updating status: {e}")
        return None

def check_health():
    """Check if the API is healthy"""
    print(f"\nChecking API health at {BASE_URL}...")
    
    try:
        response = requests.get(f"{BASE_URL}/health", timeout=5)
        response.raise_for_status()
        
        if response.json().get("ok"):
            print("✓ API is healthy and accessible")
            return True
        else:
            print("✗ API returned unhealthy status")
            return False
            
    except requests.exceptions.RequestException as e:
        print(f"✗ Cannot connect to API: {e}")
        print("\nPossible solutions:")
        print(f"  1. Ensure the API server is running on {API_HOST}")
        print(f"  2. Check that the server is bound to 0.0.0.0 (global access)")
        print(f"  3. Verify firewall settings allow traffic on port {API_PORT}")
        print(f"  4. Confirm the IP address is correct")
        return False

def main():
    print("=" * 60)
    print("APM REST API - Remote Node Access Example")
    print("=" * 60)
    print(f"Connecting to: {BASE_URL}")
    print()
    
    # Step 1: Check health
    if not check_health():
        return
    
    # Step 2: Discover peers
    peers = discover_peers()
    
    if not peers:
        print("\nNo peers found")
        return
    
    # Step 3: Get details of the first non-local peer
    remote_peer = None
    for peer in peers:
        if not peer["id"].startswith("local-"):
            remote_peer = peer
            break
    
    if remote_peer:
        get_peer_details(remote_peer["id"])
        
        # Step 4: Initiate a session (optional)
        initiate_session(remote_peer["id"])
    
    # Step 5: Update status
    update_status("online")
    
    print("\n" + "=" * 60)
    print("Example completed successfully!")
    print("=" * 60)

if __name__ == "__main__":
    main()
