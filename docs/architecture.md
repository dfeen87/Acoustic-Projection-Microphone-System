# Acoustic Projection Microphone (APM) — System Architecture

## Overview

The Acoustic Projection Microphone (APM) system is a production-oriented,
modular audio processing pipeline designed for real-time capture,
enhancement, translation, and directional projection of speech.

APM is intentionally structured as a **library-first system** with a
reference `main()` entrypoint provided for validation and demonstration.
Production deployments are expected to integrate APM into larger audio
applications that manage hardware I/O externally.

---

## High-Level Pipeline

# APM System Processing Pipeline

## Audio Processing Flow

```
┌─────────────────────────┐
│   Microphone Array      │
│  (4-channel input)      │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Beamforming Engine     │
│  • Delay-and-Sum        │
│  • Superdirective       │
│  • Adaptive Nulling     │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Echo Cancellation      │
│  • NLMS Adaptive Filter │
│  • Double-Talk Detection│
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│  Noise Suppression      │
│  • Deep Learning LSTM   │
│  • Spectral Masking     │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Voice Activity Detection│
│  • Energy Analysis      │
│  • Zero-Crossing Rate   │
│  • Hangover Mechanism   │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Speech Translation      │
│    (Async Process)      │
│  • ASR (Speech→Text)    │
│  • Neural Translation   │
│  • TTS (Text→Speech)    │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│ Directional Projection  │
│  • Phase Array Control  │
│  • Distance Attenuation │
│  • Beam Steering        │
└───────────┬─────────────┘
            │
            ▼
┌─────────────────────────┐
│     Speaker Array       │
│   (3-channel output)    │
└─────────────────────────┘
```

## Pipeline Stages

### 1. **Microphone Array**
- Multi-channel audio capture
- Spatially distributed sensors
- Typical: 4 microphones with 12mm spacing

### 2. **Beamforming Engine**
- Spatial filtering to focus on target direction
- Suppresses off-axis interference
- Uses fractional delay interpolation

### 3. **Echo Cancellation**
- Removes speaker feedback loop
- NLMS adaptive filtering
- Double-talk detection for robustness

### 4. **Noise Suppression**
- Deep learning-based denoising
- LSTM network predicts clean speech mask
- Preserves speech quality while removing noise

### 5. **Voice Activity Detection**
- Determines when speech is present
- Energy and zero-crossing rate analysis
- Hangover mechanism for smooth transitions

### 6. **Speech Translation** *(Async)*
- Real-time language translation
- Pipeline: ASR → Neural MT → TTS
- Runs asynchronously to maintain low latency

### 7. **Directional Audio Projection**
- Steers translated audio to target location
- Phase-array beam steering
- Distance-based attenuation

### 8. **Speaker Array**
- Multi-channel audio playback
- Spatially focused audio beam
- Typical: 3 speakers with 15mm spacing

---

## Performance Characteristics

| Stage | Latency | CPU Load |
|-------|---------|----------|
| Beamforming | <1ms | Low |
| Echo Cancellation | ~5ms | Medium |
| Noise Suppression | ~10ms | High |
| VAD | <1ms | Low |
| Translation | ~200ms | High* |
| Projection | <1ms | Low |

*Translation runs async and doesn't block the pipeline

## Safety Features

✅ Input validation at every stage  
✅ Graceful degradation on failures  
✅ Exception containment in async operations  
✅ Array size mismatch protection  
✅ Sample rate consistency checks  

---

## Use Cases

- **Real-time interpretation systems**
- **Smart conference rooms**
- **Accessibility devices**
- **Tour guide systems**
- **Multilingual customer service**


Each stage operates on validated audio frames and fails safely without
crashing the pipeline.

---

## Core Design Principles

### 1. Fail-Soft by Default
APM prioritizes **runtime stability** over aggressive recovery.
Invalid inputs or processing errors result in safe, empty outputs rather
than exceptions or undefined behavior.

### 2. Explicit Boundaries
- DSP stages validate inputs before processing
- Async execution boundaries contain exceptions
- No hidden global state

### 3. Deterministic Audio Flow
All audio processing is frame-based and deterministic given valid inputs.
No stage assumes ownership of I/O, clocks, or hardware buffers.

---

## Library vs Reference Execution

### Library Components
- `AudioFrame`
- Beamforming, Noise Suppression, Echo Cancellation
- Voice Activity Detection
- Translation interface
- Directional Projection
- `APMSystem` orchestration class

These are intended for **direct integration** into applications.

### Reference Entry Point
The provided `main()` function:
- Demonstrates correct usage
- Validates the full pipeline
- Does **not** represent a deployment pattern

Production systems should replace `main()` with platform-specific audio
backends (ALSA, CoreAudio, WASAPI, embedded DSP frameworks, etc.).

---

## Safety & Validation Strategy

| Layer | Protection |
|------|-----------|
| AudioFrame | Runtime validity checks |
| DSP Engines | Size and bounds validation |
| Async Tasks | Exception containment |
| Pipeline | Fail-soft empty returns |

No DSP stage assumes correctness of upstream components.

---

## Extensibility

APM is designed to be extended by:
- Replacing the translation engine implementation
- Adding alternative beamforming strategies
- Integrating hardware-specific audio I/O
- Introducing telemetry or health monitoring externally

Core DSP stages are intentionally conservative and self-contained.

---

## Non-Goals

APM does **not**:
- Manage audio drivers or devices
- Handle network transport
- Perform authentication or encryption
- Guarantee real-time deadlines under OS contention

These concerns are delegated to host applications.

---

## Summary

APM provides a hardened, modular audio intelligence pipeline that favors
clarity, safety, and composability. Its architecture is optimized for
integration into complex systems rather than monolithic execution.
