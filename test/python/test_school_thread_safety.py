#!/usr/bin/env python3
"""Tests for ThreadSafeBrain wrapper completeness and school coordinator.

Verifies that all brain-mutating methods are explicitly wrapped with RLock
in ThreadSafeBrain, and that metacognition + stall detection are wired into
the coordinator loop.
"""

import sys
import os
import queue
import threading
import time

# Add scripts dir so we can import from school.py
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))


# ---------------------------------------------------------------------------
# Mock Brain — records method calls for verification
# ---------------------------------------------------------------------------

class MockBrain:
    """Minimal mock of a nimcp Brain for testing ThreadSafeBrain wrappers."""

    def __init__(self):
        self._calls = []
        self._lock = threading.Lock()

    def _record(self, method, args, kwargs):
        with self._lock:
            self._calls.append((method, args, kwargs))

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
        return ("label", 0.9)

    def predict(self, *args, **kwargs):
        self._record('predict', args, kwargs)
        return ("label", 0.9)

    def decide(self, *args, **kwargs):
        self._record('decide', args, kwargs)
        return {}

    def consolidate(self, *args, **kwargs):
        self._record('consolidate', args, kwargs)
        time.sleep(0.01)  # Simulate work
        return {"replayed": 100}

    def save(self, *args, **kwargs):
        self._record('save', args, kwargs)
        return True

    def load(self, *args, **kwargs):
        self._record('load', args, kwargs)
        return True

    def probe(self, *args, **kwargs):
        self._record('probe', args, kwargs)
        return {
            "num_neurons": 1000,
            "avg_sparsity": 0.5,
            "current_learning_rate": 0.01,
            "total_learning_steps": 500,
            "accuracy": 0.75,
        }

    def cache_communities(self, *args, **kwargs):
        self._record('cache_communities', args, kwargs)
        return {"num_communities": 10, "modularity": 0.6}

    def invalidate_community_cache(self, *args, **kwargs):
        self._record('invalidate_community_cache', args, kwargs)

    def audio_cortex_process(self, *args, **kwargs):
        self._record('audio_cortex_process', args, kwargs)
        return [0.0] * 1024

    def visual_cortex_process(self, *args, **kwargs):
        self._record('visual_cortex_process', args, kwargs)
        return [0.0] * 1024

    def speech_cortex_process(self, *args, **kwargs):
        self._record('speech_cortex_process', args, kwargs)
        return [0.0] * 1024

    def ti_compute_unified_lr(self, *args, **kwargs):
        self._record('ti_compute_unified_lr', args, kwargs)
        return 0.005

    def ti_compute_modulation_state(self, *args, **kwargs):
        self._record('ti_compute_modulation_state', args, kwargs)
        return {"final_lr_factor": 1.0, "should_pause": False}

    def ti_post_batch_update(self, *args, **kwargs):
        self._record('ti_post_batch_update', args, kwargs)

    def ti_compute_decision_cycle(self, *args, **kwargs):
        self._record('ti_compute_decision_cycle', args, kwargs)
        return {"consensus_action": 0, "lr_factor": 1.0}

    def get_last_gradient_norm(self, *args, **kwargs):
        self._record('get_last_gradient_norm', args, kwargs)
        return 0.5


# Import after path setup
from school import ThreadSafeBrain, TrainingMetacognition, SchoolProgressBoard


# ============================================================================
# TestThreadSafeBrain
# ============================================================================

