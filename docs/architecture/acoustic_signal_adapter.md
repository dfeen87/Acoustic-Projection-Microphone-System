# APMS Native-VAD Metadata Adapter — Phase 2

**Status: opt-in interface only; live C++/Python binding not implemented.**

`backend/acoustic_signal_adapter.py` provides a small contract for supplying the
output of APMS `VoiceActivityDetector::detect()` to the existing continuous acoustic
context controller. The C++ `VadResult` contains `speech_detected`, `confidence`,
`snr_db`, and `energy_db`. This adapter accepts these four fields plus a
host-assigned, monotonic per-session timestamp.

## Why the activity flag matters

Native APMS VAD uses a short speech *hangover*: `speech_detected` may remain
`true` during a pause even as numeric confidence falls. The existing Python
controller used a score-based hysteresis threshold. It now also accepts an
**optional explicit speech flag** in `AcousticObservation`; when supplied,
the native flag wins. Old callers that send only scores keep their
original score-hysteresis behavior.

The adapter **does not fabricate a confidence score** to represent the native
speech flag. It preserves the original confidence and maps the flag separately.

## Explicit input contract

`AcousticSignalAdapter.ingest(vad, music)` requires:

- `APMSVadTelemetry(timestamp_ms, speech_detected, confidence, snr_db, energy_db)`
- `MusicAnalysisTelemetry(timestamp_ms, confidence, event=None)`

Both timestamps must match exactly. The caller must supply actual VAD output
and **independently measured music confidence** from an appropriate music
analysis path. Neither a native binding nor a music detector is included in
this phase. Reject mismatched timestamps instead of silently aligning or
inventing data. Context events are only caller-provided labels.

Both speech and music attention weights remain nonzero and represent
*analysis priority only*, not two live beams or sample-level mixing.
No audio is captured, recorded, transmitted, or processed by this adapter.

## Phase boundary

This increment intentionally **does not** alter `src/core/apm_system.cpp`,
`include/apm/apm_system.h`, any existing DSP, or AILEE Trust Layer.
It introduces no threads, network endpoints, background collectors, or
automatic runtime startup. Native/host telemetry bridging, source
synchronization, independent music detection, and real hardware tests
remain separately required before calling this a *live* adapter.

Run tests:

```bash
python -m unittest backend.test_acoustic_context backend.test_acoustic_signal_adapter -v
```

The `backend-ui` CI job also discovers `backend/test_*.py` via pytest.
