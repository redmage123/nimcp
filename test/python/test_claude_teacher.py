"""Unit tests for CE-19 — claude_teacher run_claude_teacher_drip surface.

ALL tests use a mocked Anthropic client. NO real API calls are made;
the `anthropic` package is not even required to run these tests (the
module is engineered to import cleanly without it).

Tests cover:
  - is_available() availability gating
  - generate_question / score_brain_response / build_corrective_exemplar
    happy paths, malformed JSON paths, exception paths
  - cost-control (cap, budget remaining, reset)
  - drip stage 1 / disabled / unavailable no-ops
  - drip happy path / low-score path / empty exemplar fallback
  - drip num_rounds loop / produce_text exception caught
  - composer integration / model + max_tokens propagation
  - topic rotation behavior
  - privacy assertion on Anthropic client kwargs
"""
import json
import os
import sys
import unittest
from unittest import mock

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import claude_teacher as ct  # noqa: E402


# ---------------------------------------------------------------------------
# Mocked Anthropic client
# ---------------------------------------------------------------------------

class _Block:
    def __init__(self, text):
        self.text = text


class _Resp:
    def __init__(self, text):
        self.content = [_Block(text)]


class FakeClient:
    """Mock that records every kwargs payload sent to .messages.create()
    and returns scripted text responses in order."""

    def __init__(self, scripted_responses=None, raise_with=None):
        # `scripted_responses` is a list of strings (raw response.text)
        # `raise_with` is an Exception instance the create() should raise.
        self.calls = []
        self._responses = list(scripted_responses or [])
        self._raise = raise_with
        self.messages = self  # alias — c.messages.create(...)

    def create(self, **kwargs):
        self.calls.append(kwargs)
        if self._raise is not None:
            raise self._raise
        if not self._responses:
            raise RuntimeError("FakeClient ran out of scripted responses")
        text = self._responses.pop(0)
        return _Resp(text)


# ---------------------------------------------------------------------------
# Fake brain (mirrors test_storytelling.FakeBrain)
# ---------------------------------------------------------------------------

class FakeBrain:
    def __init__(self, produced_text="The plant grows from a seed in the soil.",
                 confidence=0.6):
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
# Common test setup: reset module state between tests
# ---------------------------------------------------------------------------

class _ResetMixin:
    def setUp(self):
        ct.RESET_CALL_COUNTER()
        ct._TOPIC_COUNTER[1] = 0
        ct._TOPIC_COUNTER[2] = 0
        ct._TOPIC_COUNTER[3] = 0


# ---------------------------------------------------------------------------
# Availability
# ---------------------------------------------------------------------------

class TestAvailability(_ResetMixin, unittest.TestCase):
    def test_unavailable_when_anthropic_missing(self):
        with mock.patch.object(ct, "_ce19_try_import_anthropic",
                                return_value=None):
            with mock.patch.dict(os.environ, {"ANTHROPIC_API_KEY": "x"}):
                self.assertFalse(ct.is_available())

    def test_unavailable_when_key_missing(self):
        # Pretend anthropic is present but env key is not.
        with mock.patch.object(ct, "_ce19_try_import_anthropic",
                                return_value=mock.MagicMock()):
            env = {k: v for k, v in os.environ.items()
                   if k != "ANTHROPIC_API_KEY"}
            with mock.patch.dict(os.environ, env, clear=True):
                self.assertFalse(ct.is_available())

    def test_available_when_both_present(self):
        with mock.patch.object(ct, "_ce19_try_import_anthropic",
                                return_value=mock.MagicMock()):
            with mock.patch.dict(os.environ,
                                 {"ANTHROPIC_API_KEY": "sk-test"}):
                self.assertTrue(ct.is_available())


# ---------------------------------------------------------------------------
# generate_question
# ---------------------------------------------------------------------------

