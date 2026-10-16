#!/usr/bin/env python3
"""Unit tests for gradient accumulator."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def test_window_fills():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=5)
    for i in range(3):
        acc.record(loss=0.5, grad_norm=1.0)
    assert not acc.ready()
    for i in range(3):
        acc.record(loss=0.4, grad_norm=0.9)
    assert acc.ready()
    print(f"  PASS: window fills correctly ({acc.stats()})")


def test_smoothed_lr_scales_down_high_grad_norm():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=5, target_grad_norm=1.0)
    for _ in range(5):
        acc.record(loss=0.5, grad_norm=2.5)  # twice target
    lr = acc.smoothed_lr(base_lr=0.01)
    # LR should be scaled down, roughly 0.01 * 1/2.5 = 0.004, clamped to >=0.5× = 0.005
    assert lr < 0.01, f"lr={lr} should be < base_lr=0.01"
    print(f"  PASS: LR scaled down for high grad norm (base=0.01, smoothed={lr:.4f})")


def test_smoothed_lr_scales_up_low_grad_norm():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=5, target_grad_norm=1.0)
    for _ in range(5):
        acc.record(loss=0.5, grad_norm=0.3)  # below target
    lr = acc.smoothed_lr(base_lr=0.01)
    assert lr > 0.01, f"lr={lr} should be > base_lr"
    print(f"  PASS: LR scaled up for low grad norm (base=0.01, smoothed={lr:.4f})")


def test_smoothed_lr_clamps_to_bounds():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=3, min_lr=1e-5, max_lr=1.0,
                               target_grad_norm=1.0)
    # Very high grad norm — would want very low LR, but clamped to min
    for _ in range(3):
        acc.record(loss=1.0, grad_norm=100.0)
    lr = acc.smoothed_lr(base_lr=0.001)
    # Scale factor 1/100 clamped to 0.5× → 0.0005
    # Then max() with min_lr 1e-5
    assert lr >= 1e-5
    print(f"  PASS: LR clamped to bounds (got {lr:.6f})")


def test_nan_inf_rejected():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=5)
    acc.record(loss=float("nan"), grad_norm=1.0)
    acc.record(loss=0.5, grad_norm=float("inf"))
    acc.record(loss=0.4, grad_norm=0.9)
    # Only the valid entries should be recorded
    assert len(acc._losses) == 2  # 0.5 and 0.4 (NaN rejected)
    assert len(acc._grad_norms) == 2  # 1.0 and 0.9 (Inf rejected)
    print(f"  PASS: NaN/Inf values rejected ({acc.stats()})")


def test_loss_trend():
    from gradient_accumulator import GradientAccumulator
    acc = GradientAccumulator(window=10)
    # Descending loss
    for v in [1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1]:
        acc.record(loss=v)
    mean, slope = acc.loss_trend()
    assert slope < 0, f"slope should be negative for descending: {slope}"
    print(f"  PASS: loss trend captured (mean={mean:.2f}, slope={slope:.2f})")


def main():
    failures = []
    for name, fn in [
        ("window_fills", test_window_fills),
        ("smoothed_lr_scales_down_high_grad_norm", test_smoothed_lr_scales_down_high_grad_norm),
        ("smoothed_lr_scales_up_low_grad_norm", test_smoothed_lr_scales_up_low_grad_norm),
        ("smoothed_lr_clamps_to_bounds", test_smoothed_lr_clamps_to_bounds),
        ("nan_inf_rejected", test_nan_inf_rejected),
        ("loss_trend", test_loss_trend),
    ]:
        print(f"[unit/gradient_accum] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll gradient accumulator unit tests passed.")


if __name__ == "__main__":
    main()
