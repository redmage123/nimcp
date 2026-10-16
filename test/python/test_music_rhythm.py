"""Unit tests for scripts/music_rhythm.py — CE-6 music + rhythm drip.

Pure-Python tests with a fake brain stub. No daemon, no live brain. The
FakeBrain in this file deliberately does NOT have `audio_cortex_process` so
the default fallback path (train_language) is exercised; specialized
subclasses below add it for the audio-cortex path test.
"""
import os
import sys
import unittest

import numpy as np

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import music_rhythm as mr  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stubs
# ---------------------------------------------------------------------------

class FakeBrain:
    """No audio_cortex_process — exercises the train_language fallback."""

    def __init__(self, produced_text="a steady rhythm beat",
                 confidence=0.6):
        self._produced = produced_text
        self._confidence = confidence
        self.produce_calls = []
        self.learn_calls = []
        self.train_calls = []

    def produce_text(self, intent):
        # Capture as plain list — never as numpy — to verify the module
        # is doing the conversion before crossing the RPC boundary.
        self.produce_calls.append(list(intent) if intent is not None else None)
        return {"text": self._produced, "confidence": self._confidence,
                "success": True}

    def learn_language(self, text):
        self.learn_calls.append(text)

    def train_language(self, text, target_text):
        self.train_calls.append((text, target_text))


