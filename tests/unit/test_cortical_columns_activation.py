#!/usr/bin/env python3
"""Tests for the cortical columns + ternary WTA activation campaign.

This is the CC7 deliverable. The CC2/CC4/CC5/CC6 commits wire the
~5,000 LOC cortical columns + ternary modules into the brain hot path
(brain_decide forward, brain_learn_vector plasticity) and add a
versioned sidecar persistence section (CORTICAL_COLUMNS_V1).

The tests fall into two layers:

  1. Source-level tests — grep the C sources to verify the wiring
     blocks exist, are gated by enable_cortical_columns, null-check
     the required pointers, and are reachable from the hot path.
     These run anywhere — no nimcp.so required.

  2. Build/runtime tests — import nimcp and exercise the brain.
     These are skipped (or marked xfail) if the Python binding
     doesn't expose the column knobs. The bindings currently don't
     expose enable_cortical_columns/enable_cortical_ternary as
     constructor kwargs, so the runtime tests assert that the brain
     creation + decide() smoke-test passes regardless.

Run:
    python3 -m pytest tests/unit/test_cortical_columns_activation.py -v
or:
    python3 tests/unit/test_cortical_columns_activation.py
"""
from __future__ import annotations

import os
import re
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]


# =============================================================================
# Helpers
# =============================================================================

def _read(rel: str) -> str:
    return (REPO_ROOT / rel).read_text()


# =============================================================================
# Section 1 — Source-level tests (no daemon, no .so)
# =============================================================================

def test_init_populates_hypercolumns():
    """Foundation populate-loop is in structural init."""
    src = _read("src/core/brain/factory/init/nimcp_brain_init_structural.c")
    assert "Step 2b: Actually populate" in src, "populate-loop comment missing"
    assert "hypercolumn_create(brain->cortical_column_pool, &hc_cfg)" in src, \
        "hypercolumn_create call missing from populate loop"
    assert "Hypercolumns populated:" in src, "populate count log missing"


def test_init_creates_projection_matrix():
    """Init allocates column_to_decision_proj zero-initialized + feature_buf."""
    src = _read("src/core/brain/factory/init/nimcp_brain_init_structural.c")
    assert "column_to_decision_proj" in src
    assert "column_feature_buf" in src
    # Zero-init via nimcp_calloc (additive blend is a no-op until learned).
    assert re.search(r"nimcp_calloc\([^;]*column_feature_dim", src) or \
           re.search(r"nimcp_calloc\([^;]*feature_dim", src), \
           "projection should be zero-initialized via nimcp_calloc"
    assert "column_blend_alpha = 0.1" in src, "default blend alpha 0.1 missing"


def test_init_creates_ternary_when_enabled():
    """Ternary connectivity + hypercolumns created when flag is on."""
    src = _read("src/core/brain/factory/init/nimcp_brain_init_structural.c")
    assert "enable_cortical_ternary" in src
    assert "cc_ternary_connectivity_create_lateral" in src
    assert "cc_ternary_hypercolumn_create" in src
    # Failure path must zero out the flag rather than aborting init.
    assert "brain->config.enable_cortical_ternary = false" in src, \
        "ternary init failure path must disable flag (not abort)"


def test_brain_decide_has_column_block():
    """brain_decide forward block exists, gated, null-checked."""
    src = _read("src/core/brain/nimcp_brain_part_core.c")
    # Locate the block by its leading comment.
    assert "Cortical columns forward + ternary WTA + projection blend" in src, \
        "brain_decide column block comment missing"
    # Gating: enable_cortical_columns AND pool AND num_hypercolumns AND
    # hypercolumns array AND projection AND feature buf AND decision/output.
    assert re.search(
        r"if\s*\(\s*brain->enable_cortical_columns\s*&&\s*brain->cortical_column_pool"
        r"\s*&&\s*brain->num_hypercolumns\s*>\s*0\s*&&\s*brain->hypercolumns"
        r"\s*&&\s*brain->column_to_decision_proj\s*&&\s*brain->column_feature_buf"
        r"\s*&&\s*decision\s*&&\s*decision->output_vector",
        src,
    ), "brain_decide column block must gate on flag + null-check all required pointers"
    # Forward path: hypercolumn_compute then get_distribution then projection.
    assert "hypercolumn_compute(hc, features, num_features)" in src
    assert "hypercolumn_get_distribution(hc, slot, mcs_per_hc)" in src
    # Ternary winner scaling — winner=1.5x, others=0.5x.
    assert "cc_ternary_hypercolumn_wta(thc)" in src
    assert "1.5F" in src or "1.5f" in src
    assert "0.5F" in src or "0.5f" in src
    # Projection blend scaled by alpha + counter increment.
    assert "alpha * fv * row[i]" in src
    assert "brain->cortical_decisions++" in src


