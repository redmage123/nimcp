#!/usr/bin/env python3
"""Tests for the Layer A confabulation-mitigation gate (2026-04-26).

The gate lives in scripts/brain_daemon.py and substitutes predict-style
responses with "I don't know" when uncertainty signals fire. Three
signals (any one trips):
  - top-1 confidence < IDK_CONFIDENCE_THRESHOLD
  - output Shannon entropy / log(n) > IDK_ENTROPY_RATIO
  - OOD score > IDK_OOD_THRESHOLD

The downstream consumer is scripts/tests/batteries.py:run_metacognition_dk
which scans the response text for keywords like "don't know" / "cannot" /
"unknown". When the gate is hit, those keywords are guaranteed in the
response, so HIGH_CONFABULATION drops.

These tests are split into:
  - source-grep tests: confirm the helper exists and is wired into all
    three RPC handlers + counter is bumped (no daemon needed)
  - logic tests: import the helper, drive it with a mock brain

Run:
    python3 -m pytest tests/unit/test_idk_gate.py -v
or:
    python3 tests/unit/test_idk_gate.py
"""
from __future__ import annotations

import math
import os
import re
import sys
from pathlib import Path
from unittest.mock import MagicMock

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

DAEMON_SRC = (REPO_ROOT / "scripts" / "brain_daemon.py").read_text()


def _import_brain_daemon():
    """Import brain_daemon module. Skip the test if the C extension or
    other heavy deps aren't available — the helper is pure Python so it
    should always import."""
    if "brain_daemon" in sys.modules:
        return sys.modules["brain_daemon"]
    return __import__("brain_daemon")


# =============================================================================
# Section 1 — Source-grep tests (no daemon required)
# =============================================================================

def test_helper_function_exists_in_source():
    """_apply_idk_gate must be defined as a module-level function."""
    assert re.search(
        r"^def _apply_idk_gate\(result, brain, stats=None\)",
        DAEMON_SRC, re.MULTILINE,
    ), "_apply_idk_gate(result, brain, stats=None) must be defined in brain_daemon.py"


def test_predict_with_confidence_calls_gate():
    """_cmd_predict_with_confidence must route results through _apply_idk_gate."""
    m = re.search(
        r"def _cmd_predict_with_confidence\(self, req\):.*?(?=\n    def )",
        DAEMON_SRC, re.DOTALL,
    )
    assert m, "_cmd_predict_with_confidence handler must exist"
    body = m.group(0)
    assert "_apply_idk_gate" in body, (
        "_cmd_predict_with_confidence must call _apply_idk_gate so the "
        "Layer A gate fires on the primary RPC path"
    )


def test_predict_calls_gate():
    """_cmd_predict must route results through _apply_idk_gate."""
    m = re.search(
        r"def _cmd_predict\(self, req\):.*?(?=\n    def )",
        DAEMON_SRC, re.DOTALL,
    )
    assert m, "_cmd_predict handler must exist"
    body = m.group(0)
    assert "_apply_idk_gate" in body, (
        "_cmd_predict must call _apply_idk_gate so legacy callers also "
        "benefit from the Layer A confabulation gate"
    )


def test_predict_with_deadline_calls_gate():
    """_cmd_predict_with_deadline must route results through _apply_idk_gate."""
    m = re.search(
        r"def _cmd_predict_with_deadline\(self, req\):.*?(?=\n    def )",
        DAEMON_SRC, re.DOTALL,
    )
    assert m, "_cmd_predict_with_deadline handler must exist"
    body = m.group(0)
    assert "_apply_idk_gate" in body, (
        "_cmd_predict_with_deadline must call _apply_idk_gate"
    )


def test_all_threshold_env_var_names_referenced():
    """All four env-var names must be referenced in brain_daemon.py so
    operators can tune the gate without code edits."""
    expected = [
        "NIMCP_IDK_CONFIDENCE",
        "NIMCP_IDK_ENTROPY_RATIO",
        "NIMCP_IDK_OOD",
        "NIMCP_IDK_GATE",
    ]
    for name in expected:
        assert name in DAEMON_SRC, (
            f"env-var {name} must be referenced in brain_daemon.py "
            f"so the threshold is operator-tunable"
        )


