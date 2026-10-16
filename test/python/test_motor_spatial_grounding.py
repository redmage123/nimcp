"""Unit tests for MOTOR + SPATIAL grounding-event emission.

Verifies that the two grounding paths added in this session actually
fire `brain.ground_word(...)` against a mocked brain — they're the only
sources of MOTOR (modality=2) and SPATIAL (modality=4) signal in
training, so silent regression here would empty out two of the six GL
modalities without any visible failure.

Covers:
  * submit_multimodal grounds content tokens with modality=4 against the
    synthesized somato vector when somato keywords are present.
  * WorldModelCurriculum dispatches a verb-tagged ground_word call with
    modality=2 against the action vector for each scenario.

Run:
    python3 test/python/test_motor_spatial_grounding.py
"""

import faulthandler
import os
import sys
import unittest

# Dump all thread stacks if anything hangs > 45s — turns silent test
# hangs into actionable tracebacks.
faulthandler.dump_traceback_later(45, repeat=False, exit=False)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SCRIPTS = os.path.join(REPO, "scripts")
sys.path.insert(0, REPO)
sys.path.insert(0, SCRIPTS)


# Modality IDs from grounded_lexicon.h — duplicated here so a future
# header rename surfaces as a clear test failure rather than a silent
# import mismatch.
GL_MODALITY_MOTOR = 2
GL_MODALITY_SPATIAL = 4


def _stub_heavy_modules():
    """Pre-populate sys.modules with stubs for the ML/network-heavy
    modules immerse_athena imports at top-level. Without these, the
    real imports trigger sentence-transformers model downloads that
    block on network and turn a unit test into a 5-minute hang on
    constrained hosts.

    Also monkey-patches numpy.linalg.qr to a fast identity-return on
    1024-dim matrices. immerse_athena's TargetDiversifier runs 24
    sequential 1024x1024 QR decompositions at module-import time, which
    on this CPU is many minutes long. The QR result is only used as a
    rotation matrix during runtime diversify() calls, which we don't
    exercise — so a no-op identity is fine for unit-test scope.

    Each stub exposes only the symbols immerse_athena imports by name.
    The functions submit_multimodal exercises (generate_visual_frame,
    generate_audio_samples, _tokenize_for_grounding, _SOMATO_KW) are
    pure-numpy and live in immerse_athena itself, so they don't need
    real backing implementations.
    """
    import types
    import numpy as np

    def _shim(modname, attrs):
        if modname in sys.modules:
            return
        m = types.ModuleType(modname)
        for k, v in attrs.items():
            setattr(m, k, v)
        sys.modules[modname] = m

    # Speed up the 24× 1024x1024 QR loop in TargetDiversifier.__init__
    # by returning an identity matrix without doing the actual factoring.
    # Original takes ~minutes per call on this box; identity returns
    # in microseconds and yields a valid orthogonal matrix.
    _orig_qr = np.linalg.qr

    def _fast_qr(a, *args, **kwargs):
        # If a is square, return (I, a). Otherwise fall back to real qr.
        try:
            arr = np.asarray(a)
            if arr.ndim == 2 and arr.shape[0] == arr.shape[1]:
                n = arr.shape[0]
                Q = np.eye(n, dtype=arr.dtype)
                R = arr.astype(arr.dtype, copy=True)
                return Q, R
        except Exception:
            pass
        return _orig_qr(a, *args, **kwargs)

    np.linalg.qr = _fast_qr

    # claude_teacher: encode_text + batch_encode_texts return small
    # deterministic embeddings. ClaudeTeacher is a no-op class.
    def _encode_text(t, dim=1024):
        rng = np.random.RandomState(hash(str(t)) & 0xFFFFFFFF)
        return rng.randn(dim).astype(np.float32)

    def _batch_encode_texts(texts, dim=1024):
        return [_encode_text(t, dim) for t in texts]

    class _ClaudeTeacher:
        def __init__(self, *a, **kw): pass

    _shim("claude_teacher", {
        "ClaudeTeacher": _ClaudeTeacher,
        "encode_text": _encode_text,
        "batch_encode_texts": _batch_encode_texts,
    })

    # talk_to_athena: extract_embedding_from_output → identity-like.
    _shim("talk_to_athena", {
        "extract_embedding_from_output": lambda out: out,
    })

    # neural_decoder, multimodal_data, cognitive_training_data:
    # only need the names that are imported up top.
    class _NeuralDecoder:
        def __init__(self, *a, **kw): pass

    _shim("neural_decoder", {"NeuralDecoder": _NeuralDecoder})

    class _MultimodalDataLoader:
        def __init__(self, *a, **kw): pass

    _shim("multimodal_data", {"MultimodalDataLoader": _MultimodalDataLoader})

    _shim("cognitive_training_data", {
        "get_all_cognitive_data": lambda *a, **kw: [],
        "get_random_cognitive_item": lambda *a, **kw: None,
    })