class TestGenerateQuestion(_ResetMixin, unittest.TestCase):
    def test_parses_valid_json(self):
        payload = json.dumps({"question": "Why is the sky blue?",
                              "expected_concepts": ["light", "scattering",
                                                     "atmosphere"]})
        c = FakeClient(scripted_responses=[payload])
        result = ct.generate_question("why the sky is blue", 2, client=c)
        self.assertIsNotNone(result)
        self.assertEqual(result["question"], "Why is the sky blue?")
        self.assertIn("light", result["expected_concepts"])
        self.assertEqual(len(c.calls), 1)

    def test_parses_json_with_code_fences(self):
        payload = ("```json\n"
                   + json.dumps({"question": "How do plants grow?",
                                 "expected_concepts": ["seed", "water"]})
                   + "\n```")
        c = FakeClient(scripted_responses=[payload])
        result = ct.generate_question("how plants grow", 2, client=c)
        self.assertIsNotNone(result)
        self.assertEqual(result["question"], "How do plants grow?")

    def test_returns_none_on_invalid_json(self):
        c = FakeClient(scripted_responses=["this is not JSON at all"])
        result = ct.generate_question("topic", 2, client=c)
        self.assertIsNone(result)

    def test_returns_none_when_question_missing(self):
        # JSON parses but question key is empty.
        payload = json.dumps({"expected_concepts": ["a", "b"]})
        c = FakeClient(scripted_responses=[payload])
        result = ct.generate_question("topic", 2, client=c)
        self.assertIsNone(result)

    def test_returns_none_on_client_exception(self):
        c = FakeClient(raise_with=RuntimeError("simulated 5xx"))
        result = ct.generate_question("topic", 2, client=c)
        self.assertIsNone(result)

    def test_empty_topic_short_circuits(self):
        c = FakeClient(scripted_responses=["unused"])
        result = ct.generate_question("", 2, client=c)
        self.assertIsNone(result)
        self.assertEqual(len(c.calls), 0)

    def test_concept_list_coerces_non_strings(self):
        payload = json.dumps({"question": "Q?",
                              "expected_concepts": [1, 2, "three", "", None]})
        c = FakeClient(scripted_responses=[payload])
        result = ct.generate_question("topic", 2, client=c)
        self.assertIsNotNone(result)
        # Non-strings stringified, blanks/Nones dropped.
        self.assertIn("1", result["expected_concepts"])
        self.assertIn("three", result["expected_concepts"])
        self.assertNotIn("", result["expected_concepts"])

    def test_model_and_max_tokens_propagate(self):
        payload = json.dumps({"question": "Q?", "expected_concepts": ["a"]})
        c = FakeClient(scripted_responses=[payload])
        ct.generate_question("topic", 2, client=c,
                             model="claude-opus-test",
                             max_tokens=128)
        self.assertEqual(c.calls[0]["model"], "claude-opus-test")
        self.assertEqual(c.calls[0]["max_tokens"], 128)
        # Question generation runs at temperature 0.5 per spec.
        self.assertEqual(c.calls[0]["temperature"], 0.5)


# ---------------------------------------------------------------------------
# score_brain_response
# ---------------------------------------------------------------------------

