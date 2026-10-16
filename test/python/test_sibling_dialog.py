"""Unit tests for scripts/sibling_dialog.py — CE-3 self-play / sibling
dialog production + turn-taking coherence scoring.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import sibling_dialog as sd  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path. By default, returns the same fixed reply for every
    turn — tests that need varying replies pass a list to `replies`.
    """

    def __init__(self, produced_text="The cat watches the bird outside.",
                 confidence=0.7, replies=None):
        self._produced = produced_text
        self._confidence = confidence
        self._replies = list(replies) if replies is not None else None
        self._reply_idx = 0
        self.produce_calls = []
        self.learn_calls = []
        self.train_calls = []

    def produce_text(self, intent):
        self.produce_calls.append(list(intent) if intent is not None else None)
        if self._replies is not None:
            text = self._replies[self._reply_idx % len(self._replies)]
            self._reply_idx += 1
        else:
            text = self._produced
        return {"text": text, "confidence": self._confidence,
                "success": True}

    def learn_language(self, text):
        self.learn_calls.append(text)

    def train_language(self, text, target_text):
        self.train_calls.append((text, target_text))


class FakeComposer:
    """Mirrors the storytelling test fake — captures calls + returns a
    zero numpy vector."""

    def __init__(self):
        self.calls = []

    def compose(self, text=None, modality="text"):
        self.calls.append((text, modality))
        import numpy as np
        return np.zeros(1024, dtype=np.float32)


# ---------------------------------------------------------------------------
# Sibling response templates
# ---------------------------------------------------------------------------

class TestSiblingResponse(unittest.TestCase):
    def test_mirror_takes_first_six_content_words(self):
        out = sd.sibling_response(
            "The cat watches the bird outside the lonely window at dusk.",
            "the cat that watches the bird", "mirror")
        # Content words: cat, watches, bird, outside, lonely, window, dusk
        # First 6: cat watches bird outside lonely window
        self.assertTrue(out.startswith("Yes, and "))
        self.assertIn("cat watches bird outside lonely window", out)
        self.assertTrue(out.endswith("."))

    def test_ask_why_uses_first_content_word(self):
        out = sd.sibling_response(
            "The dog ran across the field.",
            "the dog running in the field", "ask_why")
        # First content word of brain_line: dog
        self.assertEqual(out, "Why dog?")

    def test_extend_takes_last_eight_content_words(self):
        out = sd.sibling_response(
            ("The cat watches the bird and the bird watches the cat "
             "and they sit there for hours."),
            "the cat that watches the bird", "extend")
        # Content words tail (8): bird watches cat sit there hours
        # (after stoplist + len>2 filter). Just assert shape + suffix.
        self.assertTrue(out.startswith("And then "))
        self.assertTrue(out.endswith("."))
        self.assertIn("hours", out)

    def test_doubt_uses_first_content_word(self):
        out = sd.sibling_response(
            "The cat watches the bird.",
            "the cat that watches the bird", "doubt")
        self.assertEqual(out, "Are you sure about cat?")

    def test_doubt_falls_back_to_topic_when_no_content_words(self):
        # brain_line has no content words (all stop words / < 3 chars)
        # but is non-empty → doubt should fall back to the topic.
        out = sd.sibling_response("the of in", "the cat that watches the bird",
                                  "doubt")
        self.assertEqual(out, "Are you sure about the cat that watches the bird?")

    def test_name_it_returns_topic_question(self):
        out = sd.sibling_response("anything you like here",
                                  "the small stone shaped like a heart",
                                  "name_it")
        self.assertEqual(out, "the small stone shaped like a heart?")

    def test_empty_brain_line_falls_back_to_name_it(self):
        # Even when caller asks for `mirror`, an empty brain_line means
        # the only sane move is to re-anchor on the topic.
        for move in sd.SIBLING_MOVES:
            out = sd.sibling_response("", "the lantern in the dark forest", move)
            self.assertEqual(out, "the lantern in the dark forest?")

        # Whitespace-only is also empty.
        out = sd.sibling_response("   \t\n  ", "the cat that watches the bird",
                                  "mirror")
        self.assertEqual(out, "the cat that watches the bird?")


# ---------------------------------------------------------------------------
# Per-turn scoring
# ---------------------------------------------------------------------------

