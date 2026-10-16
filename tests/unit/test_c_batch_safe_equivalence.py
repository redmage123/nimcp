#!/usr/bin/env python3
"""Differential test: C batch-safe implementation vs Python reference.

For each mechanism, generate random inputs, run both C and Python, verify
outputs match within floating-point tolerance.

This is the gate that must pass before the C port is trusted. Any
divergence between C and Python ref indicates a port bug.
"""
from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))

# Bit-comparable: f32 precision is ~1e-7; allow small tolerance for
# order-of-summation differences
TOL = 1e-5


def _fires_flat(B: int, N: int, p: float = 0.15, seed: int = 1) -> list[float]:
    """Generate flat [B*N] fire vector."""
    rng = random.Random(seed)
    return [1.0 if rng.random() < p else 0.0 for _ in range(B * N)]


def _fires_2d(flat: list[float], B: int, N: int) -> list[list[float]]:
    """Convert flat to list-of-lists for Python ref."""
    return [flat[b*N:(b+1)*N] for b in range(B)]


# ============================================================
# Synaptic Scaling
# ============================================================

def test_scaling_c_matches_python_batch1():
    import nimcp
    from batch_safe_homeostasis import SynapticScaling
    N = 20
    fires_flat = _fires_flat(1, N, p=0.2, seed=10)
    fires_2d = _fires_2d(fires_flat, 1, N)

    py = SynapticScaling(N, tau_rate=50.0)
    alpha = py.alpha
    py.step_batch(fires_2d)

    rate_init = [0.03] * N
    result_c = nimcp.bs_scaling_apply(rate_init, fires_flat, 1, N, float(alpha))

    max_diff = max(abs(a - b) for a, b in zip(py.rate_ema, result_c))
    assert max_diff < TOL, f"scaling batch=1 C vs Py: max|Δ|={max_diff}"
    print(f"  PASS: scaling B=1 — C ≡ Python ref (max |Δ|={max_diff:.2e})")


def test_scaling_c_matches_python_batch8():
    import nimcp
    from batch_safe_homeostasis import SynapticScaling
    N = 20
    B = 8
    fires_flat = _fires_flat(B, N, p=0.2, seed=20)
    fires_2d = _fires_2d(fires_flat, B, N)

    py = SynapticScaling(N, tau_rate=30.0)
    alpha = py.alpha
    py.step_batch(fires_2d)

    result_c = nimcp.bs_scaling_apply([0.03] * N, fires_flat, B, N, float(alpha))
    max_diff = max(abs(a - b) for a, b in zip(py.rate_ema, result_c))
    assert max_diff < TOL, f"scaling B=8 divergence: {max_diff}"
    print(f"  PASS: scaling B=8 — C ≡ Python (max |Δ|={max_diff:.2e})")


def test_scaling_c_matches_python_batch32():
    import nimcp
    from batch_safe_homeostasis import SynapticScaling
    N = 15
    B = 32
    fires_flat = _fires_flat(B, N, p=0.1, seed=30)
    fires_2d = _fires_2d(fires_flat, B, N)

    py = SynapticScaling(N, tau_rate=100.0)
    alpha = py.alpha
    py.step_batch(fires_2d)

    result_c = nimcp.bs_scaling_apply([0.03] * N, fires_flat, B, N, float(alpha))
    max_diff = max(abs(a - b) for a, b in zip(py.rate_ema, result_c))
    assert max_diff < TOL, f"scaling B=32 divergence: {max_diff}"
    print(f"  PASS: scaling B=32 — C ≡ Python (max |Δ|={max_diff:.2e})")


# ============================================================
# Short-Term Depression
# ============================================================

def test_depression_c_matches_python():
    import nimcp
    from batch_safe_homeostasis import ShortTermDepression
    N = 15
    B = 8
    fires_flat = _fires_flat(B, N, p=0.3, seed=40)
    fires_2d = _fires_2d(fires_flat, B, N)

    py = ShortTermDepression(N)
    py.step_batch(fires_2d)

    result_c = nimcp.bs_depression_apply([0.0] * N, fires_flat, B, N,
                                           0.95, 0.2, 0.5)
    max_diff = max(abs(a - b) for a, b in zip(py.depression, result_c))
    assert max_diff < TOL, f"depression divergence: {max_diff}"
    print(f"  PASS: depression B=8 — C ≡ Python (max |Δ|={max_diff:.2e})")


