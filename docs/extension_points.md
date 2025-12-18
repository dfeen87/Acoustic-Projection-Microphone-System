# Extension Points

## Purpose

This document defines **where and how** the Acoustic Projection Microphone System (APM) may be extended without violating core guarantees.

APM is designed around explicit boundaries to preserve:

- **Signal integrity** (phase, timing, amplitude fidelity)
- **Deterministic behavior** in the DSP pipeline
- **Graceful failure** over crashes or undefined behavior
- **Operational realism** under imperfect conditions

Extensions are welcome, but they must remain **additive, optional, and non-invasive**.

---

## Core Rule

**Do not introduce blocking behavior or cross-layer shortcuts into the DSP pipeline.**

If an extension requires breaking this rule, it must be justified explicitly and treated as a deliberate architecture change (not an incremental feature).

---

## Supported Extension Surfaces

### 1. Observability and Diagnostics

**Intended for:**
- Health reporting
- Runtime counters and basic telemetry
- Developer tooling and integration tests
- Non-invasive metrics collection

**Where it belongs:**
- `include/apm/apm_observability.hpp`
- `src/tools/` (tooling-only implementations)
- `scripts/healthcheck.js` (operational checks)

**Rules:**
- Must not affect decisions or control logic
- Must not add heavy logging or sampling overhead by default
- Must not be required to run the system

---

### 2. Translation Backends

**Intended for:**
- Local translation (Whisper + NLLB)
- Optional alternative translation engines (e.g., TFLite-based)
- Translation orchestration that is explicitly **non-blocking**

**Where it belongs:**
- `src/translation/` and `include/apm/` translation headers
- `scripts/translation_bridge.py` (bridge + glue utilities)
- `requirements-translation.txt` (optional dependency set)
- `docs/translation/` (usage and installation)

**Rules:**
- Translation should be asynchronous and treated as best-effort output
- Failures must return safe outputs (e.g., empty string) without crashing
- No network I/O is assumed by default; external services must be documented clearly

---

### 3. Signaling / Peer Integration

**Intended for:**
- WebSocket signaling adapters
- Peer discovery and session orchestration
- Interop with external systems via a clearly defined contract

**Where it belongs:**
- `backend/signaling/` (backend boundary)
- `frontend/adapters/` (frontend boundary)
- `docs/signaling/` (contract + behavior expectations)

**Rules:**
- Network behavior must remain isolated from core DSP execution
- Signaling failures must degrade gracefully
- Protocol changes should be reflected in contract docs and integration tests

---

### 4. Examples and Demonstrations

**Intended for:**
- Showing end-to-end usage
- Providing reference flows without contaminating production entrypoints
- Demonstrating optional features (translation, encryption, signaling)

**Where it belongs:**
- `examples/`
- `src/examples/` (if used for buildable demos)

**Rules:**
- Examples must not become required dependencies
- Examples should remain small and focused
- Avoid “magic defaults” that contradict operational guidance

---

### 5. Developer Tools

**Intended for:**
- Local debugging
- Validation utilities
- Lightweight consoles and test harnesses

**Where it belongs:**
- `src/tools/`
- `scripts/`
- `tests/`

**Rules:**
- Tools must not be linked into production targets by default
- Tools should be safe to run against imperfect environments
- Tools should prefer clarity over cleverness

---

## Explicitly Unsupported Extension Patterns

The following changes are considered **architectural violations** unless explicitly approved as a deliberate redesign:

- Injecting blocking work into the DSP loop (including translation)
- Modifying core timing assumptions without documentation and tests
- Cross-layer shortcuts (e.g., UI directly controlling DSP internals)
- Introducing hidden global state that affects signal behavior
- Adding external network dependencies as “silent defaults”
- Optimizing prematurely in ways that reduce interpretability or correctness

If you believe a change must do one of the above, open an issue or design note first and justify the trade-off.

---

## Review Expectations

For extensions and integration changes, maintainers will look for:

- Clear placement at a defined boundary
- No impact on signal integrity or deterministic DSP execution
- Explicit failure behavior (no silent corruption)
- Tests that validate boundaries (including negative/failure cases)
- Documentation updates where contracts or operator expectations change

---

## Status

This document reflects the current stable direction of the project as of **v2.3.0** and is expected to evolve conservatively.
