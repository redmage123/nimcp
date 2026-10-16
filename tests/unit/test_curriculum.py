#!/usr/bin/env python3
"""Unit tests for progressive curriculum."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class FakeSource:
    """Cycles through a wide variety of objects across many categories."""
    def __init__(self):
        self.items = [
            ("dog", "animal"), ("cat", "animal"), ("bird", "animal"),
            ("tree", "plant"), ("flower", "plant"), ("grass", "plant"),
            ("car", "vehicle"), ("bike", "vehicle"), ("train", "vehicle"),
            ("red ball", "toy"), ("teddy bear", "toy"), ("doll", "toy"),
            ("apple", "food"), ("bread", "food"), ("cheese", "food"),
            ("chair", "furniture"), ("table", "furniture"),
            ("cup", "container"), ("bowl", "container"),
            ("sun", "sky"), ("moon", "sky"),
            ("book", "object"), ("pen", "object"),
        ]
        self.idx = 0

    def get_object(self):
        o = self.items[self.idx % len(self.items)]
        self.idx += 1
        return o


def test_narrows_scope_early():
    """Early steps should see limited category variety."""
    from curriculum import ProgressiveCurriculum
    source = FakeSource()
    c = ProgressiveCurriculum(source,
                               stages=[(100, 3), (500, 10), (10**9, 10**9)],
                               category_extractor=lambda n, d: d)  # d is category

    seen_categories = set()
    for i in range(50):
        c.advance()
        name, desc = c.pick_object()
        seen_categories.add(desc)

    # Should have ≤ 3 categories in first 50 steps
    assert len(seen_categories) <= 3, (
        f"early scope too wide: {seen_categories}")
    print(f"  PASS: early scope limited to {len(seen_categories)} categories: "
          f"{seen_categories}")


def test_expands_with_progress():
    """After crossing stage boundaries, scope expands."""
    from curriculum import ProgressiveCurriculum
    source = FakeSource()
    c = ProgressiveCurriculum(source,
                               stages=[(50, 2), (200, 6), (10**9, 10**9)],
                               category_extractor=lambda n, d: d)

    # Push through stages
    for step in range(300):
        c.advance(step=step)
        c.pick_object()

    stats = c.stats()
    # At step 300, we're past stage 2 (200 cap) — should be past 6-category limit
    assert stats["active_n_categories"] >= 10**8, (
        f"scope did not expand: {stats}")
    print(f"  PASS: scope expands with progress ({stats})")


def test_graceful_when_source_lacks_category():
    """If category extractor returns empty, curriculum still works."""
    from curriculum import ProgressiveCurriculum
    source = FakeSource()
    c = ProgressiveCurriculum(source,
                               stages=[(50, 3), (10**9, 10**9)],
                               category_extractor=lambda n, d: "")

    # All items have empty category — they'll all match as "allowed"
    for _ in range(20):
        name, desc = c.pick_object()
        assert name
    print(f"  PASS: handles empty-category extractor ({c.stats()})")


def test_does_not_starve_on_category_flood():
    """If source produces out-of-scope items repeatedly, curriculum
    eventually accepts to avoid infinite rejection."""
    from curriculum import ProgressiveCurriculum

    class NarrowSource:
        """Only ever returns one category not in allowed set."""
        def __init__(self):
            self.n = 0
        def get_object(self):
            self.n += 1
            return (f"item{self.n}", "only_category")

    source = NarrowSource()
    c = ProgressiveCurriculum(source,
                               stages=[(100, 1), (10**9, 10**9)],
                               category_extractor=lambda n, d: d)

    # First pick establishes "only_category" as the one allowed.
    # Subsequent picks should all match and return quickly.
    for _ in range(10):
        name, _ = c.pick_object()
        assert name.startswith("item")
    stats = c.stats()
    assert stats["rejected"] < 100, (
        f"too many rejections: {stats}")
    print(f"  PASS: does not starve ({stats})")


def main():
    failures = []
    for name, fn in [
        ("narrows_scope_early", test_narrows_scope_early),
        ("expands_with_progress", test_expands_with_progress),
        ("graceful_when_source_lacks_category", test_graceful_when_source_lacks_category),
        ("does_not_starve_on_category_flood", test_does_not_starve_on_category_flood),
    ]:
        print(f"[unit/curriculum] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll curriculum unit tests passed.")


if __name__ == "__main__":
    main()
