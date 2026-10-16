#!/usr/bin/env python3
"""Unit tests for compressed-time sequential replay."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class MockBrain:
    def __init__(self):
        self.learn_calls = []
    def learn_vector(self, features, target, label=None, learning_rate=None):
        self.learn_calls.append({"label": label, "lr": learning_rate})
        return 0.5


def test_buffer_capacity_respected():
    from compressed_replay import CompressedReplayer
    brain = MockBrain()
    r = CompressedReplayer(brain, capacity=5, min_replay_interval_s=0)
    for i in range(10):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"item_{i}", salience=1.0)
    stats = r.stats()
    assert stats["buffer_size"] == 5, f"buffer exceeded capacity: {stats}"
    assert stats["items_recorded"] == 10
    print(f"  PASS: buffer capacity respected ({stats['buffer_size']}/{stats['capacity']})")


def test_replay_burst_calls_learn_vector():
    from compressed_replay import CompressedReplayer
    brain = MockBrain()
    r = CompressedReplayer(brain, capacity=20, min_replay_interval_s=0)
    for i in range(10):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"item_{i}", salience=1.0)
    r.replay_burst(n=5)
    assert len(brain.learn_calls) == 5
    assert r.stats()["items_replayed"] == 5
    print(f"  PASS: replay_burst invokes learn_vector 5 times")


def test_salience_bias_prefers_high_salience():
    """With bias=1.0, high-salience items should replay more often."""
    from compressed_replay import CompressedReplayer
    import random
    random.seed(42)
    brain = MockBrain()
    r = CompressedReplayer(brain, capacity=20, salience_bias=1.0,
                            min_replay_interval_s=0)
    # Half low-salience, half high-salience
    for i in range(5):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"low_{i}", salience=0.1)
    for i in range(5):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"high_{i}", salience=1.0)
    r.replay_burst(n=100)

    labels = [c["label"] for c in brain.learn_calls]
    high_count = sum(1 for l in labels if l.startswith("high_"))
    low_count = sum(1 for l in labels if l.startswith("low_"))
    assert high_count > low_count, (
        f"salience bias not working: high={high_count} low={low_count}")
    print(f"  PASS: salience bias prefers high-salience (high={high_count}, low={low_count})")


def test_record_from_training_step_computes_salience():
    from compressed_replay import CompressedReplayer
    r = CompressedReplayer(MockBrain(), capacity=20, min_replay_interval_s=0)
    # Loss 5.0 vs baseline 1.0 → surprise = 4.0 (capped to 1.0)
    r.record_from_training_step([0.1] * 10, [0.0] * 10, "surprise",
                                  loss=5.0, baseline_loss=1.0)
    # Loss = baseline = low surprise
    r.record_from_training_step([0.1] * 10, [0.0] * 10, "expected",
                                  loss=1.0, baseline_loss=1.0)
    saliences = [item.salience for item in r._buffer]
    assert saliences[0] > saliences[1], (
        f"surprise not encoded in salience: {saliences}")
    print(f"  PASS: salience reflects prediction surprise (saliences={saliences})")


def test_wallclock_cap():
    """Burst stops when target duration exceeded."""
    import time as _time
    from compressed_replay import CompressedReplayer
    brain = MockBrain()
    r = CompressedReplayer(brain, capacity=100, min_replay_interval_s=0.05)
    for i in range(50):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"item_{i}", salience=1.0)
    t0 = _time.time()
    r.replay_burst(n=100, target_wallclock_s=0.2)
    elapsed = _time.time() - t0
    assert elapsed < 0.5, f"burst took {elapsed:.2f}s (should be ≤0.3)"
    print(f"  PASS: wallclock cap respected ({elapsed:.2f}s, {r.stats()['items_replayed']} items)")


def test_stats_track_correctly():
    from compressed_replay import CompressedReplayer
    r = CompressedReplayer(MockBrain(), capacity=20, min_replay_interval_s=0)
    for i in range(8):
        r.record(features=[0.1] * 10, target=[0.0] * 10,
                  label=f"item_{i}", salience=1.0)
    r.replay_burst(n=5)
    r.replay_burst(n=3)
    stats = r.stats()
    assert stats["items_recorded"] == 8
    assert stats["items_replayed"] == 8
    print(f"  PASS: stats accumulate across bursts ({stats})")


def main():
    failures = []
    for name, fn in [
        ("buffer_capacity_respected", test_buffer_capacity_respected),
        ("replay_burst_calls_learn_vector", test_replay_burst_calls_learn_vector),
        ("salience_bias_prefers_high_salience", test_salience_bias_prefers_high_salience),
        ("record_from_training_step_computes_salience", test_record_from_training_step_computes_salience),
        ("wallclock_cap", test_wallclock_cap),
        ("stats_track_correctly", test_stats_track_correctly),
    ]:
        print(f"[unit/compressed_replay] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll compressed replay unit tests passed.")


if __name__ == "__main__":
    main()