def test_depression_c_cap_activates():
    """High firing triggers cap in both C and Python — they must agree."""
    import nimcp
    from batch_safe_homeostasis import ShortTermDepression
    N = 5
    B = 10
    # All-fire to force cap activation
    fires_flat = [1.0] * (B * N)
    fires_2d = _fires_2d(fires_flat, B, N)

    py = ShortTermDepression(N)
    py.step_batch(fires_2d)

    result_c = nimcp.bs_depression_apply([0.0] * N, fires_flat, B, N,
                                           0.95, 0.2, 0.5)
    max_diff = max(abs(a - b) for a, b in zip(py.depression, result_c))
    assert max_diff < TOL, f"depression cap divergence: {max_diff}"
    # Sanity: should all be at cap
    for v in py.depression:
        assert abs(v - 0.5) < TOL
    print(f"  PASS: depression cap path — C ≡ Python "
          f"(max |Δ|={max_diff:.2e}, all at cap)")


# ============================================================
# Metabolic Budget
# ============================================================

def test_metabolic_c_matches_python():
    import nimcp
    from batch_safe_homeostasis import MetabolicBudget
    # 3 neurons, fan_in=3 each; weights will be stored as flat [9]
    # row_ptr: [0, 3, 6, 9]
    w_flat = [2.0, -1.5, 3.0,   # row 0: total=6.5, cap=3
              0.1, 0.2, 0.1,     # row 1: total=0.4, no change
              1.0, -1.0, 0.5]    # row 2: total=2.5, cap=3 (no change)
    row_ptr = [0, 3, 6, 9]

    # Python ref: apply_flat
    mb = MetabolicBudget(n_neurons=3, cap_per_fan_in=1.0)
    w_py = list(w_flat)
    mb.apply_flat(w_py, row_ptr)

    # C implementation
    w_c = nimcp.bs_metabolic_apply(list(w_flat), row_ptr, 3, 1.0)

    max_diff = max(abs(a - b) for a, b in zip(w_py, w_c))
    assert max_diff < TOL, f"metabolic divergence: {max_diff}"
    print(f"  PASS: metabolic budget — C ≡ Python (max |Δ|={max_diff:.2e})")


# ============================================================
# Intrinsic Plasticity
# ============================================================

def test_ip_c_matches_python_constant_rate():
    """When rate_ema is constant within batch, C and Python must agree exactly."""
    import nimcp
    from batch_safe_homeostasis import IntrinsicPlasticity
    N = 10
    B = 16
    fires_flat = _fires_flat(B, N, p=0.2, seed=50)
    fires_2d = _fires_2d(fires_flat, B, N)
    rate = [0.05] * N
    rate_batch = [rate for _ in range(B)]

    py = IntrinsicPlasticity(N, eta_ip=0.3, delta_max=0.1)
    py.step_batch(fires_2d, rate_batch)

    result_c = nimcp.bs_ip_apply([0.0] * N, fires_flat, rate, B, N,
                                   0.3, 0.03, 0.1)
    max_diff = max(abs(a - b) for a, b in zip(py.threshold_offset, result_c))
    assert max_diff < TOL, f"IP divergence: {max_diff}"
    print(f"  PASS: IP (const rate) — C ≡ Python (max |Δ|={max_diff:.2e})")


def test_ip_c_respects_cap():
    """Extreme error should saturate threshold_offset at delta_max."""
    import nimcp
    N = 3
    B = 10
    # All fires
    fires_flat = [1.0] * (B * N)
    # Very high rate_ema → large positive err → delta pushes thr up to cap
    rate = [1.0] * N
    result_c = nimcp.bs_ip_apply([0.0] * N, fires_flat, rate, B, N,
                                   0.5, 0.03, 0.01)
    for v in result_c:
        assert abs(v - 0.01) < TOL, f"cap not hit: {v}"
    print(f"  PASS: IP cap at delta_max={result_c[0]}")


# ============================================================
# Inhibitory Plasticity
# ============================================================

