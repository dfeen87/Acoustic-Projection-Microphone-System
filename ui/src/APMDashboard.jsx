import React, { useRef, useState, useEffect } from "react";
import { WSSignalingClient } from "./adapters/wsSignalingClient";

export default function APMDashboard() {
  // -------------------------
  // UI State
  // -------------------------
  const [status, setStatus] = useState("idle"); // idle | calling | connected
  const [sessionId, setSessionId] = useState(null);
  const [log, setLog] = useState([]);

  // -------------------------
  // Signaling
  // -------------------------
  const signalingRef = useRef(null);
  const peerIdRef = useRef(
    "peer-" + Math.random().toString(36).slice(2)
  );

  // -------------------------
  // Audio
  // -------------------------
  const audioStreamRef = useRef(null);
  const audioElementRef = useRef(null);

  const appendLog = (msg) =>
    setLog((prev) => [...prev.slice(-30), msg]);

  // -------------------------
  // Audio lifecycle
  // -------------------------
  const startAudio = async () => {
    if (audioStreamRef.current) return;

    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true,
        },
      });

      audioStreamRef.current = stream;

      // Local monitoring (muted to avoid feedback)
      const audioEl = document.createElement("audio");
      audioEl.srcObject = stream;
      audioEl.autoplay = true;
      audioEl.muted = true;
      audioElementRef.current = audioEl;

      appendLog("audio capture started");
    } catch (err) {
      appendLog("audio error: " + err.message);
    }
  };

  const stopAudio = () => {
    if (!audioStreamRef.current) return;

    audioStreamRef.current.getTracks().forEach((t) => t.stop());
    audioStreamRef.current = null;
    audioElementRef.current = null;

    appendLog("audio capture stopped");
  };

  // -------------------------
  // Start Call
  // -------------------------
  const initiateCall = async () => {
    const newSessionId = "session-" + Date.now();
    setSessionId(newSessionId);
    setStatus("calling");

    await startAudio();

    if (!signalingRef.current) {
      signalingRef.current = new WSSignalingClient(
        "ws://localhost:8080/ws"
      );

      signalingRef.current.on("joined", (msg) =>
        appendLog(`joined room ${msg.roomId}`)
      );

      signalingRef.current.on("peer_joined", (msg) =>
        appendLog(`peer joined: ${msg.peerId}`)
      );

      signalingRef.current.on("peer_left", (msg) =>
        appendLog(`peer left: ${msg.peerId}`)
      );

      signalingRef.current.on("call", () => {
        appendLog("call received");
        setStatus("connected");
      });

      signalingRef.current.on("accept", () => {
        appendLog("call accepted");
        setStatus("connected");
      });

      signalingRef.current.on("end", () => {
        appendLog("call ended by peer");
        cleanupCall();
      });

      signalingRef.current.connect({
        roomId: newSessionId,
        peerId: peerIdRef.current,
      });
    }

    signalingRef.current.send({
      type: "call",
      sessionId: newSessionId,
    });

    appendLog("call sent");
  };

  // -------------------------
  // End Call
  // -------------------------
  const endCall = () => {
    if (signalingRef.current && sessionId) {
      signalingRef.current.send({
        type: "end",
        sessionId,
      });
    }
    cleanupCall();
  };

  const cleanupCall = () => {
    signalingRef.current?.disconnect();
    signalingRef.current = null;
    stopAudio();
    setSessionId(null);
    setStatus("idle");
    appendLog("call cleanup complete");
  };

  // -------------------------
  // Cleanup on unmount
  // -------------------------
  useEffect(() => {
    return () => {
      signalingRef.current?.disconnect();
      stopAudio();
    };
  }, []);

  // -------------------------
  // Render
  // -------------------------
  return (
    <div
      style={{
        padding: 24,
        fontFamily: "system-ui, sans-serif",
        background: "#0f172a",
        color: "#e5e7eb",
        minHeight: "100vh",
      }}
    >
      <h1>Acoustic Projection Microphone (APM)</h1>

      <p>
        <strong>Status:</strong> {status}
      </p>
      <p>
        <strong>Peer ID:</strong> {peerIdRef.current}
      </p>
      {sessionId && (
        <p>
          <strong>Session:</strong> {sessionId}
        </p>
      )}

      <div style={{ marginTop: 16 }}>
        {status === "idle" && (
          <button onClick={initiateCall}>Call</button>
        )}

        {(status === "calling" || status === "connected") && (
          <button onClick={endCall}>End Call</button>
        )}
      </div>

      <div style={{ marginTop: 24 }}>
        <h3>System Log</h3>
        <pre
          style={{
            background: "#020617",
            padding: 12,
            maxHeight: 320,
            overflowY: "auto",
            fontSize: 12,
          }}
        >
          {log.join("\n")}
        </pre>
      </div>
    </div>
  );
}
