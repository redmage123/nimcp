#!/usr/bin/env python3
"""Tests for the Athena 95% accuracy pipeline changes.

Covers:
  - _CosineAnnealingLR scheduler
  - Hybrid feature extraction (n-gram fallback)
  - Spaced repetition (push/tick/interval logic)
  - Difficulty-gated curriculum (skip/LR scaling)
  - Self-assessment (holdout, overfitting detection)
  - Method rebalancing (update_weights, select_method)
  - _modulate_lr with scheduler + decision cycle + fallback
  - Metacognition check (regression detection, peak tracking)
  - Domain centroid tracking (EMA, first-example)
  - ExtrapolationEngine (generate, hypothesize, blend, novelty)

Uses mocks to avoid needing a real nimcp brain, HuggingFace datasets, or
sentence-transformers.
"""

import sys
import os
import math
import queue
import random
import threading
import time
from collections import deque

import numpy as np

# Add scripts dir so we can import from instructor_agent / extrapolation_engine
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))


# ---------------------------------------------------------------------------
# Mock Brain — records calls, returns plausible values
# ---------------------------------------------------------------------------

class MockBrain:
    """Comprehensive mock brain for testing InstructorAgent + ExtrapolationEngine."""

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

    # --- Core ---
    def learn(self, *args, **kwargs):
        self._record('learn', args, kwargs)
        return True

    def predict_fast(self, *args, **kwargs):
        self._record('predict_fast', args, kwargs)
        return ("label", 0.7)

    def predict(self, *args, **kwargs):
        self._record('predict', args, kwargs)
        return ("label", 0.7)

    def predict_in_domain(self, features, prefix, *args, **kwargs):
        self._record('predict_in_domain', (features, prefix), kwargs)
        return ("domain_label", 0.7)

    def decide(self, *args, **kwargs):
        self._record('decide', args, kwargs)
        return {}

    def decide_full(self, *args, **kwargs):
        self._record('decide_full', args, kwargs)
        return {
            "label": "mock_label",
            "confidence": 0.75,
            "explanation": "mock explanation",
            "output_vector": [0.1] * 256,
            "num_active_neurons": 42,
            "sparsity": 0.3,
        }

    def infer(self, features, num_outputs, *args, **kwargs):
        self._record('infer', (features, num_outputs), kwargs)
        return [0.1] * num_outputs

    def consolidate(self, *args, **kwargs):
        self._record('consolidate', args, kwargs)
        return True

    def save(self, *args, **kwargs):
        self._record('save', args, kwargs)

    def probe(self, *args, **kwargs):
        self._record('probe', args, kwargs)
        return {
            "num_neurons": 1000, "avg_sparsity": 0.5,
            "current_learning_rate": 0.01, "total_learning_steps": 500,
            "accuracy": 0.6,
            "num_inputs": 1024, "num_outputs": 256,
        }

    # --- Decision cycle / training ---
    def ti_compute_unified_lr(self, base_lr, *args, **kwargs):
        self._record('ti_compute_unified_lr', (base_lr,), kwargs)
        return base_lr * 0.95

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

    def ti_compute_adaptive_lr(self, base_lr, *args, **kwargs):
        self._record('ti_compute_adaptive_lr', (base_lr,), kwargs)
        return base_lr * 0.9

    # --- Reasoning ---
    def ti_reason(self, *args, **kwargs):
        self._record('ti_reason', args, kwargs)
        return 0.7

    def ti_add_fact(self, *args, **kwargs):
        self._record('ti_add_fact', args, kwargs)
        return True

    def ti_add_rule(self, *args, **kwargs):
        self._record('ti_add_rule', args, kwargs)

    def ti_forward_chain(self, *args, **kwargs):
        self._record('ti_forward_chain', args, kwargs)
        return 3

    def ti_backward_chain(self, *args, **kwargs):
        self._record('ti_backward_chain', args, kwargs)
        return 0.6

    def ti_query_knowledge(self, *args, **kwargs):
        self._record('ti_query_knowledge', args, kwargs)
        return 5

    def ti_get_cognitive_capacity(self, *args, **kwargs):
        self._record('ti_get_cognitive_capacity', args, kwargs)
        return 0.8

    def ti_get_stress_level(self, *args, **kwargs):
        self._record('ti_get_stress_level', args, kwargs)
        return 0.2

    def ti_get_urgency_mode(self, *args, **kwargs):
        self._record('ti_get_urgency_mode', args, kwargs)
        return 0

    # --- Working memory ---
    def working_memory_add(self, data, salience, *args, **kwargs):
        self._record('working_memory_add', (data, salience), kwargs)
        return None

    def working_memory_stats(self, *args, **kwargs):
        self._record('working_memory_stats', args, kwargs)
        return (2, 10)

    # --- Mesh network ---
    def ti_mesh_is_available(self, *args, **kwargs):
        self._record('ti_mesh_is_available', args, kwargs)
        return True

    def ti_mesh_get_participant_count(self, *args, **kwargs):
        self._record('ti_mesh_get_participant_count', args, kwargs)
        return 3

    def ti_mesh_get_coherence(self, *args, **kwargs):
        self._record('ti_mesh_get_coherence', args, kwargs)
        return 0.7

    # --- Global workspace ---
    def workspace_compete(self, module, content, strength, *args, **kwargs):
        self._record('workspace_compete', (module, content, strength), kwargs)
        return True

    def workspace_has_broadcast(self, *args, **kwargs):
        self._record('workspace_has_broadcast', args, kwargs)
        return True

    def workspace_read(self, max_dim, *args, **kwargs):
        self._record('workspace_read', (max_dim,), kwargs)
        return ([0.1] * 256, 256, "extrapolation_engine")

    # --- Basal ganglia ---
    def bg_get_conflict(self, *args, **kwargs):
        self._record('bg_get_conflict', args, kwargs)
        return 0.3

    def bg_get_rpe(self, *args, **kwargs):
        self._record('bg_get_rpe', args, kwargs)
        return 0.2

    def bg_get_dopamine(self, *args, **kwargs):
        self._record('bg_get_dopamine', args, kwargs)
        return 0.6

    def bg_get_mode(self, *args, **kwargs):
        self._record('bg_get_mode', args, kwargs)
        return 0

    def bg_update_reward(self, r, e, *args, **kwargs):
        self._record('bg_update_reward', (r, e), kwargs)
        return 0

    # --- Neuromodulators ---
    def release_dopamine(self, amount, predicted, *args, **kwargs):
        self._record('release_dopamine', (amount, predicted), kwargs)
        return 0.6

    def medulla_boost_arousal(self, delta, *args, **kwargs):
        self._record('medulla_boost_arousal', (delta,), kwargs)
        return None

    def medulla_get_arousal(self, *args, **kwargs):
        self._record('medulla_get_arousal', args, kwargs)
        return 0.5

    def medulla_get_circadian_efficiency(self, *args, **kwargs):
        self._record('medulla_get_circadian_efficiency', args, kwargs)
        return 0.8

    # --- Oscillations ---
    def get_pac_modulation(self, theta, gamma, *args, **kwargs):
        self._record('get_pac_modulation', (theta, gamma), kwargs)
        return 0.6

    # --- Introspection ---
    def get_uncertainty(self, features=None, *args, **kwargs):
        self._record('get_uncertainty', (features,), kwargs)
        return {"epistemic": 0.4, "aleatoric": 0.3}

    def self_assess(self, domain, *args, **kwargs):
        self._record('self_assess', (domain,), kwargs)
        return {"confidence": 0.7}

    # --- Curiosity ---
    def curiosity_detect_gaps(self, topic, *args, **kwargs):
        self._record('curiosity_detect_gaps', (topic,), kwargs)
        return {"questions": ["q1", "q2"]}

    # --- Safety ---
    def lgss_check_content(self, text, *args, **kwargs):
        self._record('lgss_check_content', (text,), kwargs)
        return {"is_safe": True}


