"""Unit + regression tests for the caregiver critic.

Slice C of the Option 1 architectural rebuild. The critic is the external
reward signal that replaces ``cascade_self_train`` auto-confirmation — these
tests guard the rule surface that drives reward into the SNN.

Run standalone::

    python3 scripts/test_caregiver_critic.py

Or via pytest::

    pytest scripts/test_caregiver_critic.py -v
"""
from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

# Ensure scripts/ is importable when run from any cwd.
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from caregiver_critic import CaregiverCritic, CriticVerdict  # noqa: E402


class TestStage0(unittest.TestCase):
    """Stage 0 (holophrastic, 12-18 mo): exactly one in-vocab word."""

    @classmethod
    def setUpClass(cls):
        cls.critic = CaregiverCritic(stage=0)

    def test_correct_single_word_gets_full_reward(self):
        """+1.0 reward when response matches the semantic hint."""
        v = self.critic.evaluate("what color is grass?", "green")
        self.assertEqual(v.reward, 1.0)
        self.assertIsNone(v.recast)
        self.assertIn("correct_answer", v.reason)

    def test_in_vocab_wrong_referent_partial_credit(self):
        """+0.3 when single word is in vocab but not the expected hint."""
        v = self.critic.evaluate("what color is grass?", "red")
        self.assertEqual(v.reward, 0.3)
        self.assertEqual(v.recast, "green")
        self.assertIn("in_vocab_wrong_referent", v.reason)

    def test_multiword_response_punished(self):
        """Stage 0 is one-word only — two+ words is too advanced."""
        v = self.critic.evaluate("what color is grass?", "the grass")
        self.assertLess(v.reward, 0.0)
        # All words must be in vocab to qualify as "too_many_words" vs OOV.
        # "the" is out-of-vocab for stage 0 — that lands us in OOV branch.
        self.assertIn("out_of_stage_vocab", v.reason)

    def test_pure_multiword_in_vocab_punished_for_count(self):
        """Two in-vocab words at stage 0 → -0.5 (too advanced, not OOV)."""
        v = self.critic.evaluate("what color is grass?", "big dog")
        self.assertEqual(v.reward, -0.5)
        self.assertIn("too_many_words", v.reason)

    def test_out_of_vocab_punished(self):
        """-1.0 when response contains words outside the 50-word stage 0 list."""
        v = self.critic.evaluate("what color is grass?", "the grass is verdant")
        self.assertEqual(v.reward, -1.0)
        self.assertIn("out_of_stage_vocab", v.reason)
        # Should still suggest the right recast when a hint exists.
        self.assertEqual(v.recast, "green")

    def test_empty_response_is_neutral(self):
        """Empty/whitespace → 0.0 reward, no recast — no DA delivered."""
        v = self.critic.evaluate("what color is grass?", "")
        self.assertEqual(v.reward, 0.0)
        self.assertEqual(v.reason, "empty_response")

    def test_punctuation_stripped(self):
        """Trailing punctuation is stripped before vocab check."""
        v = self.critic.evaluate("what color is grass?", "green.")
        self.assertEqual(v.reward, 1.0)


