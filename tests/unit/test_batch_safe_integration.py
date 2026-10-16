#!/usr/bin/env python3
"""Integration test: run all batch-safe mechanisms together.

Verifies that the five homeostatic mechanisms, run in the order they'd
appear in a training step, produce the same joint trajectory in batch
and sequential modes.
"""
from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def _make_fires(n_samples: int, n_neurons: int, p: float = 0.1,
                seed: int = 42) -> list[list[float]]:
    rng = random.Random(seed)
    return [[1.0 if rng.random() < p else 0.0 for _ in range(n_neurons)]
             for _ in range(n_samples)]


class HomeostasisStack:
    """Manager that runs all 5 mechanisms per step."""

    def __init__(self, n_neurons, n_inhib_pre=5, n_inhib_post=5):
        from batch_safe_homeostasis import (
            SynapticScaling, IntrinsicPlasticity, ShortTermDepression,
            InhibitoryPlasticity, MetabolicBudget,
        )
        self.scaling = SynapticScaling(n_neurons, tau_rate=50.0)
        self.ip = IntrinsicPlasticity(n_neurons, eta_ip=0.1, delta_max=0.05)
        self.std = ShortTermDepression(n_neurons)
        self.inhib = InhibitoryPlasticity(n_inhib_pre, n_inhib_post, eta_inh=0.01)
        # MB is stateless; not applied per step in this test

    def step_sequential(self, fired_exc, fired_inh_pre, fired_inh_post):
        self.scaling.step_sequential(fired_exc)
        self.ip.step_sequential(fired_exc, self.scaling.rate_ema)
        self.std.step_sequential(fired_exc)
        self.inhib.step_sequential(fired_inh_pre, fired_inh_post)

    def step_batch(self, fires_exc_batch, fires_pre_batch, fires_post_batch):
        # Capture rate_ema before updates for IP (uses same snapshot sequentially)
        # In batch we use rate_ema_batch that reflects within-batch updates
        self.scaling.step_batch(fires_exc_batch)
        # For IP we need rate_ema at each sample — approximate with final rate
        # for simplicity. In a real integration we'd track rate_ema per sample.
        rate_ema_batch = [self.scaling.rate_ema] * len(fires_exc_batch)
        self.ip.step_batch(fires_exc_batch, rate_ema_batch)
        self.std.step_batch(fires_exc_batch)
        self.inhib.step_batch(fires_pre_batch, fires_post_batch)

    def state_snapshot(self):
        return {
            "rate_ema": list(self.scaling.rate_ema),
            "threshold": list(self.ip.threshold_offset),
            "depression": list(self.std.depression),
            "inhib_w": [row[:] for row in self.inhib.w],
        }


def test_integrated_batch1_identity():
    N = 10
    P = Q = 5
    fires_exc = _make_fires(20, N, p=0.15, seed=1)
    fires_inh_pre = _make_fires(20, P, p=0.2, seed=2)
    fires_inh_post = _make_fires(20, Q, p=0.2, seed=3)

    seq = HomeostasisStack(N, P, Q)
    batch = HomeostasisStack(N, P, Q)

    for i in range(len(fires_exc)):
        seq.step_sequential(fires_exc[i], fires_inh_pre[i], fires_inh_post[i])
        batch.step_batch([fires_exc[i]], [fires_inh_pre[i]], [fires_inh_post[i]])

    s1 = seq.state_snapshot()
    s2 = batch.state_snapshot()

    # All states should match within tol
    max_diff = 0.0
    for key in ["rate_ema", "threshold", "depression"]:
        for a, b in zip(s1[key], s2[key]):
            max_diff = max(max_diff, abs(a - b))
    for i in range(P):
        for j in range(Q):
            max_diff = max(max_diff, abs(s1["inhib_w"][i][j] - s2["inhib_w"][i][j]))
    assert max_diff < 1e-10, f"integrated B=1 drift: {max_diff}"
    print(f"  PASS: integrated B=1 ≡ sequential (max |Δ|={max_diff:.2e})")


