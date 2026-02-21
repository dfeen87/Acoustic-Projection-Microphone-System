# Introducing the Acoustic Projection Microphone (APM) System

**For Organizations Working at the Frontlines of Crisis Response**

---

We are proud to introduce the **Acoustic Projection Microphone (APM) System** — a production-ready, open-source platform built for real-world humanitarian operations.

### What It Does

APM is an advanced audio intelligence system that:

- 🎙️ **Captures clear speech in noisy environments** using a multi-microphone array with beamforming, noise suppression, and echo cancellation — so every word is heard, even in a crowded field station or disaster zone.
- 🌐 **Translates speech in real time across 200+ languages**, powered by OpenAI Whisper and Meta's NLLB model — completely on-device. Models are downloaded once during setup; after that the system runs entirely offline with no internet connection required.
- 📡 **Projects audio directionally** to targeted recipients, reducing cross-talk in multi-party coordination settings.
- 🔒 **Works 100% offline and privately** — conversations never leave the device.

### Why It Matters for the Red Cross and the UN

When seconds count and language barriers can cost lives, APM provides:

- **Instant multilingual communication** between responders, medical staff, and affected communities — no interpreter required.
- **Reliable operation in low-connectivity environments** like disaster zones, remote field hospitals, and refugee settlements.
- **Encrypted, on-device processing** that protects the privacy of vulnerable populations.
- **Low hardware requirements** — minimum 2-core CPU and 512 MB RAM; a standard field laptop is sufficient. A GPU is optional but reduces translation latency from ~5–8 s to ~2–4 s per sentence.

### Ready to Deploy

APM is production-ready:
- ✅ Full DSP pipeline validated (beamforming → noise suppression → echo cancellation → VAD → translation)
- ✅ REST API for multi-node coordination across field teams
- ✅ Docker support for rapid, reproducible deployment
- ✅ Cross-platform (Linux, macOS, Windows)
- ✅ One-command launch: `./start-apm.sh`

### Get Started

```bash
git clone https://github.com/dfeen87/Acoustic-Projection-Microphone-System
cd Acoustic-Projection-Microphone-System
./start-apm.sh
```

Full documentation: [README.md](README.md)
Translation setup: [TRANSLATION_QUICKSTART.md](TRANSLATION_QUICKSTART.md)

---

*Built by Don Michael Feeney Jr. | MIT License | Dedicated to Marcel Krüger*
