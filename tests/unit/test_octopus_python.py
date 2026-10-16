#!/usr/bin/env python3
"""Phase 1 smoke test for the octopus cognitive module.

Exercises the Python binding `brain.octopus_stats()` against a live
brain instance. Full C-level unit tests (arm processing, aggregation,
hook wiring) live in tests/unit/test_octopus.c and are built separately
by a future test-infra CMake target.

Coverage here:
  - Module is created by the factory (stats.enabled == True)
  - Default arm count is 8
  - All fields in the stats dict are the expected types
  - Arm broadcast states list has length n_arms
  - Stats can be queried multiple times without error
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


def test_octopus_stats_shape():
    import nimcp
    b = nimcp.Brain("octopus_test", 128, 10)
    stats = b.octopus_stats()
    assert isinstance(stats, dict), "stats should be dict"
    assert stats.get("enabled") is True, "octopus should be enabled after brain init"
    assert stats["n_arms"] == 8, f"default n_arms should be 8, got {stats['n_arms']}"
    for key in ("n_explorations", "n_integrations", "n_ethics_vetoes",
                "n_swarm_delegations", "n_world_model_updates",
                "n_dfa_computations", "n_pink_noise_injections",
                "bridge_engram_encodings", "bridge_kg_nodes_added",
                "bridge_stress_broadcasts", "bridge_fear_broadcasts",
                "bridge_amygdala_steps",
                "bridge_wm_updates", "bridge_fear_conditionings",
                "bridge_vision_samples", "bridge_audio_samples",
                "bridge_somato_samples", "bridge_snn_samples",
                "bridge_neuromod_samples", "bridge_peer_samples",
                "n_lnn_steps", "n_lnn_train_steps", "n_ng_steps",
                "n_jepa_steps"):
        assert key in stats, f"missing counter: {key}"
        assert isinstance(stats[key], int), f"{key} should be int"
    for key in ("avg_arm_confidence", "avg_arm_variance", "central_coherence",
                "avg_arm_entropy", "avg_arm_dfa",
                "bridge_last_cortisol", "bridge_last_fear",
                "last_lnn_loss", "last_jepa_loss"):
        assert key in stats, f"missing float: {key}"
        assert isinstance(stats[key], float), f"{key} should be float"
    arms = stats.get("arm_broadcast_states")
    assert isinstance(arms, list), "arm_broadcast_states should be a list"
    assert len(arms) == stats["n_arms"], "arm_broadcast_states length mismatch"
    for v in arms:
        assert isinstance(v, float), "each broadcast state must be float"
        assert 0.0 <= v <= 1.0, f"broadcast state out of [0,1]: {v}"
    print(f"  PASS: octopus_stats shape OK (n_arms={stats['n_arms']})")


def test_octopus_stats_repeatable():
    """Calling stats() many times must not crash or drift."""
    import nimcp
    b = nimcp.Brain("octopus_rep", 128, 10)
    for _ in range(50):
        s = b.octopus_stats()
        assert s["enabled"] is True
    print("  PASS: 50 consecutive stats() calls OK")


def main():
    print("[octopus_python]")
    failures = []
    for name, fn in [
        ("stats_shape", test_octopus_stats_shape),
        ("stats_repeatable", test_octopus_stats_repeatable),
    ]:
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL {name}: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll octopus Python smoke tests passed.")


if __name__ == "__main__":
    main()
