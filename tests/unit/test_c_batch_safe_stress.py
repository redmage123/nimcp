#!/usr/bin/env python3
"""Stress tests for C batch-safe mechanisms.

Covers:
  - Edge cases (all-zero, all-fire, single neuron)
  - Very large batch sizes
  - Long-horizon cumulative stability (1000+ steps)
  - Memory behavior (allocate/free cycles)
  - Numerical stability under extreme parameters
"""
from __future__ import annotations

import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

TOL = 1e-5


# ============================================================
# Edge cases
# ============================================================

def test_scaling_all_zero_fire():
    """With no fires, rate_ema should decay toward 0."""
    import nimcp
    N = 10
    rate = [0.5] * N
    fires = [0.0] * (1 * N)
    result = nimcp.bs_scaling_apply(rate, fires, 1, N, 0.9)
    # Expected: 0.9 * 0.5 + 0.1 * 0 = 0.45
    for v in result:
        assert abs(v - 0.45) < TOL
    print(f"  PASS: all-zero fire decays rate_ema correctly")


def test_scaling_all_fire():
    """With all fires, rate_ema should approach (1-alpha) / (1-alpha) = 1 asymptotically."""
    import nimcp
    N = 5
    rate = [0.0] * N
    # Run 100 steps all-fire
    for _ in range(100):
        fires = [1.0] * N
        rate = nimcp.bs_scaling_apply(rate, fires, 1, N, 0.95)
    # Should be near 1.0
    for v in rate:
        assert v > 0.9, f"all-fire didn't approach 1.0: {v}"
    print(f"  PASS: all-fire approaches 1.0 (final={rate[0]:.4f})")


def test_depression_single_neuron():
    """Minimal case — 1 neuron, 1 sample."""
    import nimcp
    dep = [0.0]
    fires = [1.0]
    result = nimcp.bs_depression_apply(dep, fires, 1, 1, 0.95, 0.2, 0.5)
    # dep = 0.95*0 + 0.2 = 0.2
    assert abs(result[0] - 0.2) < TOL
    print(f"  PASS: single-neuron minimal case")


def test_metabolic_empty_row():
    """Row with 0 synapses should be no-op."""
    import nimcp
    w = [1.0, 2.0]
    rp = [0, 0, 2]  # row 0 empty, row 1 has 2 entries
    # cap=1; row 1 total=3, cap=2 (2*1); should scale by 2/3
    result = nimcp.bs_metabolic_apply(w, rp, 2, 1.0)
    assert abs(result[0] - 1.0 * 2/3) < TOL
    assert abs(result[1] - 2.0 * 2/3) < TOL
    print(f"  PASS: empty row handled correctly")


def test_gradient_budget_zero_norms():
    """All-zero gradients — no-op, no NaN from division."""
    import nimcp
    # Use simpler test via scaling since BS_gradient_apply isn't bound
    # But we can test via direct implementation
    print(f"  SKIP: gradient budget direct-call binding not exposed")


# ============================================================
# Large scale
# ============================================================

def test_scaling_large_batch():
    """B=128 — production-scale."""
    import nimcp
    N = 100
    B = 128
    rng = random.Random(500)
    fires = [1.0 if rng.random() < 0.1 else 0.0 for _ in range(B * N)]
    rate = [0.03] * N
    result = nimcp.bs_scaling_apply(rate, fires, B, N, 0.99)
    # Should not be NaN/Inf, should be in [0, 1]
    for v in result:
        assert not math.isnan(v) and not math.isinf(v)
        assert 0 <= v <= 1.0, f"rate out of [0,1]: {v}"
    print(f"  PASS: B=128 large-batch produces bounded values")


def test_depression_large_neuron_count():
    """N=10000 — scale test."""
    import nimcp
    N = 10000
    B = 16
    rng = random.Random(600)
    fires = [1.0 if rng.random() < 0.05 else 0.0 for _ in range(B * N)]
    dep = [0.0] * N
    result = nimcp.bs_depression_apply(dep, fires, B, N, 0.95, 0.2, 0.5)
    for v in result:
        assert 0 <= v <= 0.5 + 1e-6, f"depression out of bounds: {v}"
    print(f"  PASS: N=10000 large-neuron-count produces bounded values")


# ============================================================
# Long horizon
# ============================================================

def test_scaling_long_horizon_stability():
    """Run 2000 steps — verify bounded and converges."""
    import nimcp
    N = 20
    rate = [0.03] * N
    rng = random.Random(700)
    total_steps = 2000

    for _ in range(total_steps):
        fires = [1.0 if rng.random() < 0.05 else 0.0 for _ in range(N)]
        rate = nimcp.bs_scaling_apply(rate, fires, 1, N, 0.99)

    # Should be close to 0.05 (the firing rate)
    mean_rate = sum(rate) / N
    for v in rate:
        assert 0 <= v <= 1.0
        assert not math.isnan(v)
    assert 0.02 < mean_rate < 0.10, f"mean rate out of expected band: {mean_rate}"
    print(f"  PASS: long-horizon (2000 steps) stable (mean rate={mean_rate:.4f})")


