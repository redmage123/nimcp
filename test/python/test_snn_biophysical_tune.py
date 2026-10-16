#!/usr/bin/env python3
"""Smoke test for SNN biophysical tunables exposed to Python (Wave A + B1).

Verifies that each of the 15 new `snn_tune` names round-trips through the
Python binding: set a known value, then read it back via `snn_tune_get` and
assert the dict carries it. Also verifies out-of-range values are silently
clamped (the C-level setter is a no-op, so the prior value remains).

Runs in-process (no daemon / no socket required). Matches the pattern in
tests/smoke/test_python_bindings.py: `nimcp.Brain(name, input_dim, output_dim)`.
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
# so the getter should continue to return the prior valid value.
TUNABLES = [
    # (name,                             good,   bad)
    ("anti_reward_enabled",              0.0,    None),   # bool-like: no clamp rejection
    ("anti_reward_threshold_ratio",      2.5,    0.5),    # must be > 1.0
    ("anti_reward_gain",                 0.75,   -1.0),   # must be >= 0
    ("depression_inc",                   0.25,   -0.1),   # must be in [0, 1]
    ("depression_tau_ms",                75.0,   0.5),    # must be >= 1
    ("depression_cap",                   0.4,    2.0),    # must be in [0, 1]
    ("ahp_enabled",                      1.0,    None),   # bool-like: no clamp rejection
    ("ahp_tau_ms",                       200.0,  -1.0),   # must be >= 1
    ("ahp_gain_mv",                      0.8,    100.0),  # must be in [0, 50]
    ("pump_enabled",                     1.0,    None),   # bool-like: no clamp rejection
    ("pump_tau_ms",                      4000.0, 0.5),    # must be >= 1
    ("pump_gain_mv",                     0.1,    -0.5),   # must be in [0, 50]
    ("basket_enabled",                   1.0,    None),   # bool-like: no clamp rejection
    ("basket_fraction",                  0.15,   0.8),    # must be in [0.01, 0.5]
    ("noise_ei_ratio",                   0.3,    1.5),    # must be in [0, 1]
    # Substrate adapter (Phase 1 biological-substrate wiring)
    ("substrate_enabled",                1.0,    None),   # bool-like: no clamp rejection
    ("substrate_update_period",          25.0,   0.5),    # must be in [1, 10000]
    ("substrate_spike_dropout_on",       1.0,    None),   # bool-like: no clamp rejection
    ("substrate_plasticity_mod_on",      1.0,    None),   # bool-like: no clamp rejection
]

EPS = 1e-5


def _build_brain():
    """Construct a tiny in-process brain for binding smoke tests."""
    import nimcp
    return nimcp.Brain("snn_biophysical_tune_smoke", 128, 10)


def test_all_new_tunables_present():
    """Every new name must appear in the snn_tune_get dict on a fresh brain."""
    b = _build_brain()
    params = b.snn_tune_get()
    missing = [name for name, _good, _bad in TUNABLES if name not in params]
    if missing:
        raise AssertionError(
            f"snn_tune_get missing {len(missing)} biophysical keys: {missing}"
        )
    print(f"  PASS: all {len(TUNABLES)} biophysical tunables exposed in snn_tune_get()")


def test_round_trip_set_get():
    """Setting a valid value must be reflected in the next snn_tune_get."""
    b = _build_brain()
    failed = []
    for name, good, _bad in TUNABLES:
        ok = b.snn_tune(name, good)
        if not ok:
            failed.append(f"{name}: snn_tune returned falsy")
            continue
        params = b.snn_tune_get()
        got = params.get(name)
        if got is None:
            failed.append(f"{name}: missing from get()")
            continue
        if abs(float(got) - float(good)) > EPS:
            failed.append(f"{name}: set={good} got={got}")
    if failed:
        raise AssertionError(f"Round-trip failures ({len(failed)}):\n  " + "\n  ".join(failed))
    print(f"  PASS: {len(TUNABLES)} tunables round-trip through snn_tune -> snn_tune_get")


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
        b.snn_tune(name, good)
        before = float(b.snn_tune_get()[name])
        # Attempt the bad value. The setter may either reject silently (no-op)
        # or raise. Both are acceptable; what's NOT acceptable is the bad value
        # being accepted and overwriting the good one.
        try:
            b.snn_tune(name, bad)
        except Exception:
            pass  # setter raised — that's fine
        after = float(b.snn_tune_get()[name])
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


def test_defaults_reasonable():
    """Sanity check default values match Wave A + B1 design:
    ahp/pump/basket/anti_reward default-enabled; depression cap in [0,1]; etc."""
    b = _build_brain()
    params = b.snn_tune_get()
    expect_on = ["anti_reward_enabled", "ahp_enabled", "pump_enabled", "basket_enabled"]
    off = [k for k in expect_on if float(params[k]) == 0.0]
    if off:
        raise AssertionError(
            f"expected these to default-ON: {expect_on}, but these were off: {off}"
        )
    # Range sanity
    dep_cap = float(params["depression_cap"])
    if not (0.0 <= dep_cap <= 1.0):
        raise AssertionError(f"depression_cap default out of range [0,1]: {dep_cap}")
    ei = float(params["noise_ei_ratio"])
    if not (0.0 <= ei <= 1.0):
        raise AssertionError(f"noise_ei_ratio default out of range [0,1]: {ei}")
    bf = float(params["basket_fraction"])
    if not (0.01 <= bf <= 0.5):
        raise AssertionError(f"basket_fraction default out of range [0.01, 0.5]: {bf}")

    # Substrate adapter defaults (Phase 1): all three enables default ON, update
    # period defaults to 10 steps (in valid range [1, 10000]).
    expected_substrate = {
        "substrate_enabled":           1.0,
        "substrate_spike_dropout_on":  1.0,
        "substrate_plasticity_mod_on": 1.0,
    }
    missing_sub = [k for k in expected_substrate if k not in params]
    if missing_sub:
        raise AssertionError(f"substrate keys missing from snn_tune_get: {missing_sub}")
    sub_off = [k for k, want in expected_substrate.items() if float(params[k]) != want]
    if sub_off:
        raise AssertionError(
            f"expected substrate enables default-ON ({expected_substrate}), but got: "
            + ", ".join(f"{k}={params[k]}" for k in sub_off)
        )
    sup = float(params["substrate_update_period"])
    if not (1.0 <= sup <= 10000.0):
        raise AssertionError(f"substrate_update_period default out of range [1,10000]: {sup}")
    print(f"  PASS: Wave A + B1 + substrate defaults match design (7 enables ON, ranges sane)")


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
        print(f"[snn_biophysical_tune] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        print(f"\n{len(failures)} of {len(tests)} tests failed")
        sys.exit(1)
    print(f"\nAll {len(tests)} biophysical tunable smoke tests passed.")


if __name__ == "__main__":
    main()