def test_idk_gate_trips_counter_referenced():
    """The counter `idk_gate_trips` must be both initialized and bumped."""
    assert "idk_gate_trips" in DAEMON_SRC, (
        "the gate must bump self._stats['idk_gate_trips']"
    )
    # Initialization in _stats dict.
    assert re.search(
        r'"idk_gate_trips":\s*0', DAEMON_SRC,
    ), "idk_gate_trips must be initialized to 0 in BrainService._stats"
    # And incremented somewhere (helper does this).
    assert re.search(
        r'idk_gate_trips.*\+\s*1', DAEMON_SRC,
    ), "idk_gate_trips must be incremented somewhere in the gate helper"


# =============================================================================
# Section 2 — Logic tests (import the helper, drive with mock brain)
# =============================================================================

def _make_brain_no_internal_state():
    """Mock brain that has no get_internal_state, so the OOD signal is
    purely opportunistic and falls back to the result dict."""
    brain = MagicMock(spec=[])  # no attrs at all
    return brain


def _make_brain_with_internal_state(state=None):
    """Mock brain whose get_internal_state returns the given dict."""
    brain = MagicMock()
    brain.get_internal_state.return_value = state or {}
    return brain


def test_logic_high_confidence_low_entropy_passes_through():
    """A confident, peaked, in-distribution response must NOT be substituted."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # Confidence 0.95, peaked distribution → low entropy ratio.
    result = {
        "label": "red",
        "confidence": 0.95,
        "probabilities": [0.95, 0.02, 0.02, 0.01],
    }
    out = mod._apply_idk_gate(result, brain, stats)
    assert out is result, "high-confidence result must pass through unchanged"
    assert stats["idk_gate_trips"] == 0


def test_logic_low_confidence_substitutes_idk():
    """confidence=0.10 must trip the gate with reason='low_confidence'."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    result = {"label": "red", "confidence": 0.10}
    out = mod._apply_idk_gate(result, brain, stats)
    assert isinstance(out, dict)
    assert out.get("idk_gate") is True
    assert out.get("answer") == "I don't know"
    assert out.get("label") == "I don't know"
    assert "low_confidence" in out.get("reason", "")
    assert stats["idk_gate_trips"] == 1


def test_logic_high_entropy_substitutes_idk():
    """A flat distribution must trip via the entropy signal."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # 8 classes uniform → entropy ratio = 1.0 (well above 0.70).
    # Confidence 0.50 to keep us above the low_confidence threshold.
    result = {
        "label": "?",
        "confidence": 0.50,
        "probabilities": [0.125] * 8,
    }
    out = mod._apply_idk_gate(result, brain, stats)
    assert out.get("idk_gate") is True
    assert "high_entropy" in out.get("reason", "")
    assert stats["idk_gate_trips"] == 1


def test_logic_high_ood_score_substitutes_idk():
    """A high OOD score (above IDK_OOD_THRESHOLD) must trip with reason='ood'."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # High confidence + peaked dist → only OOD can trip the gate.
    result = {
        "label": "red",
        "confidence": 0.90,
        "probabilities": [0.90, 0.05, 0.03, 0.02],
        "ood_score": 5.0,  # well above the 1.0 default
    }
    out = mod._apply_idk_gate(result, brain, stats)
    assert out.get("idk_gate") is True
    assert "ood" in out.get("reason", "")
    assert stats["idk_gate_trips"] == 1