def _import_immerse_athena():
    """Import immerse_athena with the --daemon short-circuit + heavy-
    module stubs so neither nimcp nor sentence-transformers is loaded
    for these unit tests."""
    if "immerse_athena" in sys.modules:
        return sys.modules["immerse_athena"]
    _stub_heavy_modules()
    saved_argv = sys.argv[:]
    sys.argv = ["immerse_athena.py", "--daemon"]
    try:
        import immerse_athena  # noqa: E402
    finally:
        sys.argv = saved_argv
    return immerse_athena


def _import_world_model_curriculum():
    if "world_model_curriculum" in sys.modules:
        return sys.modules["world_model_curriculum"]
    import world_model_curriculum  # noqa: E402
    return world_model_curriculum


class StubBrain:
    """Captures ground_word + ancillary calls submit_multimodal makes.

    Implements only the surface area submit_multimodal touches:
      submit_sensory, submit_sensory_batch, focus_attention,
      visual_cortex_process, ground_word.

    Each method is a no-op recorder — submit_multimodal swallows
    exceptions on most paths, so missing methods would only show up as
    silently-absent grounding events. We provide them all so the only
    behavior under test is the grounding sequence itself.
    """

    def __init__(self):
        self.ground_word_calls = []   # list of dicts
        self.submit_sensory_calls = []
        self.submit_sensory_batch_calls = []
        self.focus_attention_calls = []
        self.visual_cortex_process_calls = 0
        self.learn_vector_calls = []

    # --- sensory ------------------------------------------------------
    def submit_sensory(self, modality, data, **kwargs):
        self.submit_sensory_calls.append((modality, kwargs))

    def submit_sensory_batch(self, modalities):
        self.submit_sensory_batch_calls.append(list(modalities.keys()))

    def focus_attention(self, modality):
        self.focus_attention_calls.append(modality)

    def visual_cortex_process(self, *args, **kwargs):
        self.visual_cortex_process_calls += 1

    # --- grounding ----------------------------------------------------
    def ground_word(self, word, features, modality=5, attention=0.7,
                    valence=0.0, arousal=0.0):
        self.ground_word_calls.append({
            "word": word,
            "features": list(features),
            "modality": int(modality),
            "attention": float(attention),
            "valence": float(valence),
            "arousal": float(arousal),
        })
        return True

    # --- learning -----------------------------------------------------
    def learn_vector(self, features, target, label=None,
                     learning_rate=None, confidence=None):
        self.learn_vector_calls.append({
            "label": label,
            "learning_rate": learning_rate,
            "confidence": confidence,
        })
        return 0.123


class TestSpatialGroundingInSubmitMultimodal(unittest.TestCase):
    """submit_multimodal must emit ground_word(modality=4) at least once
    when the description contains somatosensory keywords."""

    def setUp(self):
        self.mod = _import_immerse_athena()

    def test_somato_description_emits_spatial_grounding(self):
        brain = StubBrain()
        # "rough", "stone", "cold", "heavy" are all in _SOMATO_KW so the
        # somato block fires; "rough stone" provides 2+ content tokens.
        desc = "The rough cold stone feels heavy under your hand."
        self.mod.submit_multimodal(brain, desc)

        # At least one ground_word call must have modality=SPATIAL.
        spatial = [c for c in brain.ground_word_calls
                   if c["modality"] == GL_MODALITY_SPATIAL]
        self.assertGreater(
            len(spatial), 0,
            "submit_multimodal must call ground_word with modality=4 "
            "(SPATIAL) when somato keywords are present in description")

        # Spec: SPATIAL grounding uses attention=0.6 (lower than visual
        # 0.7 because the encoding is keyword-derived).
        for call in spatial:
            self.assertAlmostEqual(call["attention"], 0.6, places=5)

        # Bound features must be the 64-dim somato vector.
        for call in spatial:
            self.assertEqual(len(call["features"]), 64)

        # Tokens must be content words — the function-word stoplist is
        # applied via _tokenize_for_grounding. "the", "under", "your"
        # are filtered; "rough" / "stone" / "cold" / "heavy" survive.
        bound_words = {c["word"] for c in spatial}
        self.assertTrue(
            bound_words & {"rough", "stone", "cold", "heavy", "feels", "hand"},
            f"Expected at least one content word, got {bound_words}")

    def test_no_somato_keywords_skips_spatial_grounding(self):
        """Descriptions without somato keywords should NOT emit
        modality=4 grounding (gating prevents empty bindings)."""
        brain = StubBrain()
        # No words from _SOMATO_KW — pure visual content.
        desc = "blue sky bright cloud high"
        self.mod.submit_multimodal(brain, desc)
        spatial = [c for c in brain.ground_word_calls
                   if c["modality"] == GL_MODALITY_SPATIAL]
        self.assertEqual(
            len(spatial), 0,
            "No somato keywords -> no SPATIAL grounding (avoids empty "
            "vector bindings)")


