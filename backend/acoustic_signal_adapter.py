# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Optional boundary from APMS VAD/music metadata into continuous context.

The native C++ APMSystem pipeline is not linked here. A host adapter must
supply genuine native VAD results and independently measured music features.
No hardware I/O, DSP, threads, network access, or audio retention is added.
"""

from dataclasses import dataclass
import math
from typing import Optional

from backend.acoustic_context import (
    AcousticObservation, ContinuousAcousticContext, ContextDecision,
)


@dataclass(frozen=True)
class APMSVadTelemetry:
    """One result from apm::VoiceActivityDetector::VadResult, timestamped by host."""

    timestamp_ms: int
    speech_detected: bool
    confidence: float
    snr_db: float
    energy_db: float


@dataclass(frozen=True)
class MusicAnalysisTelemetry:
    """Independently observed music evidence; not inferred from speech VAD."""

    timestamp_ms: int
    confidence: float
    event: Optional[str] = None


class AcousticSignalAdapter:
    """Maps aligned, caller-supplied observations into one context session.

    This adapter neither generates native VAD telemetry nor classifies music.
    The caller must manage the microphone/audio source, synchronization, and
    session lifetime. It is deliberately opt-in and never runs by default.
    """

    def __init__(self, context: Optional[ContinuousAcousticContext] = None):
        self.context = context if context is not None else ContinuousAcousticContext()

    def ingest(self, vad: APMSVadTelemetry, music: MusicAnalysisTelemetry) -> ContextDecision:
        if not isinstance(vad, APMSVadTelemetry) or not isinstance(music, MusicAnalysisTelemetry):
            raise TypeError("expected APMSVadTelemetry and MusicAnalysisTelemetry")
        if isinstance(vad.timestamp_ms, bool) or not isinstance(vad.timestamp_ms, int) or vad.timestamp_ms < 0:
            raise ValueError("VAD timestamp must be a nonnegative integer")
        if isinstance(music.timestamp_ms, bool) or not isinstance(music.timestamp_ms, int) or music.timestamp_ms != vad.timestamp_ms:
            raise ValueError("music and VAD observations must have matching timestamps")
        if not isinstance(vad.speech_detected, bool):
            raise ValueError("native speech_detected must be a boolean")
        for name, value in (("VAD confidence", vad.confidence),
                            ("VAD SNR", vad.snr_db),
                            ("VAD energy", vad.energy_db),
                            ("music confidence", music.confidence)):
            if isinstance(value, bool) or not isinstance(value, (float, int)) or not math.isfinite(value):
                raise ValueError(f"{name} must be finite and numeric")
        if not 0.0 <= vad.confidence <= 1.0:
            raise ValueError("VAD confidence must be in [0, 1]")
        if not 0.0 <= music.confidence <= 1.0:
            raise ValueError("music confidence must be in [0, 1]")
        return self.context.observe(AcousticObservation(
            timestamp_ms=vad.timestamp_ms,
            speech_confidence=vad.confidence,
            music_confidence=music.confidence,
            music_event=music.event,
            speech_detected=vad.speech_detected,
        ))

    def reset(self) -> None:
        self.context.reset()
