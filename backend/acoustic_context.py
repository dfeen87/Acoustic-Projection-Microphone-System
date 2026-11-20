# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Optional, metadata-only continuity and attention controller for APMS.

No microphone capture, beamforming, DSP, network I/O, or persistent storage occurs
here. Host applications supply timestamped classifier scores and optional music
annotations. AILEE-like confidence, hysteresis, and continuity are design ideas;
this module does not import or modify AILEE Trust Layer.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum
import math
from typing import Deque, Optional, Tuple


class Attention(str, Enum):
    SPEECH = "speech"
    MUSIC = "music"
    NEUTRAL = "neutral"


@dataclass(frozen=True)
class ContextConfig:
    speech_on: float = 0.70
    speech_off: float = 0.40
    music_on: float = 0.55
    transition_time_ms: float = 250.0
    minimum_stream_weight: float = 0.15
    history_window_ms: int = 15000
    max_music_events: int = 64

    def __post_init__(self) -> None:
        if not (0.0 <= self.speech_off < self.speech_on <= 1.0):
            raise ValueError("speech thresholds must satisfy 0 <= off < on <= 1")
        if not math.isfinite(self.music_on) or not 0.0 <= self.music_on <= 1.0:
            raise ValueError("music_on must be between zero and one")
        if not math.isfinite(self.transition_time_ms) or self.transition_time_ms <= 0.0:
            raise ValueError("transition_time_ms must be finite and positive")
        if not math.isfinite(self.minimum_stream_weight) or not 0.0 < self.minimum_stream_weight < 0.5:
            raise ValueError("minimum_stream_weight must be between zero and one half")
        if isinstance(self.history_window_ms, bool) or not isinstance(self.history_window_ms, int) or self.history_window_ms <= 0:
            raise ValueError("history_window_ms must be a positive integer")
        if isinstance(self.max_music_events, bool) or not isinstance(self.max_music_events, int) or self.max_music_events <= 0:
            raise ValueError("max_music_events must be a positive integer")


@dataclass(frozen=True)
class AcousticObservation:
    timestamp_ms: int
    speech_confidence: float
    music_confidence: float
    music_event: Optional[str] = None


@dataclass(frozen=True)
class MusicContextEvent:
    timestamp_ms: int
    label: str


@dataclass(frozen=True)
class ContextDecision:
    timestamp_ms: int
    attention: Attention
    speech_active: bool
    music_detected: bool
    speech_weight: float
    music_weight: float
    recent_music_events: Tuple[MusicContextEvent, ...]
    attention_changed: bool


class ContinuousAcousticContext:
    """Bounded, deterministic controller for an external dual-stream audio adapter.

    Weights are *relative analysis attention*, not audio gains or DSP commands.
    Both streams remain the caller's responsibility. No audio is ever stored.
    The object is stateful and must be owned by one session; do not share it
    across sessions or call it concurrently without external synchronization.
    """

    def __init__(self, config: Optional[ContextConfig] = None) -> None:
        self.config = config or ContextConfig()
        self.reset()

    def reset(self) -> None:
        """Erase all session metadata, including the bounded music history."""
        self._last_ms: Optional[int] = None
        self._speech_active = False
        self._speech_weight = 0.5
        self._attention = Attention.NEUTRAL
        self._music_events: Deque[MusicContextEvent] = deque(maxlen=self.config.max_music_events)

    def observe(self, observation: AcousticObservation) -> ContextDecision:
        """Advance one timestamped observation and return a snapshot.

        Validation is performed before modifying session state. Input scores
        must originate from an external acoustic classifier or test fixture.
        """
        stamp = observation.timestamp_ms
        if isinstance(stamp, bool) or not isinstance(stamp, int) or stamp < 0:
            raise ValueError("timestamp_ms must be a nonnegative integer")
        if self._last_ms is not None and stamp <= self._last_ms:
            raise ValueError("timestamps must increase strictly within a session")
        for name, score in (("speech_confidence", observation.speech_confidence),
                            ("music_confidence", observation.music_confidence)):
            if isinstance(score, bool) or not isinstance(score, (float, int)) or not math.isfinite(score) or not 0.0 <= score <= 1.0:
                raise ValueError(f"{name} must be a finite score in [0, 1]")
        label = observation.music_event
        if label is not None and (not isinstance(label, str) or not label.strip() or len(label) > 240):
            raise ValueError("music_event must be a nonempty label of at most 240 characters")

        previous_attention = self._attention
        music_detected = observation.music_confidence >= self.config.music_on
        # VAD hysteresis avoids rapidly alternating on threshold noise.
        if self._speech_active:
            self._speech_active = observation.speech_confidence > self.config.speech_off
        else:
            self._speech_active = observation.speech_confidence >= self.config.speech_on

        if self._speech_active:
            attention = Attention.SPEECH
        elif music_detected:
            attention = Attention.MUSIC
        else:
            attention = Attention.NEUTRAL

        lower = self.config.minimum_stream_weight
        target_speech_weight = (1.0 - lower if attention is Attention.SPEECH
                                else lower if attention is Attention.MUSIC else 0.5)
        if self._last_ms is not None:
            dt = stamp - self._last_ms
            alpha = -math.expm1(-dt / self.config.transition_time_ms)
            self._speech_weight += alpha * (target_speech_weight - self._speech_weight)
        # Both weights always remain nonzero: attention never disables a stream.
        self._speech_weight = max(lower, min(1.0 - lower, self._speech_weight))
        self._last_ms = stamp
        self._attention = attention

        cutoff = stamp - self.config.history_window_ms
        while self._music_events and self._music_events[0].timestamp_ms < cutoff:
            self._music_events.popleft()
        # Do not turn an unverified classifier event into a claimed music fact.
        if music_detected and label is not None:
            self._music_events.append(MusicContextEvent(stamp, label.strip()))

        return ContextDecision(
            timestamp_ms=stamp,
            attention=attention,
            speech_active=self._speech_active,
            music_detected=music_detected,
            speech_weight=self._speech_weight,
            music_weight=1.0 - self._speech_weight,
            recent_music_events=tuple(self._music_events),
            attention_changed=attention is not previous_attention,
        )
