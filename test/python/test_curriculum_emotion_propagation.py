"""Unit tests for the curriculum emotion-propagation seam in
scripts/immerse_athena.py.

Covers:
  * _ground_content_words forwards (valence, arousal) into brain.ground_word.
  * _ground_content_words falls back to the legacy call shape (no
    valence/arousal kwargs) when the underlying brain.ground_word raises
    TypeError — simulating an older binding stack.
  * _train_cognitive forwards (valence, arousal) into _ground_content_words.
  * The _REGISTER_EMOTION table contains entries for the registers we'll
    rely on at curriculum-call sites (victorian_children, gothic_verse,
    sacred_text_kjv, stoic_philosophy at minimum).
  * _ingest_canonical_corpus calls brain.set_grounding_emotion once per
    work, with the register-derived (valence, arousal), and resets to
    (0, 0) afterward.
  * If brain.set_grounding_emotion raises (older daemon without the RPC),
    _ingest_canonical_corpus still runs to completion.

We avoid importing immerse_athena unprotected — it pulls in nimcp + heavy
ML deps. Instead, we monkey-import via a one-shot pre-rigged sys.argv that
includes --daemon (which short-circuits the nimcp top-level import).
"""

import json
import os
import sys
import tempfile
import unittest
from unittest.mock import MagicMock, patch

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPTS = os.path.join(REPO, "scripts")
sys.path.insert(0, REPO)
sys.path.insert(0, SCRIPTS)


def _import_immerse_athena():
    """Import immerse_athena with nimcp short-circuit. Cached on second call."""
    if "immerse_athena" in sys.modules:
        return sys.modules["immerse_athena"]
    saved_argv = sys.argv[:]
    sys.argv = ["immerse_athena.py", "--daemon"]
    try:
        import immerse_athena  # noqa: E402
    finally:
        sys.argv = saved_argv
    return immerse_athena


class TestGroundContentWordsEmotion(unittest.TestCase):
    """_ground_content_words forwards valence/arousal to brain.ground_word."""

    def setUp(self):
        self.mod = _import_immerse_athena()

    def test_default_valence_arousal_zero(self):
        brain = MagicMock()
        brain.ground_word.return_value = True
        feats = [0.1, 0.2, 0.3]
        n = self.mod._ground_content_words(brain, "happy joyful playing",
                                           feats, modality=0)
        # At least one binding emitted.
        self.assertGreater(n, 0)
        # Each call defaulted to valence=0, arousal=0.
        self.assertGreater(brain.ground_word.call_count, 0)
        for call in brain.ground_word.call_args_list:
            kwargs = call.kwargs
            self.assertEqual(kwargs.get("valence"), 0.0)
            self.assertEqual(kwargs.get("arousal"), 0.0)
            self.assertEqual(kwargs.get("modality"), 0)

    def test_explicit_valence_arousal_forwarded(self):
        brain = MagicMock()
        brain.ground_word.return_value = True
        feats = [0.5, -0.2]
        self.mod._ground_content_words(brain, "raven darkness midnight",
                                       feats, modality=2,
                                       valence=-0.4, arousal=0.6)
        self.assertGreater(brain.ground_word.call_count, 0)
        for call in brain.ground_word.call_args_list:
            kwargs = call.kwargs
            self.assertAlmostEqual(kwargs.get("valence"), -0.4)
            self.assertAlmostEqual(kwargs.get("arousal"), 0.6)
            self.assertEqual(kwargs.get("modality"), 2)
            self.assertEqual(kwargs.get("attention"), 0.7)

    def test_typeerror_falls_back_to_legacy_call_shape(self):
        """If brain.ground_word doesn't accept valence/arousal kwargs (older
        binding), the helper must retry without them so grounding still
        occurs."""
        brain = MagicMock()

        # First call: TypeError (legacy binding); second call: succeed.
        def gw(word, feats, **kwargs):
            if "valence" in kwargs or "arousal" in kwargs:
                raise TypeError("ground_word() got unexpected keyword "
                                "argument 'valence'")
            return True

        brain.ground_word.side_effect = gw
        n = self.mod._ground_content_words(brain, "calm gentle warm",
                                           [0.0, 0.1], modality=5,
                                           valence=0.3, arousal=0.4)
        self.assertGreater(n, 0)
        # Both shapes appear in the call list — at least one without v/a.
        legacy_calls = [c for c in brain.ground_word.call_args_list
                        if "valence" not in c.kwargs and
                           "arousal" not in c.kwargs]
        self.assertGreater(len(legacy_calls), 0)


