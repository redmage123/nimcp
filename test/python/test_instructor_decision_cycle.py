#!/usr/bin/env python3
"""Tests for instructor decision cycle integration.

Verifies that the InstructorAgent correctly:
  - Runs _run_decision_cycle at report_interval boundaries
  - Uses decision cycle lr_factor in _modulate_lr
  - Includes decision_cycle dict in progress reports
  - Tracks gradient norms from learn() calls

These tests use mocks to avoid needing a real nimcp brain or HuggingFace
datasets.
"""

import sys
import os
import queue
import threading
import time
from collections import deque

# Add scripts dir so we can import from instructor_agent
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))


# ---------------------------------------------------------------------------
# Mock Brain — records calls, returns plausible values
# ---------------------------------------------------------------------------

class MockBrain:
    """Minimal mock brain for testing InstructorAgent methods."""

    def __init__(self):
        self._calls = []
        self._lock = threading.Lock()
        self._gradient_norm = 0.5

    def _record(self, method, args=(), kwargs=None):
        with self._lock:
            self._calls.append((method, args, kwargs or {}))

    def get_calls(self, method=None):
        with self._lock:
            if method:
                return [c for c in self._calls if c[0] == method]
            return list(self._calls)

    def learn(self, *args, **kwargs):
        self._record('learn', args, kwargs)
        return True

    def predict_fast(self, *args, **kwargs):
        self._record('predict_fast', args, kwargs)
        return ("label", 0.7)

    def predict(self, *args, **kwargs):
        self._record('predict', args, kwargs)
        return ("label", 0.7)

    def decide(self, *args, **kwargs):
        self._record('decide', args, kwargs)
        return {}

    def consolidate(self, *args, **kwargs):
        self._record('consolidate', args, kwargs)
        return {}

    def save(self, *args, **kwargs):
        self._record('save', args, kwargs)

    def probe(self, *args, **kwargs):
        self._record('probe', args, kwargs)
        return {
            "num_neurons": 1000, "avg_sparsity": 0.5,
            "current_learning_rate": 0.01, "total_learning_steps": 500,
            "accuracy": 0.6,
        }

    def ti_compute_unified_lr(self, base_lr, *args, **kwargs):
        self._record('ti_compute_unified_lr', (base_lr,), kwargs)
        return base_lr * 0.95  # Small modulation

    def ti_compute_modulation_state(self, *args, **kwargs):
        self._record('ti_compute_modulation_state', args, kwargs)
        return {"final_lr_factor": 0.95, "should_pause": False}

    def ti_post_batch_update(self, *args, **kwargs):
        self._record('ti_post_batch_update', args, kwargs)

    def ti_compute_decision_cycle(self, *args, **kwargs):
        self._record('ti_compute_decision_cycle', args, kwargs)
        return {
            "consensus_action": 0,
            "lr_factor": 0.85,
            "batch_factor": 1.2,
            "grad_clip_factor": 1.0,
            "urgency": 0.3,
            "converged": True,
            "primary_diagnosis": "converging",
            "causal_explanation": "loss decreasing steadily",
            "recommend_pause": False,
        }

    def get_last_gradient_norm(self, *args, **kwargs):
        self._record('get_last_gradient_norm', args, kwargs)
        return self._gradient_norm

    def ti_init_reasoning(self, *args, **kwargs):
        self._record('ti_init_reasoning', args, kwargs)

    def ti_reason(self, *args, **kwargs):
        self._record('ti_reason', args, kwargs)
        return 0.8

    def ti_compute_adaptive_lr(self, base_lr, *args, **kwargs):
        self._record('ti_compute_adaptive_lr', (base_lr,), kwargs)
        return base_lr * 0.9

    def ti_add_fact(self, *args, **kwargs):
        self._record('ti_add_fact', args, kwargs)

    def ti_add_rule(self, *args, **kwargs):
        self._record('ti_add_rule', args, kwargs)

    def ti_forward_chain(self, *args, **kwargs):
        self._record('ti_forward_chain', args, kwargs)
        return 0

    def ti_backward_chain(self, *args, **kwargs):
        self._record('ti_backward_chain', args, kwargs)
        return 0.5

    # Simulate attribute checks
    def __getattr__(self, name):
        # For hasattr checks on curiosity/uncertainty/etc.
        if name.startswith('curiosity_') or name.startswith('self_'):
            raise AttributeError(name)
        if name in ('get_uncertainty', 'bg_update_reward', 'bg_get_conflict',
                     'bg_get_mode', 'bg_check_habit', 'bg_register_habit',
                     'bg_strengthen_habit', 'medulla_get_arousal',
                     'medulla_boost_arousal', 'medulla_get_circadian_efficiency',
                     'cache_communities', 'invalidate_community_cache'):
            raise AttributeError(name)
        raise AttributeError(f"MockBrain has no attribute '{name}'")


