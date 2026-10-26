"""Tests for `scripts/v2_brain_adapter.py` — V2 Phase 7c shim.

Run with:
    PYTHONPATH=/tmp:scripts pytest scripts/test_v2_brain_adapter.py -q

Where `/tmp/nimcp_v2.so` is the built pybind shared object.
"""

from __future__ import annotations

import logging
import os
import random
import tempfile

import pytest

import nimcp_v2
import v2_brain_adapter as va


LAYERS = [4, 8, 3]
SEED = 0xC0FFEE


def _fresh_adapter() -> va.V2BrainAdapter:
    # Reset per-process warn tracker so each test sees a clean slate.
    va._WARNED.clear()
    return va.V2BrainAdapter(rng_seed=SEED, layers=LAYERS, activation="relu")


# -----------------------------------------------------------------------------
# (1) Direct pass-through
# -----------------------------------------------------------------------------
def test_learn_vector_matches_brain_learn() -> None:
    """learn_vector should thin-wrap Brain.learn with identical loss."""
    va._WARNED.clear()
    a = va.V2BrainAdapter(rng_seed=SEED, layers=LAYERS)
    import nimcp_v2

    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS)

    x = [0.1, 0.2, 0.3, 0.4]
    y = [1.0, 0.0, 0.0]
    # Both brains are deterministic on the same seed → same loss.
    loss_a = a.learn_vector(x, y, lr=0.01)
    loss_b = b.learn(x, y, 0.01)
    assert loss_a == pytest.approx(loss_b, rel=1e-6)


def test_predict_matches_brain_predict() -> None:
    a = va.V2BrainAdapter(rng_seed=SEED, layers=LAYERS)
    import nimcp_v2

    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS)
    x = [0.5, -0.5, 0.25, -0.25]
    assert a.predict(x) == pytest.approx(b.predict(x), rel=1e-6)
    assert a.probe(x) == pytest.approx(b.predict(x), rel=1e-6)


def test_save_and_load_roundtrip(tmp_path) -> None:
    """Adapter.save → a path the underlying Brain can reload from."""
    a = _fresh_adapter()
    x = [0.1, 0.2, 0.3, 0.4]
    # Train a step so the weights aren't just the initial seed.
    a.learn_vector(x, [1.0, 0.0, 0.0], lr=0.05)
    pred_before = a.predict(x)

    ckpt_dir = tmp_path / "ckpt"
    a.save(str(ckpt_dir))
    # Ensemble should have written something.
    assert ckpt_dir.is_dir() and any(ckpt_dir.iterdir())

    # Reload into a fresh underlying Brain via load_ensemble.
    import nimcp_v2

    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS)
    b.load_ensemble(str(ckpt_dir))
    assert b.predict(x) == pytest.approx(pred_before, rel=1e-6)


# -----------------------------------------------------------------------------
# (2) Safe no-op stubs
# -----------------------------------------------------------------------------
NOOP_SAMPLE_DEFAULTS = {
    "bg_get_conflict": 0.0,
    "bg_get_dopamine": 0.5,
    "sleep_get_state": "awake",
    "sleep_is_needed": False,
    "medulla_get_circadian_efficiency": 1.0,
}


def test_noop_methods_return_defaults_and_warn_once(caplog) -> None:
    """Sample 5 no-op methods: each returns its default + logs one WARNING."""
    a = _fresh_adapter()
    keys = list(NOOP_SAMPLE_DEFAULTS.keys())
    random.Random(0).shuffle(keys)

    caplog.set_level(logging.WARNING, logger="v2_brain_adapter")
    for name in keys:
        expected = NOOP_SAMPLE_DEFAULTS[name]
        got = getattr(a, name)()
        assert got == expected, f"{name} -> {got!r} != {expected!r}"
        # Calling a second time should NOT add another warning for that name.
        getattr(a, name)()

    # Exactly one warning record per sampled method, keyed by name in message.
    messages = [r.getMessage() for r in caplog.records if r.levelno == logging.WARNING]
    for name in keys:
        matching = [m for m in messages if f"'{name}'" in m]
        assert len(matching) == 1, (
            f"{name}: expected exactly 1 warning, got {len(matching)}: {matching}"
        )


def test_noop_list_default_is_fresh_copy() -> None:
    """Mutating one returned list must not poison the next call."""
    a = _fresh_adapter()
    first = a.octopus_explore_from_occipital()
    first.append("mutated")
    second = a.octopus_explore_from_occipital()
    assert second == []


# -----------------------------------------------------------------------------
# (3) Semantic-gap adapters
# -----------------------------------------------------------------------------
def test_train_cognitive_returns_adaptive_loss_dict() -> None:
    a = _fresh_adapter()
    out = a.train_cognitive([0.1, 0.2, 0.3, 0.4], [1.0, 0.0, 0.0], lr=0.01)
    assert isinstance(out, dict)
    assert "adaptive_loss" in out
    assert isinstance(out["adaptive_loss"], float)
    assert out["cognitive_loss"] == out["adaptive_loss"]