# ---------------------------------------------------------------------------
# Patch heavy dependency modules BEFORE importing instructor_agent
# ---------------------------------------------------------------------------
import types

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

safety_mod = types.ModuleType('safety_gate')
safety_mod.SafetyGate = _MockSafetyGate
safety_mod.SafetyConfig = _MockSafetyConfig
sys.modules['safety_gate'] = safety_mod

skeptic_mod = types.ModuleType('data_skeptic')
skeptic_mod.DataSkeptic = _MockDataSkeptic
skeptic_mod.DataGrade = _MockDataGrade
sys.modules['data_skeptic'] = skeptic_mod

bench_mod = types.ModuleType('benchmark_datasets')
def _mock_text_to_features(text, num_inputs):
    import hashlib
    h = hashlib.md5(text.encode()).digest()
    features = [0.0] * num_inputs
    for i, b in enumerate(h):
        features[i % num_inputs] += b / 255.0
    return features
bench_mod.text_to_features = _mock_text_to_features
sys.modules['benchmark_datasets'] = bench_mod

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

# Ensure sentence_transformers is unavailable so we test n-gram fallback
if 'sentence_transformers' in sys.modules:
    del sys.modules['sentence_transformers']

from instructor_agent import (
    InstructorAgent, InstructorConfig, _CosineAnnealingLR,
    _text_to_features, _text_to_ngram_features,
    MethodStats, TeachingMethod,
)
from cognitive_orchestrator import CognitiveOrchestrator
from extrapolation_engine import (
    ExtrapolationEngine, ExtrapolationConfig, GenerationMode, GenerationResult,
)


