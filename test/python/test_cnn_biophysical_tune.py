#!/usr/bin/env python3
"""Smoke test for CNN Phase 3 tunables exposed to Python (substrate + thalamic).

Verifies that each of the 7 new `cnn_tune` names round-trips through the
Python binding: set a known value, then read it back via `cnn_tune_get` and
assert the dict carries it. Also verifies out-of-range values are silently
clamped (the C-level setter is a no-op, so the prior value remains).

Mirrors the pattern in test_lnn_biophysical_tune.py. Runs in-process (no
daemon / no socket required).
"""
from __future__ import annotations

import os
import sys


def _prefer_worktree_build():
    """If a freshly-built nimcp.so lives next to this test tree's build_wt/
    output, prepend it to sys.path so we test the current branch's binding
    rather than any stale site-packages install. Safe no-op if absent.
    """
    here = os.path.dirname(os.path.abspath(__file__))
    # test/python/ -> ../../build_wt/lib/python
    candidates = [
        os.path.normpath(os.path.join(here, "..", "..", "build_wt", "lib", "python")),
        os.path.normpath(os.path.join(here, "..", "..", "build", "lib", "python")),
    ]
    # Allow explicit override via env var (CI / manual)
    explicit = os.environ.get("NIMCP_PY_BUILD_DIR")
    if explicit:
        candidates.insert(0, explicit)
    for c in candidates:
        if os.path.isfile(os.path.join(c, "nimcp.so")):
            sys.path.insert(0, c)
            return c
    return None


_prefer_worktree_build()


# Each entry: (name, safe_value_within_clamp, invalid_value_outside_clamp)
# Invalid values are silently rejected by the C setter (if-guarded assignment),
# so the getter should continue to return the prior valid value. `None` for bad
# means this is a bool-like setter (accepts any float).
TUNABLES = [
    # (name,                                   good,   bad)
    # Substrate adapter (Phase 3 biological-substrate wiring for CNN)
    ("substrate_enabled",                      1.0,    None),   # bool-like: no clamp rejection
    ("substrate_update_period",                25.0,   0.5),    # must be in [1, 10000]
    ("substrate_activation_mod_on",            1.0,    None),   # bool-like: no clamp rejection
    ("substrate_plasticity_mod_on",            1.0,    None),   # bool-like: no clamp rejection
    # Thalamic adapter (Phase 3 multi-network thalamic rollout)
    ("thalamic_enabled",                       1.0,    None),   # bool-like: no clamp rejection
    ("thalamic_featuremap_gain_on",            1.0,    None),   # bool-like: no clamp rejection
    ("thalamic_burst_dropout_reduce_on",       1.0,    None),   # bool-like: no clamp rejection
]

EPS = 1e-5


def _build_brain():
    """Construct a tiny in-process brain for binding smoke tests."""
    import nimcp
    return nimcp.Brain("cnn_biophysical_tune_smoke", 128, 10)


def test_defaults_reasonable():
    """Sanity check default values match Phase 3 design: all 7 default to 1.0,
    with substrate_update_period defaulting to 10.0 (steps between recomputes).
    """
    b = _build_brain()
    params = b.cnn_tune_get()

    # Substrate + thalamic enables/bools all default ON (1.0).
    expect_on = [
        "substrate_enabled",
        "substrate_activation_mod_on",
        "substrate_plasticity_mod_on",
        "thalamic_enabled",
        "thalamic_featuremap_gain_on",
        "thalamic_burst_dropout_reduce_on",
    ]
    off = [k for k in expect_on if float(params[k]) != 1.0]
    if off:
        raise AssertionError(
            f"expected these to default-1.0: {expect_on}, but these were not: "
            + ", ".join(f"{k}={params[k]}" for k in off)
        )
    sup = float(params["substrate_update_period"])
    if abs(sup - 10.0) > EPS:
        raise AssertionError(f"substrate_update_period default expected 10.0, got {sup}")
    # Range sanity on update period
    if not (1.0 <= sup <= 10000.0):
        raise AssertionError(f"substrate_update_period default out of range [1,10000]: {sup}")
    print(f"  PASS: Phase 3 CNN defaults match design (6 enables=1.0, update_period=10.0)")