class TestScoreDialogTurn(unittest.TestCase):
    def test_continuity_rises_with_overlap(self):
        # Brain echoes the previous line → high continuity.
        s_high = sd.score_dialog_turn(
            prev_line="Yes, and cat watches bird outside.",
            brain_line="The cat watches the bird intently outside.",
            topic="the cat that watches the bird")
        s_low = sd.score_dialog_turn(
            prev_line="Yes, and cat watches bird outside.",
            brain_line="A spaceship hurtles past distant Jupiter.",
            topic="the cat that watches the bird")
        self.assertGreater(s_high["continuity"], s_low["continuity"])
        self.assertEqual(s_low["continuity"], 0.0)

    def test_topic_stability_rises_with_topic_words(self):
        s_on = sd.score_dialog_turn(
            prev_line="Are you sure about cat?",
            brain_line="The cat watches the bird at the window.",
            topic="the cat that watches the bird")
        s_off = sd.score_dialog_turn(
            prev_line="Are you sure about cat?",
            brain_line="A turbine spins silently in distant clouds.",
            topic="the cat that watches the bird")
        self.assertGreater(s_on["topic_stability"], s_off["topic_stability"])
        self.assertEqual(s_off["topic_stability"], 0.0)

    def test_composite_is_average_of_two(self):
        s = sd.score_dialog_turn(
            prev_line="cat watches bird",
            brain_line="The cat watches the bird outside.",
            topic="the cat that watches the bird")
        self.assertAlmostEqual(
            s["composite"],
            (s["continuity"] + s["topic_stability"]) / 2.0, places=6)

    def test_token_count_matches_tokens(self):
        s = sd.score_dialog_turn(
            prev_line="x",
            brain_line="One two three four five.",
            topic="something")
        self.assertEqual(s["token_count"], 5)

    def test_empty_brain_line_zero_components(self):
        s = sd.score_dialog_turn(
            prev_line="cat watches bird",
            brain_line="",
            topic="the cat that watches the bird")
        self.assertEqual(s["continuity"], 0.0)
        self.assertEqual(s["topic_stability"], 0.0)
        self.assertEqual(s["composite"], 0.0)
        self.assertEqual(s["token_count"], 0)

    def test_empty_prev_line_continuity_zero(self):
        # No previous content words → continuity is 0.
        s = sd.score_dialog_turn(
            prev_line="the and of in",   # all stoplist
            brain_line="cat watches bird",
            topic="the cat that watches the bird")
        self.assertEqual(s["continuity"], 0.0)


# ---------------------------------------------------------------------------
# Move rotation determinism
# ---------------------------------------------------------------------------

class TestMoveRotation(unittest.TestCase):
    def test_rotation_is_deterministic_across_turns(self):
        # We can observe the rotation by capturing the sibling lines from
        # a single drip call and reverse-engineering which template made
        # each one. Easier: assert that the first 5 sibling lines come
        # from the documented rotation order.
        b = FakeBrain(produced_text="The cat watches the bird outside.")
        # 6 turns → 5 sibling lines (turn 0 is the kickoff with no sibling).
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=6, topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(result["sibling_turns"]), 5)

        sib = result["sibling_turns"]
        # _MOVE_ROTATION is [mirror, ask_why, extend, name_it, doubt]
        self.assertTrue(sib[0].startswith("Yes, and "))         # mirror
        self.assertTrue(sib[1].startswith("Why "))               # ask_why
        self.assertTrue(sib[2].startswith("And then "))          # extend
        self.assertEqual(sib[3], "the cat that watches the bird?")  # name_it
        self.assertTrue(sib[3].endswith("?"))
        self.assertTrue(sib[4].startswith("Are you sure about "))  # doubt

    def test_rotation_wraps_after_five(self):
        # 7 turns → 6 sibling lines; the 6th wraps back to mirror.
        b = FakeBrain(produced_text="The dog runs across the field.")
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=7, topic_index=2)
        self.assertEqual(len(result["sibling_turns"]), 6)
        # 6th sibling line (index 5) should be mirror again.
        self.assertTrue(result["sibling_turns"][5].startswith("Yes, and "))


# ---------------------------------------------------------------------------
# Drip driver — control flow
# ---------------------------------------------------------------------------

class TestDripStage1Noop(unittest.TestCase):
    def test_stage_1_returns_none_no_calls(self):
        b = FakeBrain()
        result = sd.run_sibling_drip(b, stage=1, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])