class TestStage1(unittest.TestCase):
    """Stage 1 (two-word, 18-24 mo): valid template + correct order."""

    @classmethod
    def setUpClass(cls):
        cls.critic = CaregiverCritic(stage=1)

    def test_agent_action_template_rewarded(self):
        """+1.0 for ``dog run`` (AGENT + ACTION)."""
        v = self.critic.evaluate("what is the dog doing?", "dog run")
        self.assertEqual(v.reward, 1.0)
        self.assertIsNone(v.recast)
        self.assertEqual(v.reason, "valid_template")

    def test_modifier_noun_template_rewarded(self):
        """+1.0 for ``big dog`` (MODIFIER + NOUN)."""
        v = self.critic.evaluate("describe the dog", "big dog")
        self.assertEqual(v.reward, 1.0)

    def test_action_patient_template_rewarded(self):
        """+1.0 for ``eat food`` (ACTION + PATIENT)."""
        v = self.critic.evaluate("what to do?", "eat food")
        self.assertEqual(v.reward, 1.0)

    def test_possessor_possessed_template_rewarded(self):
        """+1.0 for ``mama hat`` (POSSESSOR + POSSESSED)."""
        v = self.critic.evaluate("whose hat?", "mama hat")
        self.assertEqual(v.reward, 1.0)

    def test_word_order_error_recast(self):
        """-1.0 for ``run dog`` with recast=``dog run``."""
        v = self.critic.evaluate("what is the dog doing?", "run dog")
        self.assertEqual(v.reward, -1.0)
        self.assertEqual(v.recast, "dog run")
        self.assertIn("word_order_error", v.reason)

    def test_one_word_at_stage_1_punished(self):
        """Stage 1 floor is two content words — a single word gets -0.5."""
        v = self.critic.evaluate("what is the dog doing?", "dog")
        self.assertEqual(v.reward, -0.5)
        self.assertIn("below_two_word_floor", v.reason)

    def test_out_of_vocab_word_punished(self):
        """-1.0 when any word falls outside the 200-word stage 1 vocab."""
        v = self.critic.evaluate("what is happening?", "verdant grass")
        self.assertEqual(v.reward, -1.0)
        self.assertIn("out_of_stage_vocab", v.reason)

    # --- floor-not-ceiling: longer utterances are progress, not punished ---

    def test_copula_sentence_rewarded(self):
        """'the grass is green' — function words allowed, copula → SVO check.

        This is developmental PROGRESS beyond two-word telegraphic and must
        be rewarded, not punished for length or for using 'the'/'is'.
        """
        v = self.critic.evaluate("what color is grass?", "the grass is green")
        self.assertEqual(v.reward, 1.0)
        self.assertEqual(v.reason, "valid_svo")

    def test_three_content_words_rewarded(self):
        """'dog eat food' (3 content words, no copula) → rewarded SVO."""
        v = self.critic.evaluate("what is the dog doing?", "dog eat food")
        self.assertEqual(v.reward, 1.0)
        self.assertEqual(v.reason, "valid_svo")

    def test_longer_grammatical_not_punished_for_length(self):
        """A 3+ word in-vocab grammatical utterance is not punished at stage 1."""
        v = self.critic.evaluate("describe the dog", "the big dog run")
        self.assertGreater(v.reward, 0.0)

    def test_copula_inversion_punished_with_recast(self):
        """'the green is grass' → -1.0 + recast 'grass is green' (subject first)."""
        v = self.critic.evaluate("what color is grass?", "the green is grass")
        self.assertEqual(v.reward, -1.0)
        self.assertIsNotNone(v.recast)
        self.assertEqual(v.recast.split()[0], "grass")
        self.assertIn("green", v.recast)

    def test_two_word_telegraphic_still_rewarded(self):
        """Clean two-word telegraphic speech still uses template grammar."""
        v = self.critic.evaluate("what is the dog doing?", "dog run")
        self.assertEqual(v.reward, 1.0)
        self.assertEqual(v.reason, "valid_template")

    def test_function_words_only_below_floor(self):
        """A response of only function words has 0 content words → below floor."""
        v = self.critic.evaluate("what is happening?", "the is a")
        self.assertEqual(v.reward, -0.5)
        self.assertIn("below_two_word_floor", v.reason)

    def test_recast_uses_only_in_vocab_words(self):
        """Recasts must be grammatical AND in the stage's vocab."""
        v = self.critic.evaluate("what is the dog doing?", "run dog")
        self.assertIsNotNone(v.recast)
        recast_tokens = v.recast.split()
        for tok in recast_tokens:
            self.assertIn(tok, self.critic._vocab,
                          f"recast token {tok!r} not in stage 1 vocab")


