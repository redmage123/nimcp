"""Unit tests for scripts/storytelling.py — CE-1 storytelling production
+ coherence scoring.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import storytelling as st  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path."""

    def __init__(self, produced_text="The cat sat and watched the bird outside.",
                 confidence=0.7):
        self._produced = produced_text
        self._confidence = confidence
        self.produce_calls = []
        self.learn_calls = []
        self.train_calls = []

    def produce_text(self, intent):
        self.produce_calls.append(list(intent) if intent is not None else None)
        return {"text": self._produced, "confidence": self._confidence,
                "success": True}

    def learn_language(self, text):
        self.learn_calls.append(text)

    def train_language(self, text, target_text):
        self.train_calls.append((text, target_text))


# ---------------------------------------------------------------------------
# Score components
# ---------------------------------------------------------------------------

class TestScoring(unittest.TestCase):
    def test_diversity_high_for_varied(self):
        s = st.score_coherence(
            seed="The cat sat on the mat.",
            produced="A small fox crept across the gentle hill at dawn.")
        self.assertGreater(s["diversity"], 0.7)

    def test_diversity_low_for_repetition(self):
        s = st.score_coherence(
            seed="The cat sat on the mat.",
            produced="cat cat cat cat cat cat cat cat cat cat")
        self.assertLessEqual(s["diversity"], 0.2)

    def test_repetition_penalty_drops_for_collapsed_trigram(self):
        s = st.score_coherence(
            seed="The cat sat on the mat.",
            # Same trigram repeated many times — classic mode collapse signal.
            produced=("the cat sat the cat sat the cat sat the cat sat "
                      "the cat sat the cat sat the cat sat the cat sat"))
        self.assertLess(s["repetition"], 0.5)

    def test_on_topic_high_when_seed_words_present(self):
        s = st.score_coherence(
            seed="The cat sat on the mat and watched the bird.",
            produced="The cat watched the bird from the mat.")
        self.assertGreaterEqual(s["on_topic"], 0.6)

    def test_on_topic_zero_when_unrelated(self):
        s = st.score_coherence(
            seed="The cat sat on the mat and watched the bird.",
            produced="A spaceship hurtled past Jupiter on its long voyage outward.")
        self.assertEqual(s["on_topic"], 0.0)

    def test_composite_is_average_of_three(self):
        s = st.score_coherence(
            seed="The dog ran across the field chasing the red ball.",
            produced="The dog ran across the field chasing the red ball.")
        # Self-reproduction: high diversity, no repeated trigrams in 9-token
        # text, full on-topic. Composite should be near 1.0.
        self.assertGreater(s["composite"], 0.8)

    def test_empty_produced_zero_components(self):
        s = st.score_coherence(seed="Anything goes here.", produced="")
        self.assertEqual(s["diversity"], 0.0)
        self.assertEqual(s["on_topic"], 0.0)
        # Repetition is conservatively 1.0 for tokens<3 (no trigrams to be
        # repeated). Composite reflects that.
        self.assertEqual(s["repetition"], 1.0)
        self.assertAlmostEqual(s["composite"], 1.0 / 3.0, places=5)

    def test_seed_with_no_content_words(self):
        # Pure stop-word seed → on_topic floor of 0.0 (we can't score).
        s = st.score_coherence(seed="The and of in.", produced="The cat sat.")
        self.assertEqual(s["on_topic"], 0.0)


# ---------------------------------------------------------------------------
# Seed picking
# ---------------------------------------------------------------------------

class TestPickSeed(unittest.TestCase):
    def test_stage_1_returns_empty(self):
        seed, content = st.pick_seed(1, 0)
        self.assertEqual(seed, "")
        self.assertEqual(content, set())

    def test_stage_2_rotation_cycles(self):
        seeds = [st.pick_seed(2, i)[0] for i in range(20)]
        self.assertTrue(all(s for s in seeds))
        # Rotation is modular — first seed should reappear.
        self.assertEqual(seeds[0], seeds[len(st._STAGE_SEEDS[2])])

    def test_stage_3_seeds_exist(self):
        seed, content = st.pick_seed(3, 0)
        self.assertTrue(seed)
        self.assertGreater(len(content), 0)


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

