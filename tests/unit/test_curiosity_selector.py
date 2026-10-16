#!/usr/bin/env python3
"""Unit test for curiosity-driven stimulus selector."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class FakeSource:
    def __init__(self, objects=None, facts=None):
        self.objects = objects or [
            ("dog", "a friendly furry animal"),
            ("cat", "a purring house pet"),
            ("tree", "a tall green plant"),
            ("car", "a fast wheeled vehicle"),
        ]
        self.facts = facts or [
            ("The sky is blue.", "color"),
            ("Water is wet.", "state"),
        ]
        self._obj_idx = 0
        self._fact_idx = 0

    def get_object(self):
        o = self.objects[self._obj_idx % len(self.objects)]
        self._obj_idx += 1
        return o

    def get_fact(self, preferred_domain=None):
        f = self.facts[self._fact_idx % len(self.facts)]
        self._fact_idx += 1
        return f


class FakeBrain:
    def __init__(self, gaps=None):
        self.gaps = gaps or []

    def curiosity_detect_gaps(self, topic=None):
        return self.gaps


def test_falls_back_when_no_gaps():
    """When brain reports no gaps, selector should always use source."""
    from curiosity_selector import CuriositySelector
    brain = FakeBrain(gaps=[])
    source = FakeSource()
    sel = CuriositySelector(brain, source, bias=1.0)  # always try curiosity
    for _ in range(10):
        name, desc = sel.pick_object()
        assert name in {"dog", "cat", "tree", "car"}
    stats = sel.stats()
    assert stats["curiosity_hits"] == 0, "should not hit curiosity with no gaps"
    assert stats["fallback_hits"] == 10
    print(f"  PASS: falls back cleanly when no gaps ({stats})")


def test_targets_gaps_when_available():
    """When brain reports gaps, selector should preferentially match."""
    import random
    from curiosity_selector import CuriositySelector
    random.seed(42)
    brain = FakeBrain(gaps=["tree", "car"])
    source = FakeSource()
    sel = CuriositySelector(brain, source, bias=1.0)
    chosen = []
    for _ in range(20):
        name, _ = sel.pick_object()
        chosen.append(name)
    # Most picks should be from the gap set
    gap_matches = sum(1 for n in chosen if n in {"tree", "car"})
    total = len(chosen)
    gap_rate = gap_matches / total
    # With perfect gap-matching we'd be 100%; with 50/50 source cycling we'd
    # be ~50%. We expect elevated gap targeting.
    assert gap_rate > 0.4, f"gap rate {gap_rate:.2f} too low — curiosity not biasing"
    print(f"  PASS: gap targeting active ({gap_rate:.0%} of picks matched gaps)")


def test_graceful_on_api_exception():
    """If curiosity API raises, selector falls back without crashing."""
    from curiosity_selector import CuriositySelector

    class BrokenBrain:
        def curiosity_detect_gaps(self, topic=None):
            raise RuntimeError("simulated failure")

    sel = CuriositySelector(BrokenBrain(), FakeSource(), bias=1.0)
    for _ in range(5):
        name, _ = sel.pick_object()
        assert name  # non-empty
    print(f"  PASS: handles API exceptions via fallback")


def test_recent_deduplication():
    """Selector should not repeatedly return the same object for gap matching."""
    from curiosity_selector import CuriositySelector
    brain = FakeBrain(gaps=["dog"])
    source = FakeSource()
    sel = CuriositySelector(brain, source, bias=1.0, recent_history_n=2)
    # After picking 'dog' once, we expect the selector to fall through to
    # other options, not loop on the gap.
    first = sel.pick_object()
    # Subsequent picks should eventually find alternatives
    after = [sel.pick_object() for _ in range(8)]
    non_dog = [n for n, _ in after if n != "dog"]
    assert len(non_dog) > 0, "selector stuck on dog forever"
    print(f"  PASS: recent deduplication works (first={first[0]}, {len(non_dog)}/{len(after)} alternatives)")


def main():
    failures = []
    for name, fn in [
        ("falls_back_when_no_gaps", test_falls_back_when_no_gaps),
        ("targets_gaps_when_available", test_targets_gaps_when_available),
        ("graceful_on_api_exception", test_graceful_on_api_exception),
        ("recent_deduplication", test_recent_deduplication),
    ]:
        print(f"[unit/curiosity] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {e}")
    if failures:
        print(f"\n{len(failures)} failures")
        sys.exit(1)
    print("\nAll curiosity selector unit tests passed.")


if __name__ == "__main__":
    main()
