# Copyright (c) Don Michael Feeney Jr.
# Licensed under the MIT License.
"""Unit tests for optional metadata-only acoustic context decisions."""

import math
import unittest

from backend.acoustic_context import (
    AcousticObservation,
    Attention,
    ContextConfig,
    ContinuousAcousticContext,
)


class TestContinuousAcousticContext(unittest.TestCase):
    def test_music_history_persists_through_speech_and_returns_to_music(self):
        controller = ContinuousAcousticContext()
        first = controller.observe(AcousticObservation(0, 0.1, 0.95, "guitar entrance"))
        voice = controller.observe(AcousticObservation(100, 0.95, 0.9, "drum fill"))
        follow = controller.observe(AcousticObservation(200, 0.20, 0.9))
        self.assertEqual(first.attention, Attention.MUSIC)
        self.assertEqual(voice.attention, Attention.SPEECH)
        self.assertEqual(follow.attention, Attention.MUSIC)
        self.assertEqual([event.label for event in follow.recent_music_events],
                         ["guitar entrance", "drum fill"])
        self.assertGreater(follow.music_weight, 0.0)
        self.assertGreater(follow.speech_weight, 0.0)

    def test_speech_hysteresis_avoids_chatter(self):
        controller = ContinuousAcousticContext()
        states = []
        for stamp, confidence in enumerate((0.75, 0.65, 0.41, 0.40, 0.60, 0.71)):
            result = controller.observe(AcousticObservation(stamp * 100, confidence, 0.95))
            states.append(result.attention)
        self.assertEqual(states, [Attention.SPEECH] * 3 + [Attention.MUSIC] * 2 + [Attention.SPEECH])

    def test_weights_smooth_and_do_not_zero_either_stream(self):
        c = ContinuousAcousticContext(ContextConfig(transition_time_ms=200.0))
        c.observe(AcousticObservation(0, 0.0, 0.9))
        first = c.observe(AcousticObservation(100, 0.9, 0.9))
        second = c.observe(AcousticObservation(200, 0.9, 0.9))
        self.assertTrue(0.5 < first.speech_weight < second.speech_weight < 0.85)
        self.assertAlmostEqual(second.speech_weight + second.music_weight, 1.0)
        self.assertGreaterEqual(second.music_weight, 0.15)

    def test_bounded_history_and_expiry_with_no_music(self):
        c = ContinuousAcousticContext(ContextConfig(history_window_ms=200, max_music_events=2))
        c.observe(AcousticObservation(0, 0, 1, "one"))
        c.observe(AcousticObservation(100, 0, 1, "two"))
        c.observe(AcousticObservation(200, 0, 1, "three"))
        snapshot = c.observe(AcousticObservation(250, 0.9, 0))
        self.assertEqual([x.label for x in snapshot.recent_music_events], ["two", "three"])
        self.assertFalse(snapshot.music_detected)
        expired = c.observe(AcousticObservation(500, 0, 0))
        self.assertEqual(expired.recent_music_events, ())
        self.assertEqual(expired.attention, Attention.NEUTRAL)

    def test_low_confidence_music_note_is_not_presented_as_fact(self):
        c = ContinuousAcousticContext()
        result = c.observe(AcousticObservation(0, 0, 0.2, "unverified guitar"))
        self.assertFalse(result.music_detected)
        self.assertEqual(result.recent_music_events, ())

    def test_bad_inputs_are_rejected_without_state_mutation(self):
        c = ContinuousAcousticContext()
        c.observe(AcousticObservation(100, 0.1, 0.9, "opening"))
        invalid = (
            AcousticObservation(100, 0.2, 0.5),
            AcousticObservation(99, 0.2, 0.5),
            AcousticObservation(200, float("nan"), 0.9),
            AcousticObservation(200, 0.1, math.inf),
            AcousticObservation(200, -0.1, 0.5),
            AcousticObservation(200, 0.1, 1.1),
            AcousticObservation(200, 0.1, 0.9, ""),
            AcousticObservation(200, 0.1, 0.9, "x" * 241),
        )
        for value in invalid:
            with self.subTest(value=value), self.assertRaises(ValueError):
                c.observe(value)
        valid = c.observe(AcousticObservation(200, 0.1, 0.9))
        self.assertEqual([e.label for e in valid.recent_music_events], ["opening"])

    def test_reset_clears_context(self):
        c = ContinuousAcousticContext()
        c.observe(AcousticObservation(100, 0, 0.8, "bass line"))
        c.reset()
        result = c.observe(AcousticObservation(0, 0, 0.8))
        self.assertEqual(result.recent_music_events, ())
        self.assertEqual(result.attention, Attention.MUSIC)

    def test_replay_is_deterministic(self):
        samples = [
            AcousticObservation(0, 0, .8, "verse"),
            AcousticObservation(100, .8, .9),
            AcousticObservation(200, .5, .7, "chorus"),
            AcousticObservation(300, .1, .9),
        ]
        def run():
            c = ContinuousAcousticContext()
            return [c.observe(s) for s in samples]
        self.assertEqual(run(), run())

    def test_invalid_configuration(self):
        for kwargs in (
            {"speech_on": 0.3, "speech_off": 0.4},
            {"music_on": math.nan},
            {"transition_time_ms": 0},
            {"minimum_stream_weight": 0.5},
            {"max_music_events": 0},
            {"history_window_ms": -1},
        ):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                ContextConfig(**kwargs)


if __name__ == "__main__":
    unittest.main()