class TestScoreBrainResponse(_ResetMixin, unittest.TestCase):
    def test_parses_full_rubric(self):
        payload = json.dumps({"factuality": 0.7, "coherence": 0.8,
                              "on_topic": 0.6, "composite": 0.7,
                              "rationale": "Mostly correct."})
        c = FakeClient(scripted_responses=[payload])
        result = ct.score_brain_response("Q?", "A.", ["x"], client=c)
        self.assertIsNotNone(result)
        self.assertAlmostEqual(result["factuality"], 0.7)
        self.assertAlmostEqual(result["coherence"], 0.8)
        self.assertAlmostEqual(result["on_topic"], 0.6)
        self.assertAlmostEqual(result["composite"], 0.7)
        self.assertEqual(result["rationale"], "Mostly correct.")

    def test_clips_out_of_range_values(self):
        payload = json.dumps({"factuality": 1.5, "coherence": -0.5,
                              "on_topic": 0.5, "composite": 2.0})
        c = FakeClient(scripted_responses=[payload])
        result = ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertEqual(result["factuality"], 1.0)
        self.assertEqual(result["coherence"], 0.0)
        self.assertEqual(result["composite"], 1.0)

    def test_missing_keys_default_zero(self):
        # Only rationale provided — every score axis defaults to 0.0.
        payload = json.dumps({"rationale": "incomplete"})
        c = FakeClient(scripted_responses=[payload])
        result = ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertIsNotNone(result)
        self.assertEqual(result["factuality"], 0.0)
        self.assertEqual(result["coherence"], 0.0)
        self.assertEqual(result["on_topic"], 0.0)
        # No composite -> derived from the three zero axes.
        self.assertEqual(result["composite"], 0.0)

    def test_returns_none_on_client_exception(self):
        c = FakeClient(raise_with=RuntimeError("rate limit"))
        result = ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertIsNone(result)

    def test_returns_none_on_invalid_json(self):
        c = FakeClient(scripted_responses=["not json"])
        result = ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertIsNone(result)

    def test_temperature_zero_for_scoring(self):
        payload = json.dumps({"composite": 0.5})
        c = FakeClient(scripted_responses=[payload])
        ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertEqual(c.calls[0]["temperature"], 0.0)


# ---------------------------------------------------------------------------
# build_corrective_exemplar
# ---------------------------------------------------------------------------

class TestBuildCorrectiveExemplar(_ResetMixin, unittest.TestCase):
    def test_returns_text_body(self):
        c = FakeClient(scripted_responses=[
            "Plants grow from seeds. They need water and sun."])
        result = ct.build_corrective_exemplar("How do plants grow?",
                                              "asdf", ["seed", "water"],
                                              client=c)
        self.assertEqual(result,
                         "Plants grow from seeds. They need water and sun.")

    def test_strips_code_fences(self):
        c = FakeClient(scripted_responses=[
            "```\nThe sky is blue because of light scattering.\n```"])
        result = ct.build_corrective_exemplar("Q?", "?", ["light"], client=c)
        self.assertEqual(result,
                         "The sky is blue because of light scattering.")

    def test_returns_none_on_exception(self):
        c = FakeClient(raise_with=RuntimeError("network"))
        result = ct.build_corrective_exemplar("Q?", "A.", [], client=c)
        self.assertIsNone(result)

    def test_returns_none_for_empty_response(self):
        c = FakeClient(scripted_responses=["   \n  "])
        result = ct.build_corrective_exemplar("Q?", "A.", [], client=c)
        self.assertIsNone(result)


# ---------------------------------------------------------------------------
# Cost control
# ---------------------------------------------------------------------------

class TestCostControl(_ResetMixin, unittest.TestCase):
    def test_call_budget_remaining_decrements(self):
        self.assertEqual(ct.CALL_BUDGET_REMAINING(),
                         ct.MAX_CALLS_PER_SESSION)
        payload = json.dumps({"question": "Q?", "expected_concepts": []})
        c = FakeClient(scripted_responses=[payload])
        ct.generate_question("topic", 2, client=c)
        self.assertEqual(ct.CALL_BUDGET_REMAINING(),
                         ct.MAX_CALLS_PER_SESSION - 1)

    def test_reset_call_counter(self):
        ct._CALL_COUNTER = 50
        ct.RESET_CALL_COUNTER()
        self.assertEqual(ct._CALL_COUNTER, 0)
        self.assertEqual(ct.CALL_BUDGET_REMAINING(),
                         ct.MAX_CALLS_PER_SESSION)

    def test_cap_short_circuits_without_calling_mock(self):
        ct._CALL_COUNTER = ct.MAX_CALLS_PER_SESSION
        c = FakeClient(scripted_responses=["should not be used"])
        # generate_question should short-circuit BEFORE touching the mock.
        result = ct.generate_question("topic", 2, client=c)
        self.assertIsNone(result)
        self.assertEqual(len(c.calls), 0)
        # Same for score and exemplar.
        result2 = ct.score_brain_response("Q?", "A.", [], client=c)
        self.assertIsNone(result2)
        result3 = ct.build_corrective_exemplar("Q?", "A.", [], client=c)
        self.assertIsNone(result3)
        self.assertEqual(len(c.calls), 0)

    def test_failed_call_still_bumps_counter(self):
        # Conservative billing: failed API calls still consume budget.
        c = FakeClient(raise_with=RuntimeError("5xx"))
        before = ct._CALL_COUNTER
        ct.generate_question("topic", 2, client=c)
        self.assertEqual(ct._CALL_COUNTER, before + 1)