class TestDrip(unittest.TestCase):
    def test_stage_1_is_noop(self):
        b = FakeBrain()
        result = st.run_storytelling_drip(b, stage=1, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_stage_2_invokes_produce(self):
        b = FakeBrain(produced_text="The cat watched the bird from the mat.",
                      confidence=0.7)
        result = st.run_storytelling_drip(b, stage=2, composer=None,
                                           rotation_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(result["stage"], 2)
        self.assertEqual(len(b.produce_calls), 1)
        self.assertIn("composite", result)

    def test_high_score_takes_positive_path(self):
        # Reproduce the seed verbatim — composite ~ 1.0 → learn_language
        # should be called, train_language NOT called.
        seed, _ = st.pick_seed(2, 0)
        b = FakeBrain(produced_text=seed, confidence=0.9)
        result = st.run_storytelling_drip(b, stage=2, composer=None,
                                           rotation_index=0)
        self.assertGreater(result["composite"], 0.7)
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(len(b.train_calls), 0)
        self.assertEqual(b.learn_calls[0], seed)

    def test_low_score_takes_corrective_path(self):
        # Off-topic + repetitive → composite below threshold → train_language
        # on the seed itself, not learn_language.
        b = FakeBrain(produced_text="zzz zzz zzz zzz zzz zzz", confidence=0.1)
        result = st.run_storytelling_drip(b, stage=2, composer=None,
                                           rotation_index=0)
        self.assertLess(result["composite"], st.COHERENCE_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        # Re-anchor pair: (seed, seed)
        self.assertEqual(b.train_calls[0][0], b.train_calls[0][1])

    def test_produce_failure_is_caught(self):
        class BoomBrain(FakeBrain):
            def produce_text(self, intent):
                raise RuntimeError("simulated produce failure")

        b = BoomBrain()
        # Should swallow + return None, not raise.
        result = st.run_storytelling_drip(b, stage=2, composer=None,
                                           rotation_index=0)
        self.assertIsNone(result)

    def test_rotation_advances_module_counter(self):
        # Reset the counter for determinism.
        st._ROTATION_COUNTER[2] = 0
        b = FakeBrain()
        st.run_storytelling_drip(b, stage=2, composer=None)
        st.run_storytelling_drip(b, stage=2, composer=None)
        # Counter should now be 2 (each call bumps it once).
        self.assertEqual(st._ROTATION_COUNTER[2], 2)

    def test_explicit_rotation_index_does_not_bump_counter(self):
        st._ROTATION_COUNTER[2] = 5
        b = FakeBrain()
        st.run_storytelling_drip(b, stage=2, composer=None, rotation_index=99)
        self.assertEqual(st._ROTATION_COUNTER[2], 5)


# ---------------------------------------------------------------------------
# Composer integration (when one is supplied)
# ---------------------------------------------------------------------------

class TestComposerIntegration(unittest.TestCase):
    class FakeComposer:
        def __init__(self):
            self.calls = []

        def compose(self, text=None, modality="text"):
            self.calls.append((text, modality))
            import numpy as np
            return np.zeros(1024, dtype=np.float32)

    def test_composer_used_when_given(self):
        c = self.FakeComposer()
        b = FakeBrain()
        st.run_storytelling_drip(b, stage=2, composer=c, rotation_index=0)
        self.assertEqual(len(c.calls), 1)
        self.assertEqual(c.calls[0][1], "text")

    def test_composer_failure_falls_back_to_zeros(self):
        class BoomComposer:
            def compose(self, text=None, modality="text"):
                raise RuntimeError("boom")

        b = FakeBrain()
        # Should not raise — falls back to zero vector.
        result = st.run_storytelling_drip(b, stage=2, composer=BoomComposer(),
                                           rotation_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(b.produce_calls), 1)


if __name__ == "__main__":
    unittest.main()
