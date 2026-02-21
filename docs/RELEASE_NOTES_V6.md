# **APMS v6.0.0 — Production Wiring: Real Audio I/O & Local Translation Integration**

> **Release Date:** 2026-02-21  
> **Tag:** `v6.0.0`  
> **Based on:** v5.0.0 (REST API with Global Node Access)

---

## **Overview**

Version 6.0.0 transitions the Acoustic Projection Microphone System (APMS) from a network-accessible simulation prototype into a **production-ready application** capable of processing real audio hardware in real time. This release wires together the full signal chain — from physical microphone capture through DSP processing to speaker output — while simultaneously integrating the Whisper + NLLB local translation engine end-to-end.

The REST API service layer introduced in v5 is unchanged; v6 brings the underlying C++ engine up to the same production standard.

---

## **Key Changes**

### **1. Real-Time Audio I/O via PortAudio (`src/io/audio_device.cpp`)**

A new `AudioDevice` class wraps PortAudio with a clean, RAII-safe C++ interface:

- **Pimpl idiom** — hides PortAudio headers from consumers; graceful stub fallback when PortAudio is absent at compile time.
- **Float32 interleaved streams** — matches the internal `AudioFrame` format used throughout the DSP pipeline.
- **Configurable channel counts** — defaults to 4-channel microphone input and 2-channel stereo output at 48 kHz / 512-frame blocks.
- **Selectable devices** — `input_device_index` / `output_device_index` fields (−1 = system default).
- **Thread-safe callback** — the PortAudio callback acquires a try-lock; if the processing thread is busy the callback silences output instead of blocking.
- **`AudioDevice::list_devices()`** — static helper that enumerates every device PortAudio can see at runtime.

Header: `include/apm/io/audio_device.h`

### **2. Local Translation Adapter (`src/translation/local_translation_adapter.cpp`)**

A new `LocalTranslationAdapter` implements the `TranslationEngine` interface and bridges the on-device Whisper + NLLB engine (`LocalTranslationEngine`) to `APMSystem`:

- **Multi-channel mixdown** — automatically down-mixes any multi-channel `AudioFrame` to mono before passing audio to the transcription engine.
- **Async-safe future chaining** — wraps `LocalTranslationEngine::translate_async()` in a second `std::async` so the adapter never blocks the DSP thread.
- **Unified result mapping** — maps `LocalTranslationEngine::Result` fields (`transcribed_text`, `translated_text`, `confidence`) to the standard `TranslationEngine::TranslationResult` type.

Header: `include/apm/translation/local_translation_adapter.h`

### **3. `APMSystem` Translation Integration (`src/core/apm_system.cpp`)**

`APMSystem` now dynamically selects a translation backend at construction time:

- When the CMake option `ENABLE_LOCAL_TRANSLATION` is `ON` (the new default), `APMSystem` instantiates a `LocalTranslationAdapter`.
- The existing TFLite and no-op paths remain fully available through the same config flag.

### **4. Multi-Threaded Real-Time Main Loop (`src/core/main.cpp`)**

The previous placeholder `main.cpp` has been completely rewritten with a production-grade two-thread architecture:

| Thread | Responsibility |
|---|---|
| **PortAudio callback** | Captures mic samples → pushes to `input_queue`; pops from `output_queue` → writes to speaker |
| **Processing thread** | Pops from `input_queue`, deinterleaves to `AudioFrame` array, calls `APMSystem::process()`, interleaves output, pushes to `output_queue` |

Additional details:
- **`ThreadSafeQueue<T>`** — lock + condition-variable queue with configurable maximum depth; oldest frame is dropped on overflow to prevent unbounded latency growth.
- **Pre-filled silence** — output queue is seeded with four silent frames at startup to absorb the initial pipeline latency.
- **Graceful shutdown** — `SIGINT` / `SIGTERM` handlers set a global `std::atomic<bool>`, allowing both the PortAudio stream and the processing thread to drain cleanly.
- **Exception isolation** — the processing loop wraps `APMSystem::process()` in a `try/catch` so a single bad frame never crashes the daemon.

### **5. Build System Updates (`CMakeLists.txt`)**

