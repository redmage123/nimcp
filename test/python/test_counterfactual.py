"""Unit tests for scripts/counterfactual.py — CE-9 counterfactual
reasoning curriculum.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import counterfactual as cf  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path."""

    def __init__(self, produced_text="An alternate outcome unfolded indoors.",
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
# build_probe
# ---------------------------------------------------------------------------

class TestBuildProbe(unittest.TestCase):
    def setUp(self):
        # Pull a known stage-2 scenario.
        self.scenario = cf.COUNTERFACTUAL_SCENARIOS[2][0]

    def test_if_not_cause_prompt_shape(self):
        probe = cf.build_probe(self.scenario, "if_not_cause")
        self.assertIsNotNone(probe)
        self.assertIn("had not happened", probe["prompt"])
        self.assertIn(self.scenario["cause"], probe["prompt"])
        self.assertIn(self.scenario["setting"], probe["prompt"])
        self.assertEqual(probe["probe_type"], "if_not_cause")

    def test_if_different_action_prompt_shape(self):
        probe = cf.build_probe(self.scenario, "if_different_action")
        self.assertIsNotNone(probe)
        self.assertIn("acted differently", probe["prompt"])
        self.assertIn(self.scenario["agent"], probe["prompt"])

    def test_if_earlier_prompt_shape(self):
        probe = cf.build_probe(self.scenario, "if_earlier")
        self.assertIsNotNone(probe)
        self.assertIn("happened sooner", probe["prompt"])

    def test_if_no_witness_prompt_shape(self):
        probe = cf.build_probe(self.scenario, "if_no_witness")
        self.assertIsNotNone(probe)
        self.assertIn("no one had been there", probe["prompt"])

    def test_empty_expected_returns_none(self):
        # Strip alternates for one probe type; build_probe should refuse.
        scenario = dict(self.scenario)
        scenario["alternates"] = dict(scenario["alternates"])
        scenario["alternates"]["if_not_cause"] = set()
        probe = cf.build_probe(scenario, "if_not_cause")
        self.assertIsNone(probe)

    def test_expected_and_forbidden_carried_through(self):
        probe = cf.build_probe(self.scenario, "if_not_cause")
        self.assertEqual(probe["expected_keywords"],
                         self.scenario["alternates"]["if_not_cause"])
        self.assertEqual(probe["forbidden_keywords"],
                         self.scenario["forbidden"])

    def test_invalid_probe_type_returns_none(self):
        probe = cf.build_probe(self.scenario, "if_pigs_fly")
        self.assertIsNone(probe)


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

class TestScoring(unittest.TestCase):
    def test_empty_answer_flip_one_composite_half(self):
        s = cf.score_counterfactual_answer(
            "", expected={"sunny", "outside"}, forbidden={"cancelled", "rain"})
        self.assertEqual(s["recall"], 0.0)
        self.assertEqual(s["flip_score"], 1.0)
        self.assertEqual(s["specificity"], 0.0)
        self.assertAlmostEqual(s["composite"], 0.5, places=5)

    def test_answer_repeating_forbidden_drops_flip(self):
        # Answer just restates the original effect — failure to flip.
        s = cf.score_counterfactual_answer(
            "the picnic was cancelled because of rain",
            expected={"sunny", "outside"},
            forbidden={"cancelled", "rain"})
        self.assertEqual(s["flip_score"], 0.0)
        # Recall is also 0 (no expected words).
        self.assertEqual(s["recall"], 0.0)
        # Composite must be low.
        self.assertLess(s["composite"], cf.COUNTERFACTUAL_PASS_THRESHOLD)

    def test_full_expected_no_forbidden_high_composite(self):
        s = cf.score_counterfactual_answer(
            "they would picnic outside under sunny skies happily",
            expected={"sunny", "picnic", "happened", "outside"},
            forbidden={"cancelled", "rain"})
        self.assertEqual(s["flip_score"], 1.0)
        self.assertGreater(s["recall"], 0.5)
        self.assertGreater(s["specificity"], 0.5)
        self.assertGreater(s["composite"], 0.7)

    def test_flip_weighted_higher_than_recall(self):
        # Answer A: ALL expected hit but ALSO restates forbidden.
        a = cf.score_counterfactual_answer(
            "sunny picnic happened outside but the picnic was cancelled in the rain",
            expected={"sunny", "picnic", "happened", "outside"},
            forbidden={"cancelled", "rain"})
        # Answer B: PARTIAL expected, no forbidden.
        b = cf.score_counterfactual_answer(
            "they enjoyed a sunny morning and went outside together",
            expected={"sunny", "picnic", "happened", "outside"},
            forbidden={"cancelled", "rain"})
        # Counterfactual quality favors the one that flipped the outcome.
        self.assertGreater(b["composite"], a["composite"])
        self.assertEqual(a["flip_score"], 0.0)
        self.assertEqual(b["flip_score"], 1.0)

    def test_partial_forbidden_partial_flip(self):
        # Hitting one of two forbidden words → flip_score 0.5.
        s = cf.score_counterfactual_answer(
            "the rain stopped and they smiled",
            expected={"sunny", "outside"},
            forbidden={"cancelled", "rain"})
        self.assertAlmostEqual(s["flip_score"], 0.5, places=5)

    def test_specificity_caps_at_one(self):
        s = cf.score_counterfactual_answer(
            "alpha beta gamma delta epsilon zeta eta theta iota kappa",
            expected={"sunny"},
            forbidden={"cancelled"})
        self.assertEqual(s["specificity"], 1.0)


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

class TestDrip(unittest.TestCase):
    def test_stage_1_is_noop(self):
        b = FakeBrain()
        result = cf.run_counterfactual_drip(b, stage=1, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_stage_2_runs_num_probes(self):
        b = FakeBrain(produced_text="they enjoyed a sunny picnic outside happily",
                      confidence=0.7)
        result = cf.run_counterfactual_drip(
            b, stage=2, composer=None, num_probes=2, scenario_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 2)
        self.assertEqual(len(b.produce_calls), 2)
        # Each probe should use a different probe_type when rotating.
        ptypes = [r["probe_type"] for r in result]
        self.assertEqual(len(set(ptypes)), 2)

    def test_high_composite_triggers_learn(self):
        # Answer hits many expected keywords for the rain scenario's
        # if_not_cause probe and avoids forbidden.
        b = FakeBrain(
            produced_text="they enjoyed a sunny picnic that happened outside happily",
            confidence=0.9)
        result = cf.run_counterfactual_drip(
            b, stage=2, composer=None, num_probes=1, scenario_index=0)
        self.assertEqual(len(result), 1)
        # First probe rotates to "if_not_cause".
        self.assertEqual(result[0]["probe_type"], "if_not_cause")
        self.assertGreaterEqual(result[0]["composite"],
                                cf.COUNTERFACTUAL_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(len(b.train_calls), 0)

    def test_low_composite_triggers_reanchor_train(self):
        # Restate the forbidden outcome — flip_score=0, low composite.
        b = FakeBrain(
            produced_text="the picnic was cancelled and the rain kept falling",
            confidence=0.3)
        result = cf.run_counterfactual_drip(
            b, stage=2, composer=None, num_probes=1, scenario_index=0)
        self.assertEqual(len(result), 1)
        self.assertLess(result[0]["composite"], cf.COUNTERFACTUAL_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        # Re-anchor: same string for both args, contains the prompt and
        # the expected keywords.
        text, target = b.train_calls[0]
        self.assertEqual(text, target)
        self.assertIn("had not happened", text)
        # Each expected keyword should be embedded in the reanchor.
        for kw in result[0]["expected_keywords"]:
            self.assertIn(kw, text)

    def test_produce_failure_caught_continues(self):
        # First produce_text raises; the loop should swallow the failure
        # and rotate to the next probe type rather than crashing or
        # short-circuiting the entire drip.
        class BoomBrain(FakeBrain):
            def __init__(self):
                super().__init__()
                self.calls = 0
            def produce_text(self, intent):
                self.calls += 1
                if self.calls == 1:
                    raise RuntimeError("simulated produce failure")
                return {"text": "they enjoyed a sunny outdoor picnic",
                        "confidence": 0.7}

        b = BoomBrain()
        result = cf.run_counterfactual_drip(
            b, stage=2, composer=None, num_probes=1, scenario_index=0)
        # First probe failed; second probe (different probe_type) succeeds.
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        # Two produce_text invocations: the failed one and the recovery.
        self.assertEqual(b.calls, 2)
        # Successful result should NOT be the first probe type since it
        # failed — rotation moved on.
        self.assertEqual(result[0]["probe_type"], "if_different_action")

    def test_rotation_advances_module_counter(self):
        cf._ROTATION_COUNTER[2] = 0
        b = FakeBrain()
        cf.run_counterfactual_drip(b, stage=2, composer=None, num_probes=1)
        cf.run_counterfactual_drip(b, stage=2, composer=None, num_probes=1)
        self.assertEqual(cf._ROTATION_COUNTER[2], 2)

    def test_explicit_scenario_index_does_not_bump_counter(self):
        cf._ROTATION_COUNTER[2] = 7
        b = FakeBrain()
        cf.run_counterfactual_drip(
            b, stage=2, composer=None, num_probes=1, scenario_index=99)
        self.assertEqual(cf._ROTATION_COUNTER[2], 7)


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
        cf.run_counterfactual_drip(
            b, stage=2, composer=c, num_probes=1, scenario_index=0)
        self.assertEqual(len(c.calls), 1)
        self.assertEqual(c.calls[0][1], "text")
        # The composed text should be the probe prompt.
        self.assertIn("had not happened", c.calls[0][0])

    def test_composer_failure_falls_back_to_zeros(self):
        class BoomComposer:
            def compose(self, text=None, modality="text"):
                raise RuntimeError("boom")

        b = FakeBrain()
        result = cf.run_counterfactual_drip(
            b, stage=2, composer=BoomComposer(), num_probes=1, scenario_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        self.assertEqual(len(b.produce_calls), 1)


if __name__ == "__main__":
    unittest.main()