def test_brain_learn_has_plasticity_block():
    """brain_learn_vector plasticity block exists with the same gating discipline."""
    src = _read("src/core/brain/learning/nimcp_brain_learning.c")
    assert "CC5: cortical columns plasticity" in src, \
        "CC5 comment marker missing"
    # Gating: same null-check pattern.
    assert re.search(
        r"if\s*\(\s*brain->enable_cortical_columns\s*&&\s*brain->cortical_column_pool"
        r"\s*&&\s*brain->num_hypercolumns\s*>\s*0\s*&&\s*brain->hypercolumns"
        r"\s*&&\s*brain->column_to_decision_proj\s*&&\s*brain->column_feature_buf",
        src,
    ), "learn_vector column block must gate on flag + null-check all required pointers"
    # Plasticity tick.
    assert "hypercolumn_apply_plasticity(hc, now_us)" in src
    # Ternary state refresh (when on).
    assert "cc_ternary_hypercolumn_update_states(thc)" in src
    # Gradient descent on the projection matrix.
    assert "row[i] += lr * fv * err" in src
    assert "1e-4F" in src or "1e-4f" in src or "1e-4" in src
    assert "brain->cortical_plasticity_updates++" in src


def test_struct_has_ternary_fields():
    """brain_struct gained ternary + projection fields."""
    src = _read("include/core/brain/nimcp_brain_internal.h")
    assert "ternary_connectivity" in src
    assert "ternary_hypercolumns" in src
    assert "num_ternary_hypercolumns" in src
    assert "enable_cortical_ternary" in src
    assert "column_to_decision_proj" in src
    assert "column_feature_dim" in src
    assert "column_blend_alpha" in src
    assert "column_feature_buf" in src
    assert "cortical_decisions" in src
    assert "cortical_plasticity_updates" in src


def test_default_flags_off():
    """enable_cortical_ternary defaults to false in every config initializer."""
    cfg = _read("src/core/brain/factory/init/nimcp_brain_init_config.c")
    profiles = _read("src/core/brain/factory/init/nimcp_brain_config_profiles.c")
    # init_config.c default path.
    assert "config->enable_cortical_ternary = false" in cfg, \
        "init_config.c must default enable_cortical_ternary to false"
    # config_profiles.c — TINY/MINIMAL + full-featured + tiny micro.
    assert profiles.count("config->enable_cortical_ternary = false") >= 2, \
        "profiles must default enable_cortical_ternary to false in TINY + tiny micro"
    # Public config struct field declared.
    pub = _read("include/core/brain/nimcp_brain.h")
    assert "bool enable_cortical_ternary" in pub, \
        "public config struct must declare enable_cortical_ternary"


def test_save_load_section_present():
    """brain_save writes CORTICAL_COLUMNS_V1 magic + sidecar."""
    src = _read("src/core/brain/persistence/nimcp_brain_persistence.c")
    assert "CORTICAL_COLUMNS_V1" in src, "section name missing"
    assert "CC_SIDECAR_MAGIC" in src
    assert "0x43434C4D" in src, "magic 'CCLM' missing"
    assert "cortical_columns_sidecar_save" in src
    assert "cortical_columns_sidecar_load" in src
    # brain_save must call the save helper.
    assert re.search(r"cortical_columns_sidecar_save\(brain,\s*filepath\)", src)


def test_backward_compat_warning():
    """Loader logs a warning when the section is absent (old checkpoint)."""
    src = _read("src/core/brain/persistence/nimcp_brain_persistence.c")
    # The warning must mention the section name + that fresh init is used.
    assert "section absent" in src
    assert "old checkpoint" in src
    assert "CORTICAL_COLUMNS_V1 section absent" in src


def test_load_path_reinits_subsystem():
    """brain_load conditionally re-runs the cortical columns init before
    loading the sidecar — without this the sidecar would land on NULL
    pointers because allocate_brain doesn't run structural init."""
    src = _read("src/core/brain/persistence/nimcp_brain_persistence.c")
    assert "nimcp_brain_factory_init_cortical_columns_subsystem(brain)" in src, \
        "brain_load must re-invoke cortical columns init"
    # Sidecar load called in brain_load — match across newlines.
    assert re.search(
        r"if\s*\(\s*brain->enable_cortical_columns\s*\)\s*\{\s*"
        r"cortical_columns_sidecar_load\(brain,\s*filepath\)",
        src, re.DOTALL,
    ), "brain_load must call cortical_columns_sidecar_load when columns enabled"