class TestStage2(unittest.TestCase):
    """Stage 2 (telegraphic, 24-36 mo): 3-5 content words in SVO order."""

    @classmethod
    def setUpClass(cls):
        cls.critic = CaregiverCritic(stage=2)

    def test_svo_declarative_rewarded(self):
        """+1.0 for ``grass is green``."""
        v = self.critic.evaluate("what color is grass?", "grass is green")
        self.assertEqual(v.reward, 1.0)
        self.assertEqual(v.reason, "valid_svo")

    def test_svo_with_object_rewarded(self):
        """+1.0 for ``dog eat food``."""
        v = self.critic.evaluate("what is the dog doing?", "dog eat food")
        self.assertEqual(v.reward, 1.0)

    def test_inverted_interrogative_recast(self):
        """``is grass green`` for a declarative prompt → recast ``grass is green``."""
        v = self.critic.evaluate("what color is grass?", "is grass green")
        self.assertEqual(v.reward, -1.0)
        self.assertEqual(v.recast, "grass is green")
        self.assertIn("interrogative_for_declarative", v.reason)

    def test_subject_predicate_inversion_recast(self):
        """``the green is grass`` → recast ``grass is green``."""
        v = self.critic.evaluate("what color is grass?", "the green is grass")
        self.assertEqual(v.reward, -1.0)
        # Recast should reorder modifier + subject.
        self.assertIsNotNone(v.recast)
        self.assertIn("grass", v.recast)
        self.assertIn("green", v.recast)
        # Subject must come first.
        recast_tokens = v.recast.split()
        self.assertEqual(recast_tokens[0], "grass")

    def test_out_of_stage_vocab_punished(self):
        """Vocab outside stage 2's 500-word list → -1.0."""
        # "verdant" is not in the stage 2 vocab.
        v = self.critic.evaluate("what color is grass?", "grass is verdant")
        self.assertEqual(v.reward, -1.0)
        self.assertIn("out_of_stage_vocab", v.reason)

    def test_function_words_allowed(self):
        """Function words like ``the`` count as in-vocab at stage 2."""
        v = self.critic.evaluate("what is the dog doing?", "the dog is running")
        # "running" might not be in vocab (we have "run") — accept either
        # a positive verdict or an OOV verdict, but NOT a structural failure.
        if v.reward < 0:
            self.assertIn("out_of_stage_vocab", v.reason)
        else:
            self.assertGreater(v.reward, 0.0)

    def test_too_few_words_punished(self):
        """Stage 2 requires at least 3 words — fewer gets -0.5."""
        v = self.critic.evaluate("what color is grass?", "grass green")
        self.assertEqual(v.reward, -0.5)
        self.assertIn("wrong_word_count", v.reason)

    def test_too_many_words_punished(self):
        """Stage 2 caps at 5 words."""
        v = self.critic.evaluate(
            "what is happening?", "dog run fast and jump high"
        )
        self.assertLess(v.reward, 0.0)


class TestStageSwitching(unittest.TestCase):
    """``set_stage`` reloads vocab + rules without recreating the critic."""

    def test_stage_switch_reloads_vocab(self):
        critic = CaregiverCritic(stage=0)
        v0 = critic.evaluate("what is the dog doing?", "dog run")
        # Stage 0 rejects two-word "dog run" as too advanced.
        self.assertLess(v0.reward, 1.0)

        critic.set_stage(1)
        v1 = critic.evaluate("what is the dog doing?", "dog run")
        # Stage 1 rewards the same response.
        self.assertEqual(v1.reward, 1.0)

    def test_stage_3_falls_back_to_rule_based(self):
        """Stage 3 without LLM → rule-based (stage-2-style)."""
        critic = CaregiverCritic(stage=3)
        # Stage 3 reuses stage 2's vocab/rules in the absence of an LLM.
        v = critic.evaluate("what color is grass?", "grass is green")
        self.assertEqual(v.reward, 1.0)


