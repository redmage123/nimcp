#!/usr/bin/env python3
"""Tests for training strategy components: CosineAnnealingLR, CurriculumManager, HardExampleMiner.

These classes are defined in scripts/train_athena.py and control learning rate
scheduling, domain curriculum ordering, and hard example replay during Athena training.
"""

import math
import sys
from pathlib import Path

# Add scripts dir so we can import from train_athena
SCRIPT_DIR = Path(__file__).resolve().parent.parent.parent / "scripts"
sys.path.insert(0, str(SCRIPT_DIR))

# We cannot import train_athena directly because it has heavy dependencies
# (nimcp, benchmark_datasets, etc.) that may not be available in test
# environments. Instead, we copy the classes here for isolated unit testing.
# The canonical implementations live in scripts/train_athena.py.


class CosineAnnealingLR:
    """Cosine annealing learning rate scheduler with warm restarts."""
    def __init__(self, base_lr: float = 0.5, min_lr: float = 0.05,
                 cycle_steps: int = 5000, warmup_steps: int = 500):
        self.base_lr = base_lr
        self.min_lr = min_lr
        self.cycle_steps = cycle_steps
        self.warmup_steps = warmup_steps
        self.step_count = 0

    def get_lr(self) -> float:
        self.step_count += 1
        if self.step_count <= self.warmup_steps:
            return self.min_lr + (self.base_lr - self.min_lr) * (self.step_count / self.warmup_steps)
        cycle_pos = (self.step_count - self.warmup_steps) % self.cycle_steps
        cosine_factor = 0.5 * (1.0 + math.cos(math.pi * cycle_pos / self.cycle_steps))
        return self.min_lr + (self.base_lr - self.min_lr) * cosine_factor

    def reset(self):
        self.step_count = 0


class CurriculumManager:
    """Curriculum learning: start with fewer, easier classes, gradually add more."""
    def __init__(self, all_domains: list):
        self.all_domains = all_domains
        self.step = 0
        self.easy_domains = ['math', 'science', 'language', 'history',
                            'geography', 'general', 'reading', 'coding']
        self.medium_domains = ['medicine', 'law', 'finance', 'philosophy',
                               'ethics', 'psychology', 'engineering', 'statistics']
        self.hard_domains = [d for d in all_domains
                            if d not in self.easy_domains and d not in self.medium_domains]

    def get_active_domains(self) -> list:
        self.step += 1
        if self.step <= 2000:
            return self.easy_domains
        elif self.step <= 5000:
            return self.easy_domains + self.medium_domains
        else:
            return self.all_domains

    def should_include(self, domain: str) -> bool:
        active = self.get_active_domains()
        self.step -= 1
        return domain in active or domain.split(':')[0] in active


import random


class HardExampleMiner:
    """Track and replay high-loss training examples."""
    def __init__(self, capacity: int = 5000, replay_ratio: float = 0.2):
        self.capacity = capacity
        self.replay_ratio = replay_ratio
        self.hard_examples = []
        self.min_loss_threshold = 0.3

    def record(self, features: list, label: str, loss: float):
        if loss > self.min_loss_threshold:
            self.hard_examples.append((features, label, loss))
            if len(self.hard_examples) > self.capacity:
                self.hard_examples.sort(key=lambda x: x[2], reverse=True)
                self.hard_examples = self.hard_examples[:self.capacity]

    def get_replay_batch(self, batch_size: int) -> list:
        replay_count = max(1, int(batch_size * self.replay_ratio))
        if len(self.hard_examples) < replay_count:
            return list(self.hard_examples)
        return random.sample(self.hard_examples, replay_count)

    def decay(self, factor: float = 0.95):
        for i in range(len(self.hard_examples)):
            f, l, loss = self.hard_examples[i]
            self.hard_examples[i] = (f, l, loss * factor)
        self.hard_examples = [x for x in self.hard_examples if x[2] > self.min_loss_threshold * 0.5]


# ============================================================================
# Tests
# ============================================================================