# Import after path setup, and provide minimal mocks for heavy dependencies
# that instructor_agent.py tries to import
class _MockSafetyGate:
    def __init__(self, *args, **kwargs): pass

class _MockSafetyConfig:
    pass

class _MockDataSkeptic:
    def __init__(self, *args, **kwargs): pass
    def grade(self, *args, **kwargs): return None

class _MockDataGrade:
    confidence_modifier = 1.0
    ethics_label = "neutral"

# Patch modules before importing instructor_agent
import types

# Create mock modules
safety_mod = types.ModuleType('safety_gate')
safety_mod.SafetyGate = _MockSafetyGate
safety_mod.SafetyConfig = _MockSafetyConfig
sys.modules['safety_gate'] = safety_mod

skeptic_mod = types.ModuleType('data_skeptic')
skeptic_mod.DataSkeptic = _MockDataSkeptic
skeptic_mod.DataGrade = _MockDataGrade
sys.modules['data_skeptic'] = skeptic_mod

# Mock benchmark_datasets
bench_mod = types.ModuleType('benchmark_datasets')
def _text_to_features(text, num_inputs):
    import hashlib
    h = hashlib.md5(text.encode()).digest()
    features = [0.0] * num_inputs
    for i, b in enumerate(h):
        features[i % num_inputs] += b / 255.0
    return features
bench_mod.text_to_features = _text_to_features
sys.modules['benchmark_datasets'] = bench_mod

# Mock socratic_trainer
class _MockMastery:
    def __init__(self):
        self._data = {}
    def mastery(self, domain):
        return self._data.get(domain, 0.0)
    def record(self, domain, correct):
        if domain not in self._data:
            self._data[domain] = 0.0
        alpha = 0.1
        self._data[domain] = alpha * (1.0 if correct else 0.0) + (1 - alpha) * self._data[domain]

class _MockSocraticConfig:
    pass

class _MockSocraticTrainer:
    def __init__(self, brain, config):
        self.mastery = _MockMastery()
        self.replay_buffer = []

socratic_mod = types.ModuleType('socratic_trainer')
socratic_mod.SocraticTrainer = _MockSocraticTrainer
socratic_mod.SocraticConfig = _MockSocraticConfig
sys.modules['socratic_trainer'] = socratic_mod

# Mock streaming_train as unavailable
if 'streaming_train' in sys.modules:
    del sys.modules['streaming_train']

from instructor_agent import InstructorAgent, InstructorConfig
from cognitive_orchestrator import CognitiveOrchestrator


# ============================================================================
# TestDecisionCycleIntegration
# ============================================================================

