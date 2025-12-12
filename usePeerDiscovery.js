// hooks/usePeerDiscovery.js
// React hook for peer discovery and heartbeat integration

import { useState, useEffect, useCallback, useRef } from 'react';

const API_BASE = 'http://localhost:8080';
const POLL_INTERVAL = 3000; // Poll every 3 seconds

export const usePeerDiscovery = () => {
  const [peers, setPeers] = useState([]);
  const [localPeer, setLocalPeer] = useState(null);
  const [isConnected, setIsConnected] = useState(false);
  const [error, setError] = useState(null);
  const pollIntervalRef = useRef(null);
  const sessionHeartbeatRef = useRef(null);

  // Fetch peers from the service
  const fetchPeers = useCallback(async () => {
    try {
      const response = await fetch(`${API_BASE}/api/peers`);
      
      if (!response.ok) {
        throw new Error('Failed to fetch peers');
      }
      
      const data = await response.json();
      setPeers(data.peers || []);
      setIsConnected(true);
      setError(null);
    } catch (err) {
      console.error('Error fetching peers:', err);
      setError(err.message);
      setIsConnected(false);
    }
  }, []);

  // Update local status
  const updateStatus = useCallback(async (status) => {
    try {
      const response = await fetch(`${API_BASE}/api/status`, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({ status }),
      });

      if (!response.ok) {
        throw new Error('Failed to update status');
      }

      const data = await response.json();
      return data;
    } catch (err) {
      console.error('Error updating status:', err);
      setError(err.message);
      throw err;
    }
  }, []);

  // Get specific peer details
  const getPeerDetails = useCallback(async (peerId) => {
    try {
      const response = await fetch(`${API_BASE}/api/peers/${peerId}`);
      
      if (!response.ok) {
        throw new Error('Peer not found');
      }
      
      const data = await response.json();
      return data.peer;
    } catch (err) {
      console.error('Error fetching peer details:', err);
      throw err;
    }
  }, []);

  // Monitor session heartbeat
  const monitorSession = useCallback(async (sessionId, onTimeout) => {
    // Clear any existing monitor
    if (sessionHeartbeatRef.current) {
      clearInterval(sessionHeartbeatRef.current);
    }

    // Poll session status
    sessionHeartbeatRef.current = setInterval(async () => {
      try {
        const response = await fetch(`${API_BASE}/api/session/${sessionId}`);
        
        if (response.ok) {
          const data = await response.json();
          const session = data.session;

          // Check for timeout status
          if (session.status === 'timeout' && onTimeout) {
            onTimeout(sessionId);
          }
        }
      } catch (err) {
        console.error('Session heartbeat error:', err);
      }
    }, 5000);
  }, []);

  // Stop monitoring session
  const stopSessionMonitor = useCallback(() => {
    if (sessionHeartbeatRef.current) {
      clearInterval(sessionHeartbeatRef.current);
      sessionHeartbeatRef.current = null;
    }
  }, []);

  // Start polling for peers
  useEffect(() => {
    // Initial fetch
    fetchPeers();

    // Set up polling
    pollIntervalRef.current = setInterval(fetchPeers, POLL_INTERVAL);

    // Cleanup
    return () => {
      if (pollIntervalRef.current) {
        clearInterval(pollIntervalRef.current);
      }
      if (sessionHeartbeatRef.current) {
        clearInterval(sessionHeartbeatRef.current);
      }
    };
  }, [fetchPeers]);

  return {
    peers,
    localPeer,
    isConnected,
    error,
    updateStatus,
    getPeerDetails,
    refreshPeers: fetchPeers,
    monitorSession,
    stopSessionMonitor,
  };
};

// Example usage component
export const PeerDiscoveryExample = () => {
  const {
    peers,
    isConnected,
    error,
    updateStatus,
    monitorSession,
    stopSessionMonitor,
  } = usePeerDiscovery();

  const [currentStatus, setCurrentStatus] = useState('online');
  const [activeSessionId, setActiveSessionId] = useState(null);

  const handleStatusChange = async (newStatus) => {
    try {
      await updateStatus(newStatus);
      setCurrentStatus(newStatus);
    } catch (err) {
      console.error('Failed to update status:', err);
    }
  };

  const handleStartCall = (peer) => {
    const sessionId = `session-${Date.now()}`;
    setActiveSessionId(sessionId);
    
    // Start monitoring the session
    monitorSession(sessionId, (id) => {
      console.log(`Session ${id} timed out!`);
      alert('Connection lost with peer!');
      handleEndCall();
    });
  };

  const handleEndCall = () => {
    stopSessionMonitor();
    setActiveSessionId(null);
  };

  return (
    <div className="p-4">
      <div className="mb-4">
        <h2 className="text-xl font-bold mb-2">Peer Discovery Status</h2>
        <div className="flex items-center gap-2">
          <div className={`w-3 h-3 rounded-full ${isConnected ? 'bg-green-500' : 'bg-red-500'}`} />
          <span>{isConnected ? 'Connected' : 'Disconnected'}</span>
        </div>
        {error && <p className="text-red-500 mt-2">{error}</p>}
      </div>

      <div className="mb-4">
        <h3 className="font-semibold mb-2">Your Status</h3>
        <select 
          value={currentStatus}
          onChange={(e) => handleStatusChange(e.target.value)}
          className="px-4 py-2 border rounded"
        >
          <option value="online">Online</option>
          <option value="away">Away</option>
          <option value="busy">Busy</option>
        </select>
      </div>

      <div>
        <h3 className="font-semibold mb-2">Available Peers ({peers.length})</h3>
        <div className="space-y-2">
          {peers.map((peer) => (
            <div key={peer.id} className="flex items-center justify-between p-3 bg-gray-100 rounded">
              <div>
                <p className="font-medium">{peer.name}</p>
                <p className="text-sm text-gray-600">{peer.ip}</p>
              </div>
              <div className="flex items-center gap-3">
                <div className={`w-2 h-2 rounded-full ${
                  peer.status === 'online' ? 'bg-green-500' :
                  peer.status === 'away' ? 'bg-yellow-500' :
                  'bg-gray-500'
                }`} />
                <span className="text-sm text-gray-600">
                  {Math.round(peer.last_seen_ago)}s ago
                </span>
                <button
                  onClick={() => handleStartCall(peer)}
                  disabled={!!activeSessionId}
                  className="px-3 py-1 bg-blue-500 text-white rounded disabled:opacity-50"
                >
                  Call
                </button>
              </div>
            </div>
          ))}
        </div>
      </div>

      {activeSessionId && (
        <div className="mt-4 p-4 bg-green-100 rounded">
          <p className="font-semibold">Active Call Session</p>
          <p className="text-sm text-gray-600">Session ID: {activeSessionId}</p>
          <button
            onClick={handleEndCall}
            className="mt-2 px-4 py-2 bg-red-500 text-white rounded"
          >
            End Call
          </button>
        </div>
      )}
    </div>
  );
};