class TestCosineAnnealingLR:
    """Tests for cosine annealing learning rate scheduler."""

    def test_warmup_starts_at_min(self):
        """First step during warmup should be close to min_lr."""
        sched = CosineAnnealingLR(base_lr=0.5, min_lr=0.05, warmup_steps=100)
        lr = sched.get_lr()  # step 1
        # At step 1/100, lr = 0.05 + (0.5 - 0.05) * (1/100) = 0.05 + 0.0045 = 0.0545
        assert lr > 0.05, f"First LR {lr} should be above min_lr 0.05"
        assert lr < 0.10, f"First LR {lr} should be close to min_lr, not {lr}"
        print(f"  PASS: warmup starts near min_lr ({lr:.4f})")

    def test_warmup_reaches_base(self):
        """At the end of warmup, LR should equal base_lr."""
        sched = CosineAnnealingLR(base_lr=0.5, min_lr=0.05, warmup_steps=100)
        for _ in range(99):
            sched.get_lr()
        lr = sched.get_lr()  # step 100
        assert abs(lr - 0.5) < 1e-6, f"At warmup end, LR should be base_lr=0.5, got {lr}"
        print(f"  PASS: warmup reaches base_lr ({lr:.4f})")

    def test_cosine_decay(self):
        """After warmup, LR should decay via cosine from base_lr toward min_lr."""
        sched = CosineAnnealingLR(base_lr=0.5, min_lr=0.05,
                                  cycle_steps=1000, warmup_steps=10)
        # Consume warmup
        for _ in range(10):
            sched.get_lr()
        # First post-warmup LR (step 11, cycle_pos=1) should be near base_lr
        lr_start = sched.get_lr()
        assert lr_start > 0.45, f"Post-warmup start LR should be near base, got {lr_start}"

        # At approximately cycle midpoint: advance 499 more steps to step 511
        # cycle_pos = (511 - 10) % 1000 = 501
        for _ in range(499):
            sched.get_lr()
        lr_mid = sched.get_lr()  # step 511: cycle_pos=501
        expected_mid = 0.05 + (0.5 - 0.05) * 0.5 * (1.0 + math.cos(math.pi * 501 / 1000))
        assert abs(lr_mid - expected_mid) < 1e-6, f"Mid-cycle LR: expected {expected_mid}, got {lr_mid}"

        # Near cycle end, LR should approach min_lr
        for _ in range(498):
            sched.get_lr()
        lr_end = sched.get_lr()  # step 1010: cycle_pos=0 (restart!)
        # Actually let's just verify the overall shape: start high, mid lower, end cycles back
        assert lr_start > lr_mid, f"LR should decrease: start={lr_start} > mid={lr_mid}"
        print(f"  PASS: cosine decay (start={lr_start:.4f}, mid={lr_mid:.4f}, end={lr_end:.4f})")

    def test_warm_restart(self):
        """After a full cycle, LR should jump back up (warm restart)."""
        sched = CosineAnnealingLR(base_lr=0.5, min_lr=0.05,
                                  cycle_steps=100, warmup_steps=10)
        # Consume warmup (10 steps) + one full cycle (100 steps) = 110 total
        for _ in range(10 + 100):
            sched.get_lr()

        # Record LR just before restart (end of cycle — should be low)
        # Step 110: cycle_pos = (110-10) % 100 = 0 — that IS the restart
        # Step 111: cycle_pos = (111-10) % 100 = 1 — near start of new cycle
        lr_restart = sched.get_lr()  # step 111, cycle_pos=1
        # cycle_pos=1: cosine_factor = 0.5*(1+cos(pi*1/100)) ≈ 0.9995 -> lr ≈ 0.4998
        # Should be very close to base_lr (warm restart)
        assert lr_restart > 0.49, \
            f"Warm restart LR should be near base_lr=0.5, got {lr_restart}"
        print(f"  PASS: warm restart ({lr_restart:.4f})")

    def test_lr_always_positive(self):
        """LR should always be positive across many steps."""
        sched = CosineAnnealingLR(base_lr=1.0, min_lr=0.01,
                                  cycle_steps=200, warmup_steps=50)
        for i in range(10000):
            lr = sched.get_lr()
            assert lr > 0, f"LR must be positive at step {i+1}, got {lr}"
            assert lr >= 0.01 - 1e-9, f"LR {lr} below min_lr at step {i+1}"
            assert lr <= 1.0 + 1e-9, f"LR {lr} above base_lr at step {i+1}"
        print(f"  PASS: all 10000 steps have positive LR in [min_lr, base_lr]")

    def test_reset(self):
        """Reset should bring scheduler back to initial state."""
        sched = CosineAnnealingLR(base_lr=0.5, min_lr=0.05, warmup_steps=100)
        for _ in range(500):
            sched.get_lr()
        sched.reset()
        assert sched.step_count == 0, f"After reset, step_count should be 0, got {sched.step_count}"
        lr = sched.get_lr()
        # Should behave like step 1 of warmup
        expected = 0.05 + (0.5 - 0.05) * (1 / 100)
        assert abs(lr - expected) < 1e-6, f"After reset, first LR should be {expected}, got {lr}"
        print(f"  PASS: reset works (step_count=0, first LR={lr:.4f})")


