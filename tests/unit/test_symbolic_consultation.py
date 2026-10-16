#!/usr/bin/env python3
"""Unit tests for symbolic consultation."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class MockBrainVectorOnly:
    def predict(self, features):
        return ("dog", 0.75)


class MockBrainWithKG:
    def __init__(self):
        self.kg_facts = {
            "dog": [("has", "fur"), ("says", "bark"), ("is_a", "mammal")],
            "cat": [("has", "fur"), ("says", "meow"), ("is_a", "mammal")],
        }
    def predict(self, features):
        return ("dog", 0.7)
    def kg_query(self, subject=None, predicate=None):
        return self.kg_facts.get(subject, [])


class MockBrainFull:
    def __init__(self):
        self.kg_facts = {"dog": [("has", "fur"), ("says", "bark")]}
        self.semantic = {"dog": {"color": "varies", "typical_size": "medium"}}
    def predict(self, features):
        return ("dog", 0.8)
    def kg_query(self, subject=None, predicate=None):
        return self.kg_facts.get(subject, [])
    def semantic_memory_query(self, concept=None):
        return self.semantic.get(concept)
    def episodic_memory_search(self, query):
        return [{"text": f"I remember a {query}", "timestamp": 123}]


def test_vector_only_brain_passes_through():
    from symbolic_consultation import SymbolicConsultant
    consultant = SymbolicConsultant(MockBrainVectorOnly())
    r = consultant.decide([0.1] * 10)
    # No symbolic facts available → blended == vector
    assert r.vector_output == "dog"
    assert r.blended_answer == "dog"
    assert abs(r.blended_confidence - 0.75) < 0.01
    assert len(r.symbolic_facts) == 0
    print(f"  PASS: vector-only brain passes through ({r.summary()})")


def test_kg_facts_blended_into_output():
    from symbolic_consultation import SymbolicConsultant
    consultant = SymbolicConsultant(MockBrainWithKG())
    r = consultant.decide([0.1] * 10, concept_hint="dog")
    assert r.vector_output == "dog"
    assert len(r.symbolic_facts) > 0
    assert "kg" in str(r.blended_answer)
    assert "kg_query" in r.sources_consulted
    print(f"  PASS: KG facts blended into output ({len(r.symbolic_facts)} facts)")


def test_all_substrates_consulted_when_available():
    from symbolic_consultation import SymbolicConsultant
    consultant = SymbolicConsultant(MockBrainFull())
    r = consultant.decide([0.1] * 10, concept_hint="dog", query="dog")
    assert "kg_query" in r.sources_consulted
    assert "semantic_memory" in r.sources_consulted
    assert "episodic_memory" in r.sources_consulted
    assert len(r.symbolic_facts) >= 3  # kg + semantic + episodic
    print(f"  PASS: all 3 symbolic substrates consulted ({r.sources_consulted})")


def test_weights_sum_validation():
    from symbolic_consultation import SymbolicConsultant
    try:
        SymbolicConsultant(MockBrainVectorOnly(), vector_weight=0.5, symbolic_weight=0.4)
        assert False, "should have raised ValueError"
    except ValueError:
        pass
    print(f"  PASS: weight validation enforced")


def test_graceful_on_exception():
    from symbolic_consultation import SymbolicConsultant
    class RaisingBrain:
        def predict(self, features):
            raise RuntimeError("simulated")
        def kg_query(self, subject=None, predicate=None):
            raise RuntimeError("simulated")
    consultant = SymbolicConsultant(RaisingBrain())
    r = consultant.decide([0.1] * 10, concept_hint="x")
    # Should not crash, returns empty result
    assert r.vector_output is None
    print(f"  PASS: exceptions caught gracefully")


def test_confidence_blending():
    from symbolic_consultation import SymbolicConsultant
    consultant = SymbolicConsultant(MockBrainWithKG(),
                                      vector_weight=0.5, symbolic_weight=0.5)
    r = consultant.decide([0.1] * 10, concept_hint="dog")
    # vector=0.7, 3 facts → symbolic=3/5=0.6, blend=0.65
    assert 0.6 < r.blended_confidence < 0.7, f"blend={r.blended_confidence}"
    print(f"  PASS: confidence blending correct (c={r.blended_confidence:.2f})")


def main():
    failures = []
    for name, fn in [
        ("vector_only_brain_passes_through", test_vector_only_brain_passes_through),
        ("kg_facts_blended_into_output", test_kg_facts_blended_into_output),
        ("all_substrates_consulted_when_available", test_all_substrates_consulted_when_available),
        ("weights_sum_validation", test_weights_sum_validation),
        ("graceful_on_exception", test_graceful_on_exception),
        ("confidence_blending", test_confidence_blending),
    ]:
        print(f"[unit/symbolic_consultation] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll symbolic consultation unit tests passed.")


if __name__ == "__main__":
    main()
