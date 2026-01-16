import React, { useEffect, useMemo, useReducer, useRef, useState } from "react";
import {
  Activity,
  AlertCircle,
  Cpu,
  Gauge,
  Globe,
  Headphones,
  Lock,
  Mic,
  MicOff,
  Phone,
  PhoneIncoming,
  PhoneOff,
  PhoneOutgoing,
  Radio,
  Settings,
  Signal,
  Thermometer,
  Timer,
  Users,
  Volume2,
  VolumeX,
  Wifi,
  WifiOff,
  Wind,
  X,
} from "lucide-react";
import { WSSignalingClient } from "./adapters/wsSignalingClient";

const formatDuration = (start) => {
  if (!start) return "00:00";
  const elapsed = Date.now() - start;
  const minutes = Math.floor(elapsed / 60000);
  const seconds = Math.floor((elapsed % 60000) / 1000);
  return `${String(minutes).padStart(2, "0")}:${String(seconds).padStart(2, "0")}`;
};

const randomRange = (min, max, decimals = 0) => {
  const value = Math.random() * (max - min) + min;
  return Number(value.toFixed(decimals));
};

const initialState = {
  callState: "idle",
  activeSession: null,
  micMuted: false,
  speakerMuted: false,
  speakerVolume: 100,
  encryptionEnabled: true,
  translationEnabled: true,
  audioLevel: 0,
  peers: [],
  translations: [],
  sourceLang: "en",
  targetLang: "es",
  showSettings: false,
  connectionStatus: "connected",
  systemStats: {
    cpuLoad: 0,
    dspLoad: 0,
    tempC: 0,
    fanRpm: 0,
    latencyMs: 0,
    jitterMs: 0,
    packetLoss: 0,
    bitrateKbps: 0,
    snrDb: 0,
    noiseFloor: 0,
    beamAngle: 0,
    reverb: 0,
  },
  notifications: [],
  systemLog: [],
};

const reducer = (state, action) => {
  switch (action.type) {
    case "SET_CALL_STATE":
      return { ...state, callState: action.payload };
    case "SET_ACTIVE_SESSION":
      return { ...state, activeSession: action.payload };
    case "TOGGLE_MIC":
      return { ...state, micMuted: !state.micMuted };
    case "TOGGLE_SPEAKER":
      return { ...state, speakerMuted: !state.speakerMuted };
    case "SET_SPEAKER_VOLUME":
      return { ...state, speakerVolume: action.payload };
    case "TOGGLE_ENCRYPTION":
      return { ...state, encryptionEnabled: !state.encryptionEnabled };
    case "TOGGLE_TRANSLATION":
      return { ...state, translationEnabled: !state.translationEnabled };
    case "SET_AUDIO_LEVEL":
      return { ...state, audioLevel: action.payload };
    case "SET_PEERS":
      return { ...state, peers: action.payload };
    case "ADD_TRANSLATION":
      return { ...state, translations: [...state.translations.slice(-8), action.payload] };
    case "CLEAR_TRANSLATIONS":
      return { ...state, translations: [] };
    case "SET_SOURCE_LANG":
      return { ...state, sourceLang: action.payload };
    case "SET_TARGET_LANG":
      return { ...state, targetLang: action.payload };
    case "TOGGLE_SETTINGS":
      return { ...state, showSettings: !state.showSettings };
    case "SET_CONNECTION_STATUS":
      return { ...state, connectionStatus: action.payload };
    case "UPDATE_SYSTEM_STATS":
      return { ...state, systemStats: action.payload };
    case "ADD_NOTIFICATION":
      return { ...state, notifications: [...state.notifications, action.payload] };
    case "REMOVE_NOTIFICATION":
      return { ...state, notifications: state.notifications.filter(n => n.id !== action.payload) };
    case "ADD_LOG":
      return { ...state, systemLog: [...state.systemLog.slice(-50), action.payload] };
    default:
      return state;
  }
};

