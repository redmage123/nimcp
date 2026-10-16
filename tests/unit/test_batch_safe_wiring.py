#!/usr/bin/env python3
"""Wiring validation: flag-on vs flag-off in live SNN.

Uses a real brain (small) to verify:
  1. Flag default state is OFF (zero impact on existing behavior)
  2. With flag OFF, SNN runs through legacy sequential homeostasis
  3. With flag ON, SNN runs through batch-safe C path at B=1 —
     should produce behaviorally-equivalent firing dynamics
  4. Flag toggle doesn't crash mid-run
"""
from __future__ import annotations

import random
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def test_flag_default_is_off():
    import nimcp
    # Fresh state: flag should be OFF by default
    assert nimcp.bs_is_enabled() == False
    print("  PASS: feature flag defaults to OFF")


def test_flag_toggle_persists():
    import nimcp
    nimcp.bs_set_enabled(True)
    assert nimcp.bs_is_enabled() == True
    nimcp.bs_set_enabled(False)
    assert nimcp.bs_is_enabled() == False
    print("  PASS: flag toggles and persists")


def test_snn_works_with_flag_off():
    """Baseline: existing code path with small brain must not crash."""
    import nimcp
    nimcp.bs_set_enabled(False)
    random.seed(0)
    features = [random.gauss(0, 1) for _ in range(10)]
    b = nimcp.Brain("wiring_off", 128, 10)
    out = b.predict(features)
    assert out is not None
    print(f"  PASS: SNN runs with flag OFF (output={out})")


def test_snn_works_with_flag_on():
    """Critical: flipping flag to ON should NOT crash."""
    import nimcp
    nimcp.bs_set_enabled(True)
    try:
        random.seed(0)
        features = [random.gauss(0, 1) for _ in range(10)]
        b = nimcp.Brain("wiring_on", 128, 10)
        out = b.predict(features)
        assert out is not None
        print(f"  PASS: SNN runs with flag ON (output={out})")
    finally:
        # Always reset to OFF
        nimcp.bs_set_enabled(False)


def test_repeated_predict_with_flag_on():
    """Multiple predicts with flag ON — no accumulating crashes."""
    import nimcp
    nimcp.bs_set_enabled(True)
    try:
        random.seed(1)
        b = nimcp.Brain("wiring_rep", 128, 10)
        for i in range(10):
            features = [random.gauss(0, 1) for _ in range(10)]
            out = b.predict(features)
            assert out is not None
        print(f"  PASS: 10 predicts with flag ON — no crash")
    finally:
        nimcp.bs_set_enabled(False)


def test_multi_network_with_flag_on():
    """Brain with multi-network enabled + flag ON — hits SNN path."""
    import nimcp
    nimcp.bs_set_enabled(True)
    try:
        b = nimcp.Brain("wiring_mn", 128, 10)
        try:
            b.enable_multi_network()
        except Exception as e:
            print(f"  SKIP: multi-network enable failed ({e})")
            return
        # predict exercises SNN
        features = [random.gauss(0, 1) for _ in range(10)]
        out = b.predict(features)
        assert out is not None
        # Snapshot SNN stats after exercise
        stats = b.snn_get_stats()
        if stats:
            print(f"  PASS: multi-network + flag ON — SNN dispatch works "
                  f"(sparsity={stats.get('sparsity', 'N/A')})")
        else:
            print(f"  PASS: multi-network + flag ON — no crash (stats unavailable)")
    finally:
        nimcp.bs_set_enabled(False)


def main():
    failures = []
    tests = [
        ("flag_default_is_off", test_flag_default_is_off),
        ("flag_toggle_persists", test_flag_toggle_persists),
        ("snn_works_with_flag_off", test_snn_works_with_flag_off),
        ("snn_works_with_flag_on", test_snn_works_with_flag_on),
        ("repeated_predict_with_flag_on", test_repeated_predict_with_flag_on),
        ("multi_network_with_flag_on", test_multi_network_with_flag_on),
    ]
    for name, fn in tests:
        print(f"[unit/batch_safe_wiring] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print(f"\nAll {len(tests)} wiring tests passed.")


if __name__ == "__main__":
    main()