class TestTrainCognitiveForwardsEmotion(unittest.TestCase):
    """_train_cognitive forwards valence/arousal into _ground_content_words."""

    def setUp(self):
        self.mod = _import_immerse_athena()

    def test_train_cognitive_forwards_emotion(self):
        # Build a minimal-shaped Parent instance via __new__ so we don't
        # have to run the full Parent.__init__ (which wires Claude/teacher).
        Parent = self.mod.Parent
        pt = Parent.__new__(Parent)
        pt.interaction_count = 0

        brain = MagicMock()
        # train_cognitive returns dict with "loss" so the print branch is safe.
        brain.train_cognitive.return_value = {"loss": 0.5}

        captured = {}

        def fake_gcw(brain_arg, text, mfeat, modality, valence=0.0,
                     arousal=0.0):
            captured["valence"] = valence
            captured["arousal"] = arousal
            captured["modality"] = modality
            return 1

        with patch.object(self.mod, "_ground_content_words",
                          side_effect=fake_gcw) as mock_gcw:
            pt._train_cognitive(brain, "foo bar baz qux", domain=3,
                                modality_features=[0.1, 0.2],
                                modality=3,
                                valence=0.3, arousal=0.4)

        self.assertEqual(captured.get("valence"), 0.3)
        self.assertEqual(captured.get("arousal"), 0.4)
        self.assertEqual(captured.get("modality"), 3)
        self.assertEqual(mock_gcw.call_count, 1)


class TestRegisterEmotionTable(unittest.TestCase):
    """The _REGISTER_EMOTION table maps the registers we rely on."""

    def setUp(self):
        self.mod = _import_immerse_athena()

    def test_required_registers_present(self):
        required = [
            "victorian_children",
            "gothic_verse",
            "sacred_text_kjv",
            "stoic_philosophy",
        ]
        for reg in required:
            self.assertIn(reg, self.mod._REGISTER_EMOTION,
                          f"missing register {reg!r}")
            v, a = self.mod._REGISTER_EMOTION[reg]
            self.assertGreaterEqual(v, -1.0)
            self.assertLessEqual(v, 1.0)
            self.assertGreaterEqual(a, 0.0)
            self.assertLessEqual(a, 1.0)

    def test_signs_are_what_we_expect(self):
        # Gothic verse should carry negative valence (Poe is dark).
        self.assertLess(self.mod._REGISTER_EMOTION["gothic_verse"][0], 0.0)
        # Victorian children's lit (Carroll) — positive valence.
        self.assertGreater(
            self.mod._REGISTER_EMOTION["victorian_children"][0], 0.0)
        # Sacred KJV — non-negative valence, low/mid arousal.
        kjv_v, kjv_a = self.mod._REGISTER_EMOTION["sacred_text_kjv"]
        self.assertGreaterEqual(kjv_v, 0.0)
        self.assertLessEqual(kjv_a, 0.5)
        # Stoic — non-negative valence, low arousal (calm).
        stoic_v, stoic_a = self.mod._REGISTER_EMOTION["stoic_philosophy"]
        self.assertGreaterEqual(stoic_v, 0.0)
        self.assertLessEqual(stoic_a, 0.5)


class _CapturingBrain:
    """Mock brain capturing set_grounding_emotion + train/learn calls.

    Mirrors the StubBrain used by test_canonical_corpus_ingest_seam.py but
    adds the new set_grounding_emotion hook.
    """
    def __init__(self, set_emotion_raises=False):
        self.train_calls = []
        self.learn_calls = []
        self.emotion_calls = []  # ordered list of (valence, arousal)
        self.set_emotion_raises = set_emotion_raises

    def train_language(self, text, target_text=None):
        self.train_calls.append((text, target_text))

    def learn_language(self, text):
        self.learn_calls.append(text)

    def set_grounding_emotion(self, valence, arousal):
        if self.set_emotion_raises:
            raise RuntimeError("simulated older daemon (no RPC)")
        self.emotion_calls.append((valence, arousal))