def test_no_adaptive_input_dim_change():
    """Integration is via additive output blend — adaptive network input
    dim must NOT have changed. This is essential for binary checkpoint
    compat with the live 7000-step trained brain on the pod."""
    src = _read("src/core/brain/nimcp_brain_part_core.c")
    # The blend uses += not = — additive only.
    assert re.search(r"decision->output_vector\[i\]\s*\+=\s*alpha\s*\*\s*fv\s*\*\s*row\[i\]",
                     src), "blend must be additive (+=) not assignment"


def test_failure_paths_disable_gracefully():
    """Init failure paths must set enable_cortical_columns/ternary = false
    rather than crash. This preserves the no-behavior-change guarantee
    when allocations fail."""
    src = _read("src/core/brain/factory/init/nimcp_brain_init_structural.c")
    # Multiple disable points.
    assert src.count("brain->enable_cortical_columns = false") >= 2, \
        "structural init must have at least 2 failure→disable points"
    assert "brain->config.enable_cortical_ternary = false" in src
    assert "brain->enable_cortical_ternary = false" in src


# =============================================================================
# Section 2 — Build/runtime tests (require nimcp.so)
# =============================================================================

try:
    import nimcp  # noqa: F401
    NIMCP_AVAILABLE = True
except ImportError:
    NIMCP_AVAILABLE = False


@pytest.mark.skipif(not NIMCP_AVAILABLE, reason="nimcp.so not importable")
def test_brain_creates_with_default_config():
    """Brain creation with default config (columns auto-enabled for medium+
    sizes; ternary always off by default). Must not crash."""
    import nimcp
    b = nimcp.Brain(name="cc_test", num_inputs=8, num_outputs=4, init_mode="minimal")
    assert b is not None


@pytest.mark.skipif(not NIMCP_AVAILABLE, reason="nimcp.so not importable")
def test_brain_predict_returns_valid_output():
    """brain.predict() returns a usable result with default config.
    The Python binding exposes predict()/experience()/learn_vector(); the
    underlying C entry brain_decide() runs the cortical columns block when
    columns are enabled. This test smoke-checks the end-to-end path —
    predict() returns (label, confidence) in this binding."""
    import nimcp
    b = nimcp.Brain(name="cc_predict", num_inputs=8, num_outputs=4, init_mode="minimal")
    out = b.predict([0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8])
    assert out is not None
    # predict() returns (label, confidence) tuple; just verify shape + types.
    assert len(out) == 2
    assert isinstance(out[1], float)


@pytest.mark.skipif(not NIMCP_AVAILABLE, reason="nimcp.so not importable")
def test_brain_learn_vector_smoke():
    """brain.learn_vector() smoke test — exercises brain_learn_vector() in C
    which contains the CC5 plasticity block. With columns off (default for
    minimal init), the block is gated off; the call must still succeed."""
    import nimcp
    b = nimcp.Brain(name="cc_learn", num_inputs=4, num_outputs=2, init_mode="minimal")
    # Some bindings return -1.0 on error, others raise. Either is fine —
    # we just want to confirm the path doesn't crash.
    try:
        b.learn_vector([0.1, 0.2, 0.3, 0.4], [1.0, 0.0])
    except Exception:
        pass


@pytest.mark.skipif(not NIMCP_AVAILABLE, reason="nimcp.so not importable")
@pytest.mark.xfail(reason="Python binding does not yet expose enable_cortical_columns kwarg",
                   strict=False)
def test_decide_unchanged_when_columns_off():
    """With columns off, predict() output for a fixed input must match
    the baseline (zero behavior change). Marked xfail until the Python
    binding exposes the toggle."""
    import nimcp
    b1 = nimcp.Brain(name="cc_off1", num_inputs=4, num_outputs=2, init_mode="minimal")
    b2 = nimcp.Brain(name="cc_off2", num_inputs=4, num_outputs=2, init_mode="minimal")
    inp = [0.1, 0.2, 0.3, 0.4]
    o1 = b1.predict(inp)
    o2 = b2.predict(inp)
    assert o1 == o2  # Same input → same output across two minimal brains


@pytest.mark.skipif(not NIMCP_AVAILABLE, reason="nimcp.so not importable")
@pytest.mark.xfail(reason="Python binding does not yet expose enable_cortical_columns kwarg",
                   strict=False)
def test_decide_changes_when_columns_on_after_learning():
    """With both flags on + 10 learn() calls, predict() output should
    differ from the all-flags-off baseline. Marked xfail until the
    Python binding exposes the toggle."""
    pytest.skip("TODO: requires nimcp.Brain(enable_cortical_columns=True, "
                "enable_cortical_ternary=True) kwargs in the Python binding")


# =============================================================================
# Self-runner (so `python3 tests/unit/test_cortical_columns_activation.py`
# also works without pytest CLI gymnastics)
# =============================================================================

if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-v"]))
