"""Unit tests for the canonical-corpus ingest seam in scripts/immerse_athena.py.

Covers:
  * _load_canonical_state empty default + round-trip with _save_canonical_state
  * _ingest_canonical_corpus drives chunks through brain.train_language() +
    brain.learn_language() and persists the byte cursor
  * resume correctness (second invocation picks up past first invocation's bytes)
  * max_chunks_per_call honored
  * missing index.json is a no-op (corpus is optional)
  * --canonical-corpus / --no-canonical-corpus arg parses correctly

We avoid importing immerse_athena at module load time because it pulls in
nimcp + heavy ML deps. Instead we monkey-import the module via a one-shot
pre-rigged sys.argv that includes --daemon (which short-circuits the
nimcp top-level import) and --no-claude.
"""

import argparse
import json
import os
import sys
import tempfile
import unittest

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPTS = os.path.join(REPO, "scripts")
sys.path.insert(0, REPO)
sys.path.insert(0, SCRIPTS)


def _import_immerse_athena():
    """Import immerse_athena with nimcp short-circuit. Cached on second call."""
    if "immerse_athena" in sys.modules:
        return sys.modules["immerse_athena"]
    saved_argv = sys.argv[:]
    # --daemon makes the top of immerse_athena skip `import nimcp` so the
    # heavy native lib doesn't have to be initialized for these unit tests.
    sys.argv = ["immerse_athena.py", "--daemon"]
    try:
        import immerse_athena  # noqa: E402
    finally:
        sys.argv = saved_argv
    return immerse_athena


def _build_mini_corpus(root):
    """Build a tiny 2-work corpus + index.json under `root`."""
    os.makedirs(os.path.join(root, "alpha", "one"))
    os.makedirs(os.path.join(root, "beta", "two"))
    with open(os.path.join(root, "alpha", "one", "one.txt"), "w") as f:
        f.write("Alpha one paragraph.\n\nAlpha second paragraph here.\n\n"
                "Alpha third paragraph contains more words than the others.\n")
    with open(os.path.join(root, "beta", "two", "two.txt"), "w") as f:
        f.write("Beta intro paragraph here.\n\nBeta middle of work.\n\n"
                "Beta tail end with some more text to chunk.\n")
    idx = {
        "version": 1,
        "works": [
            {"id": "alpha.one", "author": "Alpha", "author_slug": "alpha",
             "title": "One", "work_slug": "one",
             "files": ["alpha/one/one.txt"],
             "language": "en", "register": "test", "year": 2026,
             "public_domain": True, "source": "synthetic",
             "stage": 1, "weight": 1.0},
            {"id": "beta.two", "author": "Beta", "author_slug": "beta",
             "title": "Two", "work_slug": "two",
             "files": ["beta/two/two.txt"],
             "language": "en", "register": "test", "year": 2026,
             "public_domain": True, "source": "synthetic",
             "stage": 1, "weight": 1.0},
        ],
    }
    with open(os.path.join(root, "index.json"), "w") as f:
        json.dump(idx, f)
    return root


class StubBrain:
    """Captures train_language + learn_language calls for assertions."""
    def __init__(self, fail_train=False, fail_learn=False):
        self.train_calls = []
        self.learn_calls = []
        self.fail_train = fail_train
        self.fail_learn = fail_learn

    def train_language(self, text, target_text=None):
        if self.fail_train:
            raise RuntimeError("simulated train failure")
        self.train_calls.append((text, target_text))

    def learn_language(self, text):
        if self.fail_learn:
            raise RuntimeError("simulated learn failure")
        self.learn_calls.append(text)


