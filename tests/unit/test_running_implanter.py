#!/usr/bin/env python3
"""Unit tests for RunningImplanter — training-derived memory consolidation."""
from __future__ import annotations

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class FullBrain:
    """Mock brain exposing all memory APIs."""
    def __init__(self):
        self.semantic_calls = []
        self.kg_calls = []
        self.episode_calls = []
    def semantic_memory_insert(self, **kwargs):
        self.semantic_calls.append(kwargs)
    def kg_add_fact(self, **kwargs):
        self.kg_calls.append(kwargs)
    def hippocampus_seed_episode(self, **kwargs):
        self.episode_calls.append(kwargs)


class PartialBrain:
    """Only has KG."""
    def __init__(self):
        self.kg_calls = []
    def kg_add_fact(self, **kwargs):
        self.kg_calls.append(kwargs)


def test_observe_buffers_events():
    from childhood_memories import RunningImplanter
    imp = RunningImplanter(FullBrain(), buffer_capacity=10)
    for i in range(5):
        imp.observe(label=f"obj_{i}", description=f"description {i}",
                     modality="visual", loss=0.5)
    assert len(imp.buffer) == 5
    assert imp.stats()["observed"] == 5
    print(f"  PASS: observe buffers events ({imp.stats()})")


def test_buffer_capacity_respected():
    from childhood_memories import RunningImplanter
    imp = RunningImplanter(FullBrain(), buffer_capacity=5)
    for i in range(10):
        imp.observe(label=f"obj_{i}", loss=0.5)
    assert len(imp.buffer) == 5
    assert imp.stats()["observed"] == 10
    print(f"  PASS: buffer capped (10 observed, {len(imp.buffer)} retained)")


def test_salience_from_loss_deviation():
    from childhood_memories import RunningImplanter
    imp = RunningImplanter(FullBrain())
    # Seed low-loss baseline
    for _ in range(20):
        imp.observe(label="background", loss=0.1)
    # High-surprise event
    imp.observe(label="surprise", loss=2.0)
    # Low-surprise event
    imp.observe(label="normal", loss=0.12)
    events = list(imp.buffer)
    surprise_ev = [e for e in events if e.label == "surprise"][0]
    normal_ev = [e for e in events if e.label == "normal"][0]
    assert surprise_ev.salience > normal_ev.salience
    print(f"  PASS: salience tracks surprise "
          f"(surprise={surprise_ev.salience:.2f}, normal={normal_ev.salience:.2f})")


def test_consolidate_high_salience_first():
    from childhood_memories import RunningImplanter
    brain = FullBrain()
    imp = RunningImplanter(brain, min_salience_for_consolidation=0.0)
    for _ in range(20):
        imp.observe(label="background", loss=0.1)
    imp.observe(label="surprise", loss=2.0,
                description="unusual event")
    n = imp.consolidate_batch(top_k=3)
    assert n > 0
    # Surprise should be among consolidated
    consolidated_labels = [e.label for e in imp.buffer if e.consolidated]
    assert "surprise" in consolidated_labels
    print(f"  PASS: high-salience events consolidated first "
          f"({n} consolidated, surprise included)")


def test_dedup_within_window():
    from childhood_memories import RunningImplanter
    brain = FullBrain()
    imp = RunningImplanter(brain, dedup_window_s=60.0,
                             min_salience_for_consolidation=0.0)
    imp.observe(label="dog", loss=1.0)
    imp.consolidate_batch(top_k=5)
    kg_calls_after_first = len(brain.kg_calls)
    # Observe same label again — should be dedup'd
    imp.observe(label="dog", loss=1.0)
    imp.consolidate_batch(top_k=5)
    kg_calls_after_second = len(brain.kg_calls)
    assert kg_calls_after_second == kg_calls_after_first
    assert imp.stats()["skipped_dedup"] >= 1
    print(f"  PASS: dedup within window (kg_calls {kg_calls_after_first} → "
          f"{kg_calls_after_second} after dup, skipped_dedup={imp.stats()['skipped_dedup']})")