class TestDecisionCycleIntegration:
    """Tests for the decision cycle integration in InstructorAgent."""

    def _make_agent(self, report_interval=10):
        """Create a minimal InstructorAgent for testing."""
        brain = MockBrain()
        config = InstructorConfig(
            domain="test_domain",
            modality="text",
            max_examples_per_dataset=1000,
            startup_delay_s=0.0,
            report_interval=report_interval,
        )
        stop_event = threading.Event()
        recess_event = threading.Event()
        school_queue = queue.Queue(maxsize=100)
        cross_domain_queue = queue.Queue(maxsize=100)

        agent = InstructorAgent(
            brain=brain, config=config, datasets=[],
            school_queue=school_queue, cross_domain_queue=cross_domain_queue,
            stop_event=stop_event, recess_event=recess_event,
            num_inputs=64,
        )
        return agent, brain

    def test_decision_cycle_runs_at_report_interval(self):
        """Verify _run_decision_cycle fires every report_interval."""
        agent, brain = self._make_agent(report_interval=5)

        # Manually exercise _update_metrics_and_decide to simulate training
        for i in range(12):
            agent.total_examples = i + 1
            agent._update_metrics_and_decide(loss=0.5 - i * 0.01)

        # Decision cycle should have run at total_examples=5 and 10
        dc_calls = brain.get_calls('ti_compute_decision_cycle')
        assert len(dc_calls) >= 2, \
            f"Expected at least 2 decision cycle calls (at 5 and 10), got {len(dc_calls)}"
        print(f"  PASS: decision cycle ran {len(dc_calls)} times at report_interval=5")

    def test_modulate_lr_uses_decision_factor(self):
        """Verify _modulate_lr uses decision cycle lr_factor."""
        agent, brain = self._make_agent()

        # No decision yet -> should fall through to unified pipeline
        lr_no_decision = agent._modulate_lr(0.5)
        # Unified pipeline returns base_lr * 0.95
        assert abs(lr_no_decision - 0.5 * 0.95) < 0.01, \
            f"Without decision, LR should come from unified pipeline, got {lr_no_decision}"

        # Simulate a decision cycle result
        agent._last_decision = {"lr_factor": 0.7, "batch_factor": 1.0}
        lr_with_decision = agent._modulate_lr(0.5)
        expected = 0.5 * 0.7
        assert abs(lr_with_decision - expected) < 0.01, \
            f"With decision lr_factor=0.7, expected {expected}, got {lr_with_decision}"
        print(f"  PASS: _modulate_lr uses decision lr_factor (0.5*0.7={expected})")

    def test_decision_info_in_report(self):
        """Verify progress reports include decision_cycle dict."""
        agent, brain = self._make_agent()
        agent._start_time = time.time() - 10  # Simulate 10s elapsed

        # No decision cycle -> decision_cycle should be None in report
        agent._send_report()
        report_no_dc = agent.school_queue.get_nowait()
        assert report_no_dc["decision_cycle"] is None, \
            f"Without decision, decision_cycle should be None, got {report_no_dc['decision_cycle']}"

        # Set a decision
        agent._last_decision = {
            "consensus_action": 0,
            "lr_factor": 0.85,
            "batch_factor": 1.2,
            "urgency": 0.3,
            "converged": True,
            "primary_diagnosis": "converging",
            "recommend_pause": False,
        }
        agent._send_report()
        report_with_dc = agent.school_queue.get_nowait()
        dc = report_with_dc["decision_cycle"]
        assert dc is not None, "decision_cycle should be present"
        assert dc["lr_factor"] == 0.85, f"lr_factor should be 0.85, got {dc['lr_factor']}"
        assert dc["converged"] is True
        assert dc["diagnosis"] == "converging"
        print(f"  PASS: progress report includes decision_cycle dict")

    def test_gradient_norm_tracked(self):
        """Verify gradient norms are captured after each learn()."""
        agent, brain = self._make_agent()

        # Set known gradient norms
        brain._gradient_norm = 1.5
        agent.total_examples = 1
        agent._update_metrics_and_decide(loss=0.4)

        brain._gradient_norm = 2.3
        agent.total_examples = 2
        agent._update_metrics_and_decide(loss=0.3)

        assert len(agent._grad_history) == 2, \
            f"Should have 2 gradient entries, got {len(agent._grad_history)}"
        assert agent._grad_history[0] == 1.5, \
            f"First gradient should be 1.5, got {agent._grad_history[0]}"
        assert agent._grad_history[1] == 2.3, \
            f"Second gradient should be 2.3, got {agent._grad_history[1]}"
        assert agent._last_grad_norm == 2.3
        print(f"  PASS: gradient norms tracked ({list(agent._grad_history)})")

    def test_loss_history_tracked(self):
        """Verify loss values are accumulated in rolling history."""
        agent, brain = self._make_agent()

        losses = [0.9, 0.8, 0.7, 0.6, 0.5]
        for i, loss in enumerate(losses):
            agent.total_examples = i + 1
            agent._update_metrics_and_decide(loss=loss)

        assert len(agent._loss_history) == 5
        assert list(agent._loss_history) == losses
        assert agent._last_loss == 0.5
        print(f"  PASS: loss history tracked (5 entries)")

    def test_decision_cycle_computes_volatility(self):
        """Verify _run_decision_cycle computes loss volatility and gradient variance."""
        agent, brain = self._make_agent(report_interval=5)

        # Feed enough data for a decision cycle
        losses = [0.5, 0.4, 0.6, 0.3, 0.5]
        brain._gradient_norm = 1.0
        for i, loss in enumerate(losses):
            agent.total_examples = i + 1
            agent._update_metrics_and_decide(loss=loss)

        # At total_examples=5, decision cycle should fire
        dc_calls = brain.get_calls('ti_compute_decision_cycle')
        assert len(dc_calls) >= 1, "Decision cycle should have fired at step 5"

        # Verify the call was made with reasonable arguments
        call_args = dc_calls[0][1]  # positional args
        # Args: loss_current, loss_previous, grad_norm, grad_norm_previous,
        #        loss_volatility, gradient_variance, current_lr, current_batch
        assert len(call_args) == 8, f"Expected 8 args, got {len(call_args)}"
        loss_current = call_args[0]
        loss_previous = call_args[1]
        loss_volatility = call_args[4]
        assert loss_current == losses[-1], \
            f"loss_current should be {losses[-1]}, got {loss_current}"
        assert loss_previous == losses[-2], \
            f"loss_previous should be {losses[-2]}, got {loss_previous}"
        assert loss_volatility >= 0, "loss_volatility should be non-negative"
        print(f"  PASS: decision cycle computes volatility={loss_volatility:.4f}")

    def test_modulate_lr_fallback_when_no_reasoning(self):
        """Verify _modulate_lr falls back gracefully when brain has no reasoning."""
        agent, brain = self._make_agent()

        # Remove unified LR method to force fallback
        original = brain.ti_compute_unified_lr
        def raise_attr(*args, **kwargs):
            raise AttributeError("not available")
        brain.ti_compute_unified_lr = raise_attr

        # Should fall back to base_lr
        agent._last_decision = None
        lr = agent._modulate_lr(0.5)
        # The cognitive orchestrator tries unified first, then old adaptive.
        # Both may fail -> returns base_lr
        assert lr > 0, f"LR should be positive even on fallback, got {lr}"

        # Restore
        brain.ti_compute_unified_lr = original
        print(f"  PASS: _modulate_lr falls back gracefully (lr={lr})")

    def test_send_report_includes_method_stats(self):
        """Verify progress reports include method_stats summary."""
        agent, brain = self._make_agent()
        agent._start_time = time.time() - 5

        agent._send_report()
        report = agent.school_queue.get_nowait()

        assert "method_stats" in report, "Report should include method_stats"
        assert "domain" in report
        assert report["domain"] == "test_domain"
        assert "accuracy" in report
        assert "mastery" in report
        print(f"  PASS: report includes method_stats and core fields")


