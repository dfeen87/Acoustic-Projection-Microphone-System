import React, { useRef, useState, useEffect } from "react";
import { WSSignalingClient } from "./adapters/wsSignalingClient";

export default function APMDashboard() {
  // -------------------------
  // UI State
  // -------------------------
  const [status, setStatus] = useState("idle"); // idle | calling | ringing | connected
  const [sessionId, setSessionId] = useState(null);
  const [incomingSessionId, setIncomingSessionId] = useState(null);
  const [log, setLog] = useState([]);

  const appendLog = (msg) =>
    setLog((prev) => [...prev.slice(-40), `[${new Date().toLocaleTimeString()}] ${msg}`]);

  // -------------------------
  // Identity
  // -------------------------
  const peerIdRef = useRef("peer-" + Math.random().toString(36).slice(2));
  const remotePeerIdRef = useRef(null);

  // -------------------------
  // Signaling
  // -------------------------
  const signalingRef = useRef(null);

  // -------------------------
  // Audio (local mic)
  // -------------------------
  const audioStreamRef = useRef(null);

  // -------------------------
  // WebRTC
  // -------------------------
  const pcRef = useRef(null);
  const remoteStreamRef = useRef(new MediaStream());
  const remoteAudioElRef = useRef(null);

  // Offer storage (when receiver gets an offer before clicking Accept)
  const pendingOfferRef = useRef(null);

  // -------------------------
  // Helpers
  // -------------------------
  const ensureRemoteAudioElement = () => {
    if (remoteAudioElRef.current) return;

    const el = document.createElement("audio");
    el.autoplay = true;
    el.playsInline = true;
    el.controls = true; // helpful during debugging; remove later if you want
    el.style.marginTop = "10px";
    el.srcObject = remoteStreamRef.current;

    remoteAudioElRef.current = el;
    document.body.appendChild(el);

    appendLog("remote audio element created (controls enabled)");
  };

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
      appendLog("microphone capture started");
      return stream;
    } catch (err) {
      appendLog("mic error: " + err.message);
      throw err;
    }
  };

  const stopAudio = () => {
    if (!audioStreamRef.current) return;
    audioStreamRef.current.getTracks().forEach((t) => t.stop());
    audioStreamRef.current = null;
    appendLog("microphone capture stopped");
  };

  const createPeerConnection = (activeSessionId) => {
    // Close any prior PC safely
    if (pcRef.current) {
      try {
        pcRef.current.close();
      } catch {}
      pcRef.current = null;
    }

    // NOTE: For LAN/most environments, STUN helps NAT traversal.
    // This is safe and standard. If you want "LAN-only" you can remove iceServers.
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
      // Add all remote tracks to our remote stream
      event.streams[0].getTracks().forEach((track) => {
        remoteStreamRef.current.addTrack(track);
      });
      ensureRemoteAudioElement();
      appendLog("received remote audio track(s)");
    };

    pc.onconnectionstatechange = () => {
      appendLog("pc state: " + pc.connectionState);
      if (pc.connectionState === "connected") {
        setStatus("connected");
      }
      if (pc.connectionState === "failed" || pc.connectionState === "closed" || pc.connectionState === "disconnected") {
        // Let user end/retry
      }
    };

    pcRef.current = pc;
    appendLog("peer connection created");
    return pc;
  };

  const ensureSignaling = (activeSessionId) => {
    if (signalingRef.current) return;

    const s = new WSSignalingClient();

    s.on("joined", (msg) => appendLog(`joined room ${msg.roomId}`));
    s.on("peer_joined", (msg) => {
      appendLog(`peer joined: ${msg.peerId}`);
      remotePeerIdRef.current = msg.peerId;
    });
    s.on("peer_left", (msg) => {
      appendLog(`peer left: ${msg.peerId}`);
      if (remotePeerIdRef.current === msg.peerId) remotePeerIdRef.current = null;
    });

    // Call control
    s.on("call", (msg) => {
      appendLog("incoming call signal");
      setIncomingSessionId(msg.sessionId);
      setStatus("ringing");
    });

    s.on("end", () => {
      appendLog("call ended by peer");
      cleanupAll();
    });

    // WebRTC signaling
    s.on("offer", (msg) => {
      appendLog("offer received");
      pendingOfferRef.current = msg;
      // If we aren't already ringing, prompt user to accept
      setIncomingSessionId(msg.sessionId);
      setStatus((prev) => (prev === "connected" ? prev : "ringing"));
    });

    s.on("answer", async (msg) => {
      appendLog("answer received");
      try {
        const pc = pcRef.current;
        if (!pc) return;
        await pc.setRemoteDescription(msg.sdp);
        appendLog("remote description set (answer)");
      } catch (err) {
        appendLog("answer handling error: " + err.message);
      }
    });

    s.on("ice", async (msg) => {
      try {
        const pc = pcRef.current;
        if (!pc) return;
        await pc.addIceCandidate(msg.candidate);
        appendLog("ice candidate added");
      } catch (err) {
        appendLog("ice error: " + err.message);
      }
    });

    s.on("close", () => appendLog("signaling socket closed"));

    signalingRef.current = s;

    // Join the room (roomId === sessionId)
    s.connect({
      roomId: activeSessionId,
      peerId: peerIdRef.current,
    });
  };

  // -------------------------
  // Caller flow
  // -------------------------
  const initiateCall = async () => {
    const newSessionId = "session-" + Date.now();
    setSessionId(newSessionId);
    setStatus("calling");
    appendLog("call initiated");

    await startAudio();
    ensureSignaling(newSessionId);

    // Notify other peer there is a call incoming
    signalingRef.current.send({
      type: "call",
      sessionId: newSessionId,
    });

    // Create offer
    try {
      const pc = createPeerConnection(newSessionId);

      // Add local mic tracks
      audioStreamRef.current.getTracks().forEach((track) => pc.addTrack(track, audioStreamRef.current));

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

      appendLog("offer sent");
    } catch (err) {
      appendLog("caller offer error: " + err.message);
      cleanupAll();
    }
  };

  // -------------------------
  // Receiver flow
  // -------------------------
  const acceptIncoming = async () => {
    const sid = incomingSessionId;
    if (!sid) return;

    setSessionId(sid);
    setIncomingSessionId(null);
    setStatus("calling"); // will flip to connected once pc connects
    appendLog("accepting incoming call");

    await startAudio();
    ensureSignaling(sid);

    const offerMsg = pendingOfferRef.current;
    if (!offerMsg || !offerMsg.sdp) {
      appendLog("no pending offer to accept yet (wait for offer)");
      // Still allow acceptance; offer may arrive after. We'll handle when it does.
      return;
    }

    try {
      const pc = createPeerConnection(sid);

      // Add local mic tracks (so remote hears us)
      audioStreamRef.current.getTracks().forEach((track) => pc.addTrack(track, audioStreamRef.current));

      await pc.setRemoteDescription(offerMsg.sdp);
      appendLog("remote description set (offer)");

      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);

      signalingRef.current.send({
        type: "answer",
        sessionId: sid,
        sdp: pc.localDescription,
      });

      // Optional accept signal (for UI)
      signalingRef.current.send({
        type: "accept",
        sessionId: sid,
      });

      appendLog("answer sent");
      pendingOfferRef.current = null;
    } catch (err) {
      appendLog("accept error: " + err.message);
      cleanupAll();
    }
  };

  const rejectIncoming = () => {
    appendLog("incoming call rejected");
    setIncomingSessionId(null);
    setStatus("idle");
    pendingOfferRef.current = null;
  };

  // -------------------------
  // End call
  // -------------------------
  const endCall = () => {
    appendLog("ending call");

    if (signalingRef.current && sessionId) {
      signalingRef.current.send({
        type: "end",
        sessionId,
      });
    }

    cleanupAll();
  };

  const cleanupAll = () => {
    // WebRTC
    if (pcRef.current) {
      try {
        pcRef.current.onicecandidate = null;
        pcRef.current.ontrack = null;
        pcRef.current.close();
      } catch {}
      pcRef.current = null;
      appendLog("peer connection closed");
    }

    // Remote stream cleanup (new stream each time avoids lingering tracks)
    remoteStreamRef.current = new MediaStream();
    if (remoteAudioElRef.current) {
      try {
        remoteAudioElRef.current.srcObject = remoteStreamRef.current;
      } catch {}
    }

    // Signaling
    signalingRef.current?.disconnect();
    signalingRef.current = null;

    // Audio
    stopAudio();

    // UI
    setSessionId(null);
    setIncomingSessionId(null);
    pendingOfferRef.current = null;
    remotePeerIdRef.current = null;
    setStatus("idle");
    appendLog("cleanup complete");
  };

  // -------------------------
  // Cleanup on unmount
  // -------------------------
  useEffect(() => {
    return () => {
      try {
        signalingRef.current?.disconnect();
      } catch {}
      try {
        if (pcRef.current) pcRef.current.close();
      } catch {}
      stopAudio();
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
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
      <h1>APM — WebRTC Audio (Prototype → Functional)</h1>

      <p><strong>Status:</strong> {status}</p>
      <p><strong>Peer ID:</strong> {peerIdRef.current}</p>
      {sessionId && <p><strong>Session:</strong> {sessionId}</p>}

      <div style={{ marginTop: 12, display: "flex", gap: 10 }}>
        {status === "idle" && (
          <button onClick={initiateCall}>Call</button>
        )}

        {status === "ringing" && (
          <>
            <button onClick={acceptIncoming}>Accept</button>
            <button onClick={rejectIncoming}>Reject</button>
          </>
        )}

        {(status === "calling" || status === "connected") && (
          <button onClick={endCall}>End Call</button>
        )}
      </div>

      <div style={{ marginTop: 18 }}>
        <h3>Notes</h3>
        <ul>
          <li>Open this app in <strong>two tabs or two browsers</strong>.</li>
          <li>Click <strong>Call</strong> on one side, then <strong>Accept</strong> on the other.</li>
          <li>Mic access works on <strong>localhost</strong>. For remote hosting, you’ll need <strong>HTTPS</strong>.</li>
        </ul>
      </div>

      <div style={{ marginTop: 18 }}>
        <h3>System Log</h3>
        <pre
          style={{
            background: "#020617",
            padding: 12,
            maxHeight: 340,
            overflowY: "auto",
            fontSize: 12,
            borderRadius: 8,
          }}
        >
          {log.join("\n")}
        </pre>
      </div>
    </div>
  );
}