class TestLLMCallable(unittest.TestCase):
    """Stage 3+ optional LLM hook."""

    def test_llm_called_for_stage_3plus(self):
        calls = []
        def fake_llm(prompt: str) -> str:
            calls.append(prompt)
            return "reward=0.8, recast=NONE, reason=looks_grammatical"

        critic = CaregiverCritic(stage=3, llm_callable=fake_llm)
        v = critic.evaluate("describe the scene", "the dog runs fast")
        self.assertEqual(len(calls), 1)
        self.assertAlmostEqual(v.reward, 0.8)
        self.assertIsNone(v.recast)
        self.assertEqual(v.reason, "looks_grammatical")

    def test_llm_exception_falls_back_to_rules(self):
        def broken_llm(prompt: str) -> str:
            raise RuntimeError("api down")

        critic = CaregiverCritic(stage=3, llm_callable=broken_llm)
        v = critic.evaluate("what color is grass?", "grass is green")
        # Fallback uses stage-2 rules.
        self.assertEqual(v.reward, 1.0)
        self.assertIn("llm_fallback", v.reason)

    def test_llm_negative_reward_parsed(self):
        def fake_llm(prompt: str) -> str:
            return "reward=-0.6, recast=the dog runs, reason=agreement_error"
        critic = CaregiverCritic(stage=3, llm_callable=fake_llm)
        v = critic.evaluate("what is the dog doing?", "the dogs runs")
        self.assertAlmostEqual(v.reward, -0.6)
        self.assertEqual(v.recast, "the dog runs")


class TestRecastGrammaticality(unittest.TestCase):
    """Recasts must be developmentally plausible (right vocab + structure)."""

    def test_stage_0_recast_is_single_word(self):
        critic = CaregiverCritic(stage=0)
        v = critic.evaluate("what color is grass?", "verdant")
        if v.recast is not None:
            self.assertEqual(len(v.recast.split()), 1,
                             "stage 0 recast must be one word")

    def test_stage_1_recast_is_two_words(self):
        critic = CaregiverCritic(stage=1)
        v = critic.evaluate("what is the dog doing?", "run dog")
        self.assertIsNotNone(v.recast)
        self.assertEqual(len(v.recast.split()), 2)

    def test_stage_2_recast_uses_in_vocab(self):
        critic = CaregiverCritic(stage=2)
        v = critic.evaluate("what color is grass?", "is grass green")
        self.assertIsNotNone(v.recast)
        for tok in v.recast.split():
            self.assertTrue(
                tok in critic._vocab or tok in critic._function_words,
                f"stage 2 recast token {tok!r} not in vocab/function words"
            )


class TestVerdictDataclass(unittest.TestCase):
    """``CriticVerdict`` field semantics."""

    def test_verdict_fields(self):
        v = CriticVerdict(reward=0.5, recast="dog run", reason="test")
        self.assertEqual(v.reward, 0.5)
        self.assertEqual(v.recast, "dog run")
        self.assertEqual(v.reason, "test")

    def test_verdict_reward_in_range(self):
        """Critic outputs must respect the [-1.0, +1.0] contract."""
        critic = CaregiverCritic(stage=0)
        prompts_responses = [
            ("what color is grass?", "green"),
            ("what color is grass?", "red"),
            ("what color is grass?", "verdant"),
            ("what color is grass?", "the grass is verdant green"),
            ("what color is grass?", ""),
            ("anything", "mama"),
        ]
        for p, r in prompts_responses:
            v = critic.evaluate(p, r)
            self.assertGreaterEqual(v.reward, -1.0,
                                    f"({p!r}, {r!r}) reward {v.reward} < -1.0")
            self.assertLessEqual(v.reward, 1.0,
                                 f"({p!r}, {r!r}) reward {v.reward} > 1.0")


if __name__ == "__main__":
    unittest.main(verbosity=2)
