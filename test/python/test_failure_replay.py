"""Unit tests for scripts/failure_replay.py -- CE-10 failure-mode replay
+ autobiographical journaling.

Pure-Python tests with a fake brain stub. No daemon, no live brain.
"""
import os
import sys
import time
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.join(REPO, "scripts"))

import failure_replay as fr  # noqa: E402


# ---------------------------------------------------------------------------
# Fake brain stub
# ---------------------------------------------------------------------------

class FakeBrain:
    """Minimal stand-in. Captures calls so tests can assert on the
    feedback path."""

    def __init__(self, produced_text="some content words here for the brain",
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


class ScriptedBrain(FakeBrain):
    """Brain that returns a different produced text per call."""

    def __init__(self, scripted_outputs):
        super().__init__()
        self._outputs = list(scripted_outputs)
        self._idx = 0

    def produce_text(self, intent):
        self.produce_calls.append(list(intent) if intent is not None else None)
        if self._idx < len(self._outputs):
            text = self._outputs[self._idx]
        else:
            text = ""
        self._idx += 1
        return {"text": text, "confidence": 0.5, "success": True}


class FakeComposer:
    def __init__(self):
        self.calls = []

    def compose(self, text=None, modality="text"):
        self.calls.append((text, modality))
        import numpy as np
        return np.zeros(1024, dtype=np.float32)


# ---------------------------------------------------------------------------
# Journal API: record / size / clear
# ---------------------------------------------------------------------------

class TestRecordFailure(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_record_returns_positive_id(self):
        eid = fr.record_failure(
            source="storytelling",
            prompt="The cat sat.",
            produced="zzz zzz zzz",
            expected_keywords={"cat", "sat"},
            composite=0.2,
            stage=2,
        )
        self.assertGreater(eid, 0)
        self.assertEqual(fr.journal_size(), 1)

    def test_dedup_within_window(self):
        eid1 = fr.record_failure(
            source="storytelling", prompt="The dog ran.",
            produced="x", expected_keywords={"dog"},
            composite=0.1, stage=2)
        self.assertGreater(eid1, 0)
        self.assertEqual(fr.journal_size(), 1)

        eid2 = fr.record_failure(
            source="storytelling", prompt="The dog ran.",
            produced="y", expected_keywords={"dog"},
            composite=0.1, stage=2)
        self.assertEqual(eid2, -1)
        self.assertEqual(fr.journal_size(), 1)

    def test_dedup_does_not_block_distinct_prompts(self):
        eid1 = fr.record_failure(
            source="a", prompt="prompt A", produced="x",
            expected_keywords=None, composite=0.1, stage=2)
        eid2 = fr.record_failure(
            source="b", prompt="prompt B", produced="y",
            expected_keywords=None, composite=0.2, stage=2)
        self.assertGreater(eid1, 0)
        self.assertGreater(eid2, 0)
        self.assertNotEqual(eid1, eid2)
        self.assertEqual(fr.journal_size(), 2)

    def test_bounded_deque_caps_at_512(self):
        # Generate 600 distinct prompts; only the last 512 should remain.
        for i in range(600):
            fr.record_failure(
                source="source-%d" % (i % 5),
                prompt="distinct prompt %d" % i,
                produced="output %d" % i,
                expected_keywords={"k%d" % i},
                composite=(i % 100) / 100.0,
                stage=(i % 2) + 2,  # 2 or 3
            )
        self.assertEqual(fr.journal_size(), 512)
        # Oldest evicted: id 1 should no longer be present.
        ids = {int(e["id"]) for e in fr._JOURNAL}
        self.assertNotIn(1, ids)
        # The most recent should still be present.
        self.assertIn(max(ids), ids)


class TestJournalClear(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_clear_empties_journal_and_recent_window(self):
        fr.record_failure(source="s", prompt="P1", produced="x",
                          expected_keywords=None, composite=0.1, stage=2)
        fr.record_failure(source="s", prompt="P2", produced="y",
                          expected_keywords=None, composite=0.2, stage=2)
        self.assertEqual(fr.journal_size(), 2)
        self.assertGreater(len(fr._RECENT_PROMPTS), 0)

        fr.journal_clear()
        self.assertEqual(fr.journal_size(), 0)
        self.assertEqual(len(fr._RECENT_PROMPTS), 0)

        # After clear, the same prompt should be accepted again (no
        # dedup leakage past clear).
        eid = fr.record_failure(
            source="s", prompt="P1", produced="z",
            expected_keywords=None, composite=0.1, stage=2)
        self.assertGreater(eid, 0)


# ---------------------------------------------------------------------------
# journal_summary
# ---------------------------------------------------------------------------

class TestJournalSummary(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_summary_counts_by_source_and_stage(self):
        fr.record_failure(source="storytelling", prompt="A",
                          produced="x", expected_keywords=None,
                          composite=0.2, stage=2)
        fr.record_failure(source="storytelling", prompt="B",
                          produced="x", expected_keywords=None,
                          composite=0.4, stage=3)
        fr.record_failure(source="counterfactual", prompt="C",
                          produced="x", expected_keywords=None,
                          composite=0.6, stage=2)

        summary = fr.journal_summary()
        self.assertEqual(summary["total"], 3)
        self.assertEqual(summary["by_source"]["storytelling"], 2)
        self.assertEqual(summary["by_source"]["counterfactual"], 1)
        self.assertEqual(summary["by_stage"][2], 2)
        self.assertEqual(summary["by_stage"][3], 1)
        # mean = (0.2 + 0.4 + 0.6) / 3 = 0.4
        self.assertAlmostEqual(summary["mean_composite"], 0.4, places=5)

    def test_summary_does_not_mutate_journal(self):
        fr.record_failure(source="s", prompt="P", produced="x",
                          expected_keywords=None, composite=0.1, stage=2)
        before_size = fr.journal_size()
        snapshot = list(fr._JOURNAL)
        _ = fr.journal_summary()
        self.assertEqual(fr.journal_size(), before_size)
        # Identity-preserve: each entry dict object is the same object.
        for old, new in zip(snapshot, list(fr._JOURNAL)):
            self.assertIs(old, new)

    def test_summary_empty_journal(self):
        summary = fr.journal_summary()
        self.assertEqual(summary["total"], 0)
        self.assertEqual(summary["by_source"], {})
        self.assertEqual(summary["mean_composite"], 0.0)


# ---------------------------------------------------------------------------
# Replay driver -- stage gating + populating
# ---------------------------------------------------------------------------

class TestReplayStageGating(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_stage_1_is_noop_even_with_journal(self):
        fr.record_failure(source="s", prompt="P", produced="x",
                          expected_keywords={"foo"}, composite=0.1, stage=1)
        b = FakeBrain()
        result = fr.run_replay_drip(b, stage=1, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])
        self.assertEqual(b.learn_calls, [])
        self.assertEqual(b.train_calls, [])

    def test_empty_journal_returns_none(self):
        b = FakeBrain()
        result = fr.run_replay_drip(b, stage=2, composer=None)
        self.assertIsNone(result)
        self.assertEqual(b.produce_calls, [])

    def test_stage_filter_picks_only_matching_stage(self):
        # Record stage-2 + stage-3 entries; ask for stage=3 only.
        eid_s2 = fr.record_failure(
            source="s", prompt="prompt-2a", produced="x",
            expected_keywords={"foo"}, composite=0.1, stage=2)
        eid_s3a = fr.record_failure(
            source="s", prompt="prompt-3a", produced="x",
            expected_keywords={"alpha", "beta"},
            composite=0.1, stage=3)
        eid_s3b = fr.record_failure(
            source="s", prompt="prompt-3b", produced="x",
            expected_keywords={"gamma"}, composite=0.2,
            stage=3)
        b = FakeBrain(produced_text="alpha beta delta epsilon zeta gamma")
        result = fr.run_replay_drip(b, stage=3, composer=None,
                                     max_replays=5)
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 2)
        picked_ids = {r["entry_id"] for r in result}
        # Only stage-3 ids should be selected; the stage-2 id must not
        # leak in.
        self.assertEqual(picked_ids, {eid_s3a, eid_s3b})
        self.assertNotIn(eid_s2, picked_ids)


# ---------------------------------------------------------------------------
# Replay driver -- feedback paths
# ---------------------------------------------------------------------------

class TestReplayFeedback(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_high_score_consolidates_and_drops_entry(self):
        # Pre-record an entry with expected={alpha,beta,gamma}; brain
        # produces those + extra content -> recall=1.0, specificity high
        # -> composite well above 0.55.
        fr.record_failure(source="storytelling", prompt="probe one",
                          produced="bad output",
                          expected_keywords={"alpha", "beta", "gamma"},
                          composite=0.1, stage=2)
        before_size = fr.journal_size()
        b = FakeBrain(
            produced_text="alpha beta gamma delta epsilon zeta")
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=1)
        self.assertEqual(len(result), 1)
        self.assertGreaterEqual(result[0]["new_composite"],
                                fr.REPLAY_PASS_THRESHOLD)
        self.assertFalse(result[0]["retained"])
        # Brain consolidated the new attempt:
        self.assertEqual(len(b.learn_calls), 1)
        self.assertEqual(len(b.train_calls), 0)
        # Entry dropped from journal:
        self.assertEqual(fr.journal_size(), before_size - 1)

    def test_low_score_re_anchors_and_retains_entry(self):
        # Entry with expected keywords; brain produces nothing useful.
        fr.record_failure(source="storytelling", prompt="probe two",
                          produced="bad",
                          expected_keywords={"alpha", "beta", "gamma"},
                          composite=0.1, stage=2)
        before_size = fr.journal_size()
        b = FakeBrain(produced_text="zz")
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=1)
        self.assertEqual(len(result), 1)
        self.assertLess(result[0]["new_composite"], fr.REPLAY_PASS_THRESHOLD)
        self.assertTrue(result[0]["retained"])
        # Re-anchor pair: (exemplar, exemplar)
        self.assertEqual(len(b.train_calls), 1)
        self.assertEqual(b.train_calls[0][0], b.train_calls[0][1])
        self.assertEqual(len(b.learn_calls), 0)
        # Entry retained:
        self.assertEqual(fr.journal_size(), before_size)

    def test_multiple_replays_in_one_drip(self):
        # 4 entries pre-recorded; max_replays=2 -> 2 produce_text calls.
        for i in range(4):
            fr.record_failure(
                source="storytelling",
                prompt="prompt %d" % i,
                produced="x",
                expected_keywords={"foo%d" % i},
                composite=0.1, stage=2,
            )
        b = FakeBrain(produced_text="zz")  # always low score
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=2)
        self.assertEqual(len(result), 2)
        self.assertEqual(len(b.produce_calls), 2)


# ---------------------------------------------------------------------------
# Replay weighting
# ---------------------------------------------------------------------------

class TestReplayWeighting(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_lowest_composite_picked_first(self):
        # 5 entries with the same recency; varying composite. Lower
        # composite should win the weight tie.
        composites = [0.9, 0.7, 0.1, 0.4, 0.3]
        ids = []
        for i, c in enumerate(composites):
            eid = fr.record_failure(
                source="s",
                prompt="prompt-w-%d" % i,
                produced="x",
                expected_keywords={"k%d" % i},
                composite=c, stage=2,
            )
            ids.append(eid)
        # Pin all timestamps to the same value so recency doesn't tilt
        # the ranking.
        same_ts = time.monotonic()
        for entry in fr._JOURNAL:
            entry["ts"] = same_ts

        b = FakeBrain(produced_text="zz")  # forces low new_composite
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=2)
        self.assertEqual(len(result), 2)
        picked_ids = {r["entry_id"] for r in result}
        # composites[2]=0.1, composites[4]=0.3 -> ids 3 and 5
        self.assertEqual(picked_ids, {ids[2], ids[4]})

    def test_recency_factor_prefers_newer(self):
        # Two entries with the SAME composite. Make one ancient and one
        # fresh; newer should win.
        eid_old = fr.record_failure(
            source="s", prompt="ancient prompt", produced="x",
            expected_keywords={"k"}, composite=0.5, stage=2)
        eid_new = fr.record_failure(
            source="s", prompt="recent prompt", produced="x",
            expected_keywords={"k"}, composite=0.5, stage=2)

        # Backdate the older entry by 1000s. recency_factor floors at
        # 0 after 600s, so the older one gets weight = 0.5; newer gets
        # weight ~= 0.5 + 1.0 = 1.5.
        for entry in fr._JOURNAL:
            if entry["id"] == eid_old:
                entry["ts"] = time.monotonic() - 1000.0

        b = FakeBrain(produced_text="zz")
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=1)
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["entry_id"], eid_new)


