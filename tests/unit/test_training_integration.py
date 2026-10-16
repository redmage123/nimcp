#!/usr/bin/env python3
"""Unit tests for TrainingIntegration — the wiring bundle for
immerse_athena.py.

Each module it wires (curiosity, curriculum, symbolic writer,
innate priors, running implanter) has its own tests. These tests verify
the integration layer itself — that it calls the right things in the
right order and handles missing APIs gracefully.
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class MockSource:
    """Stimulus source used by wrap_source()."""
    def __init__(self):
        self.items = [("dog", "furry animal"), ("cat", "purring pet"),
                       ("tree", "tall green plant"), ("ball", "round toy")]
        self.idx = 0
    def get_object(self):
        o = self.items[self.idx % len(self.items)]
        self.idx += 1
        return o
    def get_fact(self, preferred_domain=None):
        return ("The sky is blue.", "color")


class MockBrain:
    """Minimal brain exposing key APIs so integration has something to call."""
    def __init__(self):
        self.kg_calls = []
        self.episode_calls = []
        self.semantic_calls = []
        self.attention_calls = []
        self.innate_calls = []
    def kg_add_fact(self, **kw): self.kg_calls.append(kw)
    def hippocampus_seed_episode(self, **kw): self.episode_calls.append(kw)
    def semantic_memory_insert(self, **kw): self.semantic_calls.append(kw)
    def thalamus_set_attention(self, nucleus, strength):
        self.attention_calls.append((nucleus, strength))
    def innate_hardwire(self, prior_name, filters, metadata):
        self.innate_calls.append(prior_name)
    def curiosity_detect_gaps(self, topic=None):
        return []


def test_integration_initializes_with_flags():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain)
    assert ti.flags["curiosity"]
    assert ti.flags["symbolic_writer"]
    assert ti.flags["innate_priors"]
    print(f"  PASS: integration initializes with default flags")


def test_apply_innate_priors_calls_brain_hardwire():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain)
    summary = ti.apply_innate_priors()
    assert len(summary["applied"]) == 5, f"expected 5 priors, got {summary['applied']}"
    assert len(brain.innate_calls) == 5
    print(f"  PASS: innate priors applied ({summary['applied']})")


def test_wrap_source_wraps_with_both():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    source = MockSource()
    ti = TrainingIntegration(brain)
    wrapped = ti.wrap_source(source)
    # Pick should delegate through curiosity + curriculum
    name, desc = wrapped.pick_object()
    assert name in {"dog", "cat", "tree", "ball"}
    print(f"  PASS: wrap_source produces working wrapped source ({name})")


def test_before_and_after_learn_vector():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain)
    ti.before_learn_vector()
    # Attention should be boosted
    assert any(c[0] == "LGN" and c[1] > 1.0 for c in brain.attention_calls)

    ti.after_learn_vector(label="dog", description="furry",
                           modality="visual", loss=0.3)
    # Attention should be reset
    assert any(c[0] == "LGN" and c[1] == 1.0 for c in brain.attention_calls)
    # Symbolic writer should have fired
    assert len(brain.kg_calls) >= 1 or len(brain.semantic_calls) >= 1
    # Running implanter should have observed
    stats = ti.stats()
    assert stats["running_implanter"]["observed"] == 1
    print(f"  PASS: before/after hooks wire correctly "
          f"(attention={len(brain.attention_calls)}, writes={len(brain.kg_calls)})")


def test_periodic_consolidate_runs():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain, consolidation_interval_steps=5,
                               consolidation_top_k=3)
    # Observe a bunch with varied loss (for salience)
    for i in range(15):
        loss = 0.1 if i % 5 else 2.0  # every 5th is surprising
        ti.after_learn_vector(label=f"item_{i}",
                               description="",
                               modality="visual",
                               loss=loss, baseline_loss=0.3)
    # After 15 steps with interval=5, three consolidations should have fired.
    # Each will write consolidated events to brain stores.
    stats = ti.stats()
    observed = stats["running_implanter"]["observed"]
    consolidated = stats["running_implanter"]["consolidated"]
    assert observed == 15
    assert consolidated > 0
    print(f"  PASS: periodic consolidation ran ({consolidated} consolidated of {observed})")


def test_stage_transitions_reset_counter():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain)
    for _ in range(5):
        ti.after_learn_vector(label="x", loss=0.1)
    assert ti.stats()["step_counter"] == 5
    ti.begin_stage(2)
    assert ti.stats()["step_counter"] == 0
    assert ti.stats()["current_stage"] == 2
    print(f"  PASS: stage transitions reset step counter")


def test_graceful_with_disabled_flags():
    from training_integration import TrainingIntegration
    brain = MockBrain()
    ti = TrainingIntegration(brain,
                               enable_curiosity=False,
                               enable_curriculum=False,
                               enable_symbolic_writer=False,
                               enable_running_implanter=False,
                               enable_innate_priors=False)
    # Nothing should raise
    summary = ti.apply_innate_priors()
    assert summary.get("skipped_flag")
    wrapped = ti.wrap_source(MockSource())
    ti.before_learn_vector()
    ti.after_learn_vector(label="x", loss=0.1)
    ti.periodic_consolidate()
    # Stats should show all flags disabled
    stats = ti.stats()
    assert not any(stats["flags"].values())
    print(f"  PASS: all flags disabled — no writes, no crashes")


def test_graceful_with_bare_brain():
    """Brain with no APIs — integration must not crash."""
    from training_integration import TrainingIntegration

    class BareBrain:
        pass

    ti = TrainingIntegration(BareBrain())
    ti.apply_innate_priors()
    wrapped = ti.wrap_source(MockSource())
    ti.before_learn_vector()
    ti.after_learn_vector(label="test", loss=0.5)
    ti.periodic_consolidate()
    # Should complete without exception
    print(f"  PASS: bare brain — integration runs without crash")


def main():
    failures = []
    tests = [
        ("integration_initializes_with_flags", test_integration_initializes_with_flags),
        ("apply_innate_priors_calls_brain_hardwire", test_apply_innate_priors_calls_brain_hardwire),
        ("wrap_source_wraps_with_both", test_wrap_source_wraps_with_both),
        ("before_and_after_learn_vector", test_before_and_after_learn_vector),
        ("periodic_consolidate_runs", test_periodic_consolidate_runs),
        ("stage_transitions_reset_counter", test_stage_transitions_reset_counter),
        ("graceful_with_disabled_flags", test_graceful_with_disabled_flags),
        ("graceful_with_bare_brain", test_graceful_with_bare_brain),
    ]
    for name, fn in tests:
        print(f"[unit/training_integration] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print(f"\nAll {len(tests)} training integration tests passed.")


if __name__ == "__main__":
    main()