class TestCanonicalStateIO(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.ckpt = self.tmp.name
        self.mod = _import_immerse_athena()

    def tearDown(self):
        self.tmp.cleanup()

    def test_load_default_when_missing(self):
        state = self.mod._load_canonical_state(self.ckpt)
        self.assertEqual(state["version"], 1)
        self.assertEqual(state["by_work"], {})
        self.assertEqual(state["totals"]["chunks_ingested"], 0)
        self.assertEqual(state["totals"]["bytes_ingested"], 0)

    def test_save_and_load_round_trips(self):
        state = {
            "version": 1,
            "by_work": {
                "melville.moby_dick": {
                    "byte_offset": 1234, "chunks_done": 7,
                    "last_stage": 2, "last_step": 7, "done": False,
                },
            },
            "totals": {"chunks_ingested": 7, "bytes_ingested": 1234},
        }
        self.mod._save_canonical_state(self.ckpt, state)
        # File must actually be on disk under the right name.
        path = os.path.join(self.ckpt, ".canonical_corpus_state.json")
        self.assertTrue(os.path.exists(path))
        loaded = self.mod._load_canonical_state(self.ckpt)
        self.assertEqual(loaded, state)

    def test_save_is_atomic_no_tmp_left(self):
        state = self.mod._load_canonical_state(self.ckpt)
        self.mod._save_canonical_state(self.ckpt, state)
        # No .tmp orphan left after a successful write.
        tmp_path = os.path.join(self.ckpt, ".canonical_corpus_state.json.tmp")
        self.assertFalse(os.path.exists(tmp_path))


class TestIngestSeam(unittest.TestCase):
    def setUp(self):
        self.tmp_root = tempfile.TemporaryDirectory()
        self.tmp_ckpt = tempfile.TemporaryDirectory()
        self.root = _build_mini_corpus(self.tmp_root.name)
        self.ckpt = self.tmp_ckpt.name
        self.mod = _import_immerse_athena()

    def tearDown(self):
        self.tmp_root.cleanup()
        self.tmp_ckpt.cleanup()

    def test_ingest_calls_brain_and_writes_state(self):
        brain = StubBrain()
        self.mod._ingest_canonical_corpus(
            brain, stage=1,
            corpus_root=self.root,
            max_chunks_per_call=4,
            chunk_chars=200,
            checkpoint_dir=self.ckpt,
            log_every=2)
        # Both call shapes were exercised.
        self.assertGreater(len(brain.train_calls), 0)
        self.assertGreater(len(brain.learn_calls), 0)
        self.assertEqual(len(brain.train_calls), len(brain.learn_calls))
        # Capped at max_chunks_per_call.
        self.assertLessEqual(len(brain.train_calls), 4)
        # State file persisted.
        path = os.path.join(self.ckpt, ".canonical_corpus_state.json")
        self.assertTrue(os.path.exists(path))
        with open(path) as f:
            state = json.load(f)
        # At least one work has a non-zero byte_offset.
        nonzero = [w for w in state["by_work"].values()
                   if int(w.get("byte_offset", 0)) > 0]
        self.assertGreater(len(nonzero), 0)
        self.assertGreater(state["totals"]["chunks_ingested"], 0)
        self.assertGreater(state["totals"]["bytes_ingested"], 0)
        # last_stage was recorded.
        for w in nonzero:
            self.assertEqual(w.get("last_stage"), 1)

    def test_ingest_resumes_past_first_invocation(self):
        brain1 = StubBrain()
        self.mod._ingest_canonical_corpus(
            brain1, stage=1, corpus_root=self.root,
            max_chunks_per_call=2, chunk_chars=80,
            checkpoint_dir=self.ckpt, log_every=1)
        first_calls = list(brain1.train_calls)
        self.assertGreater(len(first_calls), 0)

        # Second invocation must NOT re-feed the same chunks (resume).
        brain2 = StubBrain()
        self.mod._ingest_canonical_corpus(
            brain2, stage=1, corpus_root=self.root,
            max_chunks_per_call=4, chunk_chars=80,
            checkpoint_dir=self.ckpt, log_every=1)
        second_calls = list(brain2.train_calls)
        # Some new chunks were drawn (the corpus is small but not empty).
        if second_calls:  # may be empty if corpus is fully consumed
            # No exact text in `second_calls` should equal a text in first_calls
            # for the SAME work — round-robin schedule may revisit a work that
            # in the first pass had not been touched, but the chunk text must
            # advance past the byte cursor.
            first_texts = {t for (t, _) in first_calls}
            second_texts = {t for (t, _) in second_calls}
            self.assertEqual(first_texts & second_texts, set(),
                             "Resume must not re-feed already-ingested chunks")

    def test_ingest_max_chunks_per_call_caps(self):
        brain = StubBrain()
        self.mod._ingest_canonical_corpus(
            brain, stage=1, corpus_root=self.root,
            max_chunks_per_call=2, chunk_chars=200,
            checkpoint_dir=self.ckpt, log_every=10)
        self.assertLessEqual(len(brain.train_calls), 2)
        self.assertLessEqual(len(brain.learn_calls), 2)

    def test_ingest_missing_index_is_noop(self):
        brain = StubBrain()
        empty_root = tempfile.TemporaryDirectory()
        try:
            # No index.json under empty_root.name.
            self.mod._ingest_canonical_corpus(
                brain, stage=1, corpus_root=empty_root.name,
                max_chunks_per_call=4, chunk_chars=200,
                checkpoint_dir=self.ckpt, log_every=2)
        finally:
            empty_root.cleanup()
        # Brain was never called.
        self.assertEqual(brain.train_calls, [])
        self.assertEqual(brain.learn_calls, [])
        # State file may or may not exist; must not raise either way.

    def test_ingest_no_works_for_stage_is_noop(self):
        # Mini corpus has stage=1 works only; stage 3 = empty.
        brain = StubBrain()
        self.mod._ingest_canonical_corpus(
            brain, stage=3, corpus_root=self.root,
            max_chunks_per_call=4, chunk_chars=200,
            checkpoint_dir=self.ckpt, log_every=2)
        self.assertEqual(brain.train_calls, [])
        self.assertEqual(brain.learn_calls, [])

    def test_ingest_brain_failures_are_logged_not_raised(self):
        # train_language fails — ingest must continue and advance cursor.
        brain = StubBrain(fail_train=True)
        # Should not raise.
        self.mod._ingest_canonical_corpus(
            brain, stage=1, corpus_root=self.root,
            max_chunks_per_call=2, chunk_chars=200,
            checkpoint_dir=self.ckpt, log_every=2)
        # No successful train_calls captured because they raised, but state
        # advanced — we don't re-feed bad chunks.
        self.assertEqual(brain.train_calls, [])
        path = os.path.join(self.ckpt, ".canonical_corpus_state.json")
        self.assertTrue(os.path.exists(path))


class TestArgparseFlags(unittest.TestCase):
    """Verify --canonical-corpus / --no-canonical-corpus parses correctly.

    We don't drive the immerse_athena main() (heavy), but we recreate the
    same parser shape and assert the flag round-trips. immerse_athena uses
    BooleanOptionalAction when available (Python >= 3.9), else the
    --canonical-corpus / --no-canonical-corpus pair. Both are validated.
    """
    def _build_parser(self):
        parser = argparse.ArgumentParser()
        if hasattr(argparse, "BooleanOptionalAction"):
            parser.add_argument("--canonical-corpus",
                                action=argparse.BooleanOptionalAction,
                                default=True)
        else:
            parser.add_argument("--canonical-corpus",
                                dest="canonical_corpus",
                                action="store_true", default=True)
            parser.add_argument("--no-canonical-corpus",
                                dest="canonical_corpus",
                                action="store_false")
        parser.add_argument("--canonical-chunk-chars", type=int, default=1200)
        parser.add_argument("--canonical-max-chunks-per-stage", type=int, default=0)
        parser.add_argument("--canonical-restart", action="store_true")
        parser.add_argument("--canonical-corpus-root", type=str,
                            default="data/canonical_corpus")
        return parser

    def test_default_on(self):
        parser = self._build_parser()
        ns = parser.parse_args([])
        self.assertTrue(ns.canonical_corpus)

    def test_no_canonical_corpus_disables(self):
        parser = self._build_parser()
        ns = parser.parse_args(["--no-canonical-corpus"])
        self.assertFalse(ns.canonical_corpus)

    def test_canonical_corpus_flag_keeps_on(self):
        parser = self._build_parser()
        ns = parser.parse_args(["--canonical-corpus"])
        self.assertTrue(ns.canonical_corpus)

    def test_chunk_chars_and_cap(self):
        parser = self._build_parser()
        ns = parser.parse_args(["--canonical-chunk-chars", "800",
                                "--canonical-max-chunks-per-stage", "10",
                                "--canonical-restart",
                                "--canonical-corpus-root", "/tmp/x"])
        self.assertEqual(ns.canonical_chunk_chars, 800)
        self.assertEqual(ns.canonical_max_chunks_per_stage, 10)
        self.assertTrue(ns.canonical_restart)
        self.assertEqual(ns.canonical_corpus_root, "/tmp/x")


if __name__ == "__main__":
    unittest.main()