def _build_mini_canonical_corpus(root):
    """Build a tiny corpus with two registers we can assert on:
       - carroll/victorian_children
       - poe/gothic_verse
    """
    os.makedirs(os.path.join(root, "carroll", "alice"))
    os.makedirs(os.path.join(root, "poe", "raven"))
    with open(os.path.join(root, "carroll", "alice", "alice.txt"), "w") as f:
        f.write("Alice was beginning to get very tired of sitting by her "
                "sister on the bank.\n\nThe rabbit hurried away across "
                "the garden.\n")
    with open(os.path.join(root, "poe", "raven", "raven.txt"), "w") as f:
        f.write("Once upon a midnight dreary, while I pondered weak and "
                "weary.\n\nQuoth the raven, nevermore.\n")
    idx = {
        "version": 1,
        "works": [
            {"id": "carroll.alice", "author": "Carroll",
             "author_slug": "carroll", "title": "Alice",
             "work_slug": "alice",
             "files": ["carroll/alice/alice.txt"],
             "language": "en", "register": "victorian_children",
             "year": 1865, "public_domain": True, "source": "synthetic",
             "stage": 1, "weight": 1.0},
            {"id": "poe.raven", "author": "Poe",
             "author_slug": "poe", "title": "The Raven",
             "work_slug": "raven",
             "files": ["poe/raven/raven.txt"],
             "language": "en", "register": "gothic_verse",
             "year": 1845, "public_domain": True, "source": "synthetic",
             "stage": 1, "weight": 1.0},
        ],
    }
    with open(os.path.join(root, "index.json"), "w") as f:
        json.dump(idx, f)
    return root


class TestIngestCanonicalCorpusEmotion(unittest.TestCase):
    """_ingest_canonical_corpus drives register-derived emotion through
    brain.set_grounding_emotion."""

    def setUp(self):
        self.tmp_root = tempfile.TemporaryDirectory()
        self.tmp_ckpt = tempfile.TemporaryDirectory()
        self.root = _build_mini_canonical_corpus(self.tmp_root.name)
        self.ckpt = self.tmp_ckpt.name
        self.mod = _import_immerse_athena()

    def tearDown(self):
        self.tmp_root.cleanup()
        self.tmp_ckpt.cleanup()

    def test_register_emotion_set_per_work(self):
        brain = _CapturingBrain()
        self.mod._ingest_canonical_corpus(
            brain, stage=1, corpus_root=self.root,
            max_chunks_per_call=4, chunk_chars=300,
            checkpoint_dir=self.ckpt, log_every=2)
        # Each chunk should pair: (set register emotion) → (reset to 0,0).
        self.assertGreater(len(brain.emotion_calls), 0)
        # Half the calls should be resets to (0,0).
        zero_calls = [c for c in brain.emotion_calls if c == (0.0, 0.0)]
        nonzero_calls = [c for c in brain.emotion_calls if c != (0.0, 0.0)]
        self.assertGreater(len(zero_calls), 0)
        self.assertGreater(len(nonzero_calls), 0)
        self.assertEqual(len(zero_calls), len(nonzero_calls))
        # Non-zero emotions came from the _REGISTER_EMOTION table.
        expected_emotions = {
            self.mod._REGISTER_EMOTION["victorian_children"],
            self.mod._REGISTER_EMOTION["gothic_verse"],
        }
        for v, a in nonzero_calls:
            self.assertIn((v, a), expected_emotions)
        # Train/learn must have been hit too.
        self.assertEqual(len(brain.train_calls), len(brain.learn_calls))
        self.assertGreater(len(brain.train_calls), 0)

    def test_set_emotion_failure_does_not_block_ingestion(self):
        """Older daemon: set_grounding_emotion raises. Ingestion must
        still complete — train_language + learn_language must run."""
        brain = _CapturingBrain(set_emotion_raises=True)
        # Should not raise.
        self.mod._ingest_canonical_corpus(
            brain, stage=1, corpus_root=self.root,
            max_chunks_per_call=4, chunk_chars=300,
            checkpoint_dir=self.ckpt, log_every=2)
        # No emotion was captured (every call raised) but the brain still
        # received train_language + learn_language for at least one chunk.
        self.assertEqual(brain.emotion_calls, [])
        self.assertGreater(len(brain.train_calls), 0)
        self.assertGreater(len(brain.learn_calls), 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
