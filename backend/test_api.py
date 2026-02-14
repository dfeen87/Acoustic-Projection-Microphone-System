#!/usr/bin/env python3
"""
Test the APM REST API endpoints
"""

import requests
import time
import sys

BASE_URL = "http://127.0.0.1:8080"

def test_health():
    """Test health endpoint"""
    try:
        response = requests.get(f"{BASE_URL}/health", timeout=5)
        assert response.status_code == 200
        assert response.json() == {"ok": True}
        print("✓ Health endpoint working")
        return True
    except Exception as e:
        print(f"✗ Health endpoint failed: {e}")
        return False

def test_peers():
    """Test peers endpoint"""
    try:
        response = requests.get(f"{BASE_URL}/api/peers", timeout=5)
        assert response.status_code == 200
        data = response.json()
        assert "peers" in data
        print(f"✓ Peers endpoint working ({len(data['peers'])} peers)")
        return True
    except Exception as e:
        print(f"✗ Peers endpoint failed: {e}")
        return False

def test_status_update():
    """Test status update endpoint"""
    try:
        response = requests.post(
            f"{BASE_URL}/api/status",
            json={"status": "online"},
            timeout=5
        )
        assert response.status_code == 200
        data = response.json()
        assert data["ok"] == True
        print("✓ Status update endpoint working")
        return True
    except Exception as e:
        print(f"✗ Status update endpoint failed: {e}")
        return False

def test_session_create():
    """Test session creation"""
    try:
        # First get a peer
        peers_response = requests.get(f"{BASE_URL}/api/peers", timeout=5)
        peers = peers_response.json()["peers"]
        if len(peers) < 2:
            print("⚠ Not enough peers to test session creation")
            return True
        
        # Pick a non-local peer
        peer_id = None
        for peer in peers:
            if peer["id"] != "You" and not peer["id"].startswith("local-"):
                peer_id = peer["id"]
                break
        
        if not peer_id:
            print("⚠ No suitable peer found for session test")
            return True
        
        response = requests.post(
            f"{BASE_URL}/api/session",
            params={"peer_id": peer_id},
            timeout=5
        )
        assert response.status_code == 200
        data = response.json()
        assert "session" in data
        print(f"✓ Session creation working (session: {data['session']['id']})")
        return True
    except Exception as e:
        print(f"✗ Session creation failed: {e}")
        return False

def main():
    print("=" * 60)
    print("APM REST API - Test Suite")
    print("=" * 60)
    print(f"Testing against: {BASE_URL}")
    print()
    
    # Check if server is running
    try:
        requests.get(f"{BASE_URL}/health", timeout=2)
    except Exception:
        print(f"✗ Server not running at {BASE_URL}")
        print("\nPlease start the server first:")
        print("  python3 backend/main.py")
        sys.exit(1)
    
    tests = [
        test_health,
        test_peers,
        test_status_update,
        test_session_create,
    ]
    
    passed = 0
    for test in tests:
        if test():
            passed += 1
        time.sleep(0.5)
    
    print()
    print("=" * 60)
    print(f"Results: {passed}/{len(tests)} tests passed")
    print("=" * 60)
    
    if passed == len(tests):
        print("\n✓ All tests passed!")
        sys.exit(0)
    else:
        print(f"\n✗ {len(tests) - passed} test(s) failed")
        sys.exit(1)

if __name__ == "__main__":
    main()