def test_logic_multiple_trips_comma_joined_reason():
    """When multiple signals fire, the reason must be comma-joined."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # Low confidence AND flat dist AND high OOD → all three trip.
    result = {
        "label": "?",
        "confidence": 0.05,
        "probabilities": [0.25, 0.25, 0.25, 0.25],
        "ood_score": 9.9,
    }
    out = mod._apply_idk_gate(result, brain, stats)
    reason = out.get("reason", "")
    assert "low_confidence" in reason
    assert "high_entropy" in reason
    assert "ood" in reason
    assert "," in reason, "multiple trips must be comma-joined"
    # Counter still bumps exactly once per call, regardless of N reasons.
    assert stats["idk_gate_trips"] == 1


def test_logic_disabled_via_env_bypasses_gate(monkeypatch=None):
    """Setting NIMCP_IDK_GATE=0 must bypass the gate entirely."""
    mod = _import_brain_daemon()
    # Toggle the module-level flag — equivalent to the env var being 0.
    saved = mod.IDK_GATE_ENABLED
    try:
        mod.IDK_GATE_ENABLED = False
        brain = _make_brain_no_internal_state()
        stats = {"idk_gate_trips": 0}
        result = {"label": "red", "confidence": 0.01}  # would normally trip
        out = mod._apply_idk_gate(result, brain, stats)
        assert out is result, "with gate disabled, result must pass through"
        assert stats["idk_gate_trips"] == 0
    finally:
        mod.IDK_GATE_ENABLED = saved


def test_logic_counter_increments_exactly_once_per_trip():
    """Each gate trip bumps the counter by exactly one — no double-counting
    even when multiple signals are active."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # Three separate calls, each tripping a different reason combo.
    for _ in range(3):
        result = {"label": "?", "confidence": 0.05}
        mod._apply_idk_gate(result, brain, stats)
    assert stats["idk_gate_trips"] == 3, (
        "counter must bump exactly once per call that trips the gate"
    )

    # Now a non-tripping call — counter must NOT advance.
    mod._apply_idk_gate({"label": "red", "confidence": 0.99}, brain, stats)
    assert stats["idk_gate_trips"] == 3


# =============================================================================
# Section 3 — Helper extraction primitives (lightweight invariants)
# =============================================================================

def test_helper_extract_confidence_from_dict():
    """The confidence extractor handles dicts with the canonical key."""
    mod = _import_brain_daemon()
    assert mod._idk_extract_confidence({"confidence": 0.42}) == 0.42
    assert mod._idk_extract_confidence({"adjusted_confidence": 0.7}) == 0.7
    # Tuple form (label, confidence) — predict() fallback path.
    assert mod._idk_extract_confidence(("red", 0.33)) == 0.33
    # No confidence available → None (signal unavailable, not zero).
    assert mod._idk_extract_confidence({"label": "red"}) is None


def test_helper_entropy_ratio_uniform_is_one():
    """Uniform distribution over n classes → entropy ratio = 1.0."""
    mod = _import_brain_daemon()
    ratio = mod._idk_compute_entropy_ratio([0.25, 0.25, 0.25, 0.25])
    assert ratio is not None
    assert abs(ratio - 1.0) < 1e-6


def test_helper_entropy_ratio_one_hot_is_zero():
    """One-hot distribution → entropy ratio = 0.0."""
    mod = _import_brain_daemon()
    ratio = mod._idk_compute_entropy_ratio([1.0, 0.0, 0.0, 0.0])
    assert ratio is not None
    assert ratio < 1e-6


def test_helper_ood_extracted_from_internal_state():
    """When the result lacks an OOD field, the helper falls back to
    brain.get_internal_state() — so brains that expose an OOD score there
    still gate correctly."""
    mod = _import_brain_daemon()
    brain = _make_brain_with_internal_state({"ood_score": 4.0})
    score = mod._idk_extract_ood_score(brain, {"label": "red", "confidence": 0.9})
    assert score == 4.0


def test_helper_no_signals_means_no_substitution():
    """If neither confidence nor entropy nor OOD is observable, the helper
    must NOT substitute — silent fallback, not over-eager refusal."""
    mod = _import_brain_daemon()
    brain = _make_brain_no_internal_state()
    stats = {"idk_gate_trips": 0}
    # Result with no recognizable fields whatsoever.
    result = {"opaque_payload": "..."}
    out = mod._apply_idk_gate(result, brain, stats)
    assert out is result
    assert stats["idk_gate_trips"] == 0


# =============================================================================
# Module entrypoint — allow `python3 tests/unit/test_idk_gate.py` without pytest
# =============================================================================

if __name__ == "__main__":
    failures = []
    names = sorted(n for n in globals() if n.startswith("test_"))
    for name in names:
        fn = globals()[name]
        try:
            fn()
            print(f"PASS  {name}")
        except AssertionError as e:
            print(f"FAIL  {name}: {e}")
            failures.append(name)
        except Exception as e:
            print(f"ERROR {name}: {type(e).__name__}: {e}")
            failures.append(name)
    if failures:
        print(f"\n{len(failures)} of {len(names)} tests failed")
        sys.exit(1)
    print(f"\nAll {len(names)} tests passed.")