# ---------------------------------------------------------------------------
# Replay error handling + composer
# ---------------------------------------------------------------------------

class TestReplayErrorHandling(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_produce_text_exception_in_one_replay_does_not_abort_rest(self):
        # Two entries -> two produce_text calls. First raises; second
        # returns normally. The drip should still produce a result list
        # of length 2.
        fr.record_failure(source="s", prompt="prompt-A", produced="x",
                          expected_keywords={"foo"}, composite=0.1,
                          stage=2)
        fr.record_failure(source="s", prompt="prompt-B", produced="x",
                          expected_keywords={"foo"}, composite=0.2,
                          stage=2)

        class FlakyBrain(FakeBrain):
            def __init__(self):
                super().__init__()
                self._call = 0

            def produce_text(self, intent):
                self._call += 1
                if self._call == 1:
                    raise RuntimeError("simulated produce failure")
                return super().produce_text(intent)

        b = FlakyBrain()
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=2)
        # Both replays should appear in the result list -- the failed
        # one shows empty attempt, retained=True, new_composite=0.0.
        self.assertEqual(len(result), 2)
        failed_rows = [r for r in result if r["replay_attempt"] == ""]
        self.assertEqual(len(failed_rows), 1)
        self.assertTrue(failed_rows[0]["retained"])
        # The non-failed call still went through -- 1 produce attempt
        # logged on the second call.
        self.assertEqual(len([r for r in result
                              if r["replay_attempt"] != ""]), 1)


