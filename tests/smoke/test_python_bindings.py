#!/usr/bin/env python3
"""Verify all critical Python methods are exposed and callable on a fresh brain.

Catches binding regressions, method-table corruption, and accidental removal
of important entry points.
"""
from __future__ import annotations

import sys


CRITICAL_METHODS = [
    # Core learning
    "learn", "learn_vector", "predict", "predict_fast", "predict_batch",
    "save",
    # SNN introspection
    "get_snn_stats", "snn_get_stats", "get_neuron_count",
    "get_population_history", "snn_force_quench",
    # Test battery additions
    "get_mental_health_report", "get_mental_health_check",
    "get_emotion_state", "get_internal_state",
    "predict_with_confidence", "predict_with_deadline",
    "perturb_weights", "enter_idle_with_telemetry",
    "get_inner_speech_trace", "get_hypothesis_log",
    "cow_trial_snapshot", "cow_trial_restore",
    # COW
    "snapshot_cow", "restore_cow", "destroy_cow_snapshot",
    # Attention / emotion
    "thalamus_set_attention", "bg_update_reward",
    # World model / dream
    "world_model_dream",
]


def test_all_critical_methods_exposed():
    import nimcp
    b = nimcp.Brain("bindings_test", 128, 10)
    missing = [m for m in CRITICAL_METHODS if not hasattr(b, m)]
    if missing:
        raise AssertionError(f"Missing methods: {missing}")
    print(f"  PASS: all {len(CRITICAL_METHODS)} critical methods present")


def test_methods_callable_without_crash():
    """Call each method with safe args; should return or raise, not segfault."""
    import nimcp
    b = nimcp.Brain("callable_test", 128, 10)

    # Methods that take no args or safe defaults
    no_arg_calls = [
        ("get_snn_stats", []),
        ("snn_get_stats", []),
        ("get_neuron_count", []),
        ("get_mental_health_report", []),
        ("get_emotion_state", []),
        ("get_internal_state", []),
        ("snapshot_cow", []),
    ]
    failed = []
    for name, args in no_arg_calls:
        if not hasattr(b, name):
            continue
        try:
            getattr(b, name)(*args)
        except Exception as e:
            # Non-crash exceptions are OK — we just want no segfaults
            failed.append(f"{name}: {type(e).__name__}")
    if failed:
        print(f"  NOTE: {len(failed)} methods raised (non-fatal): {failed[:3]}…")
    print(f"  PASS: {len(no_arg_calls)} methods callable without crash")


def main():
    failures = []
    for name, fn in [
        ("all_critical_methods_exposed", test_all_critical_methods_exposed),
        ("methods_callable_without_crash", test_methods_callable_without_crash),
    ]:
        print(f"[smoke/bindings] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {e}")
    if failures:
        sys.exit(1)
    print("\nAll binding smoke tests passed.")


if __name__ == "__main__":
    main()