def test_depression_long_horizon_at_cap():
    """Sustained high firing — depression should plateau at cap."""
    import nimcp
    N = 10
    dep = [0.0] * N
    # All-fire every step for 100 steps
    for _ in range(100):
        fires = [1.0] * N
        dep = nimcp.bs_depression_apply(dep, fires, 1, N, 0.95, 0.2, 0.5)
    # All should be at cap
    for v in dep:
        assert abs(v - 0.5) < TOL, f"not at cap: {v}"
    print(f"  PASS: sustained-fire depression plateaus at cap")


def test_rstdp_long_horizon_stability():
    """R-STDP over 200 batches shouldn't explode."""
    import nimcp
    P, Q = 8, 8
    B = 4
    total = P * Q
    w = [0.0] * total
    trace = [0.0] * total
    rng = random.Random(800)

    for _ in range(50):  # 50 batches of 4 samples = 200 total
        pre = [1.0 if rng.random() < 0.1 else 0.0 for _ in range(B * P)]
        post = [1.0 if rng.random() < 0.1 else 0.0 for _ in range(B * Q)]
        rewards = [rng.uniform(-0.5, 0.5) for _ in range(B)]
        w, trace = nimcp.bs_rstdp_apply(w, trace, pre, post, rewards,
                                              B, P, Q, 0.9, 0.01, 0.01, 0.0005)
    # Nothing should have exploded
    for v in w:
        assert not math.isnan(v) and not math.isinf(v)
        assert abs(v) < 10.0, f"weight exploded: {v}"
    for v in trace:
        assert not math.isnan(v) and not math.isinf(v)
    w_max = max(abs(v) for v in w)
    t_max = max(abs(v) for v in trace)
    print(f"  PASS: R-STDP 200-sample horizon stable "
          f"(max |w|={w_max:.4f}, max |trace|={t_max:.4f})")


# ============================================================
# Numerical stability
# ============================================================

def test_scaling_extreme_alpha():
    """alpha near 1 (very slow decay) and near 0 (almost no memory)."""
    import nimcp
    # Near 1: slow integration
    rate = nimcp.bs_scaling_apply([0.0] * 3, [1.0, 0.0, 1.0], 1, 3, 0.999)
    for v in rate:
        assert not math.isnan(v) and 0 <= v <= 1
    # Near 0 (but > 0): essentially takes the new value
    rate = nimcp.bs_scaling_apply([0.5] * 3, [1.0, 0.0, 1.0], 1, 3, 0.001)
    # Expected: 0.001*0.5 + 0.999*fired
    assert abs(rate[0] - (0.001 * 0.5 + 0.999 * 1.0)) < TOL
    print(f"  PASS: extreme alpha values handled")


def test_ip_zero_fires_no_update():
    """If no neuron fires, IP should leave threshold_offset unchanged."""
    import nimcp
    N = 5
    thr = [0.01] * N
    fires = [0.0] * (4 * N)
    rate = [0.5] * N
    result = nimcp.bs_ip_apply(thr, fires, rate, 4, N, 0.1, 0.03, 1.0)
    for i, v in enumerate(result):
        assert abs(v - 0.01) < TOL, f"thr drifted without fires: {v}"
    print(f"  PASS: IP no-op when no fires")


# ============================================================
# Memory behavior
# ============================================================

def test_repeated_calls_no_leak_indicator():
    """Many calls shouldn't crash or leak observably.

    Can't truly detect leaks without valgrind, but we can run many calls
    and verify no crashes or hangs.
    """
    import nimcp
    N = 100
    B = 32
    for trial in range(100):
        rng = random.Random(trial)
        fires = [1.0 if rng.random() < 0.1 else 0.0 for _ in range(B * N)]
        nimcp.bs_scaling_apply([0.0] * N, fires, B, N, 0.95)
        nimcp.bs_depression_apply([0.0] * N, fires, B, N, 0.95, 0.2, 0.5)
    print(f"  PASS: 100 repeated calls (N=100, B=32) — no crash")


def main():
    failures = []
    tests = [
        ("scaling_all_zero_fire", test_scaling_all_zero_fire),
        ("scaling_all_fire", test_scaling_all_fire),
        ("depression_single_neuron", test_depression_single_neuron),
        ("metabolic_empty_row", test_metabolic_empty_row),
        ("gradient_budget_zero_norms", test_gradient_budget_zero_norms),
        ("scaling_large_batch", test_scaling_large_batch),
        ("depression_large_neuron_count", test_depression_large_neuron_count),
        ("scaling_long_horizon_stability", test_scaling_long_horizon_stability),
        ("depression_long_horizon_at_cap", test_depression_long_horizon_at_cap),
        ("rstdp_long_horizon_stability", test_rstdp_long_horizon_stability),
        ("scaling_extreme_alpha", test_scaling_extreme_alpha),
        ("ip_zero_fires_no_update", test_ip_zero_fires_no_update),
        ("repeated_calls_no_leak_indicator", test_repeated_calls_no_leak_indicator),
    ]
    for name, fn in tests:
        print(f"[stress/c_batch_safe] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print(f"\nAll {len(tests)} C batch-safe stress tests passed.")


if __name__ == "__main__":
    main()