class TestReplayExemplarFallback(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_empty_expected_keywords_falls_back_to_produced(self):
        # When expected_keywords is empty, exemplar = prompt + " " + produced.
        # Replay should still run without raising.
        fr.record_failure(source="s", prompt="bare prompt",
                          produced="something the brain produced before",
                          expected_keywords=None, composite=0.1, stage=2)
        b = FakeBrain(produced_text="zz zz")
        result = fr.run_replay_drip(b, stage=2, composer=None,
                                     max_replays=1)
        self.assertEqual(len(result), 1)
        # train_language called with the exemplar (prompt + produced).
        self.assertEqual(len(b.train_calls), 1)
        exemplar = b.train_calls[0][0]
        self.assertIn("bare prompt", exemplar)
        self.assertIn("something the brain produced before", exemplar)


class TestReplayComposerIntegration(unittest.TestCase):
    def setUp(self):
        fr.journal_clear()

    def test_composer_called_with_exemplar(self):
        fr.record_failure(source="s", prompt="probe",
                          produced="x",
                          expected_keywords={"alpha", "beta"},
                          composite=0.1, stage=2)
        c = FakeComposer()
        b = FakeBrain(produced_text="zz")
        fr.run_replay_drip(b, stage=2, composer=c, max_replays=1)
        # Composer called once, with text containing the prompt + the
        # expected keywords.
        self.assertEqual(len(c.calls), 1)
        text, modality = c.calls[0]
        self.assertEqual(modality, "text")
        self.assertIn("probe", text)
        self.assertIn("alpha", text)
        self.assertIn("beta", text)

    def test_composer_failure_falls_back_to_zeros(self):
        fr.record_failure(source="s", prompt="probe",
                          produced="x",
                          expected_keywords={"alpha"},
                          composite=0.1, stage=2)

        class BoomComposer:
            def compose(self, text=None, modality="text"):
                raise RuntimeError("boom")

        b = FakeBrain(produced_text="zz")
        result = fr.run_replay_drip(b, stage=2, composer=BoomComposer(),
                                     max_replays=1)
        # Should not raise; should still attempt one replay.
        self.assertIsNotNone(result)
        self.assertEqual(len(result), 1)
        self.assertEqual(len(b.produce_calls), 1)


# ---------------------------------------------------------------------------
# Score helper
# ---------------------------------------------------------------------------

class TestScoreReplay(unittest.TestCase):
    def test_full_recall_and_specificity(self):
        score = fr._score_replay(
            "alpha beta gamma delta epsilon zeta",
            {"alpha", "beta", "gamma"},
        )
        self.assertEqual(score["recall"], 1.0)
        self.assertEqual(score["specificity"], 1.0)
        self.assertAlmostEqual(score["composite"], 1.0, places=5)

    def test_zero_recall(self):
        score = fr._score_replay("nothing", {"alpha", "beta"})
        self.assertEqual(score["recall"], 0.0)

    def test_no_expected_keywords_uses_specificity_only(self):
        score = fr._score_replay(
            "alpha beta gamma delta epsilon",
            None,
        )
        self.assertEqual(score["recall"], 0.0)
        self.assertEqual(score["specificity"], 1.0)
        self.assertEqual(score["composite"], 1.0)


if __name__ == "__main__":
    unittest.main()