def test_all_new_tunables_present():
    """Every new name must appear in the cnn_tune_get dict on a fresh brain."""
    b = _build_brain()
    params = b.cnn_tune_get()
    missing = [name for name, _good, _bad in TUNABLES if name not in params]
    if missing:
        raise AssertionError(
            f"cnn_tune_get missing {len(missing)} Phase 3 keys: {missing}"
        )
    print(f"  PASS: all {len(TUNABLES)} CNN tunables exposed in cnn_tune_get()")


def test_round_trip_set_get():
    """Setting a valid value must be reflected in the next cnn_tune_get."""
    b = _build_brain()
    failed = []
    for name, good, _bad in TUNABLES:
        ok = b.cnn_tune(name, good)
        if not ok:
            failed.append(f"{name}: cnn_tune returned falsy")
            continue
        params = b.cnn_tune_get()
        got = params.get(name)
        if got is None:
            failed.append(f"{name}: missing from get()")
            continue
        if abs(float(got) - float(good)) > EPS:
            failed.append(f"{name}: set={good} got={got}")
    if failed:
        raise AssertionError(f"Round-trip failures ({len(failed)}):\n  " + "\n  ".join(failed))
    print(f"  PASS: {len(TUNABLES)} tunables round-trip through cnn_tune -> cnn_tune_get")


def test_invalid_values_are_clamped():
    """Out-of-range inputs must be silent no-ops — the C setter if-guards the
    assignment. After attempting a bad value, the getter should still return the
    previously-set good value (not the bad one, not garbage)."""
    b = _build_brain()
    violated = []
    for name, good, bad in TUNABLES:
        if bad is None:
            continue  # bool-like setters accept any float (0 vs nonzero)
        # First, plant a known-good value so we can detect if the bad value
        # actually got through.
        b.cnn_tune(name, good)
        before = float(b.cnn_tune_get()[name])
        # Attempt the bad value. The setter may either reject silently (no-op)
        # or raise. Both are acceptable; what's NOT acceptable is the bad value
        # being accepted and overwriting the good one.
        try:
            b.cnn_tune(name, bad)
        except Exception:
            pass  # setter raised — that's fine
        after = float(b.cnn_tune_get()[name])
        # Clamp behavior: after == before (the if-guard rejected the write).
        # Accept either unchanged or equal-to-good; do NOT accept equal-to-bad.
        if abs(after - float(bad)) < EPS:
            violated.append(f"{name}: bad value {bad} was accepted (expected no-op)")
        elif abs(after - before) > EPS:
            violated.append(
                f"{name}: bad value {bad} changed state {before} -> {after}"
            )
    if violated:
        raise AssertionError(
            f"Clamp violations ({len(violated)}):\n  " + "\n  ".join(violated)
        )
    print(f"  PASS: invalid values silently clamped (no state corruption)")


def main():
    # Order matters: defaults_reasonable must run FIRST because the tunables
    # are process-global statics in the C library — later tests mutate them
    # and do not (and cannot) restore them.
    tests = [
        ("defaults_reasonable", test_defaults_reasonable),
        ("all_new_tunables_present", test_all_new_tunables_present),
        ("round_trip_set_get", test_round_trip_set_get),
        ("invalid_values_are_clamped", test_invalid_values_are_clamped),
    ]
    failures = []
    for name, fn in tests:
        print(f"[cnn_biophysical_tune] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        print(f"\n{len(failures)} of {len(tests)} tests failed")
        sys.exit(1)
    print(f"\nAll {len(tests)} CNN biophysical tunable smoke tests passed.")


if __name__ == "__main__":
    main()
