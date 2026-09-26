# Changelog

All notable changes to the Acoustic Projection Microphone System (APMS) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [10.0.0] - V10 Release (2026-04-18)

### Added
- **Smartglasses Acoustic-Projection-Microphone-System (A-P-M-S)**:
  - 2-microphone temple topology (14–16 cm baseline) with geometric acoustic projection onto the virtual mouth axis.
  - Phase coherence index and phase-aware spatial masking for optimal off-axis noise suppression.
  - Ultra-low-power recursive spectral IIR gain smoothing for SoC/wearable constraints (< 15–20 mW equivalent complexity).
  - Native 16 kHz sample rate, 10 ms frame (160 samples per channel) pipeline delivering sub-10 ms end-to-end processing latency.
- **AI Assistant Front-End Integration Interface**:
  - Standardized 16 kHz mono PCM 10 ms output interface contract for downstream wake-word (Sensory, Porcupine/PvRecorder) and streaming ASR (Whisper, Kaldi, ONNX Runtime) engines.
  - Integrated VAD and SNR metadata frame output (`speech_confidence`, `estimated_snr_db`, `is_speech_active`).
- **C++ DSP & Test Suite Additions**:
  - Modularized C++ DSP pipeline implementation under `src/dsp/smartglasses_apm/smartglasses_apm.cpp`.
  - Added mathematical correctness unit tests (`tests/test_smartglasses_apm_projection.cpp`) for synthetic mouth speech vs noise scenarios, phase handling, and spatial weighting.
  - Added DSP pipeline performance, latency, and numerical robustness tests (`tests/test_smartglasses_apm_dsp_pipeline.cpp`) covering silent inputs, high noise, transients, NaNs/Infs handling, and cycle/MAC estimation.
- **Architecture Documentation**:
  - Detailed A-P-M-S pseudocode reference specification (`docs/architecture/smartglasses_apm_pseudocode.md`).
  - Comprehensive spec updates and V10 summary in `docs/architecture/smartglasses_apm_spec.md`.

## [7.0.0] - 2025-12-01

### Added
- Auto-calibration mode for initial setup.
- Real-time monitoring dashboard and telemetry bridge.
- Adaptive feedback suppression.
- Local Whisper + NLLB speech translation bridge.
- ChaCha20-Poly1305 + X25519 encryption support.