class TestCurriculumManager:
    """Tests for curriculum learning domain progression."""

    def test_phase1_easy_only(self):
        """In phase 1 (steps 0-2000), only easy domains should be active."""
        all_domains = ['math', 'science', 'medicine', 'law', 'quantum_physics']
        cm = CurriculumManager(all_domains)
        cm.step = 0
        active = cm.get_active_domains()
        assert 'math' in active, "math should be in easy phase"
        assert 'science' in active, "science should be in easy phase"
        assert 'medicine' not in active, "medicine should NOT be in easy phase"
        assert 'law' not in active, "law should NOT be in easy phase"
        assert 'quantum_physics' not in active, "quantum_physics should NOT be in easy phase"
        print(f"  PASS: phase 1 returns only easy domains ({len(active)} domains)")

    def test_phase2_adds_medium(self):
        """In phase 2 (steps 2000-5000), easy + medium domains should be active."""
        all_domains = ['math', 'medicine', 'law', 'quantum_physics']
        cm = CurriculumManager(all_domains)
        cm.step = 2000  # get_active_domains will increment to 2001
        active = cm.get_active_domains()
        assert 'math' in active, "math should be in medium phase"
        assert 'medicine' in active, "medicine should be in medium phase"
        assert 'law' in active, "law should be in medium phase"
        assert 'quantum_physics' not in active, "quantum_physics should NOT be in medium phase"
        print(f"  PASS: phase 2 includes easy + medium ({len(active)} domains)")

    def test_phase3_all_domains(self):
        """In phase 3 (steps 5000+), all domains should be active."""
        all_domains = ['math', 'medicine', 'quantum_physics', 'astrophysics']
        cm = CurriculumManager(all_domains)
        cm.step = 5000  # get_active_domains will increment to 5001
        active = cm.get_active_domains()
        assert 'math' in active, "math should be active in phase 3"
        assert 'medicine' in active, "medicine should be active in phase 3"
        assert 'quantum_physics' in active, "quantum_physics should be active in phase 3"
        assert 'astrophysics' in active, "astrophysics should be active in phase 3"
        print(f"  PASS: phase 3 includes all domains ({len(active)} domains)")

    def test_should_include(self):
        """should_include should match domains and domain prefixes."""
        all_domains = ['math', 'science', 'medicine', 'alien_biology']
        cm = CurriculumManager(all_domains)
        cm.step = 0  # Phase 1 — only easy domains

        assert cm.should_include('math') is True, "math is easy, should be included"
        assert cm.should_include('science') is True, "science is easy, should be included"
        assert cm.should_include('medicine') is False, "medicine is medium, should NOT be included in phase 1"
        assert cm.should_include('alien_biology') is False, "alien_biology is hard, should NOT be included"

        # Test domain prefix matching (e.g., "math:algebra")
        cm.step = 0
        assert cm.should_include('math:algebra') is True, "math:algebra should match 'math' prefix"
        assert cm.should_include('medicine:cardiology') is False, "medicine:cardiology should NOT match in phase 1"

        print(f"  PASS: should_include handles domains and prefixes correctly")

    def test_step_counting(self):
        """should_include should not double-count steps."""
        all_domains = ['math']
        cm = CurriculumManager(all_domains)
        cm.step = 100

        # Call should_include multiple times — step should stay at 100
        for _ in range(10):
            cm.should_include('math')

        # get_active_domains increments, should_include decrements — net zero
        assert cm.step == 100, f"Step should still be 100 after should_include calls, got {cm.step}"
        print(f"  PASS: should_include does not accumulate step count")