# ---------------------------------------------------------------------------
# Drip — control flow
# ---------------------------------------------------------------------------

def _patch_available(value=True):
    """Helper: patch is_available() to a fixed truthy/falsy value."""
    return mock.patch.object(ct, "is_available", return_value=value)


class TestDripControlFlow(_ResetMixin, unittest.TestCase):
    def test_stage_1_is_noop(self):
        b = FakeBrain()
        with _patch_available(True):
            result = ct.run_claude_teacher_drip(b, stage=1)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])

    def test_disabled_is_noop(self):
        b = FakeBrain()
        with _patch_available(True):
            result = ct.run_claude_teacher_drip(b, stage=2, enabled=False)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])

    def test_unavailable_is_noop_and_logs_once(self):
        b = FakeBrain()
        with _patch_available(False):
            r1 = ct.run_claude_teacher_drip(b, stage=2)
            r2 = ct.run_claude_teacher_drip(b, stage=2)
        self.assertIsNone(r1)
        self.assertIsNone(r2)
        # _UNAVAILABLE_LOG_EMITTED should have been set after first call.
        self.assertTrue(ct._UNAVAILABLE_LOG_EMITTED)

    def test_cap_short_circuits_drip(self):
        b = FakeBrain()
        ct._CALL_COUNTER = ct.MAX_CALLS_PER_SESSION
        with _patch_available(True):
            result = ct.run_claude_teacher_drip(b, stage=2)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])


# ---------------------------------------------------------------------------
# Drip — happy path / corrective path / fallback
# ---------------------------------------------------------------------------

def _high_score_payload(composite=0.85):
    return json.dumps({"factuality": composite, "coherence": composite,
                       "on_topic": composite, "composite": composite,
                       "rationale": "looks good"})


def _low_score_payload(composite=0.1):
    return json.dumps({"factuality": composite, "coherence": composite,
                       "on_topic": composite, "composite": composite,
                       "rationale": "too vague"})


def _question_payload(q="What is a seed?",
                      concepts=("seed", "plant", "soil")):
    return json.dumps({"question": q, "expected_concepts": list(concepts)})


class TestDripHappyPath(_ResetMixin, unittest.TestCase):
    def test_high_score_calls_learn_language(self):
        b = FakeBrain(produced_text="A seed grows into a plant in soil.")
        # Two scripted calls per round: question + score (no exemplar
        # needed when composite >= threshold).
        scripted = [_question_payload(), _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        # Inject the mock client into both API entry points by patching
        # _ce19_build_client to return our shared FakeClient.
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                                topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["feedback_path"], "positive")
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(b.learn_calls[0],
                         "A seed grows into a plant in soil.")
        self.assertEqual(len(b.train_calls), 0)
        self.assertEqual(len(b.produce_calls), 1)


class TestDripLowScorePath(_ResetMixin, unittest.TestCase):
    def test_low_score_uses_corrective_exemplar(self):
        b = FakeBrain(produced_text="zzzzzz")
        scripted = [_question_payload(),
                    _low_score_payload(0.1),
                    "Plants grow from seeds. They need water and sun."]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                                topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(result[0]["feedback_path"], "exemplar")
        self.assertEqual(len(b.train_calls), 1)
        text, target = b.train_calls[0]
        self.assertEqual(text, target)  # exemplar paired with itself
        self.assertIn("seeds", text.lower())
        self.assertEqual(len(b.learn_calls), 0)