class TestMotorGroundingInWorldModelCurriculum(unittest.TestCase):
    """WorldModelCurriculum must emit ground_word(modality=2) with the
    scenario verb tag and the action vector during epoch dispatch."""

    def setUp(self):
        self.wmc_mod = _import_world_model_curriculum()

    def _run_chemistry_slice(self):
        """Chemistry scenarios are pure Python (no C lib needed) so they
        give a clean test slice without _ensure_physics()."""
        brain = StubBrain()
        wmc = self.wmc_mod.WorldModelCurriculum(brain_proxy=brain, max_level=1)
        # Level 1 chemistry = scenario_phase_transition only; verb="melt".
        wmc.run_chemistry_epoch(level=1, scenarios_per_level=1)
        return brain

    def test_chemistry_emits_motor_grounding_with_verb(self):
        brain = self._run_chemistry_slice()
        motor = [c for c in brain.ground_word_calls
                 if c["modality"] == GL_MODALITY_MOTOR]
        self.assertGreater(
            len(motor), 0,
            "WorldModelCurriculum must call ground_word with modality=2 "
            "(MOTOR) at least once per scenario")

        # Spec: MOTOR grounding uses attention=0.5.
        for call in motor:
            self.assertAlmostEqual(call["attention"], 0.5, places=5)

        # Verb for Level 1 chemistry (phase transition) is "melt".
        words = {c["word"] for c in motor}
        self.assertIn(
            "melt", words,
            f"Expected verb 'melt' for scenario_phase_transition, "
            f"got {words}")

    def test_motor_features_are_action_vector_length(self):
        """The vector bound under MOTOR must be a 1024-dim spread-encoded
        action vector — same dim as features so the lexicon stays
        layout-consistent."""
        brain = self._run_chemistry_slice()
        motor = [c for c in brain.ground_word_calls
                 if c["modality"] == GL_MODALITY_MOTOR]
        self.assertGreater(len(motor), 0)
        for call in motor:
            self.assertEqual(
                len(call["features"]), 1024,
                "Action vector must be spread-encoded to 1024 dims")

    def test_scenario_verbs_table_covers_all_scenarios(self):
        """Every scenario in PHYSICS/CHEMISTRY/BIOLOGY_SCENARIOS must
        have a verb entry. Otherwise dispatch silently skips MOTOR
        grounding for that scenario."""
        wmc_cls = self.wmc_mod.WorldModelCurriculum
        all_scenarios = set()
        for table in (wmc_cls.PHYSICS_SCENARIOS,
                      wmc_cls.CHEMISTRY_SCENARIOS,
                      wmc_cls.BIOLOGY_SCENARIOS):
            for level_gens in table.values():
                for gen in level_gens:
                    all_scenarios.add(gen.__name__)
        missing = all_scenarios - set(wmc_cls.SCENARIO_VERBS.keys())
        self.assertEqual(
            missing, set(),
            f"SCENARIO_VERBS missing entries for: {missing}")

    def test_brain_without_ground_word_does_not_raise(self):
        """If the brain proxy doesn't expose ground_word (older binding),
        epoch dispatch must continue without raising — graceful
        degradation while parallel agents wire the RPC plumbing."""
        class LegacyBrain:
            def __init__(self):
                self.learn_calls = 0

            def learn_vector(self, *args, **kwargs):
                self.learn_calls += 1
                return 0.0
            # NOTE: no ground_word attribute.

        brain = LegacyBrain()
        wmc = self.wmc_mod.WorldModelCurriculum(brain_proxy=brain, max_level=1)
        wmc.run_chemistry_epoch(level=1, scenarios_per_level=1)
        # learn_vector still fired even though ground_word is absent.
        self.assertGreater(brain.learn_calls, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
