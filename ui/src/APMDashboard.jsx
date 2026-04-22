import React, { useCallback, useEffect, useRef, useState } from "react";
import {
  Phone,
  PhoneOff,
  PhoneIncoming,
  PhoneOutgoing,
  Mic,
  MicOff,
  Volume2,
  VolumeX,
  Shield,
  Lock,
  Unlock,
  Radio,
  Users,
  Settings,
  Activity,
  Globe,
  Wifi,
  WifiOff,
  X,
} from "lucide-react";

const API_BASE = "";

const isValidIPv4 = (ip) => {
  const octets = ip.split(".");
  if (octets.length !== 4) return false;
  return octets.every((octet) => {
    if (!/^\d{1,3}$/.test(octet)) return false;
    const value = Number(octet);
    return value >= 0 && value <= 255;
  });
};

const apiFetch = (path, options = {}) => {
  const url = `${API_BASE}${path}`;
  const apiKey = localStorage.getItem("apm_api_key");
  const headers = { ...options.headers };

  if (apiKey) {
    headers["X-APM-API-Key"] = apiKey;
  }

  if (
    options.method &&
    ["POST", "PUT", "PATCH"].includes(options.method.toUpperCase())
  ) {
    headers["Content-Type"] = "application/json";
  }

  return fetch(url, { ...options, headers, credentials: "include" });
};

const localeByLang = {
  ar: "ar-SA",
  de: "de-DE",
  en: "en-US",
  es: "es-ES",
  fr: "fr-FR",
  hi: "hi-IN",
  it: "it-IT",
  ja: "ja-JP",
  ko: "ko-KR",
  pt: "pt-PT",
  ru: "ru-RU",
  zh: "zh-CN",
};

const parseApiError = async (res, fallback = "Request failed") => {
  if (res.status === 401) {
    return "Unauthorized. Open Settings and enter the API key that matches your Render APM_API_KEY value.";
  }
  try {
    const data = await res.json();
    if (typeof data?.detail === "string" && data.detail.trim()) return data.detail;
    if (typeof data?.message === "string" && data.message.trim()) return data.message;
  } catch (_) {
    // Ignore JSON parse issues; fall back below.
  }
  return `${fallback} (${res.status})`;
};

const Timer = ({ startTime }) => {
  const [now, setNow] = useState(Date.now());
  useEffect(() => {
    const interval = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(interval);
  }, []);
  const duration = Math.max(0, now - startTime);
  const m = Math.floor(duration / 60000);
  const s = String(Math.floor((duration % 60000) / 1000)).padStart(2, "0");
  return (
    <p className="text-lg font-mono">
      {m}:{s}
    </p>
  );
};

