"""Unit tests for scripts/tom_probes.py — CE-8 Theory of Mind probes.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import tom_probes as tp  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub — same shape as test_storytelling.FakeBrain.
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path."""

    def __init__(self, produced_text="The cat watches the bird outside.",
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


def _reset_module_counters():
    for s in (1, 2, 3):
        tp._SCENARIO_COUNTER[s] = 0
        tp._PROBE_CURSOR[s] = 0


# ---------------------------------------------------------------------------
# build_probe
# ---------------------------------------------------------------------------

class TestBuildProbe(unittest.TestCase):
    def setUp(self):
        self.scenario = tp.TOM_SCENARIOS[2][0]  # Anna / red box / blue box

    def test_false_belief_prompt_shape(self):
        p = tp.build_probe(self.scenario, "false_belief")
        self.assertIsNotNone(p)
        self.assertIn("In the story:", p["prompt"])
        self.assertIn("Anna", p["prompt"])
        self.assertIn("think", p["prompt"])
        self.assertEqual(p["probe_type"], "false_belief")
        self.assertEqual(p["expected_keywords"], {"red", "box"})

    def test_desire_prompt_shape(self):
        p = tp.build_probe(self.scenario, "desire")
        self.assertIsNotNone(p)
        self.assertIn("Anna", p["prompt"])
        self.assertIn("want", p["prompt"])
        self.assertEqual(p["probe_type"], "desire")

    def test_intent_prompt_shape(self):
        p = tp.build_probe(self.scenario, "intent")
        self.assertIsNotNone(p)
        self.assertIn("do next", p["prompt"])

    def test_emotion_prompt_shape(self):
        p = tp.build_probe(self.scenario, "emotion")
        self.assertIsNotNone(p)
        self.assertIn("feel", p["prompt"])

    def test_empty_expected_returns_none(self):
        # Stage 3 scenario "She wrote the letter..." has empty
        # false_belief set — should return None.
        empty_fb = next(s for s in tp.TOM_SCENARIOS[3]
                        if not s["expected"]["false_belief"])
        self.assertIsNone(tp.build_probe(empty_fb, "false_belief"))
        # But other probe types still work.
        self.assertIsNotNone(tp.build_probe(empty_fb, "desire"))

    def test_unknown_probe_type_returns_none(self):
        self.assertIsNone(tp.build_probe(self.scenario, "not_a_real_type"))


# ---------------------------------------------------------------------------
# score_tom_answer
# ---------------------------------------------------------------------------

class TestScoring(unittest.TestCase):
    def test_empty_answer_zero_composite(self):
        s = tp.score_tom_answer("", {"red", "box"})
        self.assertEqual(s["tom_recall"], 0.0)
        self.assertEqual(s["specificity"], 0.0)
        self.assertEqual(s["composite"], 0.0)

    def test_full_keyword_overlap_recall_one(self):
        s = tp.score_tom_answer(
            "Anna will look in the red box for her ball.",
            {"red", "box"})
        self.assertEqual(s["tom_recall"], 1.0)

    def test_partial_keyword_overlap(self):
        s = tp.score_tom_answer(
            "Anna will check the red shelf.",
            {"red", "box"})
        self.assertAlmostEqual(s["tom_recall"], 0.5, places=5)

    def test_specificity_low_for_one_word(self):
        s = tp.score_tom_answer("ball", {"ball", "play"})
        # Recall = 1 (ball matches), but specificity should penalize the
        # one-word answer: 1 distinct content word / 4 = 0.25.
        self.assertEqual(s["tom_recall"], 0.5)
        self.assertAlmostEqual(s["specificity"], 0.25, places=5)
        # composite = (2 * 0.5 + 0.25) / 3 = 0.4166...
        self.assertAlmostEqual(s["composite"], (1.0 + 0.25) / 3.0, places=5)

    def test_specificity_caps_at_one(self):
        # Plenty of distinct content words → specificity = 1.0.
        s = tp.score_tom_answer(
            "Anna walks toward the red shelf carrying a colorful "
            "wooden basket carefully.",
            {"red"})
        self.assertEqual(s["specificity"], 1.0)

    def test_recall_weighted_double(self):
        # Same specificity, half the recall → composite drops by exactly
        # 1/3 of the recall delta.
        full = tp.score_tom_answer(
            "Anna goes to red box looking",
            {"red", "box"})
        half = tp.score_tom_answer(
            "Anna goes to red shelf looking",
            {"red", "box"})
        self.assertAlmostEqual(full["specificity"], half["specificity"],
                               places=5)
        # Recall difference is 0.5 → composite difference should be
        # (2 * 0.5) / 3 = 0.333...
        self.assertAlmostEqual(full["composite"] - half["composite"],
                               (2.0 * 0.5) / 3.0, places=5)

    def test_empty_expected_set_zero_recall(self):
        # When expected is empty (e.g. some stage-3 false_belief), recall
        # is reported as 0.0 — but in practice build_probe filters these out
        # before they reach the scorer.
        s = tp.score_tom_answer("anything goes here", set())
        self.assertEqual(s["tom_recall"], 0.0)


# ---------------------------------------------------------------------------
# run_tom_drip
# ---------------------------------------------------------------------------

class TestDrip(unittest.TestCase):
    def setUp(self):
        _reset_module_counters()

    def test_stage_1_is_noop(self):
        b = FakeBrain()
        result = tp.run_tom_drip(b, stage=1, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_stage_2_invokes_two_produce_calls(self):
        b = FakeBrain()
        result = tp.run_tom_drip(b, stage=2, composer=None,
                                 scenario_index=0, num_probes=2)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 2)
        self.assertEqual(len(b.produce_calls), 2)

    def test_high_score_takes_positive_path(self):
        # Anna scenario, false_belief expected = {red, box}. An answer that
        # mentions both keywords plus extra content words → composite high.
        b = FakeBrain(produced_text=(
            "Anna believes the ball is still in the red box and will look "
            "there carefully."))
        result = tp.run_tom_drip(b, stage=2, composer=None,
                                 scenario_index=0, num_probes=1)
        self.assertIsNotNone(result)
        self.assertGreaterEqual(result[0]["composite"],
                                tp.TOM_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(len(b.train_calls), 0)

    def test_low_score_takes_corrective_path(self):
        b = FakeBrain(produced_text="zzz")
        result = tp.run_tom_drip(b, stage=2, composer=None,
                                 scenario_index=0, num_probes=1)
        self.assertIsNotNone(result)
        self.assertLess(result[0]["composite"], tp.TOM_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        # Re-anchor pair: (prompt+keywords, prompt+keywords).
        train_text, train_target = b.train_calls[0]
        self.assertEqual(train_text, train_target)
        # The anchor must contain the prompt scaffolding plus the expected
        # keywords sorted.
        self.assertIn("Anna", train_text)
        for kw in result[0]["expected_keywords"]:
            self.assertIn(kw, train_text)

    def test_produce_failure_is_caught(self):
        class BoomBrain(FakeBrain):
            def produce_text(self, intent):
                raise RuntimeError("simulated produce failure")

        b = BoomBrain()
        # Should not raise. With num_probes=1 and produce_text always
        # raising, no successful results → returns None.
        result = tp.run_tom_drip(b, stage=2, composer=None,
                                 scenario_index=0, num_probes=1)
        self.assertIsNone(result)
        # Critically: the call did not crash and feedback wasn't issued.
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 0)

    def test_scenario_rotation_advances_counter(self):
        _reset_module_counters()
        b = FakeBrain()
        tp.run_tom_drip(b, stage=2, composer=None, num_probes=1)
        tp.run_tom_drip(b, stage=2, composer=None, num_probes=1)
        # Counter should have bumped twice.
        self.assertEqual(tp._SCENARIO_COUNTER[2], 2)

    def test_explicit_scenario_index_does_not_bump_counter(self):
        _reset_module_counters()
        tp._SCENARIO_COUNTER[2] = 5
        b = FakeBrain()
        tp.run_tom_drip(b, stage=2, composer=None,
                        scenario_index=99, num_probes=1)
        self.assertEqual(tp._SCENARIO_COUNTER[2], 5)

    def test_empty_expected_probe_skipped(self):
        # Find the index of the stage-3 scenario "She wrote the letter..."
        # which has empty false_belief and empty (well, len=0) expected
        # for false_belief but valid expected for desire/intent/emotion.
        idx = next(i for i, s in enumerate(tp.TOM_SCENARIOS[3])
                   if not s["expected"]["false_belief"])
        b = FakeBrain()
        # Force the cursor to start at "false_belief" (index 0). With
        # num_probes=1 the drip should walk past the empty false_belief and
        # land on the next non-empty probe type → exactly 1 produce call.
        tp._PROBE_CURSOR[3] = 0
        result = tp.run_tom_drip(b, stage=3, composer=None,
                                 scenario_index=idx, num_probes=1)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        # The selected probe type must NOT be false_belief.
        self.assertNotEqual(result[0]["probe_type"], "false_belief")
        self.assertEqual(len(b.produce_calls), 1)


# ---------------------------------------------------------------------------
# Composer integration
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
        tp.run_tom_drip(b, stage=2, composer=c,
                        scenario_index=0, num_probes=1)
        self.assertGreaterEqual(len(c.calls), 1)
        self.assertEqual(c.calls[0][1], "text")

    def test_composer_failure_falls_back_to_zeros(self):
        class BoomComposer:
            def compose(self, text=None, modality="text"):
                raise RuntimeError("boom")

        b = FakeBrain()
        # Should not raise — falls back to zero vector.
        result = tp.run_tom_drip(b, stage=2, composer=BoomComposer(),
                                 scenario_index=0, num_probes=1)
        self.assertIsNotNone(result)
        self.assertEqual(len(b.produce_calls), 1)

    def test_composer_none_uses_zero_vector(self):
        b = FakeBrain()
        result = tp.run_tom_drip(b, stage=2, composer=None,
                                 scenario_index=0, num_probes=1)
        self.assertIsNotNone(result)
        # 1024-element zero list reaches produce_text.
        self.assertEqual(len(b.produce_calls[0]), 1024)
        self.assertEqual(set(b.produce_calls[0]), {0.0})


if __name__ == "__main__":
    unittest.main()
