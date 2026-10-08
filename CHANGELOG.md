# Changelog

All notable changes to the Acoustic Projection Microphone System (APMS) project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [11.0.0] - 2026-10-07

### Changed
- Established the BEDROCK baseline while preserving the DSP, API and UI architecture.
- Native session getters return owned optional snapshots; configured API authentication
  requires the key header rather than credentials issued by loading a page.
- Reject invalid numerical/configuration/profile evidence, stale or terminal session
  transitions, unjoined signaling relays and malformed translation success results.
- Make initialization, telemetry replacement and SQLite identity/incoming registration
  preserve valid state at validation/failure boundaries; stop PTT on shutdown.
- Correct callback deadlock and worker shutdown race, aliased DSP output, absent-driver status, launcher routing,
  SDK installation, version metadata and failure propagation in tests/builds.
- Enforce native Debug/Release assertions, minimal builds, backend/WebSocket tests,
  launcher startup failure, SDK packaging and test-gated release/container workflows.
- Bedrock 1.1 adversarial verification closes proxy identity spoofing, contradictory
  JSON/Unicode evidence, exceptional startup cleanup, identity collision/rollback,
  terminal-call rejection and native delay/WAV intermediate-overflow gaps.
- Keep the existing DSP timing budgets in explicit serial Release checks; correct
  compiler/standard-library pairing, build-only test dependency packaging and recursive
  Docker secret exclusions. Validate API-container authorization and restart persistence.

### Added
- Behavioral regressions and current-version consistency checks.
- [Engineering report, compatibility changes and validation limits](docs/releases/v11.0.0-bedrock.md).
- [Pass B regressions, independent evidence and V11 release-readiness assessment](docs/releases/v11.0.0-bedrock-1.1-pass-b.md).

The major increment includes incompatible public state, validation and authentication
contracts under the project's stated Semantic Versioning policy. It does not certify
hardware or model performance; earlier release history remains below.

## [10.1.0] - 2026-09-27

### Changed
- Synchronized application, API, launcher, installer, user-facing, and citation
  metadata on version 10.1.0.

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
