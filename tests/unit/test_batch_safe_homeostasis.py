#!/usr/bin/env python3
"""Differential tests — batch-safe mechanisms vs sequential equivalents.

For each mechanism: generate random fire patterns, run through both
sequential and batch paths, verify state converges within tolerance.

This is the core risk-mitigation test for Phase 4.1. A bug here means
batched training would diverge from sequential — exactly the pathology
we're trying to prevent.
"""
from __future__ import annotations

import copy
import math
import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


TOL = 1e-9


def _make_fires(n_samples: int, n_neurons: int, p: float = 0.1,
                seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    return [[1.0 if rng.random() < p else 0.0 for _ in range(n_neurons)]
             for _ in range(n_samples)]


# ============================================================
# Synaptic Scaling
# ============================================================

def test_synaptic_scaling_batch1_identity():
    """Batch size 1 must be exactly equivalent to sequential."""
    from batch_safe_homeostasis import SynapticScaling
    N = 20
    fires = _make_fires(50, N, seed=1)

    seq = SynapticScaling(N, tau_rate=50.0)
    batch = SynapticScaling(N, tau_rate=50.0)

    for sample in fires:
        seq.step_sequential(sample)
        batch.step_batch([sample])

    for i in range(N):
        assert abs(seq.rate_ema[i] - batch.rate_ema[i]) < TOL, (
            f"neuron {i}: seq={seq.rate_ema[i]} batch={batch.rate_ema[i]}")
    print(f"  PASS: scaling batch=1 ≡ sequential (max |Δ| < {TOL})")


def test_synaptic_scaling_batch_matches_sequential():
    """Batch of B samples = applying B sequential samples."""
    from batch_safe_homeostasis import SynapticScaling
    N = 20
    B = 8
    fires = _make_fires(B, N, seed=2)

    seq = SynapticScaling(N, tau_rate=30.0)
    batch = SynapticScaling(N, tau_rate=30.0)

    # Sequential: B steps
    for sample in fires:
        seq.step_sequential(sample)

    # Batch: 1 batch of B
    batch.step_batch(fires)

    max_diff = 0.0
    for i in range(N):
        d = abs(seq.rate_ema[i] - batch.rate_ema[i])
        max_diff = max(max_diff, d)
    assert max_diff < 1e-10, f"divergence: max|Δ|={max_diff}"
    print(f"  PASS: scaling batch(8) ≡ 8×sequential (max |Δ|={max_diff:.2e})")


def test_synaptic_scaling_large_batch():
    """B=32 — still exact."""
    from batch_safe_homeostasis import SynapticScaling
    N = 10
    B = 32
    fires = _make_fires(B, N, seed=3)

    seq = SynapticScaling(N, tau_rate=100.0)
    batch = SynapticScaling(N, tau_rate=100.0)

    for sample in fires:
        seq.step_sequential(sample)
    batch.step_batch(fires)

    max_diff = max(abs(seq.rate_ema[i] - batch.rate_ema[i]) for i in range(N))
    assert max_diff < 1e-8, f"B=32 divergence: max|Δ|={max_diff}"
    print(f"  PASS: scaling B=32 ≡ 32×sequential (max |Δ|={max_diff:.2e})")


# ============================================================
# Intrinsic Plasticity
# ============================================================

def test_ip_batch1_identity():
    from batch_safe_homeostasis import IntrinsicPlasticity
    N = 20
    fires = _make_fires(30, N, seed=10)
    rate_emas = [[0.03] * N for _ in range(30)]  # stable rate

    seq = IntrinsicPlasticity(N)
    batch = IntrinsicPlasticity(N)

    for i, (f, r) in enumerate(zip(fires, rate_emas)):
        seq.step_sequential(f, r)
        batch.step_batch([f], [r])

    for i in range(N):
        d = abs(seq.threshold_offset[i] - batch.threshold_offset[i])
        assert d < TOL, f"IP drift at neuron {i}: {d}"
    print(f"  PASS: IP batch=1 ≡ sequential")


def test_ip_batch_matches_sequential_when_rate_stable():
    """When rate_ema is constant within batch, IP batch ≡ sum of sequentials."""
    from batch_safe_homeostasis import IntrinsicPlasticity
    N = 10
    B = 16
    fires = _make_fires(B, N, seed=11)
    # Constant rate_ema — approximation is exact
    const_rate = [0.05] * N
    rate_emas = [const_rate for _ in range(B)]

    seq = IntrinsicPlasticity(N, eta_ip=0.1, delta_max=0.1)
    batch = IntrinsicPlasticity(N, eta_ip=0.1, delta_max=0.1)

    for f, r in zip(fires, rate_emas):
        seq.step_sequential(f, r)
    batch.step_batch(fires, rate_emas)

    max_diff = max(abs(seq.threshold_offset[i] - batch.threshold_offset[i])
                    for i in range(N))
    assert max_diff < 1e-10, f"IP divergence with const rate: {max_diff}"
    print(f"  PASS: IP B=16 ≡ 16×sequential (const rate, max |Δ|={max_diff:.2e})")


# ============================================================
# Short-Term Depression
# ============================================================

def test_std_batch1_identity():
    from batch_safe_homeostasis import ShortTermDepression
    N = 15
    fires = _make_fires(40, N, seed=20)

    seq = ShortTermDepression(N)
    batch = ShortTermDepression(N)

    for sample in fires:
        seq.step_sequential(sample)
        batch.step_batch([sample])

    max_diff = max(abs(seq.depression[i] - batch.depression[i])
                    for i in range(N))
    assert max_diff < TOL, f"STD divergence: {max_diff}"
    print(f"  PASS: STD batch=1 ≡ sequential (max |Δ|={max_diff:.2e})")


def test_std_batch_matches_sequential():
    from batch_safe_homeostasis import ShortTermDepression
    N = 10
    B = 16
    fires = _make_fires(B, N, seed=21)

    seq = ShortTermDepression(N)
    batch = ShortTermDepression(N)

    for sample in fires:
        seq.step_sequential(sample)
    batch.step_batch(fires)

    # Before cap: must be exact. After cap applied, can differ if
    # sequential capped mid-batch but batch computed total > cap.
    # For low firing rates this doesn't happen.
    max_diff = max(abs(seq.depression[i] - batch.depression[i])
                    for i in range(N))
    # Allow small numerical error
    assert max_diff < 1e-9, f"STD divergence: {max_diff}"
    print(f"  PASS: STD B={B} ≡ {B}×sequential (max |Δ|={max_diff:.2e})")


# ============================================================
# Inhibitory Plasticity
# ============================================================

def test_inhib_batch_exact_equivalence():
    from batch_safe_homeostasis import InhibitoryPlasticity
    P, Q = 5, 5
    B = 10
    fires_pre = _make_fires(B, P, p=0.3, seed=30)
    fires_post = _make_fires(B, Q, p=0.3, seed=31)

    seq = InhibitoryPlasticity(P, Q, eta_inh=0.01)
    batch = InhibitoryPlasticity(P, Q, eta_inh=0.01)

    for fp, fq in zip(fires_pre, fires_post):
        seq.step_sequential(fp, fq)
    batch.step_batch(fires_pre, fires_post)

    max_diff = 0.0
    for i in range(P):
        for j in range(Q):
            max_diff = max(max_diff, abs(seq.w[i][j] - batch.w[i][j]))
    assert max_diff < 1e-10, f"inhib divergence: {max_diff}"
    print(f"  PASS: inhibitory B={B} ≡ sum of sequential (max |Δ|={max_diff:.2e})")


# ============================================================
# Metabolic Budget (stateless — test apply correctness)
# ============================================================

def test_metabolic_budget_caps_correctly():
    from batch_safe_homeostasis import MetabolicBudget
    mb = MetabolicBudget(n_neurons=3, cap_per_fan_in=1.0)
    w = [[2.0, -1.5, 3.0],  # total=6.5, cap=3, scale=3/6.5
         [0.1, 0.2, 0.1],   # total=0.4, no change
         [1.0, -1.0, 0.5]]  # total=2.5, cap=3, no change
    fan_in = [3, 3, 3]
    mb.apply(w, fan_in)

    # Row 0: scale = 3/6.5
    expected_scale = 3.0 / 6.5
    assert abs(w[0][0] - 2.0 * expected_scale) < 1e-12
    assert abs(sum(abs(x) for x in w[0]) - 3.0) < 1e-10

    # Row 1: unchanged
    assert abs(w[1][0] - 0.1) < 1e-12
    assert abs(w[1][1] - 0.2) < 1e-12

    # Row 2: unchanged
    assert abs(w[2][0] - 1.0) < 1e-12
    print(f"  PASS: metabolic budget caps row 0, spares rows 1-2")


# ============================================================
# Global Gradient Budget
# ============================================================

def test_gradient_budget_preserves_ratios():
    """Global clip preserves relative magnitudes across networks."""
    from batch_safe_homeostasis import GlobalGradientBudget
    grads = {
        "ann": [1.0, 1.0, 1.0],   # norm = √3
        "snn": [2.0, 2.0, 2.0],   # norm = √12 = 2√3
        "lnn": [0.5, 0.5, 0.5],   # norm = √0.75
    }
    budget = GlobalGradientBudget(budget=1.0)
    clipped = budget.clip_global(grads)

    # Relative ratios must be preserved
    def norm(g): return math.sqrt(sum(x*x for x in g))
    ann_n = norm(clipped["ann"])
    snn_n = norm(clipped["snn"])
    lnn_n = norm(clipped["lnn"])
    # snn should be ~2× ann, lnn should be ~0.5× ann
    assert abs(snn_n / ann_n - 2.0) < 1e-9, f"ratio broken: {snn_n/ann_n}"
    assert abs(lnn_n / ann_n - 0.5) < 1e-9, f"ratio broken: {lnn_n/ann_n}"
    # Total should ≈ budget
    total = math.sqrt(ann_n**2 + snn_n**2 + lnn_n**2)
    assert abs(total - 1.0) < 1e-9, f"total={total}"
    print(f"  PASS: gradient budget preserves ratios; total={total:.6f}")


def test_gradient_budget_no_clip_when_under_budget():
    from batch_safe_homeostasis import GlobalGradientBudget
    grads = {"a": [0.1, 0.1], "b": [0.1, 0.1]}   # small
    budget = GlobalGradientBudget(budget=1.0)
    clipped = budget.clip_global(grads)
    for name in ["a", "b"]:
        for before, after in zip(grads[name], clipped[name]):
            assert abs(before - after) < 1e-12
    print(f"  PASS: gradient budget no-op when under budget")


# ============================================================
# R-STDP Batch
# ============================================================

def test_rstdp_batch_matches_sequential():
    """Traces AND weights must match exactly — full mathematical equivalence."""
    from batch_safe_homeostasis import BatchRSTDP
    P, Q = 4, 4
    B = 8
    pre = _make_fires(B, P, p=0.25, seed=40)
    post = _make_fires(B, Q, p=0.25, seed=41)
    # Non-constant rewards to catch averaging bugs
    rewards = [0.1, 0.9, 0.5, 0.3, 0.7, 0.2, 0.8, 0.4]

    seq = BatchRSTDP(P, Q)
    batch = BatchRSTDP(P, Q)

    for i in range(B):
        seq.step_sequential(pre[i], post[i], rewards[i])
    batch.step_batch(pre, post, rewards)

    # Traces match exactly (same recurrence)
    max_trace_diff = max(abs(seq.trace[i][j] - batch.trace[i][j])
                          for i in range(P) for j in range(Q))
    assert max_trace_diff < 1e-10, f"trace divergence: {max_trace_diff}"

    # Weights must ALSO match exactly (not just approximate — we compute
    # the exact Σ r_b · trace_b in batch form)
    max_w_diff = max(abs(seq.w[i][j] - batch.w[i][j])
                      for i in range(P) for j in range(Q))
    assert max_w_diff < 1e-10, f"weight divergence (batch not equivalent): {max_w_diff}"
    print(f"  PASS: R-STDP batch ≡ sequential EXACTLY "
          f"(trace |Δ|={max_trace_diff:.2e}, weight |Δ|={max_w_diff:.2e})")


def test_rstdp_empty_batch():
    from batch_safe_homeostasis import BatchRSTDP
    r = BatchRSTDP(3, 3)
    r.step_batch([], [], [])   # no-op
    # No crash, state unchanged
    assert r.trace[0][0] == 0.0
    print(f"  PASS: R-STDP empty batch no-op")


# ============================================================
# Long-horizon convergence test
# ============================================================

def test_long_horizon_convergence():
    """Over many batches, rate_ema trajectory converges to sequential."""
    from batch_safe_homeostasis import SynapticScaling
    N = 50
    total_samples = 500
    B = 8

    seq = SynapticScaling(N, tau_rate=50.0)
    batch = SynapticScaling(N, tau_rate=50.0)

    rng = random.Random(100)
    # Same underlying sequence, just chunked differently
    samples = [[1.0 if rng.random() < 0.1 else 0.0 for _ in range(N)]
                for _ in range(total_samples)]

    # Sequential: one at a time
    for s in samples:
        seq.step_sequential(s)

    # Batch: chunked
    for i in range(0, total_samples, B):
        chunk = samples[i:i + B]
        batch.step_batch(chunk)

    max_diff = max(abs(seq.rate_ema[i] - batch.rate_ema[i]) for i in range(N))
    # Should match within floating-point tolerance even over 500 steps
    assert max_diff < 1e-8, f"long-horizon drift: {max_diff}"
    print(f"  PASS: 500-sample long horizon — batch ≡ sequential "
          f"(max |Δ|={max_diff:.2e})")


def main():
    failures = []
    tests = [
        ("synaptic_scaling_batch1_identity", test_synaptic_scaling_batch1_identity),
        ("synaptic_scaling_batch_matches_sequential", test_synaptic_scaling_batch_matches_sequential),
        ("synaptic_scaling_large_batch", test_synaptic_scaling_large_batch),
        ("ip_batch1_identity", test_ip_batch1_identity),
        ("ip_batch_matches_sequential_when_rate_stable", test_ip_batch_matches_sequential_when_rate_stable),
        ("std_batch1_identity", test_std_batch1_identity),
        ("std_batch_matches_sequential", test_std_batch_matches_sequential),
        ("inhib_batch_exact_equivalence", test_inhib_batch_exact_equivalence),
        ("metabolic_budget_caps_correctly", test_metabolic_budget_caps_correctly),
        ("gradient_budget_preserves_ratios", test_gradient_budget_preserves_ratios),
        ("gradient_budget_no_clip_when_under_budget", test_gradient_budget_no_clip_when_under_budget),
        ("rstdp_batch_matches_sequential", test_rstdp_batch_matches_sequential),
        ("rstdp_empty_batch", test_rstdp_empty_batch),
        ("long_horizon_convergence", test_long_horizon_convergence),
    ]
    for name, fn in tests:
        print(f"[unit/batch_safe_homeostasis] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        print(f"\n{len(failures)}/{len(tests)} failed")
        sys.exit(1)
    print(f"\nAll {len(tests)} batch-safe homeostasis tests passed.")


if __name__ == "__main__":
    main()