def test_dedup_window_expires():
    from childhood_memories import RunningImplanter
    brain = FullBrain()
    imp = RunningImplanter(brain, dedup_window_s=0.01,  # 10ms
                             min_salience_for_consolidation=0.0)
    imp.observe(label="dog", loss=1.0)
    imp.consolidate_batch(top_k=5)
    time.sleep(0.05)
    imp.observe(label="dog", loss=1.0)
    imp.consolidate_batch(top_k=5)
    # Both should be consolidated
    assert imp.stats()["consolidated"] >= 2
    print(f"  PASS: dedup window expires — {imp.stats()['consolidated']} consolidations across time")


def test_salience_threshold_filters():
    from childhood_memories import RunningImplanter
    brain = FullBrain()
    imp = RunningImplanter(brain, min_salience_for_consolidation=0.5)
    # All events have salience < 0.5
    for _ in range(10):
        imp.observe(label="bg", loss=0.1)
    n = imp.consolidate_batch(top_k=5)
    # Low salience → nothing consolidated
    assert n == 0
    assert imp.stats()["skipped_salience"] >= 0
    print(f"  PASS: salience threshold filters low-surprise events "
          f"({imp.stats()})")


def test_works_with_partial_api():
    from childhood_memories import RunningImplanter
    brain = PartialBrain()
    imp = RunningImplanter(brain, min_salience_for_consolidation=0.0)
    for i in range(5):
        imp.observe(label=f"item_{i}", loss=float(i))
    n = imp.consolidate_batch(top_k=5)
    # Only kg_add_fact available — but _consolidate_one returns True if
    # any write succeeded
    assert n > 0
    assert len(brain.kg_calls) > 0
    print(f"  PASS: partial API — {n} consolidated to KG ({len(brain.kg_calls)} fact calls)")


def test_graceful_with_no_apis():
    from childhood_memories import RunningImplanter

    class EmptyBrain:
        pass

    imp = RunningImplanter(EmptyBrain(), min_salience_for_consolidation=0.0)
    imp.observe(label="item", loss=1.0)
    n = imp.consolidate_batch(top_k=5)
    # _consolidate_one returns False — no successful writes
    assert n == 0  # nothing successfully written
    print(f"  PASS: graceful with no APIs (n={n})")


def test_set_stage_widens_dedup():
    from childhood_memories import RunningImplanter
    imp = RunningImplanter(FullBrain())
    imp.set_stage(0); assert imp.dedup_window_s == 600.0
    imp.set_stage(1); assert imp.dedup_window_s == 600.0
    imp.set_stage(2); assert imp.dedup_window_s == 1800.0
    imp.set_stage(3); assert imp.dedup_window_s == 3600.0
    print(f"  PASS: dedup window expands with stage progression")


def test_reset_clears_state():
    from childhood_memories import RunningImplanter
    imp = RunningImplanter(FullBrain(), min_salience_for_consolidation=0.0)
    for _ in range(5):
        imp.observe(label="x", loss=1.0)
    imp.consolidate_batch(top_k=1)
    imp.reset()
    s = imp.stats()
    assert s["observed"] == 0
    assert s["buffer_size"] == 0
    assert s["unique_labels_implanted"] == 0
    print(f"  PASS: reset clears all state")


def main():
    failures = []
    for name, fn in [
        ("observe_buffers_events", test_observe_buffers_events),
        ("buffer_capacity_respected", test_buffer_capacity_respected),
        ("salience_from_loss_deviation", test_salience_from_loss_deviation),
        ("consolidate_high_salience_first", test_consolidate_high_salience_first),
        ("dedup_within_window", test_dedup_within_window),
        ("dedup_window_expires", test_dedup_window_expires),
        ("salience_threshold_filters", test_salience_threshold_filters),
        ("works_with_partial_api", test_works_with_partial_api),
        ("graceful_with_no_apis", test_graceful_with_no_apis),
        ("set_stage_widens_dedup", test_set_stage_widens_dedup),
        ("reset_clears_state", test_reset_clears_state),
    ]:
        print(f"[unit/running_implanter] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll running implanter unit tests passed.")


if __name__ == "__main__":
    main()