- **PortAudio detection** — tries `pkg-config portaudio-2.0` first, falls back to `find_library` / `find_path`; sets `HAVE_PORTAUDIO` config flag used by conditional compilation.
- **`ENABLE_LOCAL_TRANSLATION`** option now defaults to `ON`.
- New source files added to the build: `src/io/audio_device.cpp`, `src/translation/local_translation_adapter.cpp`.
- `config.h.in` updated with `HAVE_PORTAUDIO` and related fields.

### **6. README Overhaul (`README.md`)**

A full audit corrected every stale reference accumulated since v1:

| Area | Before | After |
|---|---|---|
| Version | `1.0.0` | `2.0.0` (matches `CMakeLists.txt`) |
| CMake minimum | `3.15+` | `3.18+` |
| Start scripts | repo-root `./start-apm.sh` | `./scripts/start-apm.sh` |
| Translation setup | `scripts/setup_translation.sh` | `scripts/setup.sh --full` |
| Quickstart guide | `TRANSLATION_QUICKSTART.md` | `docs/translation/` |
| Test binary | `apm_tests` | `apm_test` |
| Project tree | outdated | reflects `src/io/`, `src/translation/`, `src/core/main.cpp` entry point, `ui/apm-dashboard.html` |

**Features newly documented** (present in code, missing from README until now):
- End-to-end encryption — ChaCha20-Poly1305 + X25519 via libsodium
- Push-to-Talk controller — keyboard / mouse / external / software modes
- UDP call signaling with peer discovery and session management
- PortAudio real hardware audio I/O (this release)
- WAV file I/O
- `APMCore` public facade — fully documented in the API reference section
- `APMSystem::Config` default values, `reset_all()`, default constructor
- `EchoCancellationEngine::reset()` and `VoiceActivityDetector::reset()`
- `FFTProcessor` FFTW3 requirement, `WindowType` enum values

**Translation language scope clarified:**
- C++ text fallback: **English → Spanish and English → French only**
- 200+ language pairs: Python bridge (`scripts/translation_bridge.py`) required

Prerequisites section extended with `libsodium` and `PortAudio` install commands for Ubuntu and macOS.

---

## **Files Added**

| File | Description |
|---|---|
| `include/apm/io/audio_device.h` | `AudioDevice` class declaration |
| `include/apm/translation/local_translation_adapter.h` | `LocalTranslationAdapter` class declaration |
| `src/io/audio_device.cpp` | PortAudio I/O implementation (with stub fallback) |
| `src/translation/local_translation_adapter.cpp` | Translation adapter bridging `LocalTranslationEngine` ↔ `APMSystem` |

## **Files Modified**

| File | Summary of Changes |
|---|---|
| `CMakeLists.txt` | PortAudio detection, new source files, `ENABLE_LOCAL_TRANSLATION=ON` default |
| `include/apm/config.h.in` | Added `HAVE_PORTAUDIO` and related config fields |
| `src/core/apm_system.cpp` | Dynamic translation backend selection |
| `src/core/main.cpp` | Full rewrite — multi-threaded real-time loop with `ThreadSafeQueue` |
| `README.md` | Comprehensive accuracy overhaul; newly documented features and corrected paths |

---

## **Prerequisites**

### New optional dependency: PortAudio

```bash
# Ubuntu / Debian
sudo apt-get install libportaudio2 portaudio19-dev

# macOS
brew install portaudio
```

> Without PortAudio the build succeeds but the `AudioDevice` class silently no-ops. All existing WAV-file and REST-API workflows are unaffected.

### Existing optional dependency reminder: libsodium

```bash
# Ubuntu / Debian
sudo apt-get install libsodium-dev

# macOS
brew install libsodium
```

---

## **Building**

```bash
mkdir build && cd build
cmake .. -DENABLE_LOCAL_TRANSLATION=ON
cmake --build . --parallel
```

### Run the real-time audio daemon

```bash
./build/apm_main
```

The binary enumerates available audio devices on startup and begins processing with the system default microphone and speaker. Interrupt with `Ctrl+C` for a clean shutdown.

---

## **Summary**

APMS v6.0.0 delivers the final production wiring that connects real hardware to the full DSP and translation stack. The core acoustic engine, REST API, encryption, PTT controller, and call-signaling subsystems remain stable; this release adds real-time PortAudio audio capture/playback, the `LocalTranslationAdapter` integration layer, a multi-threaded main loop with thread-safe queueing, and a comprehensively corrected README that accurately documents all shipped features.