def test_decide_full_returns_output_dict() -> None:
    a = _fresh_adapter()
    result = a.decide_full([0.1, 0.2, 0.3, 0.4])
    assert isinstance(result, dict)
    assert "output" in result
    assert isinstance(result["output"], list)
    assert len(result["output"]) == LAYERS[-1]
    # Semantic gap: these are always None on V2.
    assert result["ethics"] is None


# -----------------------------------------------------------------------------
# (4) __getattr__ fallback
# -----------------------------------------------------------------------------
def test_unknown_method_returns_none_and_warns(caplog) -> None:
    a = _fresh_adapter()
    caplog.set_level(logging.WARNING, logger="v2_brain_adapter")
    assert a.foo_bar_baz(1, 2, k=3) is None
    messages = [r.getMessage() for r in caplog.records if r.levelno == logging.WARNING]
    assert any("foo_bar_baz" in m for m in messages)


# -----------------------------------------------------------------------------
# (5) Introspection
# -----------------------------------------------------------------------------
def test_get_neuron_count_matches_layer_widths() -> None:
    a = _fresh_adapter()
    # neuron_count = sum of layer widths = 4 + 8 + 3 = 15
    assert a.get_neuron_count() == sum(LAYERS)


def test_get_stats_has_adaptive_section() -> None:
    a = _fresh_adapter()
    stats = a.get_stats()
    assert isinstance(stats, dict)
    assert "adaptive" in stats
    assert stats["adaptive"]["layer_widths"] == LAYERS


# -----------------------------------------------------------------------------
# (6) Phase 9h — backend selector (Cpu / Gpu)
# -----------------------------------------------------------------------------
def test_backend_default_is_cpu() -> None:
    """Brain() with no `backend` arg should default to CPU."""
    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS)
    assert b.backend == "cpu"


def test_backend_explicit_cpu() -> None:
    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS, backend="cpu")
    assert b.backend == "cpu"


def test_backend_gpu_degrades_gracefully_on_cpu_build() -> None:
    """Backend='gpu' must boot on a CPU-only build by degrading to CPU
    forward path with a warning. The brain handle is fully functional
    afterward — Phase 9h's never-break-boot contract."""
    b = nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS, backend="gpu")
    # The reported backend mirrors the requested setting; whether the
    # GPU path is *actually* live depends on the build flags.
    assert b.backend == "gpu"
    # Inference still works (CPU forward in this build).
    out = b.predict([0.1] * LAYERS[0])
    assert len(out) == LAYERS[-1]


def test_backend_unknown_string_raises() -> None:
    import pytest
    with pytest.raises(ValueError, match="unknown backend"):
        nimcp_v2.Brain(rng_seed=SEED, layers=LAYERS, backend="cuda")


# -----------------------------------------------------------------------------
# (7) Language surface (V2 L8 + C4/C5) — real when `language` is configured.
# -----------------------------------------------------------------------------
def _language_adapter() -> va.V2BrainAdapter:
    import json

    va._WARNED.clear()
    cfg = {
        "rng_seed": SEED,
        "deterministic": True,
        "state_dir": "./nimcp-state",
        "adaptive": {"layers": LAYERS, "rng_seed": SEED, "activation": "Relu"},
        "language": {
            "semantic_dim": 8,
            "rng_seed": SEED,
            "enable_toxicity": True,
            "toxicity_ml_seed": 7,
            "spectrum_vocab_cap": None,
            "default_stage": 4,
            "min_produce_words": 1,
            "recurrent_max_iters": 1,
            "reason_in_content": False,
        },
    }
    return va.V2BrainAdapter(config_json=json.dumps(cfg))


def test_language_methods_stub_without_language(caplog) -> None:
    a = _fresh_adapter()
    with caplog.at_level(logging.WARNING, logger="v2_brain_adapter"):
        assert a.train_language("the dog runs") == {"loss": 0.0}
        assert a.generate_text("dog") == ""
        assert a.grounded_respond("dog")["confidence"] == 0.0
    assert any("train_language" in r.message for r in caplog.records)


def test_language_methods_real_with_language(caplog) -> None:
    a = _language_adapter()
    assert a._brain.has_language
    for _ in range(5):
        a.learn_language("the dog likes to run")
    out = a.train_language("the dog likes to run")
    assert 0.0 <= out["loss"] <= 1.0
    comp = a.comprehend("the dog")
    assert len(comp["vector"]) == 8
    assert comp["confidence"] > 0.0
    text = a.generate_text("the dog")
    assert isinstance(text, str) and text
    assert a.generate_text("the dog", max_tokens=1).count(" ") == 0
    r = a.grounded_respond("the dog")
    assert r["response"] and not r["blocked_for_toxicity"]
    conclusion = a.reason("what does the dog do")
    assert "conclusion" in conclusion and "lexicon_confidence" in conclusion
    a.set_reason_in_content(True)
    a.working_memory_add([0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0], 0.9)
    assert a.grounded_respond("the dog")["response"]
    # None of the language methods should have hit the no-op warning path.
    assert not any(
        n in va._WARNED
        for n in ("train_language", "learn_language", "generate_text", "grounded_respond", "comprehend", "reason")
    )
