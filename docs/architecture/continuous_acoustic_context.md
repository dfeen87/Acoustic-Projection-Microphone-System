# APMS Continuous Acoustic Context — initial metadata-only prototype

**Status: experimental, opt-in, no runtime integration.** This work is not a live music-recognition system or a ChatGPT audio integration.

## North star

**Music context persists; human speech takes temporary attention priority.** The music signal need not be acoustically constant. Continuity refers to the time-aligned *analysis context*, even while the conversation follows speech.

## Architecture boundary

This prototype is in `backend/acoustic_context.py`, the Python orchestration layer, **outside** the C++ real-time DSP loop. It borrows the *concepts* of confidence checks, hysteresis, and stable transitions from AILEE Trust Layer but does **not** import AILEE or modify its repository. `include/apm/`, core beamforming, echo cancellation, translation, and audio capture are untouched.

The input is a sequence of validated, increasing-timestamp `AcousticObservation` records:

- `speech_confidence` from a speech activity classifier, normalized to `[0, 1]`.
- `music_confidence` from an independently validated music classifier, normalized to `[0, 1]`.
- Optional `music_event`: a **caller-provided descriptive tag**, not a model-generated fact, fingerprint, song ID, or raw audio buffer.

The output is a `ContextDecision` with `attention` (`speech`, `music`, `neutral`), speech/music presence status, nonzero relative **analysis attention weights**, and bounded recent music event metadata. Weights are not volume controls, beamforming coefficients, actual stream mixing, or instructions to cease processing either stream. Each session owns its own controller; reset clears all metadata.

Speech activity has enter/exit hysteresis. Weights approach their targets smoothly according to *elapsed timestamps*, rather than depending on an arbitrary fixed frame rate. The history is bounded by age and item count. Invalid, stale, or out-of-order input is rejected before changing state. Classifier uncertainty is not conflated with acoustic ground truth.

## Example (simulated upstream signals)

```python
from backend.acoustic_context import AcousticObservation, ContinuousAcousticContext

context = ContinuousAcousticContext()
context.observe(AcousticObservation(0, 0.10, 0.90, "instrumental entrance"))
when_talking = context.observe(AcousticObservation(100, 0.92, 0.93, "drum fill"))
assert when_talking.attention.value == "speech"
assert [event.label for event in when_talking.recent_music_events] == [
    "instrumental entrance", "drum fill"
]
```

## Integration and limitations

1. **Not yet connected** to real APMS VAD, beamforming, microphones, captured Spotify music, source separation, or plugin transports. No claim of working real-time inference is made.
2. For real deployment, the APMS adapter must feed *independently observed* music confidence and event metadata, while the original full-band/reference signal stays available for music-specific analysis. The speech-enhanced 16 kHz smartglasses path is not a sufficient high-fidelity music reference by itself.
3. Two distinct spatial beams depend on geometry and actual multi-microphone channels. A single microphone or a shared mono mix cannot provide spatial separation merely by selecting two software streams. Full-band music reference or source separation may be required.
4. The prototype **does not capture, record, or persist audio**, perform network calls, infer identity, or publish events. Future capture needs explicit opt-in, per-session isolation, and retention controls.
5. Before enabling a live adapter, measure speech and music confidence calibration, false switching, end-to-end latency, alignment between streams, and music context stability under overlap, silence, abrupt changes, jitter, and dropped frames. Keep all experiments behind an explicit feature flag and maintain the existing DSP timing guarantees.

## Validation

```bash
python -m unittest backend.test_acoustic_context -v
```

Existing CI and end-to-end audio hardware checks remain separate. Passing these unit tests proves only the deterministic metadata controller, **not** music recognition, acoustic separation, or live microphone functionality.
