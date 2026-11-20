# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for the optional native-VAD metadata adapter (no microphone)."""

import math
import unittest

from backend.acoustic_context import AcousticObservation, Attention, ContinuousAcousticContext
from backend.acoustic_signal_adapter import (
    AcousticSignalAdapter, APMSVadTelemetry, MusicAnalysisTelemetry,
)


def pair(timestamp, active, speech_confidence, music_confidence, event=None):
    return (
        APMSVadTelemetry(timestamp, active, speech_confidence, 9.0, -28.0),
        MusicAnalysisTelemetry(timestamp, music_confidence, event),
    )


class TestAcousticSignalAdapter(unittest.TestCase):
    def test_native_hangover_flag_survives_low_confidence(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(0, True, 0.9, 0.9, "intro")
        start = adapter.ingest(a, b)
        a, b = pair(100, True, 0.0, 0.95, "guitar fill")
        hangover = adapter.ingest(a, b)
        self.assertTrue(hangover.speech_active)
        self.assertEqual(hangover.attention, Attention.SPEECH)
        self.assertEqual([x.label for x in hangover.recent_music_events],
                         ["intro", "guitar fill"])
        self.assertGreater(hangover.music_weight, 0.0)
        self.assertEqual(start.attention, Attention.SPEECH)

    def test_native_flag_false_overrides_old_controller_state(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(0, True, 0.9, 0.9)
        adapter.ingest(a, b)
        a, b = pair(100, False, 0.8, 0.9)
        result = adapter.ingest(a, b)
        self.assertFalse(result.speech_active)
        self.assertEqual(result.attention, Attention.MUSIC)

    def test_independently_observed_music_survives_voice(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(0, False, 0, 0.9, "verse")
        adapter.ingest(a, b)
        a, b = pair(100, True, 0.7, 0.9, "solo")
        result = adapter.ingest(a, b)
        self.assertEqual([x.label for x in result.recent_music_events], ["verse", "solo"])
        self.assertAlmostEqual(result.speech_weight + result.music_weight, 1.0)

    def test_unaligned_metadata_does_not_modify_state(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(0, False, 0.2, 0.8, "start")
        adapter.ingest(a, b)
        a, b = pair(100, True, 0.9, 0.9, "should not enter")
        wrong_music = MusicAnalysisTelemetry(99, b.confidence, b.event)
        with self.assertRaises(ValueError):
            adapter.ingest(a, wrong_music)
        result = adapter.ingest(a, b)
        self.assertEqual([x.label for x in result.recent_music_events], ["start", "should not enter"])

    def test_bad_native_values_rejected_before_state_change(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(0, False, 0.1, 0.9, "first")
        adapter.ingest(a, b)
        cases = [
            (APMSVadTelemetry(100, 1, 0.5, 3.0, -20), b),
            (APMSVadTelemetry(100, True, math.nan, 3.0, -20), b),
            (APMSVadTelemetry(100, True, 0.8, math.inf, -20), b),
            (APMSVadTelemetry(100, True, 1.5, 3.0, -20), b),
            (APMSVadTelemetry(100, True, 0.8, 3.0, math.nan), b),
            (APMSVadTelemetry(100, True, 0.8, 3.0, -20), MusicAnalysisTelemetry(100, -0.1)),
        ]
        for va, mu in cases:
            with self.subTest(vad=va, music=mu), self.assertRaises(ValueError):
                adapter.ingest(va, mu)
        a, b = pair(100, True, 0.6, 0.9)
        output = adapter.ingest(a, b)
        self.assertEqual([x.label for x in output.recent_music_events], ["first"])

    def test_original_score_only_observation_still_works(self):
        context = ContinuousAcousticContext()
        result = context.observe(AcousticObservation(0, 0.75, 0.9))
        self.assertEqual(result.attention, Attention.SPEECH)

    def test_bad_speech_override_does_not_mutate_context(self):
        context = ContinuousAcousticContext()
        with self.assertRaises(ValueError):
            context.observe(AcousticObservation(0, 0.5, 0.9, speech_detected=1))
        result = context.observe(AcousticObservation(0, 0.1, 0.9))
        self.assertEqual(result.attention, Attention.MUSIC)

    def test_session_reset_erases_old_events(self):
        adapter = AcousticSignalAdapter()
        a, b = pair(123, True, 0.9, 0.9, "old session")
        adapter.ingest(a, b)
        adapter.reset()
        a, b = pair(0, False, 0, 0.9)
        result = adapter.ingest(a, b)
        self.assertEqual(result.recent_music_events, ())


if __name__ == "__main__":
    unittest.main()