class TestThreadSafeBrain:
    """Verify ThreadSafeBrain wraps all mutating methods with RLock."""

    def test_consolidate_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps consolidate()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        # The method should be explicitly defined (not via __getattr__)
        assert 'consolidate' in type(tsb).__dict__, \
            "consolidate must be an explicit method on ThreadSafeBrain, not via __getattr__"
        result = tsb.consolidate(mode="auto")
        assert brain.get_calls('consolidate'), "consolidate should be forwarded to brain"
        assert result == {"replayed": 100}
        print(f"  PASS: consolidate wrapper exists and forwards correctly")

    def test_probe_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps probe()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'probe' in type(tsb).__dict__, \
            "probe must be an explicit method on ThreadSafeBrain"
        result = tsb.probe()
        assert brain.get_calls('probe'), "probe should be forwarded"
        assert result["num_neurons"] == 1000
        print(f"  PASS: probe wrapper exists and returns correct data")

    def test_save_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps save()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'save' in type(tsb).__dict__, \
            "save must be an explicit method on ThreadSafeBrain"
        tsb.save("/tmp/test.bin")
        calls = brain.get_calls('save')
        assert len(calls) == 1
        assert calls[0][1] == ("/tmp/test.bin",)
        print(f"  PASS: save wrapper exists and forwards path")

    def test_load_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps load()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'load' in type(tsb).__dict__, \
            "load must be an explicit method on ThreadSafeBrain"
        tsb.load("/tmp/test.bin")
        assert brain.get_calls('load'), "load should be forwarded"
        print(f"  PASS: load wrapper exists")

    def test_cache_communities_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps cache_communities()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'cache_communities' in type(tsb).__dict__, \
            "cache_communities must be an explicit method on ThreadSafeBrain"
        result = tsb.cache_communities()
        assert brain.get_calls('cache_communities'), "should be forwarded"
        assert result["num_communities"] == 10
        print(f"  PASS: cache_communities wrapper exists")

    def test_invalidate_community_cache_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps invalidate_community_cache()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'invalidate_community_cache' in type(tsb).__dict__, \
            "invalidate_community_cache must be an explicit method"
        tsb.invalidate_community_cache()
        assert brain.get_calls('invalidate_community_cache'), "should be forwarded"
        print(f"  PASS: invalidate_community_cache wrapper exists")

    def test_audio_cortex_process_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps audio_cortex_process()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'audio_cortex_process' in type(tsb).__dict__, \
            "audio_cortex_process must be an explicit method"
        result = tsb.audio_cortex_process([0.1, 0.2])
        assert brain.get_calls('audio_cortex_process'), "should be forwarded"
        assert len(result) == 1024
        print(f"  PASS: audio_cortex_process wrapper exists")

    def test_visual_cortex_process_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps visual_cortex_process()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'visual_cortex_process' in type(tsb).__dict__, \
            "visual_cortex_process must be an explicit method"
        result = tsb.visual_cortex_process([0.1], 32, 32, 3)
        assert brain.get_calls('visual_cortex_process'), "should be forwarded"
        print(f"  PASS: visual_cortex_process wrapper exists")

    def test_speech_cortex_process_wrapper_exists(self):
        """Verify ThreadSafeBrain wraps speech_cortex_process()."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        assert 'speech_cortex_process' in type(tsb).__dict__, \
            "speech_cortex_process must be an explicit method"
        result = tsb.speech_cortex_process([0.5, 0.3])
        assert brain.get_calls('speech_cortex_process'), "should be forwarded"
        print(f"  PASS: speech_cortex_process wrapper exists")

    def test_concurrent_learn_consolidate(self):
        """Verify learn() and consolidate() don't race.

        Two threads call learn() and consolidate() concurrently. With the
        RLock, they should serialize. We verify no exceptions and that
        both operations complete.
        """
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        errors = []

        def learn_loop():
            try:
                for _ in range(50):
                    tsb.learn([1.0, 2.0], "label", 0.5)
            except Exception as e:
                errors.append(("learn", e))

        def consolidate_loop():
            try:
                for _ in range(5):
                    tsb.consolidate(mode="light")
            except Exception as e:
                errors.append(("consolidate", e))

        t1 = threading.Thread(target=learn_loop)
        t2 = threading.Thread(target=consolidate_loop)
        t1.start()
        t2.start()
        t1.join(timeout=10)
        t2.join(timeout=10)

        assert not errors, f"Concurrent learn/consolidate raised errors: {errors}"
        assert len(brain.get_calls('learn')) == 50, \
            f"Expected 50 learn calls, got {len(brain.get_calls('learn'))}"
        assert len(brain.get_calls('consolidate')) == 5, \
            f"Expected 5 consolidate calls, got {len(brain.get_calls('consolidate'))}"
        print(f"  PASS: concurrent learn + consolidate (50 + 5 calls, no races)")

    def test_concurrent_save_probe(self):
        """Verify save() and probe() serialize under concurrent access."""
        brain = MockBrain()
        tsb = ThreadSafeBrain(brain)
        errors = []

        def save_loop():
            try:
                for i in range(10):
                    tsb.save(f"/tmp/ckpt_{i}.bin")
            except Exception as e:
                errors.append(("save", e))

        def probe_loop():
            try:
                for _ in range(20):
                    result = tsb.probe()
                    assert isinstance(result, dict)
            except Exception as e:
                errors.append(("probe", e))

        t1 = threading.Thread(target=save_loop)
        t2 = threading.Thread(target=probe_loop)
        t1.start()
        t2.start()
        t1.join(timeout=10)
        t2.join(timeout=10)

        assert not errors, f"Concurrent save/probe errors: {errors}"
        print(f"  PASS: concurrent save + probe (no races)")

    def test_getattr_fallback(self):
        """Verify __getattr__ proxies non-wrapped attributes."""
        brain = MockBrain()
        brain.custom_attr = 42
        tsb = ThreadSafeBrain(brain)
        assert tsb.custom_attr == 42, "Custom attributes should proxy through __getattr__"
        print(f"  PASS: __getattr__ fallback works")

    def test_all_expected_wrappers_present(self):
        """Verify all required wrappers are explicitly defined."""
        required = [
            'learn', 'predict_fast', 'predict', 'decide',
            'ti_compute_unified_lr', 'ti_compute_modulation_state',
            'ti_post_batch_update', 'ti_compute_decision_cycle',
            'get_last_gradient_norm',
            'consolidate', 'save', 'load', 'probe',
            'cache_communities', 'invalidate_community_cache',
            'audio_cortex_process', 'visual_cortex_process',
            'speech_cortex_process',
        ]
        missing = [m for m in required if m not in ThreadSafeBrain.__dict__]
        assert not missing, f"Missing explicit wrappers: {missing}"
        print(f"  PASS: all {len(required)} required wrappers present")


# ============================================================================
# TestMetacognitionWiring
# ============================================================================

class TestMetacognitionWiring:
    """Verify metacognition._domain_stats gets populated during training."""

    def test_domain_stats_populated_from_reports(self):
        """Verify metacognition.update() populates _domain_stats from reports."""
        mc = TrainingMetacognition(type('L', (), {'log': lambda s, m: None})())

        # Simulate report processing
        mc.update("math", 0.65, 0.35, 500)
        mc.update("science", 0.45, 0.55, 300)

        assert "math" in mc._domain_stats, "math should be in domain stats"
        assert "science" in mc._domain_stats, "science should be in domain stats"
        assert abs(mc._domain_stats["math"]["ema_accuracy"] - 0.65) < 1e-9, \
            f"First update should set ema_accuracy to ~0.65, got {mc._domain_stats['math']['ema_accuracy']}"
        assert mc._domain_stats["math"]["total_examples"] == 500
        print(f"  PASS: domain stats populated from reports")

    def test_stall_detection_triggers_recess(self):
        """Verify stalled learning is detected by metacognition.

        Simulates a domain that shows no accuracy improvement across many
        reports. After enough stalled updates, get_stalled_domains() should
        return the stalled domain.
        """
        mc = TrainingMetacognition(type('L', (), {'log': lambda s, m: None})())

        # Feed 10 reports with identical accuracy -> stall
        for _ in range(10):
            mc.update("language", 0.42, 0.58, 100)

        stalled = mc.get_stalled_domains()
        assert "language" in stalled, \
            f"language should be stalled after 10 identical reports, got {stalled}"

        # Verify assessment reflects stalled status
        assessment = mc.assess()
        assert assessment["language"]["status"] == "stalled"
        assert assessment["language"]["priority"] > 1.0, \
            "Stalled domains should have priority > 1.0"
        print(f"  PASS: stall detection works (stall_count={mc._domain_stats['language']['stall_count']})")

    def test_metacognition_update_ema(self):
        """Verify EMA smoothing works correctly across multiple updates."""
        mc = TrainingMetacognition(type('L', (), {'log': lambda s, m: None})())

        mc.update("math", 0.5, 0.8, 100)
        mc.update("math", 1.0, 0.1, 100)
        # EMA with alpha=0.1: 0.1*1.0 + 0.9*0.5 = 0.55
        assert abs(mc._domain_stats["math"]["ema_accuracy"] - 0.55) < 0.01, \
            f"EMA should be 0.55, got {mc._domain_stats['math']['ema_accuracy']}"
        print(f"  PASS: EMA smoothing correct")

    def test_mastery_detection_prevents_over_training(self):
        """Verify mastered domains get low priority in assessment."""
        mc = TrainingMetacognition(type('L', (), {'log': lambda s, m: None})())

        for _ in range(15):
            mc.update("math", 0.9, 0.1, 100)

        assert mc._domain_stats["math"]["mastered"] is True
        assessment = mc.assess()
        assert assessment["math"]["priority"] < 1.0, \
            "Mastered domains should have reduced priority"
        assert assessment["math"]["recommendation"] == "reduce_slots"
        print(f"  PASS: mastery reduces priority ({assessment['math']['priority']:.2f})")


# ============================================================================
# TestStallDetectionWiring
# ============================================================================

class TestStallDetectionWiring:
    """Verify is_learning_stalled() from CognitiveOrchestrator works."""

    def test_is_learning_stalled_detects_plateau(self):
        """Verify is_learning_stalled returns True for flat accuracy."""
        from cognitive_orchestrator import CognitiveOrchestrator

        brain = MockBrain()
        co = CognitiveOrchestrator(brain)

        # 15 identical accuracies -> stalled
        accs = [0.42] * 15
        assert co.is_learning_stalled(accs) is True, \
            "Should detect stall with constant accuracy"
        print(f"  PASS: is_learning_stalled detects plateau")

    def test_is_learning_stalled_not_stalled_when_improving(self):
        """Verify is_learning_stalled returns False when accuracy improves."""
        from cognitive_orchestrator import CognitiveOrchestrator

        brain = MockBrain()
        co = CognitiveOrchestrator(brain)

        # Steadily improving
        accs = [0.3 + 0.02 * i for i in range(15)]
        assert co.is_learning_stalled(accs) is False, \
            "Should NOT detect stall with improving accuracy"
        print(f"  PASS: is_learning_stalled allows improving domains")

    def test_is_learning_stalled_needs_minimum_window(self):
        """Verify is_learning_stalled requires minimum window size."""
        from cognitive_orchestrator import CognitiveOrchestrator

        brain = MockBrain()
        co = CognitiveOrchestrator(brain)

        # Too few data points
        accs = [0.42] * 5
        assert co.is_learning_stalled(accs, window=10) is False, \
            "Should return False with fewer than window data points"
        print(f"  PASS: is_learning_stalled requires minimum window")


# ============================================================================
# TestSchoolDomainAccuracyHistory
# ============================================================================

class TestSchoolDomainAccuracyHistory:
    """Verify per-domain accuracy history accumulation in School._drain_reports."""

    def test_drain_reports_accumulates_accuracy(self):
        """Verify _drain_reports builds per-domain accuracy history."""
        # We can't easily instantiate School without a real brain, so we
        # test the logic in isolation by simulating what _drain_reports does.
        from school import TrainingMetacognition
        from cognitive_orchestrator import CognitiveOrchestrator

        brain = MockBrain()
        co = CognitiveOrchestrator(brain)
        mc = TrainingMetacognition(type('L', (), {'log': lambda s, m: None})())
        history = {}

        # Simulate _drain_reports logic
        reports = [
            {"domain": "math", "accuracy": 0.5, "loss": 0.5, "total_examples": 100},
            {"domain": "math", "accuracy": 0.6, "loss": 0.4, "total_examples": 200},
            {"domain": "science", "accuracy": 0.3, "loss": 0.7, "total_examples": 50},
        ]

        for report in reports:
            domain = report["domain"]
            mc.update(domain, report["accuracy"], report["loss"], report["total_examples"])
            if domain not in history:
                history[domain] = []
            history[domain].append(report["accuracy"])

        assert len(history["math"]) == 2, f"math should have 2 entries, got {len(history['math'])}"
        assert len(history["science"]) == 1
        assert history["math"] == [0.5, 0.6]

        # Stall check
        assert co.is_learning_stalled(history["math"]) is False, \
            "2 data points is less than default window, should not be stalled"
        print(f"  PASS: drain_reports accumulates per-domain accuracy history")


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
        TestThreadSafeBrain,
        TestMetacognitionWiring,
        TestStallDetectionWiring,
        TestSchoolDomainAccuracyHistory,
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