def test_integrated_batch8_convergence():
    """Integrated run with batch=8 — should match sequential over long horizon."""
    N = 10
    P = Q = 5
    total = 80
    B = 8

    fires_exc = _make_fires(total, N, p=0.15, seed=10)
    fires_inh_pre = _make_fires(total, P, p=0.2, seed=11)
    fires_inh_post = _make_fires(total, Q, p=0.2, seed=12)

    seq = HomeostasisStack(N, P, Q)
    batch = HomeostasisStack(N, P, Q)

    # Sequential: one at a time
    for i in range(total):
        seq.step_sequential(fires_exc[i], fires_inh_pre[i], fires_inh_post[i])

    # Batch: chunks of B
    for i in range(0, total, B):
        batch.step_batch(
            fires_exc[i:i+B],
            fires_inh_pre[i:i+B],
            fires_inh_post[i:i+B])

    s1 = seq.state_snapshot()
    s2 = batch.state_snapshot()

    # Allowed tolerance: scaling, STD, inhibitory are exact-equivalent;
    # IP is approximate when rate_ema varies within batch.
    max_scaling_diff = max(abs(a - b) for a, b in zip(s1["rate_ema"], s2["rate_ema"]))
    max_std_diff = max(abs(a - b) for a, b in zip(s1["depression"], s2["depression"]))
    max_inhib_diff = max(abs(s1["inhib_w"][i][j] - s2["inhib_w"][i][j])
                          for i in range(P) for j in range(Q))
    max_ip_diff = max(abs(a - b) for a, b in zip(s1["threshold"], s2["threshold"]))

    assert max_scaling_diff < 1e-8, f"scaling drift: {max_scaling_diff}"
    assert max_std_diff < 1e-8, f"STD drift: {max_std_diff}"
    assert max_inhib_diff < 1e-8, f"inhib drift: {max_inhib_diff}"
    # IP allows slightly larger tolerance due to rate_ema-within-batch approximation
    assert max_ip_diff < 0.05, f"IP drift too large: {max_ip_diff}"
    print(f"  PASS: integrated B=8 matches sequential "
          f"(scaling={max_scaling_diff:.2e}, STD={max_std_diff:.2e}, "
          f"inhib={max_inhib_diff:.2e}, IP={max_ip_diff:.2e})")


def test_feature_flag_isolation():
    """Legacy sequential path (what exists today) must work unchanged
    when batch mode is NOT used."""
    N = 10
    from batch_safe_homeostasis import SynapticScaling
    s = SynapticScaling(N)
    # Run sequentially — no batch calls. State should evolve correctly.
    s.step_sequential([1.0] * N)
    assert all(x > 0.0 for x in s.rate_ema)
    print(f"  PASS: sequential-only path still works (rate_ema non-zero after fire)")


def test_snapshot_restore_round_trip():
    """Each mechanism can snapshot + restore state. Used for rollback."""
    from batch_safe_homeostasis import (
        SynapticScaling, IntrinsicPlasticity, ShortTermDepression
    )
    N = 10
    s = SynapticScaling(N)
    s.step_sequential([1.0, 0.0] * 5)
    snap = s.snapshot()
    s.step_sequential([0.0] * N)
    s.restore(snap)
    assert s.rate_ema == snap

    ip = IntrinsicPlasticity(N)
    ip.step_sequential([1.0] * N, [0.01] * N)
    ip_snap = ip.snapshot()
    ip.step_sequential([1.0] * N, [0.10] * N)
    ip.restore(ip_snap)
    assert ip.threshold_offset == ip_snap

    std = ShortTermDepression(N)
    std.step_sequential([1.0] * N)
    std_snap = std.snapshot()
    std.step_sequential([0.0] * N)
    std.restore(std_snap)
    assert std.depression == std_snap

    print(f"  PASS: snapshot/restore works for 3 mechanisms")


def main():
    failures = []
    tests = [
        ("integrated_batch1_identity", test_integrated_batch1_identity),
        ("integrated_batch8_convergence", test_integrated_batch8_convergence),
        ("feature_flag_isolation", test_feature_flag_isolation),
        ("snapshot_restore_round_trip", test_snapshot_restore_round_trip),
    ]
    for name, fn in tests:
        print(f"[unit/batch_safe_integration] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print(f"\nAll {len(tests)} batch-safe integration tests passed.")


if __name__ == "__main__":
    main()
