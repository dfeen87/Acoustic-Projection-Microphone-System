import React, { useState, useEffect, useRef } from 'react';
import { Phone, PhoneOff, PhoneIncoming, PhoneOutgoing, Mic, MicOff, Volume2, VolumeX, Shield, Lock, Unlock, Radio, Users, Settings, Activity, Globe, Wifi, WifiOff } from 'lucide-react';

// Mock API - Replace with actual backend calls
const API_BASE = 'http://localhost:8080';

const APMDashboard = () => {
  const [callState, setCallState] = useState('idle'); // idle, calling, ringing, connected
  const [activeSession, setActiveSession] = useState(null);
  const [micMuted, setMicMuted] = useState(false);
  const [speakerMuted, setSpeakerMuted] = useState(false);
  const [encryptionEnabled, setEncryptionEnabled] = useState(true);
  const [translationEnabled, setTranslationEnabled] = useState(true);
  const [audioLevel, setAudioLevel] = useState(0);
  const [peers, setPeers] = useState([]);
  const [translations, setTranslations] = useState([]);
  const [localParticipant, setLocalParticipant] = useState({
    id: 'local-' + Math.random().toString(36).substr(2, 9),
    name: 'You',
    ip: '192.168.1.100'
  });
  const [sourceLang, setSourceLang] = useState('en');
  const [targetLang, setTargetLang] = useState('es');
  const [showSettings, setShowSettings] = useState(false);
  const [connectionStatus, setConnectionStatus] = useState('connected');

  // New V7.0.0 State
  const [metrics, setMetrics] = useState({ peak_db: -96, rms_db: -96, snr_db: 0, latency_ms: 0 });
  const [diagnostics, setDiagnostics] = useState({ ok: true, message: 'OK' });
  const [profiles, setProfiles] = useState([]);
  const [activeProfile, setActiveProfile] = useState('Default');
  const [showCalibration, setShowCalibration] = useState(false);
  const [calibrationState, setCalibrationState] = useState({ step: 'Idle', progress: 0, result: null });

  // Fetch diagnostics (1Hz)
  useEffect(() => {
    const fetchDiagnostics = async () => {
      try {
        const diagRes = await fetch(`${API_BASE}/api/health/diagnostics`);
        if (diagRes.ok) setDiagnostics(await diagRes.json());
      } catch (e) {
        setConnectionStatus('disconnected');
      }
    };

    fetchDiagnostics();
    const interval = setInterval(fetchDiagnostics, 1000);
    return () => clearInterval(interval);
  }, []);

  // Fetch real-time telemetry metrics (4Hz)
  useEffect(() => {
    const fetchMetrics = async () => {
      try {
        const metRes = await fetch(`${API_BASE}/api/metrics`);
        if (metRes.ok) {
          const data = await metRes.json();
          setMetrics(data);

          if (data.offline) {
            setConnectionStatus('degraded');
          } else {
            setConnectionStatus('connected');
          }
        }
      } catch (e) {
        // Fallback handled in diagnostics fetch, but we might set degraded
        setConnectionStatus('degraded');
      }
    };

    fetchMetrics();
    const interval = setInterval(fetchMetrics, 250);
    return () => clearInterval(interval);
  }, []);

  // Fetch profiles
  useEffect(() => {
    const fetchProfiles = async () => {
      try {
        const res = await fetch(`${API_BASE}/api/profiles`);
        if (res.ok) {
          const data = await res.json();
          setProfiles(data.profiles);
          setActiveProfile(data.active);
        }
      } catch (e) {}
    };
    fetchProfiles();
  }, []);

  // Calibration polling
  useEffect(() => {
    if (!showCalibration) return;
    const interval = setInterval(async () => {
      try {
        const res = await fetch(`${API_BASE}/api/calibration`);
        if (res.ok) setCalibrationState(await res.json());
      } catch (e) {}
    }, 500);
    return () => clearInterval(interval);
  }, [showCalibration]);

  const handleCalibrationAction = async (action) => {
    try {
      await fetch(`${API_BASE}/api/calibration`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ action })
      });
    } catch (e) {}
  };

  // Audio visualization
  useEffect(() => {
    if (callState === 'connected') {
      const interval = setInterval(() => {
        setAudioLevel(Math.random() * 100);
      }, 100);
      return () => clearInterval(interval);
    } else {
      setAudioLevel(0);
    }
  }, [callState]);

  // Mock peer discovery
  useEffect(() => {
    const mockPeers = [
      { id: 'peer1', name: 'Alice Cooper', ip: '192.168.1.101', status: 'online' },
      { id: 'peer2', name: 'Bob Martinez', ip: '192.168.1.102', status: 'online' },
      { id: 'peer3', name: 'Carol Zhang', ip: '192.168.1.103', status: 'away' },
    ];
    setPeers(mockPeers);
  }, []);

  // Mock translation stream
  useEffect(() => {
    if (callState === 'connected' && translationEnabled) {
      const interval = setInterval(() => {
        const mockTranslations = [
          { orig: 'Hello, how are you?', trans: 'Hola, ¿cómo estás?', from: 'remote', time: Date.now() },
          { orig: 'I am doing great!', trans: '¡Estoy muy bien!', from: 'local', time: Date.now() },
          { orig: 'What time is the meeting?', trans: '¿A qué hora es la reunión?', from: 'remote', time: Date.now() },
        ];
        const randomTrans = mockTranslations[Math.floor(Math.random() * mockTranslations.length)];
        setTranslations(prev => [...prev.slice(-9), { ...randomTrans, time: Date.now() }]);
      }, 5000);
      return () => clearInterval(interval);
    }
  }, [callState, translationEnabled]);

  const initiateCall = (peer) => {
    setCallState('calling');
    setActiveSession({
      id: 'session-' + Date.now(),
      peer: peer,
      startTime: Date.now(),
      encrypted: encryptionEnabled
    });
    
    // Mock call connection
    setTimeout(() => {
      setCallState('connected');
    }, 2000);
  };

  const endCall = () => {
    setCallState('idle');
    setActiveSession(null);
    setTranslations([]);
  };

  const acceptCall = () => {
    setCallState('connected');
  };

  const rejectCall = () => {
    setCallState('idle');
    setActiveSession(null);
  };

  const languages = [
    { code: 'en', name: 'English' },
    { code: 'es', name: 'Spanish' },
    { code: 'fr', name: 'French' },
    { code: 'de', name: 'German' },
    { code: 'ja', name: 'Japanese' },
    { code: 'zh', name: 'Chinese' },
    { code: 'ar', name: 'Arabic' },
    { code: 'ru', name: 'Russian' },
    { code: 'pt', name: 'Portuguese' },
    { code: 'it', name: 'Italian' },
  ];

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900 text-white p-4">
      {/* Header */}
      <div className="max-w-7xl mx-auto mb-6">
        <div className="flex items-center justify-between bg-black/30 backdrop-blur-xl rounded-2xl p-4 border border-purple-500/20">
          <div className="flex items-center gap-4">
            <div className="relative">
              <Radio className="w-10 h-10 text-purple-400" />
              <div className="absolute -top-1 -right-1 w-3 h-3 bg-green-400 rounded-full animate-pulse" />
            </div>
            <div>
              <h1 className="text-2xl font-bold bg-gradient-to-r from-purple-400 to-pink-400 bg-clip-text text-transparent">
                APM System v7.0
              </h1>
              <p className="text-sm text-gray-400">Acoustic Projection & Translation</p>
            </div>
          </div>
          
          <div className="flex items-center gap-4">
            {!diagnostics.ok && (
              <div className="flex items-center gap-2 bg-red-500/20 px-3 py-1 rounded-lg border border-red-500/50">
                <span className="text-sm text-red-400">⚠️ {diagnostics.message}</span>
              </div>
            )}
            <div className="flex items-center gap-2 bg-green-500/20 px-3 py-1 rounded-lg">
              {connectionStatus === 'connected' ? <Wifi className="w-4 h-4 text-green-400" /> : <WifiOff className="w-4 h-4 text-red-400" />}
              <span className="text-sm">{connectionStatus === 'connected' ? 'Connected' : 'Offline'}</span>
            </div>
            
            <button 
              onClick={() => setShowSettings(!showSettings)}
              className="p-2 hover:bg-white/10 rounded-lg transition-colors settings-toggle"
            >
              <Settings className="w-5 h-5" />
            </button>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Left Panel - Call Controls & Status */}
        <div className="lg:col-span-2 space-y-6">
          {/* Active Call Panel */}
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-xl font-semibold flex items-center gap-2">
                <Activity className="w-5 h-5 text-purple-400" />
                Active Session
              </h2>
              {encryptionEnabled && (
                <div className="flex items-center gap-2 bg-green-500/20 px-3 py-1 rounded-lg">
                  <Lock className="w-4 h-4 text-green-400" />
                  <span className="text-sm">E2E Encrypted</span>
                </div>
              )}
            </div>

            {callState === 'idle' && (
              <div className="text-center py-12">
                <Phone className="w-16 h-16 mx-auto mb-4 text-gray-500" />
                <p className="text-gray-400 mb-2">No active call</p>
                <p className="text-sm text-gray-500">Select a peer to start a call</p>
              </div>
            )}

            {callState === 'calling' && (
              <div className="text-center py-12">
                <div className="relative inline-block mb-4">
                  <PhoneOutgoing className="w-16 h-16 text-purple-400 animate-pulse" />
                  <div className="absolute inset-0 w-16 h-16 border-4 border-purple-400 rounded-full animate-ping" />
                </div>
                <p className="text-lg mb-2">Calling {activeSession?.peer?.name}...</p>
                <p className="text-sm text-gray-400">{activeSession?.peer?.ip}</p>
                <button 
                  onClick={endCall}
                  className="mt-6 px-6 py-3 bg-red-500 hover:bg-red-600 rounded-xl transition-colors"
                >
                  Cancel
                </button>
              </div>
            )}

            {callState === 'ringing' && (
              <div className="text-center py-12">
                <div className="relative inline-block mb-4">
                  <PhoneIncoming className="w-16 h-16 text-green-400 animate-bounce" />
                </div>
                <p className="text-lg mb-2">Incoming call from {activeSession?.peer?.name}</p>
                <p className="text-sm text-gray-400 mb-6">{activeSession?.peer?.ip}</p>
                <div className="flex gap-4 justify-center">
                  <button 
                    onClick={acceptCall}
                    className="px-8 py-3 bg-green-500 hover:bg-green-600 rounded-xl transition-colors flex items-center gap-2"
                  >
                    <Phone className="w-5 h-5" />
                    Accept
                  </button>
                  <button 
                    onClick={rejectCall}
                    className="px-8 py-3 bg-red-500 hover:bg-red-600 rounded-xl transition-colors flex items-center gap-2"
                  >
                    <PhoneOff className="w-5 h-5" />
                    Reject
                  </button>
                </div>
              </div>
            )}

            {callState === 'connected' && activeSession && (
              <div>
                <div className="flex items-center justify-between mb-6 p-4 bg-purple-500/10 rounded-xl">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 rounded-full bg-gradient-to-br from-purple-400 to-pink-400 flex items-center justify-center text-xl font-bold">
                      {activeSession.peer.name.charAt(0)}
                    </div>
                    <div>
                      <p className="font-semibold">{activeSession.peer.name}</p>
                      <p className="text-sm text-gray-400">{activeSession.peer.ip}</p>
                    </div>
                  </div>
                  <div className="text-right">
                    <p className="text-sm text-gray-400">Duration</p>
                    <p className="text-lg font-mono">
                      {Math.floor((Date.now() - activeSession.startTime) / 60000)}:
                      {String(Math.floor(((Date.now() - activeSession.startTime) % 60000) / 1000)).padStart(2, '0')}
                    </p>
                  </div>
                </div>

                {/* Audio Visualization */}
                <div className="mb-6">
                  <div className="flex items-center gap-2 mb-2">
                    <Activity className="w-4 h-4 text-purple-400" />
                    <span className="text-sm text-gray-400">Audio Level</span>
                  </div>
                  <div className="h-16 bg-black/50 rounded-lg p-2 flex items-end gap-1">
                    {Array.from({ length: 40 }).map((_, i) => (
                      <div
                        key={i}
                        className="flex-1 bg-gradient-to-t from-purple-600 to-pink-500 rounded-sm transition-all duration-100"
                        style={{
                          height: `${Math.max(5, Math.random() * audioLevel)}%`,
                          opacity: Math.random() * 0.5 + 0.5
                        }}
                      />
                    ))}
                  </div>
                </div>

                {/* Controls */}
                <div className="flex gap-4 justify-center">
                  <button
                    onClick={() => setMicMuted(!micMuted)}
                    className={`p-4 rounded-xl transition-colors ${
                      micMuted ? 'bg-red-500 hover:bg-red-600' : 'bg-white/10 hover:bg-white/20'
                    }`}
                  >
                    {micMuted ? <MicOff className="w-6 h-6" /> : <Mic className="w-6 h-6" />}
                  </button>
                  
                  <button
                    onClick={() => setSpeakerMuted(!speakerMuted)}
                    className={`p-4 rounded-xl transition-colors ${
                      speakerMuted ? 'bg-red-500 hover:bg-red-600' : 'bg-white/10 hover:bg-white/20'
                    }`}
                  >
                    {speakerMuted ? <VolumeX className="w-6 h-6" /> : <Volume2 className="w-6 h-6" />}
                  </button>

                  <button
                    onClick={() => setTranslationEnabled(!translationEnabled)}
                    className={`p-4 rounded-xl transition-colors ${
                      translationEnabled ? 'bg-purple-500 hover:bg-purple-600' : 'bg-white/10 hover:bg-white/20'
                    }`}
                  >
                    <Globe className="w-6 h-6" />
                  </button>

                  <button
                    onClick={endCall}
                    className="p-4 bg-red-500 hover:bg-red-600 rounded-xl transition-colors"
                  >
                    <PhoneOff className="w-6 h-6" />
                  </button>
                </div>
              </div>
            )}
          </div>

          {/* Translation Display */}
          {translationEnabled && callState === 'connected' && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-xl font-semibold flex items-center gap-2">
                  <Globe className="w-5 h-5 text-purple-400" />
                  Real-time Translation
                </h2>
                <div className="flex gap-2 text-sm">
                  <span className="px-3 py-1 bg-purple-500/20 rounded-lg">{sourceLang.toUpperCase()}</span>
                  <span className="text-gray-500">→</span>
                  <span className="px-3 py-1 bg-pink-500/20 rounded-lg">{targetLang.toUpperCase()}</span>
                </div>
              </div>
              
              <div className="space-y-3 max-h-64 overflow-y-auto custom-scrollbar">
                {translations.length === 0 ? (
                  <p className="text-center text-gray-500 py-8">Waiting for speech...</p>
                ) : (
                  translations.map((trans, idx) => (
                    <div
                      key={idx}
                      className={`p-4 rounded-xl ${
                        trans.from === 'local' 
                          ? 'bg-purple-500/20 ml-8' 
                          : 'bg-white/5 mr-8'
                      }`}
                    >
                      <p className="text-sm text-gray-400 mb-1">{trans.orig}</p>
                      <p className="text-base">{trans.trans}</p>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}
        </div>

        {/* Calibration Modal */}
        {showCalibration && (
          <div className="fixed inset-0 bg-black/80 backdrop-blur-sm z-50 flex items-center justify-center p-4">
            <div className="bg-slate-900 border border-purple-500/30 rounded-2xl p-6 max-w-md w-full">
              <h2 className="text-xl font-bold mb-4 flex items-center gap-2">
                <Settings className="w-5 h-5 text-purple-400" />
                Auto-Calibration
              </h2>

              <div className="mb-6">
                <div className="flex justify-between text-sm mb-2 text-gray-400">
                  <span>Progress</span>
                  <span>{Math.round(calibrationState.progress * 100)}%</span>
                </div>
                <div className="w-full bg-white/10 rounded-full h-2 overflow-hidden">
                  <div
                    className="bg-gradient-to-r from-purple-500 to-pink-500 h-full transition-all duration-300"
                    style={{ width: `${calibrationState.progress * 100}%` }}
                  />
                </div>
              </div>

              <div className="bg-black/40 rounded-xl p-4 mb-6">
                <h3 className="font-semibold text-purple-300 mb-2">Current Step: {calibrationState.step}</h3>
                <p className="text-sm text-gray-400">
                  {calibrationState.step === 'Idle' && 'Ready to begin calibration.'}
                  {calibrationState.step === 'MeasureNoiseFloor' && 'Measuring ambient noise. Please remain quiet...'}
                  {calibrationState.step === 'MeasureGain' && 'Please speak at a normal volume...'}
                  {calibrationState.step === 'MeasureLatency' && 'Estimating round-trip latency...'}
                  {calibrationState.step === 'Complete' && 'Calibration successful!'}
                </p>

                {calibrationState.result && (
                  <div className="mt-4 pt-4 border-t border-white/10 space-y-2 text-sm text-green-300">
                    <div>Noise Floor: {calibrationState.result.rms_noise_floor_db.toFixed(1)} dB</div>
                    <div>Suggested Gain: {(calibrationState.result.recommended_input_gain * 100).toFixed(0)}%</div>
                    <div>Est. Latency: {calibrationState.result.estimated_latency_ms.toFixed(1)} ms</div>
                  </div>
                )}
              </div>

              <div className="flex justify-end gap-3">
                <button
                  onClick={() => {
                    handleCalibrationAction('cancel');
                    setShowCalibration(false);
                  }}
                  className="px-4 py-2 rounded-lg bg-white/10 hover:bg-white/20 transition-colors"
                >
                  Close
                </button>
                {calibrationState.step === 'Idle' ? (
                  <button
                    onClick={() => handleCalibrationAction('start')}
                    className="px-4 py-2 rounded-lg bg-purple-500 hover:bg-purple-600 transition-colors start-btn"
                  >
                    Start
                  </button>
                ) : calibrationState.step !== 'Complete' ? (
                  <button
                    onClick={() => handleCalibrationAction('advance')}
                    className="px-4 py-2 rounded-lg bg-pink-500 hover:bg-pink-600 transition-colors next-step-btn"
                  >
                    Next Step
                  </button>
                ) : null}
              </div>
            </div>
          </div>
        )}

        {/* Right Panel - Peers & Settings */}
        <div className="space-y-6">
          {/* Peers List */}
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <Users className="w-5 h-5 text-purple-400" />
              Available Peers
            </h2>
            
            <div className="space-y-3">
              {peers.map(peer => (
                <div
                  key={peer.id}
                  className="p-4 bg-white/5 hover:bg-white/10 rounded-xl transition-colors cursor-pointer group"
                  onClick={() => callState === 'idle' && initiateCall(peer)}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-3">
                      <div className="w-10 h-10 rounded-full bg-gradient-to-br from-purple-400 to-pink-400 flex items-center justify-center text-lg font-bold">
                        {peer.name.charAt(0)}
                      </div>
                      <div>
                        <p className="font-semibold">{peer.name}</p>
                        <p className="text-xs text-gray-400">{peer.ip}</p>
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className={`w-2 h-2 rounded-full ${
                        peer.status === 'online' ? 'bg-green-400' : 'bg-yellow-400'
                      }`} />
                      {callState === 'idle' && (
                        <Phone className="w-5 h-5 text-purple-400 opacity-0 group-hover:opacity-100 transition-opacity" />
                      )}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Settings Panel */}
          {showSettings && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
                <Settings className="w-5 h-5 text-purple-400" />
                Settings
              </h2>
              
              <div className="space-y-4">
                {/* Profiles & Calibration */}
                <div className="pt-2 pb-4 border-b border-white/10">
                  <div className="flex items-center justify-between mb-4">
                    <label className="text-sm font-semibold text-gray-300">Active Profile</label>
                    <button
                      onClick={() => setShowCalibration(true)}
                      className="text-xs bg-purple-500/20 hover:bg-purple-500/40 text-purple-300 px-3 py-1 rounded-lg transition-colors border border-purple-500/30 run-calibration-btn"
                    >
                      Run Calibration
                    </button>
                  </div>
                  <select
                    value={activeProfile}
                    onChange={(e) => setActiveProfile(e.target.value)}
                    className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500 mb-2"
                  >
                    {profiles.map(p => (
                      <option key={p.name} value={p.name}>{p.name}</option>
                    ))}
                  </select>

                  {/* Real-time Metrics Mini-Display */}
                  <div className="bg-black/40 rounded-lg p-3 mt-4 space-y-2 text-xs relative overflow-hidden">
                    {metrics.offline && (
                      <div className="absolute inset-0 bg-black/60 flex items-center justify-center z-10 backdrop-blur-sm">
                        <span className="text-red-400 font-semibold flex items-center gap-2">
                          <span className="w-2 h-2 rounded-full bg-red-500 animate-pulse"></span>
                          Telemetry Offline
                        </span>
                      </div>
                    )}
                    <div className={`transition-opacity ${metrics.offline ? 'opacity-30' : 'opacity-100'}`}>
                      <div className="flex justify-between">
                        <span className="text-gray-400">Peak Level</span>
                        <span className={metrics.clipping ? "text-red-400 font-bold" : "text-green-400"}>
                          {metrics.peak_db.toFixed(1)} dB {metrics.clipping && " (CLIP)"}
                        </span>
                      </div>
                      <div className="flex justify-between mt-2">
                        <span className="text-gray-400">SNR</span>
                        <span className="text-blue-400">{metrics.snr_db.toFixed(1)} dB</span>
                      </div>
                      <div className="flex justify-between mt-2">
                        <span className="text-gray-400">Est. Latency</span>
                        <span className="text-yellow-400">{metrics.latency_ms.toFixed(1)} ms</span>
                      </div>
                    </div>
                  </div>
                </div>

                <div>
                  <label className="text-sm text-gray-400 mb-2 block">Source Language</label>
                  <select 
                    value={sourceLang}
                    onChange={(e) => setSourceLang(e.target.value)}
                    className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500"
                  >
                    {languages.map(lang => (
                      <option key={lang.code} value={lang.code}>{lang.name}</option>
                    ))}
                  </select>
                </div>

                <div>
                  <label className="text-sm text-gray-400 mb-2 block">Target Language</label>
                  <select 
                    value={targetLang}
                    onChange={(e) => setTargetLang(e.target.value)}
                    className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500"
                  >
                    {languages.map(lang => (
                      <option key={lang.code} value={lang.code}>{lang.name}</option>
                    ))}
                  </select>
                </div>

                <div className="pt-4 border-t border-white/10">
                  <div className="flex items-center justify-between mb-3">
                    <span className="text-sm">End-to-End Encryption</span>
                    <button
                      onClick={() => setEncryptionEnabled(!encryptionEnabled)}
                      className={`relative w-12 h-6 rounded-full transition-colors ${
                        encryptionEnabled ? 'bg-green-500' : 'bg-gray-600'
                      }`}
                    >
                      <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                        encryptionEnabled ? 'translate-x-6' : ''
                      }`} />
                    </button>
                  </div>

                  <div className="flex items-center justify-between">
                    <span className="text-sm">Real-time Translation</span>
                    <button
                      onClick={() => setTranslationEnabled(!translationEnabled)}
                      className={`relative w-12 h-6 rounded-full transition-colors ${
                        translationEnabled ? 'bg-purple-500' : 'bg-gray-600'
                      }`}
                    >
                      <div className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                        translationEnabled ? 'translate-x-6' : ''
                      }`} />
                    </button>
                  </div>
                </div>

                <div className="pt-4 border-t border-white/10">
                  <p className="text-xs text-gray-500">
                    Local Participant: {localParticipant.name}<br/>
                    IP: {localParticipant.ip}<br/>
                    Port: 5060
                  </p>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>

      <style jsx>{`
        .custom-scrollbar::-webkit-scrollbar {
          width: 6px;
        }
        .custom-scrollbar::-webkit-scrollbar-track {
          background: rgba(0, 0, 0, 0.2);
          border-radius: 10px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb {
          background: rgba(168, 85, 247, 0.4);
          border-radius: 10px;
        }
        .custom-scrollbar::-webkit-scrollbar-thumb:hover {
          background: rgba(168, 85, 247, 0.6);
        }
      `}</style>
    </div>
  );
};

export default APMDashboard;