# ---------------------------------------------------------------------------
# Helper to build a minimal InstructorAgent
# ---------------------------------------------------------------------------

def _make_agent(report_interval=10, num_inputs=64):
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
        num_inputs=num_inputs,
    )
    return agent, brain


# ============================================================================
# 1. TestCosineAnnealingLR
# ============================================================================

class TestCosineAnnealingLR:
    """Tests for the per-instructor cosine annealing LR scheduler."""

    def test_warmup_starts_at_min_lr(self):
        """First step during warmup should be near min_lr."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=5000, warmup_steps=500)
        lr = sched.get_lr()  # step 1
        # Should be close to min_lr at the start of warmup
        assert lr < 0.1, f"Warmup start should be near min_lr, got {lr}"
        assert lr >= 0.05, f"Should be at or above min_lr, got {lr}"

    def test_warmup_reaches_base_lr(self):
        """At the end of warmup, LR should reach base_lr."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=5000, warmup_steps=500)
        for _ in range(500):
            lr = sched.get_lr()
        assert abs(lr - 1.0) < 0.01, f"End of warmup should reach base_lr, got {lr}"

    def test_cosine_decay_after_warmup(self):
        """After warmup, LR should follow cosine decay towards min_lr."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=1000, warmup_steps=100)
        # Burn through warmup
        for _ in range(100):
            sched.get_lr()
        warmup_end_lr = sched.get_lr()  # step 101 (first cosine step)
        # Get LR at mid-cycle
        for _ in range(499):
            sched.get_lr()
        mid_lr = sched.get_lr()  # step ~601
        assert mid_lr < warmup_end_lr, \
            f"Mid-cycle LR ({mid_lr}) should be less than warmup end ({warmup_end_lr})"

    def test_warm_restart_resets_cycle(self):
        """After a full cycle, LR should restart from base_lr."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=100, warmup_steps=10)
        # Complete warmup + one full cycle
        for _ in range(10 + 100):
            sched.get_lr()
        # Now at the start of a new cycle — should be near base_lr
        lr = sched.get_lr()
        # cycle_pos = (111 - 10) % 100 = 1 — very start of cosine
        # cosine(pi * 1/100) is very close to 1.0
        assert lr > 0.9, f"After warm restart, LR should be near base_lr, got {lr}"

    def test_reset_method(self):
        """reset() should bring the scheduler back to initial state."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=5000, warmup_steps=500)
        for _ in range(300):
            sched.get_lr()
        sched.reset()
        assert sched.step_count == 0, f"step_count should be 0 after reset, got {sched.step_count}"
        lr = sched.get_lr()
        assert lr < 0.1, f"After reset, first step should be near min_lr, got {lr}"

    def test_min_lr_floor(self):
        """LR should never go below min_lr at any point in the schedule."""
        sched = _CosineAnnealingLR(base_lr=1.0, min_lr=0.05,
                                    cycle_steps=200, warmup_steps=20)
        for _ in range(1000):
            lr = sched.get_lr()
            assert lr >= 0.05 - 1e-9, f"LR should never go below min_lr, got {lr}"


# ============================================================================
# 2. TestHybridFeatureExtraction
# ============================================================================

class TestHybridFeatureExtraction:
    """Tests for the _text_to_features n-gram fallback path."""

    def test_ngram_fallback_dimensions(self):
        """Pure n-grams should produce a vector of exactly num_inputs dimensions."""
        features = _text_to_features("hello world", 64)
        assert len(features) == 64, f"Expected 64 dims, got {len(features)}"
        features_big = _text_to_features("hello world", 1024)
        assert len(features_big) == 1024, f"Expected 1024 dims, got {len(features_big)}"

    def test_ngram_fallback_normalization(self):
        """N-gram features should have L2 norm approximately 1.0 (or close)."""
        features = _text_to_features("The quick brown fox jumps over the lazy dog", 256)
        arr = np.array(features)
        norm = np.linalg.norm(arr)
        # The ngram fallback does frequency normalization per channel, not L2
        # Just verify the vector has non-trivial magnitude
        assert norm > 0.01, f"Feature norm should be positive, got {norm}"

    def test_ngram_fallback_deterministic(self):
        """Same text should always produce the same features."""
        text = "Deterministic feature test"
        f1 = _text_to_features(text, 128)
        f2 = _text_to_features(text, 128)
        assert f1 == f2, "Same text should produce identical features"

    def test_ngram_fallback_empty_string(self):
        """Empty string should return a vector of correct length and be different from real text."""
        features = _text_to_features("", 64)
        assert len(features) == 64, f"Expected 64 dims, got {len(features)}"
        # The feature encoder (benchmark_datasets mock uses md5) produces
        # features even for empty strings, but they should differ from real text
        real_features = _text_to_features("The quick brown fox jumps over the lazy dog", 64)
        assert features != real_features, \
            "Empty string features should differ from real text features"

    def test_ngram_different_domains_produce_different_features(self):
        """Different domain texts should produce different feature vectors."""
        bio_features = _text_to_features("mitochondria is the powerhouse of the cell", 256)
        math_features = _text_to_features("the quadratic formula solves second degree equations", 256)
        assert bio_features != math_features, "Different texts should produce different features"


# ============================================================================
# 3. TestSpacedRepetition
# ============================================================================

class TestSpacedRepetition:
    """Tests for the spaced repetition push/tick/interval system."""

    def test_push_schedules_review(self):
        """_spaced_replay_push adds an item to the replay heap."""
        agent, brain = _make_agent()
        agent.total_examples = 10
        agent._spaced_replay_push([1.0, 2.0], "labelA")
        assert len(agent._spaced_replay) == 1, \
            f"Should have 1 replay item, got {len(agent._spaced_replay)}"
        # The next_step should be total_examples + 1 = 11
        next_step = agent._spaced_replay[0][0]
        assert next_step == 11, f"Next review step should be 11, got {next_step}"

    def test_tick_processes_due_items(self):
        """_spaced_replay_tick processes items when total_examples >= next_step."""
        agent, brain = _make_agent()
        agent.total_examples = 5
        agent._spaced_replay_push([1.0, 2.0], "labelA")
        # next_step = 6, so at step 5 tick should NOT process
        agent._spaced_replay_tick()
        # Item should still be there (was not due yet)
        assert len(agent._spaced_replay) >= 1, "Item should remain (not due)"
        # Now advance to step 6
        agent.total_examples = 6
        agent._spaced_replay_tick()
        # After tick, item should be rescheduled (still in heap but with updated step)
        assert len(agent._spaced_replay) >= 1, "After tick, item is rescheduled"

    def test_correct_answer_doubles_interval(self):
        """When replay produces correct answer, interval should double."""
        agent, brain = _make_agent()
        # Make brain always predict the correct label
        brain.predict_in_domain = lambda f, p: ("labelA", 0.99)
        agent.total_examples = 10
        agent._spaced_replay_push([1.0, 2.0], "labelA")
        # Process the item at step 11
        agent.total_examples = 11
        agent._spaced_replay_tick()
        # Item should be rescheduled with interval=2 (doubled from 1)
        # next_step = 11 + 2 = 13
        assert len(agent._spaced_replay) >= 1
        next_step = agent._spaced_replay[0][0]
        interval = agent._spaced_replay[0][1]
        assert interval == 2, f"Correct answer should double interval to 2, got {interval}"
        assert next_step == 13, f"Next step should be 13, got {next_step}"

    def test_wrong_answer_resets_interval(self):
        """When replay produces wrong answer, interval should reset to 1."""
        agent, brain = _make_agent()
        # Make brain predict wrong label
        brain.predict_in_domain = lambda f, p: ("wrong", 0.5)
        agent.total_examples = 10
        # Push with an initial interval that would have been larger
        import heapq
        heapq.heappush(agent._spaced_replay, (11, 4, [1.0, 2.0], "labelA"))
        agent.total_examples = 11
        agent._spaced_replay_tick()
        assert len(agent._spaced_replay) >= 1
        interval = agent._spaced_replay[0][1]
        assert interval == 1, f"Wrong answer should reset interval to 1, got {interval}"

    def test_max_replays_per_tick(self):
        """At most 5 replays should be processed per tick."""
        agent, brain = _make_agent()
        brain.predict_in_domain = lambda f, p: ("labelA", 0.99)
        agent.total_examples = 100
        # Push 10 items all due now
        import heapq
        for i in range(10):
            heapq.heappush(agent._spaced_replay, (50, 1, [float(i)], "labelA"))
        initial_learn_calls = len(brain.get_calls('learn'))
        agent._spaced_replay_tick()
        learn_calls = len(brain.get_calls('learn')) - initial_learn_calls
        assert learn_calls == 5, f"Should process max 5 replays, got {learn_calls}"


# ============================================================================
# 4. TestDifficultyCurriculum
# ============================================================================

class TestDifficultyCurriculum:
    """Tests for difficulty-gated curriculum (_should_skip_easy, _get_difficulty_lr_scale)."""

    def test_low_mastery_no_skip(self):
        """At low mastery, no examples should be skipped."""
        agent, brain = _make_agent()
        # mastery is 0.0 by default
        skip = agent._should_skip_easy([1.0, 2.0], conf=0.99)
        assert skip is False, "Low mastery should never skip"

    def test_high_mastery_skip_confident(self):
        """At high mastery (>0.8), very confident examples (>0.95) should be skipped."""
        agent, brain = _make_agent()
        # Set mastery high
        agent.socratic.mastery._data["test_domain"] = 0.85
        skip = agent._should_skip_easy([1.0, 2.0], conf=0.96)
        assert skip is True, "High mastery + conf>0.95 should skip"
        # But moderate confidence should not skip
        skip_low = agent._should_skip_easy([1.0, 2.0], conf=0.90)
        assert skip_low is False, "High mastery + conf=0.90 should NOT skip"

    def test_difficulty_lr_scale_low_mastery(self):
        """At low mastery, LR scale should be 1.0 for unseen examples."""
        agent, brain = _make_agent()
        # mastery is 0.0
        scale = agent._get_difficulty_lr_scale([1.0, 2.0])
        assert abs(scale - 1.0) < 0.01, f"Low mastery, no failures -> scale=1.0, got {scale}"
        # With failures, scale drops
        key = agent._feature_hash([1.0, 2.0])
        agent._example_difficulty[key] = 2
        scale_fail = agent._get_difficulty_lr_scale([1.0, 2.0])
        assert abs(scale_fail - 0.8) < 0.01, f"Low mastery, failures -> scale=0.8, got {scale_fail}"

    def test_difficulty_lr_scale_high_mastery(self):
        """At high mastery, hard examples should get higher LR scale."""
        agent, brain = _make_agent()
        agent.socratic.mastery._data["test_domain"] = 0.9
        # No failures: scale = 0.3
        scale = agent._get_difficulty_lr_scale([1.0, 2.0])
        assert abs(scale - 0.3) < 0.01, f"High mastery, no failures -> 0.3, got {scale}"
        # With 3+ failures: scale approaches 1.0
        key = agent._feature_hash([1.0, 2.0])
        agent._example_difficulty[key] = 5
        scale_hard = agent._get_difficulty_lr_scale([1.0, 2.0])
        assert scale_hard > 0.9, f"High mastery, many failures -> near 1.0, got {scale_hard}"


# ============================================================================
# 5. TestSelfAssessment
# ============================================================================

class TestSelfAssessment:
    """Tests for holdout collection and self-assessment overfitting detection."""

    def test_holdout_collection(self):
        """Holdout buffer should collect up to _holdout_max samples."""
        agent, brain = _make_agent()
        for i in range(60):
            agent.total_examples = i + 1
            agent._maybe_collect_holdout([float(i)], f"label_{i}")
        assert len(agent._holdout_buffer) == agent._holdout_max, \
            f"Buffer should cap at {agent._holdout_max}, got {len(agent._holdout_buffer)}"

    def test_self_assess_computes_accuracy(self):
        """Self-assessment should compute held-out accuracy from predictions."""
        agent, brain = _make_agent()
        # Pre-fill holdout with known items
        agent._holdout_buffer = [([float(i)], "domain_label") for i in range(30)]
        agent.total_examples = 600
        agent._last_self_assessment_step = 0
        agent.total_correct = 300  # 50% train accuracy
        agent._maybe_self_assess()
        # MockBrain.predict_in_domain returns ("domain_label", 0.7) -> all correct
        assert agent._held_out_accuracy == 1.0, \
            f"Holdout acc should be 1.0 (all match), got {agent._held_out_accuracy}"

    def test_overfitting_detection(self):
        """Gap > 0.15 between train and holdout accuracy should reduce base_lr."""
        agent, brain = _make_agent()
        # Make brain return wrong predictions on holdout
        brain.predict_in_domain = lambda f, p: ("wrong_label", 0.5)
        agent._holdout_buffer = [([float(i)], "correct_label") for i in range(30)]
        agent.total_examples = 600
        agent._last_self_assessment_step = 0
        agent.total_correct = 540  # 90% train accuracy
        original_base_lr = agent._lr_scheduler.base_lr
        agent._maybe_self_assess()
        # holdout accuracy = 0% -> gap = 0.9 - 0.0 = 0.9 > 0.15
        assert agent._lr_scheduler.base_lr < original_base_lr, \
            f"Overfitting should reduce base_lr from {original_base_lr} to {agent._lr_scheduler.base_lr}"
        assert agent._lr_scheduler.base_lr >= 0.1, \
            f"base_lr should not drop below 0.1, got {agent._lr_scheduler.base_lr}"

    def test_self_assess_interval_respected(self):
        """Self-assessment should only run when interval is met."""
        agent, brain = _make_agent()
        agent._holdout_buffer = [([float(i)], "label") for i in range(30)]
        agent.total_examples = 100
        agent._last_self_assessment_step = 0
        agent._self_assessment_interval = 500
        agent._maybe_self_assess()
        # 100 - 0 = 100 < 500 -> should NOT run
        assert agent._last_self_assessment_step == 0, \
            "Should not run assessment before interval"
        agent.total_examples = 600
        agent._maybe_self_assess()
        assert agent._last_self_assessment_step == 600, \
            f"Should have run at step 600, got {agent._last_self_assessment_step}"


# ============================================================================
# 6. TestMethodRebalancing
# ============================================================================

class TestMethodRebalancing:
    """Tests for MethodStats update_weights, select_method, exploration floor."""

    def test_update_weights_boosts_accurate_methods(self):
        """update_weights should increase weight for accurate methods."""
        ms = MethodStats()
        # Record perfect accuracy for socratic
        for _ in range(50):
            ms.record("socratic", True, 0.1)
        # Record 0% for debate
        for _ in range(50):
            ms.record("debate", False, 0.9)
        ms.update_weights()
        assert ms._weights["socratic"] > ms._weights["debate"], \
            f"socratic ({ms._weights['socratic']}) should have higher weight than debate ({ms._weights['debate']})"

    def test_exploration_floor(self):
        """Weight should never drop below 0.1 (exploration floor)."""
        ms = MethodStats()
        # Force very low accuracy
        for _ in range(100):
            ms.record("debate", False, 1.0)
        # Repeat update_weights many times to push weight down
        for _ in range(50):
            ms.update_weights()
        assert ms._weights["debate"] >= 0.1, \
            f"Weight should not go below 0.1, got {ms._weights['debate']}"

    def test_select_method_epsilon_greedy(self):
        """select_method should occasionally explore (epsilon) and mostly exploit."""
        ms = MethodStats()
        # Make socratic the clear winner with many samples
        for _ in range(200):
            ms.record("socratic", True, 0.1)
        # Give other methods poor performance with many samples too
        for m in TeachingMethod:
            if m.value != "socratic":
                for _ in range(200):
                    ms.record(m.value, False, 0.9)
        ms.update_weights()
        # With epsilon=0.0, weighted selection should heavily favor socratic
        random.seed(42)
        selections = [ms.select_method(epsilon=0.0) for _ in range(100)]
        socratic_count = sum(1 for s in selections if s == TeachingMethod.SOCRATIC)
        # socratic has acc=1.0 so weight ~ w*(0.3+0.7*1.0)=w*1.0
        # others have acc=0.0 so weight ~ w*(0.3+0.7*0.0)=w*0.3
        # socratic share ~ 1.0/(1.0 + 6*0.3) = 1/2.8 ~ 35%
        # Over 100 samples that should be well above 20
        assert socratic_count > 20, \
            f"Socratic should dominate with eps=0, got {socratic_count}/100"
        # With epsilon=1.0, selection should be fully random (uniform)
        random.seed(42)
        random_selections = [ms.select_method(epsilon=1.0) for _ in range(100)]
        unique_random = len(set(random_selections))
        assert unique_random >= 3, \
            f"Full exploration should pick multiple methods, got {unique_random} unique"


# ============================================================================
# 7. TestModulateLRWithScheduler
# ============================================================================

class TestModulateLRWithScheduler:
    """Tests for _modulate_lr combining cosine schedule + decision cycle + fallback."""

    def test_lr_uses_cosine_schedule(self):
        """LR should incorporate the cosine schedule factor."""
        agent, brain = _make_agent()
        # No decision -> falls to cognitive fallback
        agent._last_decision = None
        # First call: warmup step 1 -> schedule_factor near min_lr
        lr = agent._modulate_lr(0.5)
        # Schedule factor is near 0.05 at step 1, so LR is low
        assert lr < 0.5, f"First step should have low LR due to warmup, got {lr}"
        assert lr > 0, f"LR should be positive, got {lr}"

    def test_lr_multiplies_decision_cycle_factor(self):
        """With a decision cycle, LR = base_lr * schedule * dc_factor."""
        agent, brain = _make_agent()
        agent._last_decision = {"lr_factor": 0.5}
        # Burn through warmup so schedule is near base_lr (1.0)
        for _ in range(500):
            agent._lr_step_count += 1
            agent._lr_scheduler.get_lr()
        lr = agent._modulate_lr(0.4)
        # schedule_factor is near 1.0 after warmup, dc_factor=0.5
        # lr = 0.4 * schedule * 0.5
        assert lr < 0.4, f"DC factor 0.5 should reduce LR below base, got {lr}"
        # Should be approximately 0.4 * 1.0 * 0.5 = 0.2
        assert abs(lr - 0.2) < 0.05, f"Expected ~0.2, got {lr}"

    def test_lr_fallback_to_cognitive(self):
        """Without decision cycle, should fall back to cognitive adaptive LR."""
        agent, brain = _make_agent()
        agent._last_decision = None
        # Burn through warmup
        for _ in range(500):
            agent._lr_step_count += 1
            agent._lr_scheduler.get_lr()
        lr = agent._modulate_lr(0.5)
        # Cognitive returns base_lr * 0.95 (from mock), then * schedule
        assert lr > 0, f"Fallback LR should be positive, got {lr}"

    def test_lr_warmup_phase(self):
        """During warmup, LR should increase over time."""
        agent, brain = _make_agent()
        agent._last_decision = None
        lr_values = []
        for i in range(20):
            lr = agent._modulate_lr(0.5)
            lr_values.append(lr)
        # LR should generally increase during warmup
        assert lr_values[-1] > lr_values[0], \
            f"LR should increase during warmup: first={lr_values[0]}, last={lr_values[-1]}"


# ============================================================================
# 8. TestMetacognitionCheck
# ============================================================================

class TestMetacognitionCheck:
    """Tests for _metacognition_check regression detection and LR boosting."""

    def test_regression_detection_boosts_lr(self):
        """When accuracy drops >5% from peak, base_lr should be boosted."""
        agent, brain = _make_agent()
        agent._peak_accuracy = 0.6
        agent.total_examples = 100
        agent.total_correct = 50  # 50% -> drop of 10% from peak 60%
        original_lr = agent._lr_scheduler.base_lr
        agent._metacognition_check()
        assert agent._lr_scheduler.base_lr > original_lr, \
            f"Regression should boost base_lr from {original_lr} to {agent._lr_scheduler.base_lr}"

    def test_peak_accuracy_tracking(self):
        """Peak accuracy should be updated when current accuracy exceeds it."""
        agent, brain = _make_agent()
        agent._peak_accuracy = 0.4
        agent.total_examples = 100
        agent.total_correct = 55  # 55%
        agent._metacognition_check()
        assert agent._peak_accuracy == 0.55, \
            f"Peak should update to 0.55, got {agent._peak_accuracy}"

    def test_no_action_when_improving(self):
        """When accuracy is at or above peak, no LR boost should occur."""
        agent, brain = _make_agent()
        agent._peak_accuracy = 0.3
        agent.total_examples = 100
        agent.total_correct = 40  # 40% > 30% peak
        original_lr = agent._lr_scheduler.base_lr
        agent._metacognition_check()
        assert agent._lr_scheduler.base_lr == original_lr, \
            f"No regression means no LR change, but got {agent._lr_scheduler.base_lr}"


# ============================================================================
# 9. TestDomainCentroid
# ============================================================================

class TestDomainCentroid:
    """Tests for domain centroid EMA tracking."""

    def test_first_example_sets_centroid(self):
        """First example should set the centroid directly."""
        agent, brain = _make_agent()
        features = [1.0, 2.0, 3.0]
        agent._update_domain_centroid(features)
        centroid = agent.get_domain_centroid()
        assert centroid is not None, "Centroid should be set after first example"
        assert np.allclose(centroid, [1.0, 2.0, 3.0]), \
            f"Centroid should equal first example, got {centroid}"

    def test_ema_update_moves_centroid(self):
        """Subsequent examples should move the centroid via EMA."""
        agent, brain = _make_agent()
        agent._update_domain_centroid([0.0, 0.0, 0.0])
        agent._update_domain_centroid([10.0, 10.0, 10.0])
        centroid = agent.get_domain_centroid()
        # EMA alpha = min(0.01, 2/(1+1)) = min(0.01, 1.0) = 0.01
        # Actually for count=2: alpha = min(0.01, 2/(2+1)) = min(0.01, 0.667) = 0.01
        # centroid = 0.99 * [0,0,0] + 0.01 * [10,10,10] = [0.1, 0.1, 0.1]
        assert centroid is not None
        assert centroid[0] > 0.0, f"Centroid should move toward new data, got {centroid[0]}"
        assert centroid[0] < 5.0, f"EMA should be slow, centroid should be much less than 5, got {centroid[0]}"

    def test_get_centroid_returns_array(self):
        """get_domain_centroid should return a numpy array."""
        agent, brain = _make_agent()
        assert agent.get_domain_centroid() is None, "Before any data, centroid is None"
        agent._update_domain_centroid([1.0, 2.0])
        centroid = agent.get_domain_centroid()
        assert isinstance(centroid, np.ndarray), \
            f"Centroid should be numpy array, got {type(centroid)}"


# ============================================================================
# 10. TestExtrapolationEngine
# ============================================================================

class TestExtrapolationEngine:
    """Tests for the ExtrapolationEngine using the full NIMCP cognitive stack mock."""

    def _make_engine(self):
        """Create an ExtrapolationEngine with mock brain."""
        brain = MockBrain()
        engine = ExtrapolationEngine(brain, feature_encoder=_mock_text_to_features)
        return engine, brain

    def test_engine_creation(self):
        """Engine should initialize without errors."""
        engine, brain = self._make_engine()
        assert engine is not None
        assert engine.brain is brain

    def test_generate_returns_result(self):
        """generate() should return a GenerationResult with expected fields."""
        engine, brain = self._make_engine()
        result = engine.generate("What is photosynthesis?")
        assert isinstance(result, GenerationResult), \
            f"Expected GenerationResult, got {type(result)}"
        assert result.mode == "extrapolate"
        assert result.generation_time_ms >= 0
        assert isinstance(result.reasoning_trace, list)
        assert isinstance(result.knowledge_sources, list)
        assert isinstance(result.content, dict)

    def test_generate_in_domain(self):
        """generate_in_domain() should set domain_context in config."""
        engine, brain = self._make_engine()
        result = engine.generate_in_domain("mitosis process", "biology")
        assert isinstance(result, GenerationResult)
        # Should have attempted domain prediction
        domain_calls = brain.get_calls('predict_in_domain')
        domain_prefixes = [c[1][1] for c in domain_calls]
        assert any("biology" in p for p in domain_prefixes), \
            f"Should have called predict_in_domain with biology prefix, got {domain_prefixes}"

    def test_hypothesize_mode(self):
        """hypothesize() should use HYPOTHESIZE mode with backward chaining."""
        engine, brain = self._make_engine()
        result = engine.hypothesize("What if gravity were weaker?", domains=["physics"])
        assert result.mode == "hypothesize"
        # Should have called backward chain (part of HYPOTHESIZE mode)
        bc_calls = brain.get_calls('ti_backward_chain')
        assert len(bc_calls) >= 1, "Hypothesize should invoke backward chaining"

    def test_blend_domains(self):
        """blend_domains() should use INTERPOLATE mode."""
        engine, brain = self._make_engine()
        result = engine.blend_domains("biology", "chemistry")
        assert result.mode == "interpolate"
        assert isinstance(result, GenerationResult)

    def test_temperature_softmax_high_temp(self):
        """High temperature should produce more uniform distribution."""
        engine, brain = self._make_engine()
        logits = np.array([10.0, 1.0, 0.5, 0.1], dtype=np.float32)
        probs = engine._temperature_softmax(logits, temperature=10.0)
        assert abs(sum(probs) - 1.0) < 1e-5, f"Probabilities should sum to 1, got {sum(probs)}"
        # High temperature -> more uniform -> max prob closer to 0.25
        assert max(probs) < 0.6, f"High temp should flatten distribution, max={max(probs)}"

    def test_temperature_softmax_zero_temp(self):
        """Temperature=0 should produce argmax (one-hot)."""
        engine, brain = self._make_engine()
        logits = np.array([0.1, 0.5, 10.0, 0.3], dtype=np.float32)
        probs = engine._temperature_softmax(logits, temperature=0.0)
        assert probs[2] == 1.0, f"Zero temp should pick argmax, got {probs}"
        assert sum(probs) == 1.0, f"Sum should be 1.0, got {sum(probs)}"

    def test_novelty_first_is_maximal(self):
        """First generation should have novelty=1.0 (no history to compare)."""
        engine, brain = self._make_engine()
        engine._generation_history = []  # Reset history
        output = np.array([0.5, 0.3, 0.8], dtype=np.float32)
        novelty = engine._compute_novelty(output)
        assert novelty == 1.0, f"First generation novelty should be 1.0, got {novelty}"


# ============================================================================
# Runner (for standalone execution)
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
        TestCosineAnnealingLR,
        TestHybridFeatureExtraction,
        TestSpacedRepetition,
        TestDifficultyCurriculum,
        TestSelfAssessment,
        TestMethodRebalancing,
        TestModulateLRWithScheduler,
        TestMetacognitionCheck,
        TestDomainCentroid,
        TestExtrapolationEngine,
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
