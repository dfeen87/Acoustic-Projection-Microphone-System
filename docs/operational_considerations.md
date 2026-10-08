# Operational Considerations

This document outlines practical, real-world considerations for running,
testing, and extending the **Acoustic Projection Microphone System (APM)**
in non-ideal environments.

The intent is to surface operational assumptions explicitly, document
failure behavior, and encourage disciplined, production-minded deployment
practices.

APM is designed to behave predictably under imperfect conditions rather
than optimizing for laboratory idealism.

---

## Assumptions (and Their Limits)

### The system assumes:
- Imperfect and drifting sensors
- Clock drift between acquisition, processing, and output stages
- Variable latency across execution environments
- Environmental, acoustic, and electromagnetic noise
- Shared system resources and scheduling jitter

### The system does **not** assume:
- Laboratory conditions
- Perfect synchronization
- Unlimited compute or memory
- Ideal or stationary signal-to-noise ratios
- Continuous availability of all inputs

Core design choices explicitly reflect these constraints.

---

## Audio Frame Expectations

Recommended baseline parameters:
- Sample Rate: **48 kHz**
- Frame Size: **~20 ms (960 samples @ 48 kHz)**
- Channels: **1 per microphone**

Other configurations are possible, but deviations should be validated
carefully under load and noise.

Frame-based processing is assumed throughout the pipeline.

---

## Performance Characteristics

Key factors influencing performance include:
- Input sample rate and buffer sizing
- Processing window duration
- Hardware clock stability
- System load and scheduling jitter
- Translation backend latency (dominant contributor)

Approximate behavior by stage:

| Stage | Latency | CPU Load |
|------|---------|----------|
| Beamforming | <1 ms | Low |
| Echo Cancellation | ~5 ms | Medium |
| Noise Suppression | ~10 ms | High |
| Voice Activity Detection | <1 ms | Low |
| Translation | ~200 ms | High* |
| Projection | <1 ms | Low |

\* Translation is submitted asynchronously, but `APMSystem::process()` waits
for its future. The complete pipeline therefore has no hard real-time or
sub-10 ms guarantee. The separate smartglasses DSP measurements concern that
engine alone; measure capture, inference and output together on target hardware.

Performance tuning should be:
- Incremental
- Measured with real signals
- Validated under stress, not just nominal conditions

Aggressive optimization without measurement is discouraged.

---

## Threading & Concurrency

- DSP stages execute synchronously within a processing task
- Translation is executed asynchronously via `std::future`
- Exceptions inside async tasks are explicitly contained
- Failures return safe, empty outputs rather than crashing the pipeline

APM does **not** manage thread pools or scheduling policies.

---

## Failure Modes

Expected failure scenarios include:
- Input signal dropout or saturation
- Clock desynchronization between acquisition and processing
- Numerical instability under extreme noise
- Resource contention on shared systems
- Translation backend timeouts or errors

The system is designed to:
- Fail **softly** rather than crash
- Degrade gracefully when possible
- Preserve diagnostic context
- Avoid undefined behavior

Silent data corruption is treated as a critical fault.

---

## Reset & State Management

APM maintains internal state for:
- Echo cancellation
- Noise suppression
- Voice activity detection

Use `reset_all()`:
- After stream discontinuities
- When switching input devices
- Following prolonged silence
- After detected fault conditions

Resetting state is inexpensive and preferred over attempting recovery
from unknown conditions.

---

## Monitoring & Diagnostics

Recommended operational practices:
- Log signal quality and frame validity metrics
- Track timing drift and buffer underruns
- Monitor CPU and memory utilization under load
- Observe translation latency separately from DSP latency

Operational visibility is preferred over opaque performance gains.

---

## Deployment Notes

When deploying in production-like environments:
- Validate assumptions using real hardware
- Start with conservative configuration parameters
- Version configuration alongside code
- Avoid environment-specific behavior baked into logic
- Prefer explicit configuration over implicit defaults

Deployment correctness is more important than peak throughput.

---

## Security Considerations

- The native DSP library does not start networking merely by construction;
  the API, telemetry and signaling services expose separate network boundaries.
- Local translation writes a temporary WAV and invokes a Python subprocess.
- Translation backends may introduce external dependencies.

Security-sensitive deployments should audit translation implementations
and isolate external services appropriately.

## Version 11 operational contracts

The full native pipeline accepts matching, mono microphone/reference frames at
the configured sample rate, with finite samples in [-1, 1] and finite direction.
Invalid input returns no output before DSP or monitoring state changes. Valid
processing and `reset_all()` are serialized; externally exposed calibration and
profile objects still require caller coordination. This is not a transaction
around arbitrary exceptions in every DSP stage.

With `APM_API_KEY` configured, protected API requests require
`X-APM-API-Key`; loading the dashboard cannot issue authorization. With
`SIGNALING_WS_TOKEN` configured, WebSocket joins require that token. Neither
setting supplies individual user identity or per-room authorization. Configure
the ASGI server's trusted proxy addresses deliberately: application code uses
its resolved client address, not raw forwarded headers.

Missing PortAudio cannot report active capture. Malformed telemetry preserves
the last valid record; disconnected/stale readings identify fallback status.
Calibration cannot become valid without accepted audio observations, but its
latency estimate remains a software estimate, not a hardware measurement.

See the [11.0.0 BEDROCK report](releases/v11.0.0-bedrock.md) for the invariant
map, behavioral compatibility changes and remaining validation requirements.

---

## Testing Philosophy

Testing should extend beyond ideal inputs and include:
- Deterministic unit tests
- Noise-injected and adversarial signal cases
- Boundary condition stress testing
- Long-duration stability runs
- Resource contention scenarios

Synthetic perfection is not a substitute for operational realism.

---

## Future Hardening Areas

Potential future improvements include:
- Automated health checks and self-tests
- Adaptive parameter bounding
- Runtime anomaly detection
- Improved observability hooks

These are intentionally deferred to avoid premature complexity.

---

## Status

This document reflects current operational understanding as of **v11.0.0**.

Operational guidance will evolve conservatively as real deployment
experience accumulates.