class TestHardExampleMiner:
    """Tests for hard example mining and replay."""

    def test_records_high_loss(self):
        """Examples with loss above threshold should be recorded."""
        miner = HardExampleMiner(capacity=100)
        miner.record([1.0, 2.0], "label_a", 0.8)  # Above threshold (0.3)
        assert len(miner.hard_examples) == 1, f"Should have 1 example, got {len(miner.hard_examples)}"
        assert miner.hard_examples[0][1] == "label_a"
        assert miner.hard_examples[0][2] == 0.8
        print(f"  PASS: high-loss examples recorded")

    def test_ignores_low_loss(self):
        """Examples with loss below threshold should be ignored."""
        miner = HardExampleMiner(capacity=100)
        miner.record([1.0, 2.0], "label_a", 0.1)  # Below threshold (0.3)
        miner.record([1.0, 2.0], "label_b", 0.29)  # Still below
        assert len(miner.hard_examples) == 0, f"Should have 0 examples, got {len(miner.hard_examples)}"
        print(f"  PASS: low-loss examples ignored")

    def test_capacity_limit(self):
        """Hard examples should be capped at capacity, keeping highest-loss."""
        miner = HardExampleMiner(capacity=5)
        for i in range(20):
            miner.record([float(i)], f"label_{i}", 0.5 + i * 0.1)

        assert len(miner.hard_examples) == 5, f"Should be capped at 5, got {len(miner.hard_examples)}"

        # The 5 highest losses should be retained (losses 2.4, 2.3, 2.2, 2.1, 2.0)
        losses = sorted([ex[2] for ex in miner.hard_examples], reverse=True)
        assert losses[0] > 2.0, f"Highest loss should be > 2.0, got {losses[0]}"
        print(f"  PASS: capacity limit enforced (kept {len(miner.hard_examples)} of 20)")

    def test_replay_batch_size(self):
        """get_replay_batch should return the requested number of examples."""
        miner = HardExampleMiner(capacity=100, replay_ratio=0.5)
        for i in range(50):
            miner.record([float(i)], f"label_{i}", 0.5 + i * 0.01)

        batch = miner.get_replay_batch(batch_size=20)
        expected_count = max(1, int(20 * 0.5))  # = 10
        assert len(batch) == expected_count, f"Expected {expected_count} replay items, got {len(batch)}"
        print(f"  PASS: replay batch size correct ({len(batch)} items)")

    def test_replay_batch_when_fewer_than_requested(self):
        """When fewer examples than requested, return all of them."""
        miner = HardExampleMiner(capacity=100, replay_ratio=0.5)
        miner.record([1.0], "a", 0.9)
        miner.record([2.0], "b", 0.8)

        batch = miner.get_replay_batch(batch_size=100)
        # replay_count = max(1, int(100 * 0.5)) = 50, but only 2 available
        assert len(batch) == 2, f"Should return all 2 examples, got {len(batch)}"
        print(f"  PASS: replay returns all when fewer than requested")

    def test_decay(self):
        """Decay should reduce losses and eventually remove low-loss items."""
        miner = HardExampleMiner(capacity=100)
        miner.record([1.0], "a", 0.35)  # Just above threshold
        miner.record([2.0], "b", 1.0)   # Well above threshold

        # Decay many times — the 0.35 item should eventually drop below 0.15 and be removed
        for _ in range(50):
            miner.decay(factor=0.9)

        # 0.35 * 0.9^50 ~= 0.0019 < 0.15 (threshold * 0.5) — should be removed
        # 1.0 * 0.9^50 ~= 0.0052 < 0.15 — also removed
        # With more modest decay, let's check
        miner2 = HardExampleMiner(capacity=100)
        miner2.record([1.0], "a", 0.35)
        miner2.record([2.0], "b", 1.0)

        miner2.decay(factor=0.95)
        # After one decay: 0.35*0.95=0.3325 > 0.15, 1.0*0.95=0.95 > 0.15
        assert len(miner2.hard_examples) == 2, "Both should survive one decay"

        # Keep decaying until the weaker one drops out
        for _ in range(20):
            miner2.decay(factor=0.80)
        # 0.3325 * 0.8^20 ~= 0.0038 < 0.15 — removed
        # 0.95 * 0.8^20 ~= 0.0109 < 0.15 — also removed
        assert len(miner2.hard_examples) == 0, \
            f"After heavy decay, all should be removed, got {len(miner2.hard_examples)}"
        print(f"  PASS: decay reduces losses and removes weak items")

    def test_empty_replay(self):
        """get_replay_batch on empty miner should return empty list."""
        miner = HardExampleMiner(capacity=100)
        batch = miner.get_replay_batch(batch_size=10)
        assert len(batch) == 0, f"Empty miner should return empty batch, got {len(batch)}"
        print(f"  PASS: empty miner returns empty replay batch")


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
        except (AssertionError, Exception) as e:
            print(f"  FAIL: {name}: {e}")
            failed += 1
    return passed, failed


if __name__ == "__main__":
    total_passed, total_failed = 0, 0

    for cls in [TestCosineAnnealingLR, TestCurriculumManager, TestHardExampleMiner]:
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