class TestDripStage2(unittest.TestCase):
    def test_stage_2_produces_num_turns_brain_calls(self):
        b = FakeBrain(produced_text="The cat watches the bird outside.")
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=4, topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(result["stage"], 2)
        self.assertEqual(result["num_turns"], 4)
        self.assertEqual(len(b.produce_calls), 4)
        self.assertEqual(len(result["brain_turns"]), 4)

    def test_coherent_dialog_takes_positive_path(self):
        # Brain replies stay on-topic and echo the sibling → high
        # composite → learn_language called.
        b = FakeBrain(produced_text=(
            "The cat watches the bird at the window in the dark forest."))
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=4, topic_index=0)
        self.assertIsNotNone(result)
        self.assertGreaterEqual(result["composite"],
                                sd.COHERENCE_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(len(b.train_calls), 0)
        # learn_language receives the joined brain turns.
        self.assertIn("cat", b.learn_calls[0])
        self.assertIn("bird", b.learn_calls[0])

    def test_collapsed_dialog_takes_corrective_path(self):
        # Off-topic + repetitive → composite below threshold →
        # train_language(topic, topic) called.
        b = FakeBrain(produced_text="zzz zzz zzz zzz zzz zzz")
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=4, topic_index=0)
        self.assertIsNotNone(result)
        self.assertLess(result["composite"], sd.COHERENCE_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        topic = sd.TOPIC_SEEDS[2][0]
        # Re-anchor pair: (topic, topic).
        self.assertEqual(b.train_calls[0], (topic, topic))

    def test_produce_failure_in_one_turn_does_not_raise(self):
        class FlakyBrain(FakeBrain):
            def produce_text(self, intent):
                self.produce_calls.append(intent)
                # Fail on the second call only; succeed otherwise.
                if len(self.produce_calls) == 2:
                    raise RuntimeError("simulated produce failure")
                return {"text": "The cat watches the bird outside.",
                        "confidence": 0.7, "success": True}

        b = FlakyBrain()
        # Should not raise. The drip should keep going; result is not
        # None as long as at least one turn succeeded.
        result = sd.run_sibling_drip(b, stage=2, composer=None,
                                     num_turns=4, topic_index=0)
        self.assertIsNotNone(result)
        # 3 successful turns out of 4 attempts.
        self.assertEqual(result["num_turns"], 3)


class TestDripStage3(unittest.TestCase):
    def test_stage_3_picks_from_stage_3_topics(self):
        b = FakeBrain(produced_text="The promise was made and kept by them all.")
        result = sd.run_sibling_drip(b, stage=3, composer=None,
                                     num_turns=4, topic_index=1)
        self.assertIsNotNone(result)
        self.assertEqual(result["stage"], 3)
        self.assertEqual(result["topic"], sd.TOPIC_SEEDS[3][1])


# ---------------------------------------------------------------------------
# Composer integration
# ---------------------------------------------------------------------------

class TestComposerIntegration(unittest.TestCase):
    def test_composer_used_when_given(self):
        c = FakeComposer()
        b = FakeBrain()
        sd.run_sibling_drip(b, stage=2, composer=c, num_turns=3, topic_index=0)
        # composer.compose called once per brain turn (3 turns).
        self.assertEqual(len(c.calls), 3)
        # Modality is always "text".
        for _, modality in c.calls:
            self.assertEqual(modality, "text")
        # First call's text is the topic kickoff.
        self.assertEqual(c.calls[0][0], sd.TOPIC_SEEDS[2][0])
        # Subsequent calls include the topic anchor.
        self.assertIn(sd.TOPIC_SEEDS[2][0], c.calls[1][0])

    def test_composer_failure_falls_back_to_zeros(self):
        class BoomComposer:
            def compose(self, text=None, modality="text"):
                raise RuntimeError("boom")

        b = FakeBrain()
        # Should not raise — falls back to zero vector.
        result = sd.run_sibling_drip(b, stage=2, composer=BoomComposer(),
                                     num_turns=2, topic_index=0)
        self.assertIsNotNone(result)
        self.assertEqual(len(b.produce_calls), 2)


# ---------------------------------------------------------------------------
# Topic rotation counter
# ---------------------------------------------------------------------------

class TestTopicRotation(unittest.TestCase):
    def test_rotation_counter_advances(self):
        sd._TOPIC_ROTATION[2] = 0
        b = FakeBrain()
        sd.run_sibling_drip(b, stage=2, composer=None, num_turns=2)
        sd.run_sibling_drip(b, stage=2, composer=None, num_turns=2)
        self.assertEqual(sd._TOPIC_ROTATION[2], 2)

    def test_explicit_topic_index_does_not_bump_counter(self):
        sd._TOPIC_ROTATION[2] = 5
        b = FakeBrain()
        sd.run_sibling_drip(b, stage=2, composer=None, num_turns=2,
                            topic_index=99)
        self.assertEqual(sd._TOPIC_ROTATION[2], 5)


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------

class TestEdgeCases(unittest.TestCase):
    def test_empty_topic_list_for_stage_returns_none(self):
        # Artificially clear stage 2 topics, restore at end.
        original = sd.TOPIC_SEEDS[2]
        sd.TOPIC_SEEDS[2] = []
        try:
            b = FakeBrain()
            result = sd.run_sibling_drip(b, stage=2, composer=None)
            self.assertIsNone(result)
            self.assertEqual(b.produce_calls, [])
        finally:
            sd.TOPIC_SEEDS[2] = original

    def test_unknown_stage_returns_none(self):
        b = FakeBrain()
        result = sd.run_sibling_drip(b, stage=99, composer=None)
        self.assertIsNone(result)

    def test_sibling_moves_constant_has_five_entries(self):
        self.assertEqual(len(sd.SIBLING_MOVES), 5)
        self.assertIn("mirror", sd.SIBLING_MOVES)
        self.assertIn("ask_why", sd.SIBLING_MOVES)
        self.assertIn("extend", sd.SIBLING_MOVES)
        self.assertIn("doubt", sd.SIBLING_MOVES)
        self.assertIn("name_it", sd.SIBLING_MOVES)

    def test_threshold_is_a_simple_float(self):
        self.assertIsInstance(sd.COHERENCE_PASS_THRESHOLD, float)
        self.assertGreater(sd.COHERENCE_PASS_THRESHOLD, 0.0)
        self.assertLess(sd.COHERENCE_PASS_THRESHOLD, 1.0)


if __name__ == "__main__":
    unittest.main()
