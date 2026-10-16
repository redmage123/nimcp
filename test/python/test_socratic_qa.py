"""Unit tests for scripts/socratic_qa.py — CE-2 Socratic Q&A on
ingested texts.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import socratic_qa as sq  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub (mirrors test_storytelling.FakeBrain)
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path."""

    def __init__(self, produced_text="The river flows quickly through valleys.",
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
# Question generation
# ---------------------------------------------------------------------------

class TestGenerateQuestions(unittest.TestCase):
    def test_empty_chunk_returns_empty(self):
        self.assertEqual(sq.generate_questions(""), [])
        self.assertEqual(sq.generate_questions(None or ""), [])

    def test_short_chunk_returns_empty(self):
        # Below MIN_CHUNK_LEN (25 chars) → []
        self.assertEqual(sq.generate_questions("Tiny."), [])
        self.assertEqual(sq.generate_questions("Just a few words here"), [])

    def test_chunk_with_only_stopwords_returns_empty(self):
        # No content words → no question can be emitted (no expected_keywords).
        chunk = "the and of in on at to but if so or as is are was were be"
        out = sq.generate_questions(chunk)
        self.assertEqual(out, [])

    def test_what_is_picks_most_frequent_content_word(self):
        # 'mountain' appears thrice; should anchor the what_is question.
        chunk = ("The mountain rose above the village. The mountain was tall. "
                 "Children climbed the mountain together in spring.")
        out = sq.generate_questions(chunk, max_questions=4)
        self.assertTrue(out)
        kinds = [q["kind"] for q in out]
        self.assertIn("what_is", kinds)
        wi = next(q for q in out if q["kind"] == "what_is")
        self.assertEqual(wi["question"], "What is mountain?")
        self.assertIn("mountain", wi["expected_keywords"])

    def test_who_did_fires_on_capitalized_subject_plus_verb(self):
        chunk = ("Marcus walked across the field. He carried a heavy basket "
                 "of apples toward the river before sunset.")
        out = sq.generate_questions(chunk, max_questions=4)
        kinds = [q["kind"] for q in out]
        self.assertIn("who_did", kinds)
        wd = next(q for q in out if q["kind"] == "who_did")
        # Subject is 'Marcus' (proper noun → 'Who'); verb is 'walked'.
        self.assertTrue(wd["question"].startswith("Who "))
        self.assertIn("walked", wd["question"])
        self.assertIn("marcus", wd["expected_keywords"])

    def test_where_fires_on_prep_plus_capitalized_noun(self):
        chunk = ("The travellers wandered through Veloria for many days, "
                 "tasting strange fruit and listening to the ancient songs.")
        out = sq.generate_questions(chunk, max_questions=4)
        kinds = [q["kind"] for q in out]
        self.assertIn("where", kinds)
        w = next(q for q in out if q["kind"] == "where")
        self.assertEqual(w["question"], "Where did this happen?")
        self.assertIn("veloria", w["expected_keywords"])

    def test_summary_always_valid_when_content_words_exist(self):
        # No SVO, no where-pattern; only summary should fire (and what_is
        # for the most frequent word).
        chunk = ("orange yellow purple cyan magenta orange orange purple "
                 "yellow magenta orange purple")
        out = sq.generate_questions(chunk, max_questions=5)
        kinds = {q["kind"] for q in out}
        self.assertIn("summary", kinds)
        s = next(q for q in out if q["kind"] == "summary")
        self.assertEqual(s["question"], "Tell me about this passage.")
        # Top-5 content words should populate expected_keywords.
        self.assertGreaterEqual(len(s["expected_keywords"]), 1)
        self.assertLessEqual(len(s["expected_keywords"]), 5)
        self.assertIn("orange", s["expected_keywords"])

    def test_max_questions_cap_respected(self):
        chunk = ("Marcus walked across Veloria carrying apples through "
                 "the gardens. The mountain rose above. Marcus rested "
                 "by the river. Marcus laughed and Marcus sang.")
        # All 4 kinds could fire, but cap at 2.
        out = sq.generate_questions(chunk, max_questions=2)
        self.assertEqual(len(out), 2)

    def test_max_questions_zero_returns_empty(self):
        chunk = ("The mountain rose above the village. Marcus walked "
                 "through Veloria carrying apples.")
        self.assertEqual(sq.generate_questions(chunk, max_questions=0), [])

    def test_long_chunk_does_not_explode(self):
        # 5000 chars; should still cap output at max_questions and not raise.
        chunk = ("The mountain rose above the village. Marcus walked. " * 200)
        out = sq.generate_questions(chunk, max_questions=3)
        self.assertLessEqual(len(out), 3)
        self.assertGreater(len(out), 0)

    def test_question_dicts_have_expected_shape(self):
        chunk = ("Marcus walked across Veloria carrying apples and "
                 "singing softly under the tall trees.")
        out = sq.generate_questions(chunk, max_questions=4)
        for q in out:
            self.assertIn("question", q)
            self.assertIn("expected_keywords", q)
            self.assertIn("kind", q)
            self.assertIsInstance(q["question"], str)
            self.assertIsInstance(q["expected_keywords"], set)
            self.assertIsInstance(q["kind"], str)
            # Every emitted keyword must be a non-stopword content word.
            for k in q["expected_keywords"]:
                self.assertNotIn(k, sq._STOPWORDS)
                self.assertGreaterEqual(len(k), sq._MIN_KEYWORD_LEN)


# ---------------------------------------------------------------------------
# Scoring
# ---------------------------------------------------------------------------

class TestScoreAnswer(unittest.TestCase):
    def test_empty_answer_zero_components(self):
        s = sq.score_answer("", {"mountain", "river"})
        self.assertEqual(s["keyword_recall"], 0.0)
        self.assertEqual(s["specificity"], 0.0)
        self.assertEqual(s["composite"], 0.0)
        self.assertEqual(s["tokens"], 0)

    def test_whitespace_answer_zero_components(self):
        s = sq.score_answer("   \n  \t ", {"mountain"})
        self.assertEqual(s["composite"], 0.0)
        self.assertEqual(s["tokens"], 0)

    def test_full_keyword_overlap_recall_1(self):
        s = sq.score_answer(
            "The mountain rose high above the river valley peacefully.",
            {"mountain", "river"})
        self.assertEqual(s["keyword_recall"], 1.0)

    def test_partial_keyword_overlap(self):
        s = sq.score_answer(
            "The mountain rose high above the village.",
            {"mountain", "river", "valley", "forest"})
        # 1 of 4 expected → 0.25 recall.
        self.assertAlmostEqual(s["keyword_recall"], 0.25, places=5)

    def test_specificity_penalizes_one_token_answer(self):
        s = sq.score_answer("mountain", {"mountain"})
        # 1 distinct content word / 5 = 0.2.
        self.assertAlmostEqual(s["specificity"], 0.2, places=5)
        # Recall is 1.0 (single keyword present).
        self.assertEqual(s["keyword_recall"], 1.0)
        # Composite = (2*1.0 + 0.2)/3 = 0.7333...
        self.assertAlmostEqual(s["composite"], (2.0 + 0.2) / 3.0, places=5)

    def test_specificity_caps_at_1(self):
        s = sq.score_answer(
            "mountain river forest valley village castle marketplace harbour",
            {"mountain"})
        self.assertEqual(s["specificity"], 1.0)

    def test_composite_weights_recall_double(self):
        # Recall=1.0, specificity=0.2 → composite = (2.0 + 0.2)/3 = 0.7333
        # If recall and specificity were equally weighted, composite would
        # be (1.0 + 0.2)/2 = 0.6 — distinct from the actual formula.
        s = sq.score_answer("mountain", {"mountain"})
        self.assertAlmostEqual(s["composite"], (2.0 + 0.2) / 3.0, places=5)
        self.assertNotAlmostEqual(s["composite"], 0.6, places=2)

    def test_no_expected_keywords_recall_zero(self):
        s = sq.score_answer("Some answer text here for tokens.", set())
        self.assertEqual(s["keyword_recall"], 0.0)
        # Specificity can still be > 0 since we have content tokens.
        self.assertGreater(s["specificity"], 0.0)

    def test_none_expected_keywords_handled(self):
        s = sq.score_answer("Some text.", None)
        self.assertEqual(s["keyword_recall"], 0.0)
        # Should not raise.

    def test_tokens_count_correct(self):
        s = sq.score_answer("the cat sat on the mat", {"cat"})
        self.assertEqual(s["tokens"], 6)


# ---------------------------------------------------------------------------
# Drip driver
# ---------------------------------------------------------------------------

class TestDrip(unittest.TestCase):
    def setUp(self):
        # Reset module counter to keep log-line behavior deterministic.
        sq._DRIP_COUNTER = 0

    def test_stage_1_is_noop(self):
        b = FakeBrain()
        result = sq.run_socratic_drip(
            b, stage=1,
            chunk_text="The mountain rose above the village every spring.")
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_stage_2_invokes_produce_text_per_question(self):
        chunk = ("Marcus walked across Veloria carrying apples. The "
                 "mountain rose above the village.")
        b = FakeBrain(produced_text="Marcus walked across Veloria carrying"
                                    " apples mountain river valley.")
        result = sq.run_socratic_drip(b, stage=2,
                                      chunk_text=chunk,
                                      max_questions=3)
        self.assertIsNotNone(result)
        self.assertGreater(len(result), 0)
        # produce_text is called once per question generated.
        self.assertEqual(len(b.produce_calls), len(result))

    def test_short_chunk_returns_empty_list(self):
        b = FakeBrain()
        result = sq.run_socratic_drip(b, stage=2, chunk_text="hi.")
        self.assertEqual(result, [])
        self.assertEqual(b.produce_calls, [])

    def test_no_questions_returns_empty_list(self):
        b = FakeBrain()
        chunk = "the and of in on at to but if so or as is are was were be do does did"
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk)
        self.assertEqual(result, [])
        self.assertEqual(b.produce_calls, [])

    def test_high_score_triggers_learn_language(self):
        # Brain answers with a string that contains every expected keyword
        # plus enough variety for full specificity → composite well above 0.5.
        chunk = ("The mountain rose above the village. The mountain was "
                 "tall. Children climbed the mountain together.")
        # Force a single question via what_is by making the chunk concise.
        answer = "mountain river valley forest harbour castle bridge"
        b = FakeBrain(produced_text=answer)
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                                      max_questions=1)
        self.assertEqual(len(result), 1)
        self.assertGreaterEqual(result[0]["composite"],
                                sq.COMPREHENSION_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(b.learn_calls[0], answer)
        self.assertEqual(len(b.train_calls), 0)

    def test_low_score_triggers_train_language_with_question_and_keywords(self):
        # Brain answers with garbage → composite < 0.5 → train_language
        # called with (q + " " + sorted(keywords)) repeated as both args.
        chunk = ("The mountain rose above the village. The mountain was "
                 "tall. Children climbed the mountain together.")
        b = FakeBrain(produced_text="zzz")
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                                      max_questions=1)
        self.assertEqual(len(result), 1)
        self.assertLess(result[0]["composite"],
                        sq.COMPREHENSION_PASS_THRESHOLD)
        self.assertEqual(len(b.learn_calls), 0)
        self.assertEqual(len(b.train_calls), 1)
        text, target = b.train_calls[0]
        self.assertEqual(text, target)  # re-anchor pair
        # Anchor must contain the question text.
        self.assertIn(result[0]["question"], text)
        # And at least one of the expected keywords.
        self.assertTrue(any(k in text for k in result[0]["expected_keywords"]))

    def test_produce_failure_caught_and_skipped(self):
        class BoomBrain(FakeBrain):
            def produce_text(self, intent):
                raise RuntimeError("simulated produce failure")

        b = BoomBrain()
        chunk = ("Marcus walked across Veloria carrying apples. The "
                 "mountain rose above the village.")
        # Should not raise.
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                                      max_questions=2)
        # Empty results because every produce_text raised; loop continued
        # without appending.
        self.assertEqual(result, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_partial_failure_continues_loop(self):
        # First call raises, second call succeeds — ensure we still get a
        # result entry from the second.
        class FlakyBrain(FakeBrain):
            def __init__(self):
                super().__init__(produced_text="mountain river valley forest harbour")
                self._calls = 0

            def produce_text(self, intent):
                self._calls += 1
                if self._calls == 1:
                    raise RuntimeError("flake")
                return super().produce_text(intent)

        b = FlakyBrain()
        chunk = ("Marcus walked across Veloria carrying apples. The "
                 "mountain rose above the village. Children climbed.")
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                                      max_questions=3)
        # At least one survived.
        self.assertGreaterEqual(len(result), 1)

    def test_composer_used_when_given(self):
        class FakeComposer:
            def __init__(self):
                self.calls = []

            def compose(self, text=None, modality="text"):
                self.calls.append((text, modality))
                import numpy as np
                return np.zeros(1024, dtype=np.float32)

        c = FakeComposer()
        b = FakeBrain()
        chunk = ("The mountain rose above the village. The mountain was "
                 "tall. Children climbed the mountain.")
        sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                             composer=c, max_questions=2)
        # Composer should be called at least once per question generated.
        self.assertGreaterEqual(len(c.calls), 1)
        # All calls used the text modality.
        for _, modality in c.calls:
            self.assertEqual(modality, "text")

    def test_stage_3_runs_same_as_stage_2(self):
        b = FakeBrain(produced_text="mountain river valley forest harbour castle")
        chunk = ("The mountain rose above the village. Children climbed "
                 "the mountain together every spring.")
        result = sq.run_socratic_drip(b, stage=3, chunk_text=chunk,
                                      max_questions=2)
        self.assertIsNotNone(result)
        self.assertGreater(len(result), 0)
        for r in result:
            self.assertEqual(r["stage"], 3)

    def test_result_dict_has_expected_shape(self):
        b = FakeBrain(produced_text="mountain river valley")
        chunk = ("The mountain rose above the village. Children climbed "
                 "the mountain together every spring.")
        result = sq.run_socratic_drip(b, stage=2, chunk_text=chunk,
                                      max_questions=1)
        self.assertEqual(len(result), 1)
        r = result[0]
        for key in ("question", "answer", "expected_keywords", "kind",
                    "composite", "keyword_recall", "specificity",
                    "tokens", "stage"):
            self.assertIn(key, r)
        # expected_keywords serialized as a sorted list (JSON-friendly).
        self.assertIsInstance(r["expected_keywords"], list)
        self.assertEqual(r["expected_keywords"], sorted(r["expected_keywords"]))


# ---------------------------------------------------------------------------
# Edge cases
# ---------------------------------------------------------------------------

class TestEdgeCases(unittest.TestCase):
    def test_chunk_with_no_content_words_skips_cleanly(self):
        # Chunk long enough to pass the length gate but with no qualifying
        # content words → []
        chunk = "a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a a"
        self.assertEqual(sq.generate_questions(chunk), [])

    def test_chunk_with_only_short_words(self):
        # Words < MIN_KEYWORD_LEN don't qualify as content keywords.
        chunk = "cat sat on the mat by red bed in his car or the bus to and"
        out = sq.generate_questions(chunk)
        # Either empty (nothing >= 4 chars) or summary fires only if any
        # content words exist. Here all are <4 except the stopwords filtered.
        self.assertEqual(out, [])

    def test_unicode_handled_without_crash(self):
        # Tokenizer is ASCII-letters-only; unicode just gets dropped. Should
        # not raise.
        chunk = ("The mountain rose above the village. Cafe naive jalapeno "
                 "resume curriculum. Children climbed the mountain together.")
        out = sq.generate_questions(chunk, max_questions=3)
        # Should produce something (mountain is the dominant content word).
        self.assertGreater(len(out), 0)


if __name__ == "__main__":
    unittest.main()