const APMDashboard = () => {
  const [state, dispatch] = useReducer(reducer, initialState);
  const translationScrollRef = useRef(null);
  
  // WebRTC refs
  const peerIdRef = useRef("peer-" + Math.random().toString(36).slice(2, 9));
  const remotePeerIdRef = useRef(null);
  const signalingRef = useRef(null);
  const audioStreamRef = useRef(null);
  const pcRef = useRef(null);
  const remoteStreamRef = useRef(new MediaStream());
  const remoteAudioRef = useRef(null);
  const pendingOfferRef = useRef(null);
  const [incomingSessionId, setIncomingSessionId] = useState(null);
  const [remoteAudioConnected, setRemoteAudioConnected] = useState(false);

  const localParticipant = useMemo(
    () => ({
      id: peerIdRef.current,
      name: "You",
      ip: "192.168.1.100",
      device: "APM Edge Node",
      codec: "OPUS 48kHz",
    }),
    []
  );

  const appendLog = (msg) => {
    const timestamp = new Date().toLocaleTimeString();
    dispatch({ type: "ADD_LOG", payload: `[${timestamp}] ${msg}` });
  };

  const showNotification = (message, type = "info") => {
    const id = Date.now();
    dispatch({ type: "ADD_NOTIFICATION", payload: { id, message, type } });
    setTimeout(() => {
      dispatch({ type: "REMOVE_NOTIFICATION", payload: id });
    }, 4000);
  };

  // Audio management
  const startAudio = async () => {
    if (audioStreamRef.current) return audioStreamRef.current;

    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true,
        },
      });

      audioStreamRef.current = stream;
      appendLog("Microphone capture started");
      return stream;
    } catch (err) {
      appendLog("Mic error: " + err.message);
      showNotification("Microphone access denied", "error");
      throw err;
    }
  };

  const stopAudio = () => {
    if (!audioStreamRef.current) return;
    audioStreamRef.current.getTracks().forEach((t) => t.stop());
    audioStreamRef.current = null;
    appendLog("Microphone capture stopped");
  };

  // WebRTC peer connection
  const createPeerConnection = (activeSessionId) => {
    if (pcRef.current) {
      try {
        pcRef.current.close();
      } catch {}
      pcRef.current = null;
    }

    const pc = new RTCPeerConnection({
      iceServers: [{ urls: "stun:stun.l.google.com:19302" }],
    });

    pc.onicecandidate = (event) => {
      if (event.candidate) {
        signalingRef.current?.send({
          type: "ice",
          sessionId: activeSessionId,
          candidate: event.candidate,
          toPeerId: remotePeerIdRef.current || undefined,
        });
      }
    };

    pc.ontrack = (event) => {
      event.streams[0].getTracks().forEach((track) => {
        remoteStreamRef.current.addTrack(track);
      });
      setRemoteAudioConnected(true);
      appendLog("Received remote audio track(s)");
      showNotification("Audio connected", "success");
    };

    pc.onconnectionstatechange = () => {
      appendLog("PC state: " + pc.connectionState);
      if (pc.connectionState === "connected") {
        dispatch({ type: "SET_CALL_STATE", payload: "connected" });
      }
      if (pc.connectionState === "failed" || pc.connectionState === "closed") {
        showNotification("Connection lost", "error");
      }
    };

    pcRef.current = pc;
    appendLog("Peer connection created");
    return pc;
  };

  // Signaling setup
  const ensureSignaling = (activeSessionId) => {
    if (signalingRef.current) return;

    const s = new WSSignalingClient();

    s.on("joined", (msg) => {
      appendLog(`Joined room ${msg.roomId}`);
      dispatch({ type: "SET_CONNECTION_STATUS", payload: "connected" });
    });

    s.on("peer_joined", (msg) => {
      appendLog(`Peer joined: ${msg.peerId}`);
      remotePeerIdRef.current = msg.peerId;
      showNotification("Peer joined", "info");
    });

    s.on("peer_left", (msg) => {
      appendLog(`Peer left: ${msg.peerId}`);
      if (remotePeerIdRef.current === msg.peerId) remotePeerIdRef.current = null;
      showNotification("Peer left", "info");
    });

    s.on("call", (msg) => {
      appendLog("Incoming call signal");
      setIncomingSessionId(msg.sessionId);
      dispatch({ type: "SET_CALL_STATE", payload: "ringing" });
      dispatch({
        type: "SET_ACTIVE_SESSION",
        payload: {
          id: msg.sessionId,
          peer: state.peers[0] || { name: "Remote Peer", ip: "Unknown", role: "Unknown" },
          startTime: null,
          encrypted: state.encryptionEnabled,
          codec: "OPUS 48kHz",
          region: "Edge West-2",
        },
      });
      showNotification("Incoming call", "info");
    });

    s.on("end", () => {
      appendLog("Call ended by peer");
      showNotification("Call ended", "info");
      cleanupAll();
    });

    s.on("offer", (msg) => {
      appendLog("Offer received");
      pendingOfferRef.current = msg;
      setIncomingSessionId(msg.sessionId);
      if (state.callState !== "connected") {
        dispatch({ type: "SET_CALL_STATE", payload: "ringing" });
      }
    });

    s.on("answer", async (msg) => {
      appendLog("Answer received");
      try {
        const pc = pcRef.current;
        if (!pc) return;
        await pc.setRemoteDescription(msg.sdp);
        appendLog("Remote description set (answer)");
      } catch (err) {
        appendLog("Answer handling error: " + err.message);
      }
    });

    s.on("ice", async (msg) => {
      try {
        const pc = pcRef.current;
        if (!pc) return;
        await pc.addIceCandidate(msg.candidate);
        appendLog("ICE candidate added");
      } catch (err) {
        appendLog("ICE error: " + err.message);
      }
    });

    s.on("close", () => {
      appendLog("Signaling socket closed");
      dispatch({ type: "SET_CONNECTION_STATUS", payload: "disconnected" });
    });

    signalingRef.current = s;

    s.connect({
      roomId: activeSessionId,
      peerId: peerIdRef.current,
    });
  };

  // Initiate call (caller)
  const initiateCall = async (peer) => {
    const newSessionId = "session-" + Date.now();
    dispatch({ type: "SET_CALL_STATE", payload: "calling" });
    dispatch({
      type: "SET_ACTIVE_SESSION",
      payload: {
        id: newSessionId,
        peer,
        startTime: Date.now(),
        encrypted: state.encryptionEnabled,
        codec: "OPUS 48kHz",
        region: "Edge West-2",
      },
    });
    appendLog("Call initiated");
    showNotification(`Calling ${peer.name}...`, "info");

    try {
      await startAudio();
      ensureSignaling(newSessionId);

      signalingRef.current.send({
        type: "call",
        sessionId: newSessionId,
      });

      const pc = createPeerConnection(newSessionId);
      audioStreamRef.current.getTracks().forEach((track) => 
        pc.addTrack(track, audioStreamRef.current)
      );

      const offer = await pc.createOffer({
        offerToReceiveAudio: true,
        offerToReceiveVideo: false,
      });

      await pc.setLocalDescription(offer);

      signalingRef.current.send({
        type: "offer",
        sessionId: newSessionId,
        sdp: pc.localDescription,
      });

      appendLog("Offer sent");
    } catch (err) {
      appendLog("Caller offer error: " + err.message);
      showNotification("Call failed: " + err.message, "error");
      cleanupAll();
    }
  };

  // Accept incoming call
  const acceptCall = async () => {
    const sid = incomingSessionId;
    if (!sid) return;

    setIncomingSessionId(null);
    dispatch({ type: "SET_CALL_STATE", payload: "calling" });
    appendLog("Accepting incoming call");
    showNotification("Accepting call...", "info");

    try {
      await startAudio();
      ensureSignaling(sid);

      const offerMsg = pendingOfferRef.current;
      if (!offerMsg || !offerMsg.sdp) {
        appendLog("No pending offer to accept yet");
        return;
      }

      const pc = createPeerConnection(sid);
      audioStreamRef.current.getTracks().forEach((track) => 
        pc.addTrack(track, audioStreamRef.current)
      );

      await pc.setRemoteDescription(offerMsg.sdp);
      appendLog("Remote description set (offer)");

      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);

      signalingRef.current.send({
        type: "answer",
        sessionId: sid,
        sdp: pc.localDescription,
      });

      signalingRef.current.send({
        type: "accept",
        sessionId: sid,
      });

      appendLog("Answer sent");
      pendingOfferRef.current = null;

      // Update session start time
      if (state.activeSession) {
        dispatch({
          type: "SET_ACTIVE_SESSION",
          payload: {
            ...state.activeSession,
            startTime: Date.now(),
          },
        });
      }
    } catch (err) {
      appendLog("Accept error: " + err.message);
      showNotification("Failed to accept call: " + err.message, "error");
      cleanupAll();
    }
  };

  // Reject incoming call
  const rejectCall = () => {
    appendLog("Incoming call rejected");
    showNotification("Call rejected", "info");
    setIncomingSessionId(null);
    dispatch({ type: "SET_CALL_STATE", payload: "idle" });
    dispatch({ type: "SET_ACTIVE_SESSION", payload: null });
    pendingOfferRef.current = null;
  };

  // End call
  const endCall = () => {
    appendLog("Ending call");

    if (signalingRef.current && state.activeSession) {
      signalingRef.current.send({
        type: "end",
        sessionId: state.activeSession.id,
      });
    }

    if (state.activeSession) {
      showNotification(`Call ended with ${state.activeSession.peer.name}`, "info");
    }
    cleanupAll();
  };

  // Cleanup
  const cleanupAll = () => {
    if (pcRef.current) {
      try {
        pcRef.current.onicecandidate = null;
        pcRef.current.ontrack = null;
        pcRef.current.close();
      } catch {}
      pcRef.current = null;
      appendLog("Peer connection closed");
    }

    remoteStreamRef.current = new MediaStream();
    if (remoteAudioRef.current) {
      remoteAudioRef.current.srcObject = null;
    }
    setRemoteAudioConnected(false);

    signalingRef.current?.disconnect();
    signalingRef.current = null;

    stopAudio();

    dispatch({ type: "SET_ACTIVE_SESSION", payload: null });
    setIncomingSessionId(null);
    pendingOfferRef.current = null;
    remotePeerIdRef.current = null;
    dispatch({ type: "SET_CALL_STATE", payload: "idle" });
    dispatch({ type: "CLEAR_TRANSLATIONS" });
    appendLog("Cleanup complete");
  };

  // Audio level simulation
  useEffect(() => {
    if (state.callState === "connected") {
      const interval = setInterval(() => {
        dispatch({ type: "SET_AUDIO_LEVEL", payload: randomRange(45, 92) });
      }, 120);
      return () => clearInterval(interval);
    }
    dispatch({ type: "SET_AUDIO_LEVEL", payload: 0 });
    return undefined;
  }, [state.callState]);

  // Mock peers
  useEffect(() => {
    const mockPeers = [
      { id: "peer1", name: "Alice Cooper", ip: "192.168.1.101", status: "online", role: "Field Ops" },
      { id: "peer2", name: "Bob Martinez", ip: "192.168.1.102", status: "online", role: "Command" },
      { id: "peer3", name: "Carol Zhang", ip: "192.168.1.103", status: "away", role: "Interpreter" },
      { id: "peer4", name: "David Kim", ip: "192.168.1.104", status: "online", role: "Tech Lead" },
    ];
    dispatch({ type: "SET_PEERS", payload: mockPeers });
  }, []);

  // System stats simulation
  useEffect(() => {
    const interval = setInterval(() => {
      if (state.callState === "connected") {
        dispatch({
          type: "UPDATE_SYSTEM_STATS",
          payload: {
            cpuLoad: randomRange(28, 62, 1),
            dspLoad: randomRange(35, 71, 1),
            tempC: randomRange(42, 58, 1),
            fanRpm: randomRange(2200, 3200),
            latencyMs: randomRange(18, 36, 1),
            jitterMs: randomRange(2.1, 6.8, 1),
            packetLoss: randomRange(0.0, 0.8, 2),
            bitrateKbps: randomRange(96, 148),
            snrDb: randomRange(18, 28, 1),
            noiseFloor: randomRange(-52, -45, 1),
            beamAngle: randomRange(-12, 14, 1),
            reverb: randomRange(0.32, 0.54, 2),
          },
        });
      } else {
        dispatch({
          type: "UPDATE_SYSTEM_STATS",
          payload: {
            cpuLoad: randomRange(12, 22, 1),
            dspLoad: randomRange(18, 32, 1),
            tempC: randomRange(34, 40, 1),
            fanRpm: randomRange(1500, 2100),
            latencyMs: randomRange(0, 1, 1),
            jitterMs: randomRange(0, 0.4, 1),
            packetLoss: 0,
            bitrateKbps: 0,
            snrDb: randomRange(12, 18, 1),
            noiseFloor: randomRange(-58, -50, 1),
            beamAngle: 0,
            reverb: randomRange(0.38, 0.6, 2),
          },
        });
      }
    }, 1400);

    return () => clearInterval(interval);
  }, [state.callState]);

  // Translation simulation
  useEffect(() => {
    if (state.callState === "connected" && state.translationEnabled) {
      const interval = setInterval(() => {
        const mockTranslations = [
          { orig: "You should have a clear channel now.", trans: "Ahora deberías tener un canal claro.", from: "remote" },
          { orig: "We are switching to beam steer mode.", trans: "Estamos cambiando al modo de dirección del haz.", from: "local" },
          { orig: "Confirm the mic array is aligned.", trans: "Confirma que la matriz de micrófonos está alineada.", from: "remote" },
          { orig: "Signal strength is optimal.", trans: "La intensidad de la señal es óptima.", from: "local" },
          { orig: "Adjusting noise cancellation parameters.", trans: "Ajustando parámetros de cancelación de ruido.", from: "remote" },
        ];
        const next = mockTranslations[Math.floor(Math.random() * mockTranslations.length)];
        dispatch({
          type: "ADD_TRANSLATION",
          payload: { ...next, time: Date.now() },
        });
      }, 5200);
      return () => clearInterval(interval);
    }
    return undefined;
  }, [state.callState, state.translationEnabled]);

  // Remote audio setup
  useEffect(() => {
    if (!remoteAudioRef.current) return;
    remoteAudioRef.current.srcObject = remoteStreamRef.current;
  }, [remoteAudioConnected]);

  useEffect(() => {
    if (!remoteAudioRef.current) return;
    remoteAudioRef.current.muted = state.speakerMuted;
  }, [state.speakerMuted]);

  useEffect(() => {
    if (!remoteAudioRef.current) return;
    remoteAudioRef.current.volume = state.speakerVolume / 100;
  }, [state.speakerVolume]);

  // Auto-scroll translations
  useEffect(() => {
    if (translationScrollRef.current) {
      translationScrollRef.current.scrollTop = translationScrollRef.current.scrollHeight;
    }
  }, [state.translations]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      try {
        signalingRef.current?.disconnect();
      } catch {}
      try {
        if (pcRef.current) pcRef.current.close();
      } catch {}
      if (remoteAudioRef.current) {
        remoteAudioRef.current.srcObject = null;
      }
      stopAudio();
    };
  }, []);

  const languages = [
    { code: "en", name: "English" },
    { code: "es", name: "Spanish" },
    { code: "fr", name: "French" },
    { code: "de", name: "German" },
    { code: "ja", name: "Japanese" },
    { code: "zh", name: "Chinese" },
    { code: "ar", name: "Arabic" },
    { code: "ru", name: "Russian" },
    { code: "pt", name: "Portuguese" },
    { code: "it", name: "Italian" },
    { code: "ko", name: "Korean" },
    { code: "hi", name: "Hindi" },
  ];

  const getStatusColor = (status) => {
    switch (status) {
      case "online": return "bg-green-400";
      case "away": return "bg-yellow-400";
      case "offline": return "bg-gray-400";
      default: return "bg-gray-400";
    }
  };

  const getStatColor = (stat, value) => {
    if (stat === "cpuLoad" || stat === "dspLoad") {
      if (value > 80) return "text-red-400";
      if (value > 60) return "text-yellow-400";
      return "text-green-400";
    }
    if (stat === "tempC") {
      if (value > 70) return "text-red-400";
      if (value > 55) return "text-yellow-400";
      return "text-green-400";
    }
    if (stat === "packetLoss") {
      if (value > 1) return "text-red-400";
      if (value > 0.5) return "text-yellow-400";
      return "text-green-400";
    }
    if (stat === "latencyMs") {
      if (value > 100) return "text-red-400";
      if (value > 50) return "text-yellow-400";
      return "text-green-400";
    }
    return "text-white";
  };

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900 text-white p-4">
      {/* Hidden audio element for remote stream */}
      <audio ref={remoteAudioRef} autoPlay playsInline className="hidden" />

      {/* Notifications */}
      <div className="fixed top-4 right-4 z-50 space-y-2 max-w-md">
        {state.notifications.map((notification) => (
          <div
            key={notification.id}
            className="bg-black/90 backdrop-blur-xl border border-purple-500/30 rounded-xl p-4 flex items-center gap-3 animate-[slideIn_0.3s_ease-out]"
          >
            <AlertCircle className={`w-5 h-5 ${
              notification.type === "success" ? "text-green-400" :
              notification.type === "error" ? "text-red-400" :
              "text-blue-400"
            }`} />
            <p className="flex-1 text-sm">{notification.message}</p>
            <button
              onClick={() => dispatch({ type: "REMOVE_NOTIFICATION", payload: notification.id })}
              className="hover:bg-white/10 rounded p-1 transition-colors"
            >
              <X className="w-4 h-4" />
            </button>
          </div>
        ))}
      </div>

      {/* Header */}
      <div className="max-w-7xl mx-auto mb-6">
        <div className="flex flex-wrap items-center justify-between gap-4 bg-black/30 backdrop-blur-xl rounded-2xl p-4 border border-purple-500/20 shadow-2xl">
          <div className="flex items-center gap-4">
            <div className="relative">
              <Radio className="w-10 h-10 text-purple-400" />
              <div className="absolute -top-1 -right-1 w-3 h-3 bg-green-400 rounded-full animate-pulse shadow-lg shadow-green-400/50" />
            </div>
            <div>
              <h1 className="text-2xl font-bold bg-gradient-to-r from-purple-400 to-pink-400 bg-clip-text text-transparent">
                APM System v2.3
              </h1>
              <p className="text-sm text-gray-400">Acoustic Projection & Translation • {localParticipant.id}</p>
            </div>
          </div>

          <div className="flex flex-wrap items-center gap-3">
            <div className={`flex items-center gap-2 px-3 py-1 rounded-lg ${
              state.connectionStatus === "connected" ? "bg-green-500/20" : "bg-red-500/20"
            }`}>
              {state.connectionStatus === "connected" ? (
                <Wifi className="w-4 h-4 text-green-400" />
              ) : (
                <WifiOff className="w-4 h-4 text-red-400" />
              )}
              <span className="text-sm">
                {state.connectionStatus === "connected" ? "Connected" : "Offline"}
              </span>
            </div>
            <div className="flex items-center gap-2 bg-white/10 px-3 py-1 rounded-lg text-sm">
              <Signal className="w-4 h-4 text-purple-300" />
              RTT <span className={getStatColor("latencyMs", state.systemStats.latencyMs)}>{state.systemStats.latencyMs}</span> ms
            </div>
            {remoteAudioConnected && (
              <div className="flex items-center gap-2 bg-green-500/20 px-3 py-1 rounded-lg text-sm">
                <Volume2 className="w-4 h-4 text-green-400" />
                Audio Active
              </div>
            )}
            <button
              onClick={() => dispatch({ type: "TOGGLE_SETTINGS" })}
              className={`p-2 rounded-lg transition-colors ${
                state.showSettings ? "bg-purple-500/20" : "hover:bg-white/10"
              }`}
              aria-label="Toggle settings"
            >
              <Settings className="w-5 h-5" />
            </button>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Main Call Area */}
        <div className="lg:col-span-2 space-y-6">
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20 shadow-2xl">
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-xl font-semibold flex items-center gap-2">
                <Activity className="w-5 h-5 text-purple-400" />
                Active Session
              </h2>
              {state.encryptionEnabled && state.callState === "connected" && (
                <div className="flex items-center gap-2 bg-green-500/20 px-3 py-1 rounded-lg animate-pulse">
                  <Lock className="w-4 h-4 text-green-400" />
                  <span className="text-sm">E2E Encrypted</span>
                </div>
              )}
            </div>

            {state.callState === "idle" && (
              <div className="text-center py-16">
                <Phone className="w-20 h-20 mx-auto mb-4 text-gray-500" />
                <p className="text-xl text-gray-300 mb-2">No active call</p>
                <p className="text-sm text-gray-500 mb-4">Select a peer to start a secure WebRTC call</p>
                <p className="text-xs text-gray-600">Open this page in two tabs or browsers to test</p>
              </div>
            )}

            {state.callState === "calling" && (
              <div className="text-center py-16">
                <div className="relative inline-block mb-6">
                  <PhoneOutgoing className="w-20 h-20 text-purple-400 animate-pulse" />
                  <div className="absolute inset-0 w-20 h-20 border-4 border-purple-400 rounded-full animate-ping" />
                </div>
                <p className="text-xl mb-2">Calling {state.activeSession?.peer?.name}...</p>
                <p className="text-sm text-gray-400 mb-1">{state.activeSession?.peer?.ip}</p>
                <p className="text-xs text-gray-500">{state.activeSession?.peer?.role}</p>
                <button
                  onClick={endCall}
                  className="mt-8 px-8 py-3 bg-red-500 hover:bg-red-600 rounded-xl transition-all hover:scale-105 shadow-lg"
                >
                  Cancel
                </button>
              </div>
            )}

            {state.callState === "ringing" && (
              <div className="text-center py-16">
                <div className="relative inline-block mb-6">
                  <PhoneIncoming className="w-20 h-20 text-green-400 animate-bounce" />
                </div>
                <p className="text-xl mb-2">Incoming call from {state.activeSession?.peer?.name}</p>
                <p className="text-sm text-gray-400 mb-1">{state.activeSession?.peer?.ip}</p>
                <p className="text-xs text-gray-500 mb-8">{state.activeSession?.peer?.role}</p>
                <div className="flex gap-4 justify-center">
                  <button
                    onClick={acceptCall}
                    className="px-10 py-3 bg-green-500 hover:bg-green-600 rounded-xl transition-all hover:scale-105 shadow-lg flex items-center gap-2"
                  >
                    <Phone className="w-5 h-5" />
                    Accept
                  </button>
                  <button
                    onClick={rejectCall}
                    className="px-10 py-3 bg-red-500 hover:bg-red-600 rounded-xl transition-all hover:scale-105 shadow-lg flex items-center gap-2"
                  >
                    <PhoneOff className="w-5 h-5" />
                    Reject
                  </button>
                </div>
              </div>
            )}

            {state.callState === "connected" && state.activeSession && (
              <div className="space-y-6">
                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  <div className="md:col-span-2 flex items-center gap-4 p-4 bg-gradient-to-r from-purple-500/20 to-pink-500/20 rounded-xl border border-purple-400/20">
                    <div className="w-14 h-14 rounded-full bg-gradient-to-br from-purple-400 to-pink-400 flex items-center justify-center text-2xl font-bold shadow-lg">
                      {state.activeSession.peer.name.charAt(0)}
                    </div>
                    <div className="flex-1">
                      <p className="font-semibold text-lg">{state.activeSession.peer.name}</p>
                      <p className="text-sm text-gray-400">{state.activeSession.peer.ip}</p>
                      <p className="text-xs text-gray-500">{state.activeSession.peer.role} • {state.activeSession.region}</p>
                    </div>
                  </div>
                  <div className="p-4 bg-white/5 rounded-xl flex flex-col justify-between border border-white/5">
                    <p className="text-sm text-gray-400">Duration</p>
                    <p className="text-3xl font-mono tabular-nums font-bold text-purple-300">{formatDuration(state.activeSession.startTime)}</p>
                    <p className="text-xs text-gray-500">Codec {state.activeSession.codec}</p>
                  </div>
                </div>

                <div>
                  <div className="flex items-center justify-between mb-2">
                    <div className="flex items-center gap-2">
                      <Activity className="w-4 h-4 text-purple-400" />
                      <span className="text-sm text-gray-400">Audio Level</span>
                    </div>
                    <div className="text-xs text-gray-500">
                      SNR <span className={getStatColor("snrDb", state.systemStats.snrDb)}>{state.systemStats.snrDb}</span> dB
                    </div>
                  </div>
                  <div className="h-20 bg-black/50 rounded-lg p-2 flex items-end gap-1 border border-purple-500/10">
                    {Array.from({ length: 50 }).map((_, i) => (
                      <div
                        key={i}
                        className="flex-1 bg-gradient-to-t from-purple-600 via-pink-500 to-purple-400 rounded-sm transition-all duration-75"
                        style={{
                          height: `${Math.max(10, (state.audioLevel + Math.sin(i + state.audioLevel / 8) * 15) / 1.3)}%`,
                          opacity: 0.6 + (i % 5) * 0.05,
                        }}
                      />
                    ))}
                  </div>
                </div>

                <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                  <div className="p-4 bg-white/5 rounded-xl border border-white/5">
                    <div className="flex items-center gap-2 text-sm text-gray-400 mb-3">
                      <Signal className="w-4 h-4 text-purple-300" />
                      Network
                    </div>
                    <div className="space-y-2 text-sm">
                      <div className="flex justify-between">
                        <span>Latency</span>
                        <span className={getStatColor("latencyMs", state.systemStats.latencyMs)}>{state.systemStats.latencyMs} ms</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Jitter</span>
                        <span>{state.systemStats.jitterMs} ms</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Packet loss</span>
                        <span className={getStatColor("packetLoss", state.systemStats.packetLoss)}>{state.systemStats.packetLoss}%</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Bitrate</span>
                        <span>{state.systemStats.bitrateKbps} kbps</span>
                      </div>
                    </div>
                  </div>
                  <div className="p-4 bg-white/5 rounded-xl border border-white/5">
                    <div className="flex items-center gap-2 text-sm text-gray-400 mb-3">
                      <Gauge className="w-4 h-4 text-purple-300" />
                      DSP Chain
                    </div>
                    <div className="space-y-2 text-sm">
                      <div className="flex justify-between">
                        <span>Beam angle</span>
                        <span>{state.systemStats.beamAngle}°</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Noise floor</span>
                        <span>{state.systemStats.noiseFloor} dB</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Reverb RT60</span>
                        <span>{state.systemStats.reverb}s</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Gain staging</span>
                        <span>{state.micMuted ? "Muted" : "+2.5 dB"}</span>
                      </div>
                    </div>
                  </div>
                  <div className="p-4 bg-white/5 rounded-xl border border-white/5">
                    <div className="flex items-center gap-2 text-sm text-gray-400 mb-3">
                      <Cpu className="w-4 h-4 text-purple-300" />
                      Hardware
                    </div>
                    <div className="space-y-2 text-sm">
                      <div className="flex justify-between">
                        <span>CPU Load</span>
                        <span className={getStatColor("cpuLoad", state.systemStats.cpuLoad)}>{state.systemStats.cpuLoad}%</span>
                      </div>
                      <div className="flex justify-between">
                        <span>DSP Load</span>
                        <span className={getStatColor("dspLoad", state.systemStats.dspLoad)}>{state.systemStats.dspLoad}%</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Temp</span>
                        <span className={getStatColor("tempC", state.systemStats.tempC)}>{state.systemStats.tempC}°C</span>
                      </div>
                      <div className="flex justify-between">
                        <span>Fan</span>
                        <span>{state.systemStats.fanRpm} RPM</span>
                      </div>
                    </div>
                  </div>
                </div>

                <div className="flex flex-wrap gap-4 justify-center pt-2">
                  <button
                    onClick={() => {
                      dispatch({ type: "TOGGLE_MIC" });
                      if (audioStreamRef.current) {
                        audioStreamRef.current.getAudioTracks().forEach(track => {
                          track.enabled = state.micMuted;
                        });
                      }
                      showNotification(state.micMuted ? "Microphone unmuted" : "Microphone muted", "info");
                    }}
                    className={`p-4 rounded-xl transition-all hover:scale-105 shadow-lg ${
                      state.micMuted ? "bg-red-500 hover:bg-red-600" : "bg-white/10 hover:bg-white/20"
                    }`}
                    aria-label={state.micMuted ? "Unmute microphone" : "Mute microphone"}
                  >
                    {state.micMuted ? <MicOff className="w-6 h-6" /> : <Mic className="w-6 h-6" />}
                  </button>

                  <button
                    onClick={() => {
                      dispatch({ type: "TOGGLE_SPEAKER" });
                      showNotification(state.speakerMuted ? "Speaker unmuted" : "Speaker muted", "info");
                    }}
                    className={`p-4 rounded-xl transition-all hover:scale-105 shadow-lg ${
                      state.speakerMuted ? "bg-red-500 hover:bg-red-600" : "bg-white/10 hover:bg-white/20"
                    }`}
                    aria-label={state.speakerMuted ? "Unmute speaker" : "Mute speaker"}
                  >
                    {state.speakerMuted ? <VolumeX className="w-6 h-6" /> : <Volume2 className="w-6 h-6" />}
                  </button>

                  <button
                    onClick={() => {
                      dispatch({ type: "TOGGLE_TRANSLATION" });
                      showNotification(state.translationEnabled ? "Translation disabled" : "Translation enabled", "info");
                    }}
                    className={`p-4 rounded-xl transition-all hover:scale-105 shadow-lg ${
                      state.translationEnabled ? "bg-purple-500 hover:bg-purple-600" : "bg-white/10 hover:bg-white/20"
                    }`}
                    aria-label={state.translationEnabled ? "Disable translation" : "Enable translation"}
                  >
                    <Globe className="w-6 h-6" />
                  </button>

                  <button
                    onClick={endCall}
                    className="p-4 bg-red-500 hover:bg-red-600 rounded-xl transition-all hover:scale-105 shadow-lg"
                    aria-label="End call"
                  >
                    <PhoneOff className="w-6 h-6" />
                  </button>
                </div>

                {/* Volume Control */}
                <div className="p-4 bg-white/5 rounded-xl border border-white/5">
                  <div className="flex items-center justify-between mb-2">
                    <span className="text-sm text-gray-400">Speaker Volume</span>
                    <span className="text-sm font-mono">{state.speakerVolume}%</span>
                  </div>
                  <input
                    type="range"
                    min="0"
                    max="100"
                    value={state.speakerVolume}
                    onChange={(e) => dispatch({ type: "SET_SPEAKER_VOLUME", payload: Number(e.target.value) })}
                    className="w-full h-2 bg-gray-700 rounded-lg appearance-none cursor-pointer accent-purple-500"
                  />
                </div>
              </div>
            )}
          </div>

          {state.translationEnabled && state.callState === "connected" && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-xl font-semibold flex items-center gap-2">
                  <Globe className="w-5 h-5 text-purple-400" />
                  Real-time Translation
                </h2>
                <div className="flex gap-2 text-sm">
                  <span className="px-3 py-1 bg-purple-500/20 rounded-lg">
                    {state.sourceLang.toUpperCase()}
                  </span>
                  <span className="text-gray-500">→</span>
                  <span className="px-3 py-1 bg-pink-500/20 rounded-lg">
                    {state.targetLang.toUpperCase()}
                  </span>
                </div>
              </div>

              <div ref={translationScrollRef} className="space-y-3 max-h-64 overflow-y-auto scrollbar-thin scrollbar-thumb-purple-500/50 scrollbar-track-white/5">
                {state.translations.length === 0 ? (
                  <p className="text-center text-gray-500 py-8">Waiting for speech...</p>
                ) : (
                  state.translations.map((trans, idx) => (
                    <div
                      key={idx}
                      className={`p-4 rounded-xl transition-all duration-300 ${
                        trans.from === "local" ? "bg-purple-500/20 ml-8" : "bg-white/5 mr-8"
                      }`}
                    >
                      <div className="flex items-center justify-between text-xs text-gray-400 mb-1">
                        <span className="font-medium">{trans.from === "local" ? localParticipant.name : state.activeSession?.peer?.name}</span>
                        <span>{new Date(trans.time).toLocaleTimeString()}</span>
                      </div>
                      <p className="text-sm text-gray-400 italic mb-1">"{trans.orig}"</p>
                      <p className="text-base font-medium">{trans.trans}</p>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}
        </div>

        {/* Sidebar */}
        <div className="space-y-6">
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <Users className="w-5 h-5 text-purple-400" />
              Available Peers
            </h2>

            <div className="space-y-3">
              {state.peers.map((peer) => (
                <div
                  key={peer.id}
                  className="p-4 bg-white/5 hover:bg-white/10 rounded-xl transition-colors cursor-pointer group"
                  onClick={() => state.callState === "idle" && initiateCall(peer)}
                >
                  <div className="flex items-center justify-between">
                    <div className="flex items-center gap-3">
                      <div className="w-10 h-10 rounded-full bg-gradient-to-br from-purple-400 to-pink-400 flex items-center justify-center text-lg font-bold">
                        {peer.name.charAt(0)}
                      </div>
                      <div>
                        <p className="font-semibold">{peer.name}</p>
                        <p className="text-xs text-gray-400">{peer.role}</p>
                        <p className="text-xs text-gray-500">{peer.ip}</p>
                      </div>
                    </div>
                    <div className="flex items-center gap-2">
                      <div className={`w-2 h-2 rounded-full ${getStatusColor(peer.status)}`} />
                      {state.callState === "idle" && (
                        <Phone className="w-5 h-5 text-purple-400 opacity-0 group-hover:opacity-100 transition-opacity" />
                      )}
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>

          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <Headphones className="w-5 h-5 text-purple-400" />
              Environment
            </h2>
            <div className="space-y-3 text-sm">
              <div className="flex justify-between">
                <span className="text-gray-400 flex items-center gap-2">
                  <Wind className="w-4 h-4 text-purple-300" /> Noise Floor
                </span>
                <span>{state.systemStats.noiseFloor} dB</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-400 flex items-center gap-2">
                  <Timer className="w-4 h-4 text-purple-300" /> RT60
                </span>
                <span>{state.systemStats.reverb}s</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-400 flex items-center gap-2">
                  <Gauge className="w-4 h-4 text-purple-300" /> Beam Angle
                </span>
                <span>{state.systemStats.beamAngle}°</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-400 flex items-center gap-2">
                  <Thermometer className="w-4 h-4 text-purple-300" /> Temp
                </span>
                <span className={getStatColor("tempC", state.systemStats.tempC)}>{state.systemStats.tempC}°C</span>
              </div>
            </div>
          </div>

          {state.showSettings && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
                <Settings className="w-5 h-5 text-purple-400" />
                Settings
              </h2>

              <div className="space-y-4">
                <div>
                  <label className="text-sm text-gray-400 mb-2 block">Source Language</label>
                  <select
                    value={state.sourceLang}
                    onChange={(e) => dispatch({ type: "SET_SOURCE_LANG", payload: e.target.value })}
                    className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500 text-white"
                  >
                    {languages.map((lang) => (
                      <option key={lang.code} value={lang.code} className="bg-slate-800">
                        {lang.name}
                      </option>
                    ))}
                  </select>
                </div>

                <div>
                  <label className="text-sm text-gray-400 mb-2 block">Target Language</label>
                  <select
                    value={state.targetLang}
                    onChange={(e) => dispatch({ type: "SET_TARGET_LANG", payload: e.target.value })}
                    className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500 text-white"
                  >
                    {languages.map((lang) => (
                      <option key={lang.code} value={lang.code} className="bg-slate-800">
                        {lang.name}
                      </option>
                    ))}
                  </select>
                </div>

                <div className="pt-4 border-t border-white/10">
                  <div className="flex items-center justify-between mb-3">
                    <span className="text-sm">End-to-End Encryption</span>
                    <button
                      onClick={() => dispatch({ type: "TOGGLE_ENCRYPTION" })}
                      className={`relative w-12 h-6 rounded-full transition-colors ${
                        state.encryptionEnabled ? "bg-green-500" : "bg-gray-600"
                      }`}
                    >
                      <div
                        className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                          state.encryptionEnabled ? "translate-x-6" : ""
                        }`}
                      />
                    </button>
                  </div>

                  <div className="flex items-center justify-between">
                    <span className="text-sm">Real-time Translation</span>
                    <button
                      onClick={() => dispatch({ type: "TOGGLE_TRANSLATION" })}
                      className={`relative w-12 h-6 rounded-full transition-colors ${
                        state.translationEnabled ? "bg-purple-500" : "bg-gray-600"
                      }`}
                    >
                      <div
                        className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                          state.translationEnabled ? "translate-x-6" : ""
                        }`}
                      />
                    </button>
                  </div>
                </div>

                <div className="pt-4 border-t border-white/10 text-xs text-gray-500 space-y-2">
                  <div>
                    <p>Local Participant: {localParticipant.name}</p>
                    <p>Device: {localParticipant.device}</p>
                  </div>
                  <div>
                    <p>Peer ID: {localParticipant.id}</p>
                    <p>IP: {localParticipant.ip}</p>
                    <p>Codec: {localParticipant.codec}</p>
                  </div>
                </div>
              </div>
            </div>
          )}

          {/* System Log */}
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <Activity className="w-5 h-5 text-purple-400" />
              System Log
            </h2>
            <pre className="text-xs bg-black/50 p-3 rounded-lg max-h-48 overflow-y-auto scrollbar-thin scrollbar-thumb-purple-500/50 scrollbar-track-white/5 font-mono">
              {state.systemLog.length === 0 ? "No events yet..." : state.systemLog.join("\n")}
            </pre>
          </div>
        </div>
      </div>
    </div>
  );
};

export default APMDashboard;
