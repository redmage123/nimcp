#!/usr/bin/env python3
"""Tests for TrainingMetacognition."""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

class MockLogger:
    def log(self, msg): pass

# Import after path setup
from school import TrainingMetacognition

def test_initial_state():
    mc = TrainingMetacognition(MockLogger())
    assert mc._assessment_count == 0
    assert len(mc._domain_stats) == 0

def test_update_creates_domain():
    mc = TrainingMetacognition(MockLogger())
    mc.update("math", 0.5, 0.8, 100)
    assert "math" in mc._domain_stats

def test_ema_smoothing():
    mc = TrainingMetacognition(MockLogger())
    mc.update("math", 0.5, 0.8, 100)
    mc.update("math", 1.0, 0.1, 100)
    # EMA with alpha=0.1: 0.1*1.0 + 0.9*0.5 = 0.55
    assert abs(mc._domain_stats["math"]["ema_accuracy"] - 0.55) < 0.01

def test_stall_detection():
    mc = TrainingMetacognition(MockLogger())
    for i in range(10):
        mc.update("math", 0.5, 0.8, 100)  # Same accuracy each time
    assert mc._domain_stats["math"]["stall_count"] >= 5

def test_mastery_detection():
    mc = TrainingMetacognition(MockLogger())
    for i in range(15):
        mc.update("math", 0.9, 0.1, 100)
    assert mc._domain_stats["math"]["mastered"] is True

def test_assess_priorities():
    mc = TrainingMetacognition(MockLogger())
    # Mastered domain
    for i in range(15):
        mc.update("math", 0.9, 0.1, 100)
    # Stalled domain
    for i in range(10):
        mc.update("language", 0.3, 0.9, 100)

    results = mc.assess()
    assert results["math"]["status"] == "mastered"
    assert results["math"]["priority"] < 1.0
    assert results["language"]["status"] == "stalled"
    assert results["language"]["priority"] > 1.0

def test_priority_ranking():
    mc = TrainingMetacognition(MockLogger())
    # Math: high accuracy with >10 assessments -> mastered (low priority)
    for i in range(15):
        mc.update("math", 0.9, 0.1, 100)
    # Language: low accuracy, stalled (high priority)
    for i in range(10):
        mc.update("language", 0.3, 0.9, 100)
    mc.assess()
    ranking = mc.get_priority_ranking()
    # Language should be higher priority (stalled > mastered)
    assert ranking[0][0] == "language"

def test_dashboard_format():
    mc = TrainingMetacognition(MockLogger())
    mc.update("math", 0.5, 0.8, 100)
    mc.assess()
    dashboard = mc.format_dashboard()
    assert "math" in dashboard
    assert "Domain" in dashboard

def test_get_stalled_domains():
    mc = TrainingMetacognition(MockLogger())
    for i in range(10):
        mc.update("math", 0.5, 0.8, 100)
    assert "math" in mc.get_stalled_domains()

def test_get_mastered_domains():
    mc = TrainingMetacognition(MockLogger())
    for i in range(15):
        mc.update("math", 0.9, 0.1, 100)
    assert "math" in mc.get_mastered_domains()

def test_empty_assess():
    mc = TrainingMetacognition(MockLogger())
    results = mc.assess()
    assert len(results) == 0

if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    passed = 0
    failed = 0
    for test in tests:
        try:
            test()
            print(f"  PASS: {test.__name__}")
            passed += 1
        except Exception as e:
            print(f"  FAIL: {test.__name__}: {e}")
            failed += 1
    print(f"\n{passed}/{passed+failed} tests passed")
    sys.exit(1 if failed else 0)