class FakeBrainWithAudioCortex(FakeBrain):
    """Adds `audio_cortex_process` so the preferred path is exercised."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.audio_calls = []

    def audio_cortex_process(self, samples):
        self.audio_calls.append(samples)
        return {"ok": True}


# ---------------------------------------------------------------------------
# Feature generator
# ---------------------------------------------------------------------------

class TestRhythmFeatures(unittest.TestCase):
    def test_shape_default(self):
        feats = mr.generate_rhythm_features("steady_quarter")
        self.assertEqual(feats.shape, (64, 13))

    def test_shape_custom_frames(self):
        feats = mr.generate_rhythm_features("march_4_4", frames=128)
        self.assertEqual(feats.shape, (128, 13))

    def test_dtype_is_float32(self):
        feats = mr.generate_rhythm_features("waltz_3_4")
        self.assertEqual(feats.dtype, np.float32)

    def test_determinism_same_pattern(self):
        a = mr.generate_rhythm_features("clave_son")
        b = mr.generate_rhythm_features("clave_son")
        self.assertTrue(np.array_equal(a, b))

    def test_determinism_across_frame_counts_consistent_shape(self):
        # Different frame counts should produce different shapes — but each
        # individual (pattern, frames) combination must stay deterministic.
        a = mr.generate_rhythm_features("rumba", frames=32)
        b = mr.generate_rhythm_features("rumba", frames=32)
        self.assertEqual(a.shape, (32, 13))
        self.assertTrue(np.array_equal(a, b))

    def test_different_patterns_produce_different_features(self):
        a = mr.generate_rhythm_features("steady_quarter")
        b = mr.generate_rhythm_features("waltz_3_4")
        self.assertFalse(np.array_equal(a, b))

    def test_onsets_have_higher_energy_than_rests(self):
        # Steady quarter — onsets at frames 0, 21, 42, 63 (rounded). The
        # mean magnitude across onset-region rows should exceed the rest
        # rows.
        feats = mr.generate_rhythm_features("steady_quarter", frames=64)
        # Onset frames are at 0, 21, 42, 63 for quarter beats.
        onset_idx = [0, 21, 42, 63]
        # Pick rest frames a few away from any onset (frame 10 is between
        # the first two onsets, frame 30 between 2nd and 3rd, etc).
        rest_idx = [10, 30, 52]
        onset_energy = feats[onset_idx].mean()
        rest_energy = feats[rest_idx].mean()
        self.assertGreater(onset_energy, rest_energy)

    def test_unknown_pattern_raises_keyerror(self):
        with self.assertRaises(KeyError):
            mr.generate_rhythm_features("not_a_real_pattern")

    def test_zero_frames_raises_valueerror(self):
        with self.assertRaises(ValueError):
            mr.generate_rhythm_features("steady_quarter", frames=0)

    def test_all_palette_patterns_generate_cleanly(self):
        # Every pattern advertised in RHYTHM_PATTERNS must be backed by an
        # onset-table entry and produce finite features.
        for stage_patterns in mr.RHYTHM_PATTERNS.values():
            for name in stage_patterns:
                feats = mr.generate_rhythm_features(name)
                self.assertEqual(feats.shape[1], 13)
                self.assertTrue(np.all(np.isfinite(feats)))


# ---------------------------------------------------------------------------
# Description generator
# ---------------------------------------------------------------------------

class TestMelodyDescription(unittest.TestCase):
    def test_non_empty_for_known_pattern(self):
        d = mr.generate_melody_description("steady_quarter", stage=1)
        self.assertTrue(d)
        self.assertGreater(len(d), 5)

    def test_contains_pattern_keywords(self):
        d = mr.generate_melody_description("waltz_3_4", stage=2)
        # waltz appears in the description verbatim.
        self.assertIn("waltz", d.lower())

    def test_stage_prefix_changes_with_stage(self):
        d1 = mr.generate_melody_description("steady_quarter", stage=1)
        d3 = mr.generate_melody_description("steady_quarter", stage=3)
        # Different prefixes (stage 1: "I hear ", stage 3: "Notice the rhythm in ")
        self.assertNotEqual(d1, d3)

    def test_unknown_pattern_degrades_gracefully(self):
        # Unknown patterns shouldn't crash — they should degrade to a
        # generic stand-in description.
        d = mr.generate_melody_description("nonsense_pattern", stage=2)
        self.assertTrue(d)
        self.assertIn("nonsense_pattern", d)


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

class TestScoring(unittest.TestCase):
    def test_empty_response_scores_zero(self):
        s = mr.score_audio_response("", ["steady", "rhythm", "beat"])
        self.assertEqual(s["composite"], 0.0)
        self.assertEqual(s["recall"], 0.0)

    def test_full_keyword_overlap_scores_high(self):
        s = mr.score_audio_response(
            "I hear a steady rhythm beat",
            ["steady", "rhythm", "beat"])
        # Recall should be 1.0 (all three keywords present).
        self.assertEqual(s["recall"], 1.0)
        # Composite weighted (2*1 + spec) / 3 — even with low specificity,
        # composite should be well above the 0.45 pass threshold.
        self.assertGreater(s["composite"], mr.MUSIC_PASS_THRESHOLD)

    def test_zero_keyword_overlap_scores_zero(self):
        s = mr.score_audio_response(
            "spaceship orbits the silver moon",
            ["steady", "rhythm", "beat", "drum"])
        self.assertEqual(s["recall"], 0.0)
        self.assertEqual(s["composite"], 0.0)

    def test_partial_overlap(self):
        s = mr.score_audio_response(
            "I hear a steady drum",
            ["steady", "rhythm", "beat", "drum"])
        # 2 of 4 keywords matched.
        self.assertAlmostEqual(s["recall"], 0.5, places=5)
        self.assertGreater(s["composite"], 0.0)
        self.assertLess(s["composite"], 1.0)

    def test_no_expected_keywords_returns_zero(self):
        # Charitable degenerate case — composite is 0 not 1 (we can't
        # evaluate, so don't false-positive).
        s = mr.score_audio_response("I hear a steady drum", [])
        self.assertEqual(s["composite"], 0.0)


# ---------------------------------------------------------------------------
# Drip — stage 1 must be active
# ---------------------------------------------------------------------------

class TestDripStage1(unittest.TestCase):
    def test_stage_1_invokes_brain_calls(self):
        # Reset rotation for determinism.
        mr._ROTATION_COUNTER[1] = 0
        b = FakeBrain(produced_text="I hear a steady drum beat",
                      confidence=0.7)
        results = mr.run_music_drip(b, stage=1, num_patterns=2)
        self.assertIsNotNone(results)
        # Stage 1 produces num_patterns results.
        self.assertEqual(len(results), 2)
        # produce_text must have been called for each pattern.
        self.assertEqual(len(b.produce_calls), 2)
        # Either learn_calls or train_calls (depending on score) must have
        # advanced — drip is NOT a no-op.
        self.assertGreater(len(b.learn_calls) + len(b.train_calls), 0)

    def test_stage_1_palette_has_4_patterns(self):
        self.assertEqual(len(mr.RHYTHM_PATTERNS[1]), 4)


# ---------------------------------------------------------------------------
# Drip — stages 2 and 3
# ---------------------------------------------------------------------------

class TestDripStages23(unittest.TestCase):
    def test_stage_2_produces_num_patterns_results(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrain(produced_text="a marching drum pattern in four-four time",
                      confidence=0.8)
        results = mr.run_music_drip(b, stage=2, num_patterns=3)
        self.assertEqual(len(results), 3)
        self.assertEqual(len(b.produce_calls), 3)

    def test_stage_3_produces_num_patterns_results(self):
        mr._ROTATION_COUNTER[3] = 0
        b = FakeBrain(produced_text="a polyrhythm of three against two",
                      confidence=0.7)
        results = mr.run_music_drip(b, stage=3, num_patterns=2)
        self.assertEqual(len(results), 2)
        self.assertEqual(len(b.produce_calls), 2)

    def test_unknown_stage_returns_none(self):
        b = FakeBrain()
        result = mr.run_music_drip(b, stage=99)
        self.assertIsNone(result)


# ---------------------------------------------------------------------------
# Drip — audio path selection
# ---------------------------------------------------------------------------

class TestAudioPathSelection(unittest.TestCase):
    def test_audio_cortex_path_when_available(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrainWithAudioCortex(
            produced_text="a marching drum pattern in four-four time",
            confidence=0.8)
        results = mr.run_music_drip(b, stage=2, num_patterns=2)
        self.assertEqual(len(results), 2)
        # audio_cortex_process should have been called once per pattern.
        self.assertEqual(len(b.audio_calls), 2)
        # produce_text always follows ingest.
        self.assertEqual(len(b.produce_calls), 2)
        # ingest_path is recorded in the result for visibility.
        self.assertEqual(results[0]["ingest_path"], "audio_cortex")

    def test_audio_cortex_payload_is_flat_float_list(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrainWithAudioCortex()
        mr.run_music_drip(b, stage=2, num_patterns=1)
        samples = b.audio_calls[0]
        # Must be a plain list, not a numpy array — RPC layer needs JSON-able.
        self.assertIsInstance(samples, list)
        # 64 frames * 13 features = 832 floats.
        self.assertEqual(len(samples), 832)
        # All floats.
        self.assertTrue(all(isinstance(s, float) for s in samples))

    def test_train_language_fallback_when_no_audio_cortex(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrain(  # no audio_cortex_process
            produced_text="aaa",  # low score → forces train re-anchor too
            confidence=0.1)
        results = mr.run_music_drip(b, stage=2, num_patterns=1)
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0]["ingest_path"], "train_language")
        # train_language was called (at least the ingest call). We don't pin
        # the exact count since the feedback path may also call it on a
        # low score.
        self.assertGreaterEqual(len(b.train_calls), 1)


# ---------------------------------------------------------------------------
# Drip — feedback gate
# ---------------------------------------------------------------------------

class TestFeedbackGate(unittest.TestCase):
    def test_high_score_takes_learn_path(self):
        # High recall → composite > threshold → learn_language called.
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrainWithAudioCortex(
            # Response containing many waltz-description keywords.
            produced_text="listen to a waltz rhythm in three-four time with one strong beat",
            confidence=0.9)
        # rotation_index=1 selects "waltz_3_4" (index 1 in stage 2 palette).
        results = mr.run_music_drip(b, stage=2, num_patterns=1,
                                     pattern_index=1)
        self.assertEqual(results[0]["pattern"], "waltz_3_4")
        self.assertGreater(results[0]["composite"], mr.MUSIC_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        # No re-anchor train_language (audio path used the audio_cortex RPC,
        # so train_language is only called on the corrective branch).
        self.assertEqual(len(b.train_calls), 0)

    def test_low_score_takes_train_reanchor_path(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrainWithAudioCortex(
            produced_text="zzz qqq xxx",  # no keyword overlap
            confidence=0.1)
        results = mr.run_music_drip(b, stage=2, num_patterns=1,
                                     pattern_index=0)
        self.assertLess(results[0]["composite"], mr.MUSIC_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        # Re-anchor: train_language(description, description).
        self.assertEqual(len(b.train_calls), 1)
        self.assertEqual(b.train_calls[0][0], b.train_calls[0][1])


# ---------------------------------------------------------------------------
# Drip — rotation
# ---------------------------------------------------------------------------

class TestRotation(unittest.TestCase):
    def test_explicit_pattern_index_does_not_bump_counter(self):
        mr._ROTATION_COUNTER[2] = 5
        b = FakeBrainWithAudioCortex()
        mr.run_music_drip(b, stage=2, num_patterns=2, pattern_index=0)
        # Counter must remain 5 — explicit index bypasses the bump.
        self.assertEqual(mr._ROTATION_COUNTER[2], 5)

    def test_implicit_rotation_advances_counter(self):
        mr._ROTATION_COUNTER[2] = 0
        b = FakeBrainWithAudioCortex()
        mr.run_music_drip(b, stage=2, num_patterns=3)
        # Counter should now equal 3 (one bump per pattern).
        self.assertEqual(mr._ROTATION_COUNTER[2], 3)


# ---------------------------------------------------------------------------
# Robustness — exceptions in brain calls don't crash the drip
# ---------------------------------------------------------------------------

class TestRobustness(unittest.TestCase):
    def test_produce_failure_skips_pattern(self):
        class BoomBrain(FakeBrainWithAudioCortex):
            def produce_text(self, intent):
                raise RuntimeError("simulated produce failure")

        mr._ROTATION_COUNTER[2] = 0
        b = BoomBrain()
        # Should not raise — exception is swallowed at drip level.
        results = mr.run_music_drip(b, stage=2, num_patterns=2)
        self.assertEqual(results, [])

    def test_audio_ingest_failure_is_caught(self):
        class BoomAudioBrain(FakeBrain):
            def audio_cortex_process(self, samples):
                raise RuntimeError("simulated audio failure")

        mr._ROTATION_COUNTER[2] = 0
        b = BoomAudioBrain()
        # Drip should swallow the audio failure and continue.
        results = mr.run_music_drip(b, stage=2, num_patterns=1)
        # No produce_text reached because ingest failed first.
        self.assertEqual(results, [])
        self.assertEqual(len(b.produce_calls), 0)


if __name__ == "__main__":
    unittest.main()
