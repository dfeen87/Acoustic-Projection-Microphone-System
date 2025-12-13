// frontend/adapters/apmBackendAdapter.js

export async function fetchPeers() {
  const res = await fetch("/api/peers");
  if (!res.ok) throw new Error("Failed to fetch peers");
  return res.json();
}

export async function createSession(peerId) {
  const res = await fetch(`/api/session?peer_id=${peerId}`, {
    method: "POST",
  });
  if (!res.ok) throw new Error("Failed to create session");
  return res.json();
}

export async function endSession(sessionId) {
  return fetch(`/api/session/${sessionId}/end`, { method: "POST" });
}