def test_inhibitory_c_matches_python():
    import nimcp
    from batch_safe_homeostasis import InhibitoryPlasticity
    P, Q = 5, 5
    B = 10
    fires_pre_flat = _fires_flat(B, P, p=0.3, seed=60)
    fires_post_flat = _fires_flat(B, Q, p=0.3, seed=61)
    fires_pre_2d = _fires_2d(fires_pre_flat, B, P)
    fires_post_2d = _fires_2d(fires_post_flat, B, Q)

    py = InhibitoryPlasticity(P, Q, eta_inh=0.01, target_rate=0.03)
    py.step_batch(fires_pre_2d, fires_post_2d)

    # Flatten Python weights
    w_init = [0.0] * (P * Q)
    w_flat_py = []
    for i in range(P):
        for j in range(Q):
            w_flat_py.append(py.w[i][j])

    # Run C
    w_c = nimcp.bs_inhibitory_apply(w_init, fires_pre_flat, fires_post_flat,
                                       B, P, Q, 0.01, 0.03)
    max_diff = max(abs(a - b) for a, b in zip(w_flat_py, w_c))
    assert max_diff < TOL, f"inhibitory divergence: {max_diff}"
    print(f"  PASS: inhibitory B={B} — C ≡ Python (max |Δ|={max_diff:.2e})")


# ============================================================
# R-STDP
# ============================================================

def test_rstdp_c_matches_python():
    """C R-STDP must match Python reference EXACTLY — weights + traces."""
    import nimcp
    from batch_safe_homeostasis import BatchRSTDP
    P, Q = 4, 4
    B = 8
    pre_flat = _fires_flat(B, P, p=0.25, seed=70)
    post_flat = _fires_flat(B, Q, p=0.25, seed=71)
    pre_2d = _fires_2d(pre_flat, B, P)
    post_2d = _fires_2d(post_flat, B, Q)
    rewards = [0.1, 0.9, 0.5, 0.3, 0.7, 0.2, 0.8, 0.4]

    # Python reference
    py = BatchRSTDP(P, Q)
    py.step_batch(pre_2d, post_2d, rewards)

    # Flatten Python w and trace
    w_py_flat = [py.w[i][j] for i in range(P) for j in range(Q)]
    trace_py_flat = [py.trace[i][j] for i in range(P) for j in range(Q)]

    # C reference starts from same initial state (all zeros)
    w_init = [0.0] * (P * Q)
    trace_init = [0.0] * (P * Q)
    w_c, trace_c = nimcp.bs_rstdp_apply(w_init, trace_init, pre_flat, post_flat,
                                            rewards, B, P, Q,
                                            0.9, 0.01, 0.01, 0.0005)

    max_w = max(abs(a - b) for a, b in zip(w_py_flat, w_c))
    max_t = max(abs(a - b) for a, b in zip(trace_py_flat, trace_c))
    assert max_w < TOL, f"R-STDP weight divergence: {max_w}"
    assert max_t < TOL, f"R-STDP trace divergence: {max_t}"
    print(f"  PASS: R-STDP EXACT equivalence — "
          f"weights max |Δ|={max_w:.2e}, traces max |Δ|={max_t:.2e}")


# ============================================================
# Feature flag
# ============================================================

def test_feature_flag_toggles():
    import nimcp
    nimcp.bs_set_enabled(False)
    assert nimcp.bs_is_enabled() == False
    nimcp.bs_set_enabled(True)
    assert nimcp.bs_is_enabled() == True
    nimcp.bs_set_enabled(False)  # reset
    print(f"  PASS: feature flag toggles correctly")


# ============================================================
# Self-test
# ============================================================

def test_c_self_test_zero_failures():
    import nimcp
    n = nimcp.bs_self_test()
    assert n == 0, f"C self-test had {n} failures"
    print(f"  PASS: C self-test: 0 failures")


def main():
    failures = []
    tests = [
        ("c_self_test_zero_failures", test_c_self_test_zero_failures),
        ("scaling_c_matches_python_batch1", test_scaling_c_matches_python_batch1),
        ("scaling_c_matches_python_batch8", test_scaling_c_matches_python_batch8),
        ("scaling_c_matches_python_batch32", test_scaling_c_matches_python_batch32),
        ("depression_c_matches_python", test_depression_c_matches_python),
        ("depression_c_cap_activates", test_depression_c_cap_activates),
        ("metabolic_c_matches_python", test_metabolic_c_matches_python),
        ("ip_c_matches_python_constant_rate", test_ip_c_matches_python_constant_rate),
        ("ip_c_respects_cap", test_ip_c_respects_cap),
        ("inhibitory_c_matches_python", test_inhibitory_c_matches_python),
        ("rstdp_c_matches_python", test_rstdp_c_matches_python),
        ("feature_flag_toggles", test_feature_flag_toggles),
    ]
    for name, fn in tests:
        print(f"[unit/c_batch_safe_equivalence] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        print(f"\n{len(failures)}/{len(tests)} failed")
        sys.exit(1)
    print(f"\nAll {len(tests)} C/Python equivalence tests passed.")


if __name__ == "__main__":
    main()
