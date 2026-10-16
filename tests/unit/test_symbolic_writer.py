#!/usr/bin/env python3
"""Unit tests for symbolic writer."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class MockBrainWithAllApis:
    def __init__(self):
        self.kg_calls = []
        self.episode_calls = []
        self.semantic_calls = []

    def kg_add_fact(self, **kwargs):
        self.kg_calls.append(kwargs)

    def hippocampus_seed_episode(self, **kwargs):
        self.episode_calls.append(kwargs)

    def semantic_memory_insert(self, **kwargs):
        self.semantic_calls.append(kwargs)


class MockBrainNoApis:
    pass


class MockBrainPartialApis:
    def __init__(self):
        self.kg_calls = []
    def kg_add_fact(self, **kwargs):
        self.kg_calls.append(kwargs)


def test_all_layers_written_when_apis_present():
    from symbolic_writer import SymbolicWriter
    brain = MockBrainWithAllApis()
    sw = SymbolicWriter(brain)
    result = sw.record(label="dog", modality="visual",
                        context={"description": "furry animal"})
    assert result["kg"] and result["episode"] and result["semantic"]
    assert len(brain.kg_calls) == 1
    assert brain.kg_calls[0]["subject"] == "dog"
    assert len(brain.episode_calls) == 1
    assert "furry" in brain.episode_calls[0]["text"]
    assert len(brain.semantic_calls) == 1
    print(f"  PASS: all layers written ({sw.stats()})")


def test_graceful_when_no_apis():
    from symbolic_writer import SymbolicWriter
    brain = MockBrainNoApis()
    sw = SymbolicWriter(brain)
    result = sw.record(label="cat", modality="audio")
    assert not result["kg"] and not result["episode"] and not result["semantic"]
    stats = sw.stats()
    assert stats["kg_writes"] == 0
    print(f"  PASS: graceful with no APIs ({stats})")


def test_partial_apis():
    from symbolic_writer import SymbolicWriter
    brain = MockBrainPartialApis()
    sw = SymbolicWriter(brain)
    result = sw.record(label="bird", modality="visual")
    assert result["kg"] and not result["episode"] and not result["semantic"]
    assert len(brain.kg_calls) == 1
    print(f"  PASS: partial APIs used, missing ones skipped")


def test_errors_counted_but_dont_raise():
    from symbolic_writer import SymbolicWriter

    class RaisingBrain:
        def kg_add_fact(self, **kwargs):
            raise RuntimeError("simulated failure")
        def hippocampus_seed_episode(self, **kwargs):
            raise RuntimeError("simulated failure")
        def semantic_memory_insert(self, **kwargs):
            raise RuntimeError("simulated failure")

    sw = SymbolicWriter(RaisingBrain())
    result = sw.record(label="x")  # should not raise
    assert not result["kg"] and not result["episode"] and not result["semantic"]
    assert sw.stats()["errors"] == 3
    print(f"  PASS: exceptions caught and counted ({sw.stats()})")


def main():
    failures = []
    for name, fn in [
        ("all_layers_written_when_apis_present", test_all_layers_written_when_apis_present),
        ("graceful_when_no_apis", test_graceful_when_no_apis),
        ("partial_apis", test_partial_apis),
        ("errors_counted_but_dont_raise", test_errors_counted_but_dont_raise),
    ]:
        print(f"[unit/symbolic_writer] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll symbolic writer unit tests passed.")


if __name__ == "__main__":
    main()