class TestDripQuestionAnchorFallback(_ResetMixin, unittest.TestCase):
    def test_empty_exemplar_falls_back_to_question_anchor(self):
        b = FakeBrain(produced_text="zzzzzz")
        scripted = [_question_payload("Why do leaves fall?"),
                    _low_score_payload(0.1),
                    "   "]  # exemplar comes back blank
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                                topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(result[0]["feedback_path"], "question_anchor")
        self.assertEqual(len(b.train_calls), 1)
        text, target = b.train_calls[0]
        self.assertEqual(text, target)
        self.assertEqual(text, "Why do leaves fall?")


# ---------------------------------------------------------------------------
# Drip — multi-round + exception tolerance
# ---------------------------------------------------------------------------

class TestDripMultiRound(_ResetMixin, unittest.TestCase):
    def test_num_rounds_two_loops(self):
        b = FakeBrain(produced_text="A seed grows into a plant.")
        # Two rounds * two API calls each (question + score, both pass).
        scripted = [_question_payload(),
                    _high_score_payload(0.9),
                    _question_payload(q="What is soil made of?"),
                    _high_score_payload(0.85)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, num_rounds=2,
                                                topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 2)
        self.assertEqual(len(b.produce_calls), 2)
        self.assertEqual(len(b.learn_calls), 2)


class TestDripProduceTextException(_ResetMixin, unittest.TestCase):
    def test_produce_text_exception_caught(self):
        # produce_text raises in round 1 -> drip continues to round 2.
        class BoomThenOK(FakeBrain):
            def __init__(self):
                super().__init__(produced_text="A plant grows from seed.")
                self._first = True

            def produce_text(self, intent):
                if self._first:
                    self._first = False
                    raise RuntimeError("simulated produce failure")
                return super().produce_text(intent)

        b = BoomThenOK()
        # Round 1: question is consumed, but produce raises before score
        # is ever called. Round 2: question + score consumed normally.
        scripted = [_question_payload(),
                    _question_payload(q="Round 2 Q"),
                    _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, num_rounds=2,
                                                topic_index=0)
        # We expect 1 successful round (round 2). Round 1 was skipped.
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        self.assertEqual(len(b.learn_calls), 1)


# ---------------------------------------------------------------------------
# Composer integration
# ---------------------------------------------------------------------------

class _FakeComposer:
    def __init__(self):
        self.calls = []

    def compose(self, text=None, modality="text"):
        self.calls.append((text, modality))
        import numpy as np
        return np.zeros(1024, dtype=np.float32)