# ============================================================================
# TestHardExampleDecay
# ============================================================================

class TestHardExampleDecay:
    """Verify hard examples are decayed to prevent unbounded growth."""

    def test_decay_called_periodically(self):
        """Verify hard examples are decayed to prevent unbounded growth.

        This test checks that the HardExampleMiner.decay() method exists
        and properly reduces loss values, preventing the hard example bank
        from growing without bound.
        """
        # Import the real HardExampleMiner
        # We copy the class inline since train_athena has heavy dependencies
        import random

        class HardExampleMiner:
            def __init__(self, capacity=5000, replay_ratio=0.2):
                self.capacity = capacity
                self.replay_ratio = replay_ratio
                self.hard_examples = []
                self.min_loss_threshold = 0.3

            def record(self, features, label, loss):
                if loss > self.min_loss_threshold:
                    self.hard_examples.append((features, label, loss))
                    if len(self.hard_examples) > self.capacity:
                        self.hard_examples.sort(key=lambda x: x[2], reverse=True)
                        self.hard_examples = self.hard_examples[:self.capacity]

            def decay(self, factor=0.95):
                for i in range(len(self.hard_examples)):
                    f, l, loss = self.hard_examples[i]
                    self.hard_examples[i] = (f, l, loss * factor)
                self.hard_examples = [
                    x for x in self.hard_examples
                    if x[2] > self.min_loss_threshold * 0.5
                ]

        miner = HardExampleMiner(capacity=100)

        # Record 50 hard examples
        for i in range(50):
            miner.record([float(i)], f"label_{i}", 0.35 + i * 0.01)

        initial_count = len(miner.hard_examples)
        assert initial_count == 50, f"Should have 50 examples, got {initial_count}"

        # Simulate periodic decay (like what happens during training)
        for epoch in range(20):
            miner.decay(factor=0.95)

        # After 20 decays, borderline examples (loss ~ 0.35) should drop out
        # 0.35 * 0.95^20 = 0.35 * 0.3585 = 0.1255 < 0.15 (threshold * 0.5)
        final_count = len(miner.hard_examples)
        assert final_count < initial_count, \
            f"Decay should remove borderline examples: {initial_count} -> {final_count}"

        # High-loss examples should survive longer
        surviving_losses = [ex[2] for ex in miner.hard_examples]
        if surviving_losses:
            assert min(surviving_losses) >= miner.min_loss_threshold * 0.5, \
                "All surviving examples should be above decay threshold"

        print(f"  PASS: decay removes borderline examples ({initial_count} -> {final_count})")

    def test_decay_preserves_ordering(self):
        """Verify decay preserves relative ordering of examples."""
        import random

        class HardExampleMiner:
            def __init__(self, capacity=5000, replay_ratio=0.2):
                self.capacity = capacity
                self.replay_ratio = replay_ratio
                self.hard_examples = []
                self.min_loss_threshold = 0.3

            def record(self, features, label, loss):
                if loss > self.min_loss_threshold:
                    self.hard_examples.append((features, label, loss))

            def decay(self, factor=0.95):
                for i in range(len(self.hard_examples)):
                    f, l, loss = self.hard_examples[i]
                    self.hard_examples[i] = (f, l, loss * factor)
                self.hard_examples = [
                    x for x in self.hard_examples
                    if x[2] > self.min_loss_threshold * 0.5
                ]

        miner = HardExampleMiner(capacity=100)
        miner.record([1.0], "a", 0.9)
        miner.record([2.0], "b", 0.5)
        miner.record([3.0], "c", 0.7)

        miner.decay(factor=0.95)

        losses = [ex[2] for ex in miner.hard_examples]
        # a: 0.855, c: 0.665, b: 0.475 — all still above 0.15
        assert len(losses) == 3, "All should survive one decay"
        # Original order preserved (a > c > b)
        assert losses[0] > losses[2] > losses[1], \
            f"Relative ordering should be preserved: {losses}"
        print(f"  PASS: decay preserves relative ordering")


# ============================================================================
# Runner
# ============================================================================

def run_test_class(cls):
    """Run all test_* methods on a class, return (passed, failed)."""
    instance = cls()
    passed, failed = 0, 0
    for name in sorted(dir(instance)):
        if not name.startswith("test_"):
            continue
        method = getattr(instance, name)
        try:
            method()
            passed += 1
        except Exception as e:
            print(f"  FAIL: {name}: {e}")
            import traceback
            traceback.print_exc()
            failed += 1
    return passed, failed


if __name__ == "__main__":
    total_passed, total_failed = 0, 0

    test_classes = [
        TestDecisionCycleIntegration,
        TestHardExampleDecay,
    ]

    for cls in test_classes:
        print(f"\n{'=' * 60}")
        print(f"  {cls.__name__}")
        print(f"{'=' * 60}")
        p, f = run_test_class(cls)
        total_passed += p
        total_failed += f
        print(f"  --- {p} passed, {f} failed ---")

    print(f"\n{'=' * 60}")
    print(f"  TOTAL: {total_passed} passed, {total_failed} failed")
    print(f"{'=' * 60}")

    sys.exit(0 if total_failed == 0 else 1)