const APMDashboard = () => {
  const [callState, setCallState] = useState("idle"); // idle, calling, ringing, connected
  const [activeSession, setActiveSession] = useState(null);
  const [micMuted, setMicMuted] = useState(false);
  const [speakerMuted, setSpeakerMuted] = useState(false);
  const [encryptionEnabled, setEncryptionEnabled] = useState(true);
  const [translationEnabled, setTranslationEnabled] = useState(true);
  const [audioLevel, setAudioLevel] = useState(0);
  const [peers, setPeers] = useState([]);
  const [translations, setTranslations] = useState([]);
  const [localParticipant, setLocalParticipant] = useState({
    id: "local-" + Math.random().toString(36).substr(2, 9),
    name: "You",
    ip: "192.168.1.100",
  });
  const [sourceLang, setSourceLang] = useState("en");
  const [targetLang, setTargetLang] = useState("es");
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [backendReady, setBackendReady] = useState(false);
  const [showLoadingOverlay, setShowLoadingOverlay] = useState(true);
  const [connectionStatus, setConnectionStatus] = useState("connected");
  const [toasts, setToasts] = useState([]);
  const [authPromptShown, setAuthPromptShown] = useState(false);
  const [apiKey, setApiKey] = useState(
    localStorage.getItem("apm_api_key") || "",
  );
  const [serverAuthEnabled, setServerAuthEnabled] = useState(false);
  const [onlineStatus, setOnlineStatus] = useState("online");
  const [incomingPopupVisible, setIncomingPopupVisible] = useState(false);
  const speechRecognitionRef = useRef(null);
  const shouldRunRecognitionRef = useRef(false);
  const speechUnsupportedNotifiedRef = useRef(false);
  const translationCursorRef = useRef(0);

  const addToast = (message) => {
    const id = Date.now();
    setToasts((prev) => [...prev, { id, message }]);
    setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
    }, 3000);
  };

  const speakTranslation = useCallback(
    (text, langCode) => {
      if (!text || speakerMuted || !("speechSynthesis" in window)) return;
      const utterance = new SpeechSynthesisUtterance(text);
      utterance.lang =
        localeByLang[langCode?.toLowerCase?.()] ||
        langCode ||
        localeByLang[targetLang] ||
        "en-US";
      window.speechSynthesis.speak(utterance);
    },
    [speakerMuted, targetLang],
  );

  const promptForApiKey = () => {
    if (authPromptShown || serverAuthEnabled) return;
    setAuthPromptShown(true);
    setSettingsOpen(true);
    addToast(
      "API authentication is enabled. Enter the API key in Settings to continue.",
    );
  };

  useEffect(() => {
    let cancelled = false;
    const check = async () => {
      while (!cancelled) {
        try {
          const res = await fetch("/health");
          if (res.ok) {
            setBackendReady(true);
            return;
          }
        } catch (e) {
          // Server is still waking up.
        }
        await new Promise((r) => setTimeout(r, 2000));
      }
    };
    check();
    return () => {
      cancelled = true;
    };
  }, []);

  useEffect(() => {
    if (!backendReady) return;
    const timer = setTimeout(() => setShowLoadingOverlay(false), 500);
    return () => clearTimeout(timer);
  }, [backendReady]);

  useEffect(() => {
    const handleEsc = (e) => {
      if (e.key === "Escape") setSettingsOpen(false);
    };
    if (settingsOpen) window.addEventListener("keydown", handleEsc);
    return () => window.removeEventListener("keydown", handleEsc);
  }, [settingsOpen]);

  // New V8.1.0 State
  const [metrics, setMetrics] = useState({
    peak_db: -96,
    rms_db: -96,
    snr_db: 0,
    latency_ms: 0,
  });
  const [diagnostics, setDiagnostics] = useState({ ok: true, message: "OK" });
  const [profiles, setProfiles] = useState([]);
  const [activeProfile, setActiveProfile] = useState("Default");
  const [showCalibration, setShowCalibration] = useState(false);
  const [calibrationState, setCalibrationState] = useState({
    step: "Idle",
    progress: 0,
    result: null,
  });

  // Fetch server config on mount to detect if auth is pre-configured via
  // the server-managed session cookie (APM_API_KEY env var on the backend).
  useEffect(() => {
    fetch("/api/config", { credentials: "include" })
      .then((r) => r.json())
      .then((data) => setServerAuthEnabled(Boolean(data.auth_enabled)))
      .catch((err) => console.error("Failed to fetch server config:", err));
  }, []);

  // Fetch local peer status
  useEffect(() => {
    const fetchStatus = async () => {
      try {
        const res = await apiFetch("/api/status");
        if (res.ok) {
          const data = await res.json();
          setOnlineStatus(data.status);
          setLocalParticipant((prev) => ({ ...prev, id: data.peer_id }));
        } else if (res.status === 401) {
          promptForApiKey();
        }
      } catch (e) {}
    };
    fetchStatus();
  }, []);

  const toggleOnlineStatus = async () => {
    const previousStatus = onlineStatus;
    const newStatus = onlineStatus === "online" ? "away" : "online";
    setOnlineStatus(newStatus);
    try {
      const res = await apiFetch("/api/status", {
        method: "POST",
        body: JSON.stringify({ status: newStatus }),
      });
      if (!res.ok) {
        setOnlineStatus(previousStatus);
        addToast(await parseApiError(res, "Failed to update status"));
      }
    } catch (e) {
      setOnlineStatus(previousStatus);
      addToast("Failed to update status (network)");
    }
  };

  // Fetch diagnostics (1Hz)
  useEffect(() => {
    const fetchDiagnostics = async () => {
      try {
        const diagRes = await apiFetch(`/api/health/diagnostics`);
        if (diagRes.ok) setDiagnostics(await diagRes.json());
      } catch (e) {
        setConnectionStatus("disconnected");
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
        const metRes = await apiFetch(`/api/metrics`);
        if (metRes.ok) {
          const data = await metRes.json();
          setMetrics(data);

          if (data.offline) {
            setConnectionStatus("degraded");
          } else {
            setConnectionStatus("connected");
          }
        }
      } catch (e) {
        // Fallback handled in diagnostics fetch, but we might set degraded
        setConnectionStatus("degraded");
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
        const res = await apiFetch(`/api/profiles`);
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
        const res = await apiFetch(`/api/calibration`);
        if (res.ok) setCalibrationState(await res.json());
      } catch (e) {}
    }, 500);
    return () => clearInterval(interval);
  }, [showCalibration]);

  const handleCalibrationAction = async (action) => {
    try {
      await apiFetch(`/api/calibration`, {
        method: "POST",
        body: JSON.stringify({ action }),
      });
    } catch (e) {}
  };

  // Audio visualization
  useEffect(() => {
    if (callState === "connected") {
      const interval = setInterval(() => {
        setAudioLevel(Math.random() * 100);
      }, 100);
      return () => clearInterval(interval);
    } else {
      setAudioLevel(0);
    }
  }, [callState]);

  // Fetch peers
  const fetchPeers = async () => {
    try {
      const res = await apiFetch("/api/peers");
      if (res.ok) {
        const data = await res.json();
        setPeers(data.peers.filter((p) => p.id !== localParticipant.id));
      }
    } catch (e) {}
  };

  useEffect(() => {
    fetchPeers();
    const interval = setInterval(fetchPeers, 5000);
    return () => clearInterval(interval);
  }, [localParticipant.id]);

  const [newPeerName, setNewPeerName] = useState("");
  const [newPeerIp, setNewPeerIp] = useState("");

  const handleAddPeer = async (e) => {
    e.preventDefault();
    if (!newPeerName || !isValidIPv4(newPeerIp)) return;
    try {
      const res = await apiFetch("/api/peers", {
        method: "POST",
        body: JSON.stringify({ name: newPeerName, ip: newPeerIp }),
      });
      if (res.ok) {
        setNewPeerName("");
        setNewPeerIp("");
        addToast("Peer added");
        fetchPeers();
      } else {
        addToast(await parseApiError(res, "Failed to add peer"));
      }
    } catch (e) {
      addToast("Failed to add peer (network)");
    }
  };

  const handleDeletePeer = async (e, peerId) => {
    e.stopPropagation();
    try {
      const res = await apiFetch(`/api/peers/${peerId}`, { method: "DELETE" });
      if (res.ok) {
        fetchPeers();
      }
    } catch (e) {}
  };

  const streamTranslationToScreen = async (spokenText) => {
    const text = spokenText?.trim();
    if (!text) return;

    let translatedText = text;
    const source = sourceLang?.trim()?.toLowerCase() || "en";
    const target = targetLang?.trim()?.toLowerCase() || "es";

    if (source !== target) {
      try {
        const response = await apiFetch("/api/translate", {
          method: "POST",
          body: JSON.stringify({
            text,
            source_lang: source,
            target_lang: target,
          }),
        });

        if (response.ok) {
          const payload = await response.json();
          if (typeof payload?.translated_text === "string" && payload.translated_text.trim()) {
            translatedText = payload.translated_text.trim();
          }
        }
      } catch (_) {
        // Fall through to optional in-browser bridge below.
      }
    }

    const bridgeTranslator = globalThis?.APMTranslationBridge?.translate;
    if (translatedText === text && source !== target && typeof bridgeTranslator === "function") {
      try {
        const result = await bridgeTranslator({
          text,
          sourceLang: source,
          targetLang: target,
        });
        if (typeof result === "string" && result.trim()) {
          translatedText = result.trim();
        } else if (typeof result?.translatedText === "string" && result.translatedText.trim()) {
          translatedText = result.translatedText.trim();
        }
      } catch (_) {
        // Gracefully fall back to transcript text when the translation bridge is unavailable.
      }
    }

    setTranslations((prev) => [
      ...prev.slice(-9),
      {
        orig: text,
        trans: translatedText,
        from: "local",
        time: Date.now(),
      },
    ]);

    speakTranslation(translatedText, target);

    if (activeSession?.id && translatedText) {
      try {
        await apiFetch(`/api/session/${activeSession.id}/translation`, {
          method: "POST",
          body: JSON.stringify({
            sender_peer_id: localParticipant.id,
            source_language: source,
            target_language: target,
            original_text: text,
            translated_text: translatedText,
            timestamp_ms: Date.now(),
          }),
        });
      } catch (_) {
        // Keep local UX responsive even if delivery fails.
      }
    }
  };

  // Real-time on-screen translation stream from microphone speech.
  useEffect(() => {
    if (!(callState === "connected" && translationEnabled)) {
      shouldRunRecognitionRef.current = false;
      if (speechRecognitionRef.current) {
        speechRecognitionRef.current.onresult = null;
        speechRecognitionRef.current.onend = null;
        speechRecognitionRef.current.onerror = null;
        speechRecognitionRef.current.stop();
        speechRecognitionRef.current = null;
      }
      return undefined;
    }

    const SpeechRecognition =
      window.SpeechRecognition || window.webkitSpeechRecognition;

    if (!SpeechRecognition) {
      if (!speechUnsupportedNotifiedRef.current) {
        addToast("Speech recognition is not available in this browser.");
        speechUnsupportedNotifiedRef.current = true;
      }
      return undefined;
    }

    shouldRunRecognitionRef.current = true;
    const recognition = new SpeechRecognition();
    speechRecognitionRef.current = recognition;
    recognition.lang = localeByLang[sourceLang] || sourceLang || "en-US";
    recognition.continuous = true;
    recognition.interimResults = true;

    recognition.onresult = (event) => {
      for (let i = event.resultIndex; i < event.results.length; i += 1) {
        const result = event.results[i];
        if (!result.isFinal) continue;
        const transcript = result[0]?.transcript?.trim();
        if (transcript) {
          streamTranslationToScreen(transcript);
        }
      }
    };

    recognition.onerror = (event) => {
      if (event?.error === "not-allowed") {
        addToast("Microphone permission is required for real-time translation.");
      }
    };

    recognition.onend = () => {
      if (!shouldRunRecognitionRef.current) return;
      try {
        recognition.start();
      } catch (_) {
        // start() can throw if already started; ignore and rely on next onend.
      }
    };

    try {
      recognition.start();
    } catch (_) {
      addToast("Unable to start voice capture for translation.");
    }

    return () => {
      shouldRunRecognitionRef.current = false;
      recognition.onresult = null;
      recognition.onend = null;
      recognition.onerror = null;
      recognition.stop();
      if (speechRecognitionRef.current === recognition) {
        speechRecognitionRef.current = null;
      }
    };
  }, [activeSession?.id, callState, localParticipant.id, sourceLang, targetLang, translationEnabled]);

  useEffect(() => {
    if (!(activeSession?.id && callState === "connected" && translationEnabled)) {
      translationCursorRef.current = 0;
      return undefined;
    }

    const pollIncomingTranslations = async () => {
      try {
        const res = await apiFetch(
          `/api/session/${activeSession.id}/translations?since_ms=${translationCursorRef.current}`,
        );
        if (!res.ok) return;
        const data = await res.json();
        const entries = Array.isArray(data?.translations) ? data.translations : [];
        if (entries.length === 0) return;

        let maxTimestamp = translationCursorRef.current;
        const remoteEntries = entries
          .filter((entry) => entry?.sender_peer_id && entry.sender_peer_id !== localParticipant.id)
          .map((entry) => {
            const timestamp = Number(entry.timestamp_ms) || Date.now();
            maxTimestamp = Math.max(maxTimestamp, timestamp);
            return {
              orig: entry.original_text || "",
              trans: entry.translated_text || "",
              from: "remote",
              time: timestamp,
              lang: entry.target_language || targetLang,
            };
          })
          .filter((entry) => entry.orig && entry.trans);

        entries.forEach((entry) => {
          const timestamp = Number(entry?.timestamp_ms) || 0;
          maxTimestamp = Math.max(maxTimestamp, timestamp);
        });
        translationCursorRef.current = maxTimestamp;

        if (remoteEntries.length > 0) {
          setTranslations((prev) => [...prev, ...remoteEntries].slice(-10));
          remoteEntries.forEach((entry) => speakTranslation(entry.trans, entry.lang));
        }
      } catch (_) {
        // Non-fatal polling errors.
      }
    };

    pollIncomingTranslations();
    const interval = setInterval(pollIncomingTranslations, 900);
    return () => clearInterval(interval);
  }, [activeSession?.id, callState, localParticipant.id, speakTranslation, translationEnabled]);

  const initiateCall = async (peer) => {
    setCallState("calling");
    try {
      const res = await apiFetch(`/api/session?peer_id=${peer.id}`, {
        method: "POST",
      });
      if (res.ok) {
        const data = await res.json();
        setActiveSession({
          id: data.session.id,
          peer: peer,
          startTime: Date.now(),
          encrypted: encryptionEnabled,
        });
      } else {
        setCallState("idle");
        addToast(await parseApiError(res, "Failed to start call"));
      }
    } catch (e) {
      setCallState("idle");
      addToast("Failed to start call (network)");
    }
  };

  const endCall = async () => {
    if (activeSession) {
      try {
        await apiFetch(`/api/session/${activeSession.id}/end`, {
          method: "POST",
        });
      } catch (e) {}
    }
    setCallState("idle");
    setActiveSession(null);
    setIncomingPopupVisible(false);
    setTranslations([]);
    addToast("Call ended");
  };

  const acceptCall = async () => {
    if (activeSession) {
      try {
        await apiFetch(`/api/session/${activeSession.id}/accept`, {
          method: "POST",
        });
      } catch (e) {}
    }
    setCallState("connected");
    setIncomingPopupVisible(false);
    addToast(`Connected to ${activeSession?.peer?.name}`);
  };

  const rejectCall = async () => {
    if (activeSession) {
      try {
        await apiFetch(`/api/session/${activeSession.id}/end`, {
          method: "POST",
        });
      } catch (e) {}
    }
    setCallState("idle");
    setActiveSession(null);
    setIncomingPopupVisible(false);
  };

  useEffect(() => {
    if (callState === "connected" || callState === "calling") return;
    const pollIncomingSession = async () => {
      try {
        const res = await apiFetch("/api/session/incoming");
        if (!res.ok) return;
        const data = await res.json();

        if (!data.session) {
          if (callState === "ringing" && !activeSession) {
            setCallState("idle");
            setIncomingPopupVisible(false);
          }
          return;
        }

        const incomingSessionId = data.session.id;
        if (!activeSession || activeSession.id !== incomingSessionId) {
          setActiveSession({
            id: incomingSessionId,
            peer: { id: data.session.peer_id, name: "Incoming caller" },
            startTime: Date.now(),
            encrypted: encryptionEnabled,
          });
        }
        setCallState("ringing");
        setIncomingPopupVisible(true);
      } catch (e) {}
    };

    pollIncomingSession();
    const interval = setInterval(pollIncomingSession, 1000);
    return () => clearInterval(interval);
  }, [callState, activeSession, encryptionEnabled]);

  useEffect(() => {
    if (!activeSession || callState === "idle") return;
    const interval = setInterval(async () => {
      try {
        const res = await apiFetch(`/api/session/${activeSession.id}`);
        if (res.ok) {
          const data = await res.json();
          if (
            data.session.status === "connected" &&
            callState !== "connected"
          ) {
            setCallState("connected");
            addToast(`Connected to ${activeSession.peer.name}`);
          } else if (
            data.session.status === "ended" ||
            data.session.status === "timeout"
          ) {
            endCall();
          }
        }
      } catch (e) {}
    }, 1000);
    return () => clearInterval(interval);
  }, [activeSession, callState]);

  const [dialIp, setDialIp] = useState("");
  const handleDial = (char) => {
    if (char === "delete") {
      setDialIp((prev) => prev.slice(0, -1));
    } else {
      setDialIp((prev) => prev + char);
    }
  };
  const handleCallDialed = async () => {
    if (!dialIp) return;
    const existing = peers.find((p) => p.ip === dialIp);
    if (existing) {
      initiateCall(existing);
    } else {
      try {
        const registerRes = await apiFetch("/api/peers", {
          method: "POST",
          body: JSON.stringify({
            name: `Peer ${dialIp}`,
            ip: dialIp,
          }),
        });

        if (!registerRes.ok) {
          addToast(await parseApiError(registerRes, "Failed to register peer"));
          return;
        }

        const registerData = await registerRes.json();
        fetchPeers();
        initiateCall(registerData.peer);
      } catch (e) {
        addToast("Failed to register peer (network)");
      }
    }
  };

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
  ];

  return (
    <div className="min-h-screen bg-gradient-to-br from-slate-900 via-purple-900 to-slate-900 text-white p-4 relative">
      {showLoadingOverlay && (
        <div
          className={`fixed inset-0 bg-black/80 backdrop-blur-sm z-50 flex items-center justify-center transition-opacity duration-500 ease-out ${backendReady ? "opacity-0 pointer-events-none" : "opacity-100"}`}
        >
          <div className="text-center px-6">
            <Radio
              className="w-16 h-16 text-purple-300 mx-auto animate-spin"
              style={{ animationDuration: "3s" }}
            />
            <p className="mt-6 text-2xl font-bold text-white">APM System v8.1</p>
            <p className="mt-3 text-white/60 animate-pulse">Waking up server...</p>
            <p className="mt-2 text-sm text-white/40">
              Free tier instances sleep after inactivity. This takes about 30–60
              seconds.
            </p>
          </div>
        </div>
      )}

      {incomingPopupVisible && callState === "ringing" && activeSession && (
        <div className="fixed right-4 top-4 z-[70] w-full max-w-sm rounded-2xl border border-purple-400/50 bg-slate-900/95 p-4 shadow-2xl backdrop-blur">
          <div className="flex items-start justify-between gap-3">
            <div>
              <p className="text-xs uppercase tracking-wider text-purple-300">
                Incoming call
              </p>
              <p className="mt-1 text-lg font-semibold text-white">
                {activeSession?.peer?.name || "Incoming caller"}
              </p>
              <p className="text-xs text-slate-300">Session: {activeSession.id}</p>
            </div>
            <button
              onClick={rejectCall}
              className="rounded-full p-1 text-slate-300 transition hover:bg-slate-700 hover:text-white"
              aria-label="Dismiss incoming call popup"
            >
              <X className="h-4 w-4" />
            </button>
          </div>
          <div className="mt-4 flex gap-2">
            <button
              onClick={acceptCall}
              className="flex-1 rounded-xl bg-green-600 px-3 py-2 text-sm font-semibold transition hover:bg-green-500"
            >
              Accept
            </button>
            <button
              onClick={rejectCall}
              className="flex-1 rounded-xl bg-red-600 px-3 py-2 text-sm font-semibold transition hover:bg-red-500"
            >
              Decline
            </button>
          </div>
        </div>
      )}

      <div
        className={`transition-all duration-500 ${showLoadingOverlay ? "blur-sm pointer-events-none select-none" : ""}`}
      >
      {/* Toasts container */}
      <div className="fixed top-4 right-4 z-50 flex flex-col gap-2">
        {toasts.map((toast) => (
          <div
            key={toast.id}
            className="bg-black/80 backdrop-blur-md border border-white/10 px-4 py-2 rounded-lg shadow-lg flex items-center gap-3 animate-in fade-in slide-in-from-top-2"
          >
            <span className="text-sm">{toast.message}</span>
            <button
              onClick={() =>
                setToasts((prev) => prev.filter((t) => t.id !== toast.id))
              }
              className="text-gray-400 hover:text-white"
            >
              <X className="w-4 h-4" />
            </button>
          </div>
        ))}
      </div>

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
                APM System v8.1
              </h1>
              <p className="text-sm text-gray-400">
                Acoustic Projection & Translation
              </p>
            </div>
          </div>

          <div className="flex items-center gap-4">
            {!diagnostics.ok && (
              <div className="flex items-center gap-2 bg-red-500/20 px-3 py-1 rounded-lg border border-red-500/50">
                <span className="text-sm text-red-400">
                  ⚠️ {diagnostics.message}
                </span>
              </div>
            )}

            <div className="flex items-center gap-2 mr-2">
              <span className="text-sm text-gray-400">Status</span>
              <button
                onClick={toggleOnlineStatus}
                className={`relative w-16 h-8 rounded-full transition-colors flex items-center ${
                  onlineStatus === "online" ? "bg-green-500" : "bg-gray-600"
                }`}
              >
                <div
                  className={`absolute top-1 left-1 w-6 h-6 bg-white rounded-full transition-transform flex items-center justify-center ${
                    onlineStatus === "online" ? "translate-x-8" : ""
                  }`}
                >
                  {onlineStatus === "online" ? (
                    <Wifi className="w-4 h-4 text-green-500" />
                  ) : (
                    <WifiOff className="w-4 h-4 text-gray-500" />
                  )}
                </div>
              </button>
            </div>

            <button
              onClick={() => setSettingsOpen(true)}
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

            {callState === "idle" && (
              <div className="text-center py-12">
                <Phone className="w-16 h-16 mx-auto mb-4 text-gray-500" />
                <p className="text-gray-400 mb-2">No active call</p>
                <p className="text-sm text-gray-500">
                  Select a peer to start a call
                </p>
              </div>
            )}

            {callState === "calling" && (
              <div className="text-center py-12">
                <div className="relative inline-block mb-4">
                  <PhoneOutgoing className="w-16 h-16 text-purple-400 animate-pulse" />
                  <div className="absolute inset-0 w-16 h-16 border-4 border-purple-400 rounded-full animate-ping" />
                </div>
                <p className="text-lg mb-2">
                  Calling {activeSession?.peer?.name}...
                </p>
                <p className="text-sm text-gray-400">
                  {activeSession?.peer?.ip}
                </p>
                <button
                  onClick={endCall}
                  className="mt-6 px-6 py-3 bg-red-500 hover:bg-red-600 rounded-xl transition-colors"
                >
                  Cancel
                </button>
              </div>
            )}

            {callState === "ringing" && (
              <div className="text-center py-12">
                <div className="relative inline-block mb-4">
                  <PhoneIncoming className="w-16 h-16 text-green-400 animate-bounce" />
                </div>
                <p className="text-lg mb-2">
                  Incoming call from {activeSession?.peer?.name}
                </p>
                <p className="text-sm text-gray-400 mb-6">
                  {activeSession?.peer?.ip}
                </p>
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

            {callState === "connected" && activeSession && (
              <div>
                <div className="flex items-center justify-between mb-6 p-4 bg-purple-500/10 rounded-xl">
                  <div className="flex items-center gap-4">
                    <div className="w-12 h-12 rounded-full bg-gradient-to-br from-purple-400 to-pink-400 flex items-center justify-center text-xl font-bold">
                      {activeSession.peer.name.charAt(0)}
                    </div>
                    <div>
                      <p className="font-semibold flex items-center gap-2">
                        {activeSession.peer.name}
                        {metrics.latency_ms < 50 && (
                          <span className="text-[10px] px-2 py-0.5 bg-green-500/20 text-green-400 rounded-full border border-green-500/30">
                            Excellent
                          </span>
                        )}
                        {metrics.latency_ms >= 50 &&
                          metrics.latency_ms <= 150 && (
                            <span className="text-[10px] px-2 py-0.5 bg-yellow-500/20 text-yellow-400 rounded-full border border-yellow-500/30">
                              Good
                            </span>
                          )}
                        {metrics.latency_ms > 150 && (
                          <span className="text-[10px] px-2 py-0.5 bg-red-500/20 text-red-400 rounded-full border border-red-500/30">
                            Poor
                          </span>
                        )}
                      </p>
                      <p className="text-sm text-gray-400">
                        {activeSession.peer.ip}
                      </p>
                    </div>
                  </div>
                  <div className="text-right">
                    <p className="text-sm text-gray-400">Duration</p>
                    <Timer startTime={activeSession.startTime} />
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
                          opacity: Math.random() * 0.5 + 0.5,
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
                      micMuted
                        ? "bg-red-500 hover:bg-red-600"
                        : "bg-white/10 hover:bg-white/20"
                    }`}
                  >
                    {micMuted ? (
                      <MicOff className="w-6 h-6" />
                    ) : (
                      <Mic className="w-6 h-6" />
                    )}
                  </button>

                  <button
                    onClick={() => setSpeakerMuted(!speakerMuted)}
                    className={`p-4 rounded-xl transition-colors ${
                      speakerMuted
                        ? "bg-red-500 hover:bg-red-600"
                        : "bg-white/10 hover:bg-white/20"
                    }`}
                  >
                    {speakerMuted ? (
                      <VolumeX className="w-6 h-6" />
                    ) : (
                      <Volume2 className="w-6 h-6" />
                    )}
                  </button>

                  <button
                    onClick={() => setTranslationEnabled(!translationEnabled)}
                    className={`p-4 rounded-xl transition-colors ${
                      translationEnabled
                        ? "bg-purple-500 hover:bg-purple-600"
                        : "bg-white/10 hover:bg-white/20"
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
          {translationEnabled && callState === "connected" && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <div className="flex items-center justify-between mb-4">
                <h2 className="text-xl font-semibold flex items-center gap-2">
                  <Globe className="w-5 h-5 text-purple-400" />
                  Live Call Translation
                </h2>
                <p className="text-xs text-gray-400">
                  Transcript capture + spoken translated audio output
                </p>
                <div className="flex gap-2 text-sm">
                  <span className="px-3 py-1 bg-purple-500/20 rounded-lg">
                    {sourceLang.toUpperCase()}
                  </span>
                  <span className="text-gray-500">→</span>
                  <span className="px-3 py-1 bg-pink-500/20 rounded-lg">
                    {targetLang.toUpperCase()}
                  </span>
                </div>
              </div>

              <div className="space-y-3 max-h-64 overflow-y-auto custom-scrollbar">
                {translations.length === 0 ? (
                  <p className="text-center text-gray-500 py-8">
                    Listening for live call speech...
                  </p>
                ) : (
                  translations.map((trans, idx) => (
                    <div
                      key={idx}
                      className={`p-4 rounded-xl border ${
                        trans.from === "local"
                          ? "bg-purple-500/20 border-purple-500/30"
                          : "bg-white/5 border-white/10"
                      }`}
                    >
                      <p className="text-xs uppercase tracking-wide text-gray-400 mb-1">
                        {trans.from === "local" ? "You said" : "Peer said"}
                      </p>
                      <p className="text-sm text-gray-400 mb-1">{trans.orig}</p>
                      <p className="text-xs uppercase tracking-wide text-gray-500 mb-1">
                        Spoken translation
                      </p>
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
                <h3 className="font-semibold text-purple-300 mb-2">
                  Current Step: {calibrationState.step}
                </h3>
                <p className="text-sm text-gray-400">
                  {calibrationState.step === "Idle" &&
                    "Ready to begin calibration."}
                  {calibrationState.step === "MeasureNoiseFloor" &&
                    "Measuring ambient noise. Please remain quiet..."}
                  {calibrationState.step === "MeasureGain" &&
                    "Please speak at a normal volume..."}
                  {calibrationState.step === "MeasureLatency" &&
                    "Estimating round-trip latency..."}
                  {calibrationState.step === "Complete" &&
                    "Calibration successful!"}
                </p>

                {calibrationState.result && (
                  <div className="mt-4 pt-4 border-t border-white/10 space-y-2 text-sm text-green-300">
                    <div>
                      Noise Floor:{" "}
                      {calibrationState.result.rms_noise_floor_db.toFixed(1)} dB
                    </div>
                    <div>
                      Suggested Gain:{" "}
                      {(
                        calibrationState.result.recommended_input_gain * 100
                      ).toFixed(0)}
                      %
                    </div>
                    <div>
                      Est. Latency:{" "}
                      {calibrationState.result.estimated_latency_ms.toFixed(1)}{" "}
                      ms
                    </div>
                  </div>
                )}
              </div>

              <div className="flex justify-end gap-3">
                <button
                  onClick={() => {
                    handleCalibrationAction("cancel");
                    setShowCalibration(false);
                  }}
                  className="px-4 py-2 rounded-lg bg-white/10 hover:bg-white/20 transition-colors"
                >
                  Close
                </button>
                {calibrationState.step === "Idle" ? (
                  <button
                    onClick={() => handleCalibrationAction("start")}
                    className="px-4 py-2 rounded-lg bg-purple-500 hover:bg-purple-600 transition-colors start-btn"
                  >
                    Start
                  </button>
                ) : calibrationState.step !== "Complete" ? (
                  <button
                    onClick={() => handleCalibrationAction("advance")}
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
          {/* Dial Pad */}
          {callState === "idle" && (
            <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
              <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
                <Phone className="w-5 h-5 text-purple-400" />
                Dial
              </h2>
              <div className="bg-black/40 p-4 rounded-xl mb-4">
                <div className="text-2xl font-mono text-center tracking-wider h-8 min-h-8">
                  {dialIp || <span className="text-gray-600">Enter IP</span>}
                </div>
              </div>
              <div className="grid grid-cols-3 gap-2 mb-4">
                {["1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0"].map(
                  (key) => (
                    <button
                      key={key}
                      onClick={() => handleDial(key)}
                      className="p-3 bg-white/5 hover:bg-white/10 rounded-xl text-lg font-semibold transition-colors"
                    >
                      {key}
                    </button>
                  ),
                )}
                <button
                  onClick={() => handleDial("delete")}
                  className="p-3 bg-white/5 hover:bg-white/10 rounded-xl text-lg font-semibold transition-colors flex items-center justify-center text-red-400"
                >
                  ⌫
                </button>
              </div>
              <button
                onClick={handleCallDialed}
                className="w-full py-4 bg-green-500 hover:bg-green-600 rounded-xl flex items-center justify-center gap-2 transition-colors font-bold"
              >
                <Phone className="w-5 h-5" />
                Call
              </button>
            </div>
          )}

          {/* Peers List */}
          <div className="bg-black/30 backdrop-blur-xl rounded-2xl p-6 border border-purple-500/20">
            <h2 className="text-xl font-semibold mb-4 flex items-center gap-2">
              <Users className="w-5 h-5 text-purple-400" />
              Available Peers
            </h2>

            <div className="space-y-3">
              {peers.length === 0 ? (
                <p className="text-gray-400 text-sm text-center py-4">
                  No peers yet — add one below to get started
                </p>
              ) : (
                peers.map((peer) => (
                  <div
                    key={peer.id}
                    className="p-4 bg-white/5 hover:bg-white/10 rounded-xl transition-colors cursor-pointer group relative"
                    onClick={() => callState === "idle" && initiateCall(peer)}
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
                        <div
                          className={`w-2 h-2 rounded-full ${
                            peer.status === "online"
                              ? "bg-green-400"
                              : "bg-yellow-400"
                          }`}
                        />
                        {callState === "idle" && (
                          <Phone className="w-5 h-5 text-purple-400 opacity-0 group-hover:opacity-100 transition-opacity" />
                        )}
                        {callState === "idle" && (
                          <button
                            onClick={(e) => handleDeletePeer(e, peer.id)}
                            className="ml-2 p-1 hover:bg-red-500/20 rounded text-red-400 opacity-0 group-hover:opacity-100 transition-opacity"
                          >
                            <X className="w-4 h-4" />
                          </button>
                        )}
                      </div>
                    </div>
                  </div>
                ))
              )}
            </div>

            <form
              onSubmit={handleAddPeer}
              className="mt-6 pt-6 border-t border-white/10"
            >
              <h3 className="text-sm font-semibold mb-3">Add Peer</h3>
              <div className="space-y-3">
                <input
                  type="text"
                  placeholder="Name"
                  value={newPeerName}
                  onChange={(e) => setNewPeerName(e.target.value)}
                  className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500 text-sm"
                  required
                />
                <input
                  type="text"
                  placeholder="IP Address (e.g. 192.168.1.100)"
                  value={newPeerIp}
                  onChange={(e) => setNewPeerIp(e.target.value)}
                  className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500 text-sm"
                  pattern="^(?:[0-9]{1,3}\.){3}[0-9]{1,3}$"
                  required
                />
                <button
                  type="submit"
                  className="w-full py-2 bg-purple-500 hover:bg-purple-600 rounded-lg transition-colors text-sm font-semibold"
                >
                  Add
                </button>
              </div>
            </form>
          </div>

        </div>
      </div>
      </div>

      {/* Settings Modal */}
      {settingsOpen && (
        <div
          className="fixed inset-0 bg-black/60 backdrop-blur-sm z-40 flex items-center justify-center"
          onClick={() => setSettingsOpen(false)}
        >
          <div
            className="bg-gray-900 border border-white/10 rounded-2xl p-6 w-full max-w-md mx-4 shadow-2xl max-h-[90vh] overflow-y-auto"
            onClick={(e) => e.stopPropagation()}
          >
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-lg font-semibold text-white flex items-center gap-2">
                <Settings className="w-5 h-5" /> Settings
              </h2>
              <button
                onClick={() => setSettingsOpen(false)}
                className="text-white/40 hover:text-white transition-colors"
              >
                ✕
              </button>
            </div>

            <div className="space-y-4">
              <div>
                <label className="text-sm text-gray-400 mb-2 block">API Key</label>
                {serverAuthEnabled ? (
                  <div className="flex items-center gap-2 rounded-lg border border-green-500/30 bg-green-500/10 px-4 py-2">
                    <Shield className="w-4 h-4 text-green-400 shrink-0" />
                    <p className="text-xs text-green-300">
                      Authentication is pre-configured by the server. No key
                      required.
                    </p>
                  </div>
                ) : (
                  <>
                    <input
                      type="password"
                      value={apiKey}
                      onChange={(e) => {
                        setApiKey(e.target.value);
                        localStorage.setItem("apm_api_key", e.target.value);
                      }}
                      placeholder="Enter API Key"
                      className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500"
                    />
                    <p className="mt-2 text-xs text-gray-400">
                      Use the same value configured as <code>APM_API_KEY</code>{" "}
                      in Render environment settings.
                    </p>
                  </>
                )}
              </div>

              {/* Profiles & Calibration */}
              <div className="pt-2 pb-4 border-b border-white/10">
                <div className="flex items-center justify-between mb-4">
                  <label className="text-sm font-semibold text-gray-300">
                    Active Profile
                  </label>
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
                  {profiles.map((p) => (
                    <option key={p.name} value={p.name}>
                      {p.name}
                    </option>
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
                  <div
                    className={`transition-opacity ${metrics.offline ? "opacity-30" : "opacity-100"}`}
                  >
                    <div className="flex justify-between">
                      <span className="text-gray-400">Peak Level</span>
                      <span
                        className={
                          metrics.clipping
                            ? "text-red-400 font-bold"
                            : "text-green-400"
                        }
                      >
                        {metrics.peak_db.toFixed(1)} dB{" "}
                        {metrics.clipping && " (CLIP)"}
                      </span>
                    </div>
                    <div className="flex justify-between mt-2">
                      <span className="text-gray-400">SNR</span>
                      <span className="text-blue-400">
                        {metrics.snr_db.toFixed(1)} dB
                      </span>
                    </div>
                    <div className="flex justify-between mt-2">
                      <span className="text-gray-400">Est. Latency</span>
                      <span className="text-yellow-400">
                        {metrics.latency_ms.toFixed(1)} ms
                      </span>
                    </div>
                  </div>
                </div>
              </div>

              <div>
                <label className="text-sm text-gray-400 mb-2 block">
                  Source Language
                </label>
                <select
                  value={sourceLang}
                  onChange={(e) => setSourceLang(e.target.value)}
                  className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500"
                >
                  {languages.map((lang) => (
                    <option key={lang.code} value={lang.code}>
                      {lang.name}
                    </option>
                  ))}
                </select>
              </div>

              <div>
                <label className="text-sm text-gray-400 mb-2 block">
                  Target Language
                </label>
                <select
                  value={targetLang}
                  onChange={(e) => setTargetLang(e.target.value)}
                  className="w-full bg-white/5 border border-white/10 rounded-lg px-4 py-2 focus:outline-none focus:border-purple-500"
                >
                  {languages.map((lang) => (
                    <option key={lang.code} value={lang.code}>
                      {lang.name}
                    </option>
                  ))}
                </select>
              </div>

              <div className="pt-4 border-t border-white/10">
                <div className="flex items-center justify-between mb-3">
                  <span className="text-sm">End-to-End Encryption</span>
                  <button
                    onClick={() => setEncryptionEnabled(!encryptionEnabled)}
                    className={`relative w-12 h-6 rounded-full transition-colors ${
                      encryptionEnabled ? "bg-green-500" : "bg-gray-600"
                    }`}
                  >
                    <div
                      className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                        encryptionEnabled ? "translate-x-6" : ""
                      }`}
                    />
                  </button>
                </div>

                <div className="flex items-center justify-between">
                  <span className="text-sm">Real-time Translation</span>
                  <button
                    onClick={() => setTranslationEnabled(!translationEnabled)}
                    className={`relative w-12 h-6 rounded-full transition-colors ${
                      translationEnabled ? "bg-purple-500" : "bg-gray-600"
                    }`}
                  >
                    <div
                      className={`absolute top-1 left-1 w-4 h-4 bg-white rounded-full transition-transform ${
                        translationEnabled ? "translate-x-6" : ""
                      }`}
                    />
                  </button>
                </div>
              </div>

              <div className="pt-4 border-t border-white/10">
                <p className="text-xs text-gray-500">
                  Local Participant: {localParticipant.name}
                  <br />
                  IP: {localParticipant.ip}
                  <br />
                  Port: 5060
                </p>
              </div>
            </div>

            <div className="mt-6 pt-4 border-t border-white/10">
              <button
                onClick={() => setSettingsOpen(false)}
                className="w-full py-2 rounded-lg bg-purple-600 hover:bg-purple-500 text-white font-medium transition-colors"
              >
                Done
              </button>
            </div>
          </div>
        </div>
      )}

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