class TestComposer(_ResetMixin, unittest.TestCase):
    def test_composer_used_when_given(self):
        b = FakeBrain(produced_text="A.")
        comp = _FakeComposer()
        scripted = [_question_payload(), _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            ct.run_claude_teacher_drip(b, stage=2, composer=comp,
                                       num_rounds=1, topic_index=0)
        self.assertEqual(len(comp.calls), 1)
        self.assertEqual(comp.calls[0][1], "text")

    def test_composer_none_falls_back_to_zeros(self):
        b = FakeBrain(produced_text="A.")
        scripted = [_question_payload(), _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            result = ct.run_claude_teacher_drip(b, stage=2, composer=None,
                                                num_rounds=1, topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(b.produce_calls), 1)
        # The fallback intent vector is 1024 zeros.
        self.assertEqual(len(b.produce_calls[0]), 1024)


# ---------------------------------------------------------------------------
# Topic rotation
# ---------------------------------------------------------------------------

class TestTopicRotation(_ResetMixin, unittest.TestCase):
    def test_implicit_rotation_advances_counter(self):
        b = FakeBrain(produced_text="A.")
        scripted = []
        # Two rounds across two drip calls -> two implicit topic bumps.
        for _ in range(2):
            scripted.extend([_question_payload(), _high_score_payload(0.9)])
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            ct.run_claude_teacher_drip(b, stage=2, num_rounds=1)
            ct.run_claude_teacher_drip(b, stage=2, num_rounds=1)
        self.assertEqual(ct._TOPIC_COUNTER[2], 2)

    def test_explicit_topic_index_does_not_bump(self):
        b = FakeBrain(produced_text="A.")
        ct._TOPIC_COUNTER[2] = 7
        scripted = [_question_payload(), _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                       topic_index=3)
        # Counter unchanged because index was explicit.
        self.assertEqual(ct._TOPIC_COUNTER[2], 7)


# ---------------------------------------------------------------------------
# Privacy
# ---------------------------------------------------------------------------

class TestPrivacy(_ResetMixin, unittest.TestCase):
    """Audit-style check: every kwargs payload sent to the Anthropic
    client must contain ONLY public-text content. No keys or values
    referencing checkpoint paths, weights, synapse data, neuron state."""

    FORBIDDEN_SUBSTRINGS = ("checkpoint", "weights", "synapse", "neuron",
                            "/var/run/athena", "athena_auto_", "tau_base",
                            "g_exc", "g_inh", "membrane")

    def _flatten(self, obj):
        """Yield (key, value) pairs across nested dicts/lists/strings.
        Used to scan an entire kwargs payload for forbidden substrings."""
        if isinstance(obj, dict):
            for k, v in obj.items():
                yield (str(k), v)
                yield from self._flatten(v)
        elif isinstance(obj, (list, tuple)):
            for v in obj:
                yield from self._flatten(v)

    def test_no_forbidden_terms_in_api_payload(self):
        b = FakeBrain(produced_text="A plant grows from a seed.")
        scripted = [_question_payload(),
                    _low_score_payload(0.1),
                    "Plants grow from seeds with water and light."]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                       topic_index=0)

        # 3 calls: question, score, exemplar.
        self.assertEqual(len(c.calls), 3)
        for call in c.calls:
            # Inspect every key + every leaf value we can stringify.
            for k, v in self._flatten(call):
                key_str = k.lower()
                for forbidden in self.FORBIDDEN_SUBSTRINGS:
                    self.assertNotIn(
                        forbidden, key_str,
                        f"Forbidden substring {forbidden!r} found in API "
                        f"kwarg key: {k}")
                if isinstance(v, str):
                    val_str = v.lower()
                    for forbidden in self.FORBIDDEN_SUBSTRINGS:
                        self.assertNotIn(
                            forbidden, val_str,
                            f"Forbidden substring {forbidden!r} found in "
                            f"API kwarg value: {v}")

    def test_only_public_kwargs_used(self):
        # Spec compliance: every API call passes ONLY model, max_tokens,
        # temperature, system, messages. No backdoor channels.
        b = FakeBrain(produced_text="A.")
        scripted = [_question_payload(), _high_score_payload(0.9)]
        c = FakeClient(scripted_responses=scripted)
        with _patch_available(True), \
             mock.patch.object(ct, "_ce19_build_client",
                               return_value=c):
            ct.run_claude_teacher_drip(b, stage=2, num_rounds=1,
                                       topic_index=0)
        allowed = {"model", "max_tokens", "temperature", "system",
                   "messages"}
        for call in c.calls:
            self.assertTrue(set(call.keys()).issubset(allowed),
                            f"Unexpected kwargs: {set(call.keys()) - allowed}")


# ---------------------------------------------------------------------------
# Smoke test: module-level constants
# ---------------------------------------------------------------------------

class TestModuleConstants(unittest.TestCase):
    def test_pass_threshold_is_half(self):
        self.assertAlmostEqual(ct.TEACHER_PASS_THRESHOLD, 0.5)

    def test_topic_seeds_have_stages_2_and_3(self):
        self.assertEqual(ct.TEACHER_TOPIC_SEEDS[1], [])
        self.assertGreaterEqual(len(ct.TEACHER_TOPIC_SEEDS[2]), 6)
        self.assertGreaterEqual(len(ct.TEACHER_TOPIC_SEEDS[3]), 6)

    def test_max_calls_default(self):
        # Default is 200 unless env override is set in this process.
        self.assertGreaterEqual(ct.MAX_CALLS_PER_SESSION, 1)


if __name__ == "__main__":
    unittest.main()
