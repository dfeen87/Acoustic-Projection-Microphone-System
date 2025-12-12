# APM Push-to-Talk (PTT) & Call Signaling System

## Overview

APM now includes a fully integrated Push‑to‑Talk controller and UDP‑based Call Signaling subsystem designed for real‑world communication workflows. These features enable intentional audio capture, walkie‑talkie style operation, incoming call notifications, session management, and network‑ready communication.

This document provides a complete overview of the architecture, features, usage, and integration details.

---

# 🎙️ Push-to-Talk (PTT) Controller

The PTT Controller ensures that audio is only captured when the user intentionally transmits. This eliminates background noise, accidental recordings, and environmental interference.

## Features
- **Software-controlled PTT**
  - Press, release, and toggle modes
- **Debouncing**
  - Ignores accidental micro-presses
- **Cooldown period**
  - Prevents rapid re-triggering
- **Audio buffering**
  - Captures speech only during transmission
- **State callbacks**
  - `IDLE → TRANSMITTING → COOLDOWN`
- **Beep feedback**
  - Optional press/release tones
- **Statistics tracking**
  - Transmission count, duration, sample totals

## Example Usage

```cpp
#include "apm/ptt_controller.hpp"
#include "apm/apm_core.hpp"

apm::PTTController ptt;
apm::APMCore translator;

ptt.initialize();
translator.initialize();

ptt.on_audio_available([&](const std::vector<float>& audio) {
    auto result = translator.translate_audio(audio);
    std::cout << "Translation: " << result.translated_text << "\n";
});

ptt.press();
// user speaks...
ptt.release();
```

---

# 📞 Call Signaling System

A lightweight UDP-based signaling protocol enabling call setup, notifications, session management, and peer discovery.

## Features
- **Call initiation**
  - `CALL_REQUEST` packets with metadata
- **Call acceptance / rejection**
- **Call termination**
- **Session IDs**
  - Unique per-call identifiers
- **Heartbeat keep-alive**
- **Ring tone notifications**
- **Peer discovery**
  - Local network broadcast
- **Multi-party support ready**
- **Encryption key exchange**
  - Integrates with APM's crypto module

## Call State Machine

```
IDLE → CALLING → RINGING → CONNECTED → ENDED
         ↓           ↓
      TIMEOUT    REJECTED
```

## Example Usage

```cpp
#include "apm/call_signaling.hpp"

apm::signaling::CallSignaling signaling;

signaling.initialize(my_info, 5060);

signaling.on_incoming_call([&](const CallSession& session) {
    std::cout << "Incoming call from: " << session.caller.display_name << "\n";
    signaling.accept_call(session.session_id);
});

signaling.initiate_call(remote_person);
```

---

# 🔄 Full Workflow Integration

```
PTT Press → Audio Capture → Whisper STT → NLLB Translation → Encryption → Call Signaling → Recipient
```

- PTT ensures intentional audio capture  
- Translation pipeline processes speech  
- Encryption secures the message  
- Call Signaling delivers it to the recipient  
- Recipient receives ring tone + notification  

---

# 📦 Files Included

### Headers
- `include/apm/ptt_controller.hpp`
- `include/apm/call_signaling.hpp`

### Implementations
- `src/ptt_controller.cpp`
- `src/call_signaling.cpp`

### Example
- `examples/ptt_call_example.cpp`

**Total:** ~1,650 lines of production code

# ✅ Summary

The PTT + Call Signaling subsystem transforms APM into a fully operational communication platform:

- Intentional audio capture  
- Walkie‑talkie style operation  
- Incoming call notifications  
- Session management  
- Network-ready signaling  
- Encryption integration  
- Real-world deployment capability  

This subsystem is now fully integrated into APM and ready for production use.
