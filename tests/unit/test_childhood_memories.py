#!/usr/bin/env python3
"""Unit tests for childhood memory generation + implantation."""
from __future__ import annotations

import json
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))


def test_generator_produces_all_layers():
    from childhood_memories import MemoryGenerator
    with tempfile.TemporaryDirectory() as tmp:
        gen = MemoryGenerator(output_dir=tmp, seed=1, vector_dim=32)
        summary = gen.generate_all()

        # All layers produced
        assert summary["concepts"] > 0
        assert summary["kg_triples"] > 0
        assert summary["episodes"] > 0
        assert summary["phonological"] > 0
        assert summary["narrative"] == 1

        # Files exist
        tmp_path = Path(tmp)
        for fname in ["concepts.json", "kg_triples.json", "episodes.json",
                       "phonological.json", "narrative_identity.json",
                       "manifest.json"]:
            assert (tmp_path / fname).exists(), f"missing {fname}"
        print(f"  PASS: all 6 files produced ({summary})")


def test_generator_is_deterministic():
    """Same seed + fixed reference_timestamp must produce byte-identical output."""
    from childhood_memories import MemoryGenerator
    REF_TS = 1700000000.0  # pinned timestamp for cross-run determinism
    with tempfile.TemporaryDirectory() as tmp1, tempfile.TemporaryDirectory() as tmp2:
        gen1 = MemoryGenerator(output_dir=tmp1, seed=42, vector_dim=32,
                                reference_timestamp=REF_TS)
        gen2 = MemoryGenerator(output_dir=tmp2, seed=42, vector_dim=32,
                                reference_timestamp=REF_TS)
        gen1.generate_all()
        gen2.generate_all()

        # Compare concept vectors — must be bit-identical
        with open(Path(tmp1) / "concepts.json") as f:
            c1 = json.load(f)["items"]
        with open(Path(tmp2) / "concepts.json") as f:
            c2 = json.load(f)["items"]
        assert len(c1) == len(c2)
        for a, b in zip(c1, c2):
            assert a["concept"] == b["concept"]
            assert a["grounded_vector"] == b["grounded_vector"], (
                f"vectors differ for {a['concept']}")

        # Compare episode timestamps — now deterministic via ref_ts
        with open(Path(tmp1) / "episodes.json") as f:
            e1 = json.load(f)["items"]
        with open(Path(tmp2) / "episodes.json") as f:
            e2 = json.load(f)["items"]
        assert len(e1) == len(e2)
        for a, b in zip(e1, e2):
            assert a["timestamp"] == b["timestamp"], (
                f"episode timestamps differ: {a['timestamp']} vs {b['timestamp']}")
        print(f"  PASS: deterministic across runs with fixed ref_ts "
              f"({len(c1)} concepts, {len(e1)} episodes)")


def test_grounded_vectors_are_normalized():
    from childhood_memories import MemoryGenerator
    import math
    with tempfile.TemporaryDirectory() as tmp:
        gen = MemoryGenerator(output_dir=tmp, seed=1, vector_dim=64)
        gen.generate_all()
        with open(Path(tmp) / "concepts.json") as f:
            data = json.load(f)
        for item in data["items"]:
            v = item["grounded_vector"]
            norm = math.sqrt(sum(x * x for x in v))
            assert abs(norm - 1.0) < 1e-4, (
                f"{item['concept']} vector not unit-normalized (norm={norm})")
        print(f"  PASS: all vectors unit-normalized")


def test_implanter_graceful_without_apis():
    from childhood_memories import MemoryImplanter, MemoryGenerator

    class EmptyBrain:
        pass

    with tempfile.TemporaryDirectory() as tmp:
        MemoryGenerator(output_dir=tmp, seed=1, vector_dim=16).generate_all()
        brain = EmptyBrain()
        implanter = MemoryImplanter(brain, memory_dir=tmp)
        result = implanter.implant_all()
        # Should complete with 0 successes but no crash
        assert result.concepts_attempted > 0
        assert result.concepts_implanted == 0  # no API
        assert not result.errors  # no crashes, just skips
        print(f"  PASS: graceful without APIs ({result.summary()})")


def test_implanter_with_partial_api():
    from childhood_memories import MemoryImplanter, MemoryGenerator

    class PartialBrain:
        def __init__(self):
            self.kg_calls = []
            self.episode_calls = []
        def kg_add_fact(self, **kwargs):
            self.kg_calls.append(kwargs)
        def hippocampus_seed_episode(self, **kwargs):
            self.episode_calls.append(kwargs)

    with tempfile.TemporaryDirectory() as tmp:
        MemoryGenerator(output_dir=tmp, seed=1, vector_dim=16).generate_all()
        brain = PartialBrain()
        result = MemoryImplanter(brain, memory_dir=tmp).implant_all()
        # kg + episodes should implant.
        # Concepts fall back to kg_add_fact when semantic_memory_insert absent,
        # so they also succeed on this partial brain.
        # phono + narrative should skip (no API).
        assert result.kg_triples_implanted > 0, "KG writes expected"
        assert result.episodes_implanted > 0, "Episode writes expected"
        assert result.concepts_implanted > 0, "Concept fallback to kg_add_fact expected"
        assert result.phono_implanted == 0, "Phono should skip without API"
        assert not result.narrative_implanted, "Narrative should skip without API"
        print(f"  PASS: partial API coverage works ({result.summary()})")


def test_verifier_returns_meaningful_result():
    from childhood_memories import MemoryImplanter, MemoryGenerator, verify_retrievable

    class QueryableBrain:
        def __init__(self):
            self.kg = {}
        def kg_add_fact(self, subject, predicate, object_, **kw):
            self.kg.setdefault(subject, []).append((predicate, object_))
        def kg_query(self, subject=None, predicate=None):
            return self.kg.get(subject, [])

    with tempfile.TemporaryDirectory() as tmp:
        MemoryGenerator(output_dir=tmp, seed=1, vector_dim=16).generate_all()
        brain = QueryableBrain()
        MemoryImplanter(brain, memory_dir=tmp).implant_all()

        vr = verify_retrievable(brain, memory_dir=tmp, sample_n=5)
        # At least KG checks should pass since we implemented kg_query
        assert vr.kg_checks > 0
        assert vr.kg_hits > 0
        print(f"  PASS: verifier runs and reports hits ({vr.summary()})")


def main():
    failures = []
    for name, fn in [
        ("generator_produces_all_layers", test_generator_produces_all_layers),
        ("generator_is_deterministic", test_generator_is_deterministic),
        ("grounded_vectors_are_normalized", test_grounded_vectors_are_normalized),
        ("implanter_graceful_without_apis", test_implanter_graceful_without_apis),
        ("implanter_with_partial_api", test_implanter_with_partial_api),
        ("verifier_returns_meaningful_result", test_verifier_returns_meaningful_result),
    ]:
        print(f"[unit/childhood_memories] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll childhood memory unit tests passed.")


if __name__ == "__main__":
    main()
