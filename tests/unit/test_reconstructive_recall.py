#!/usr/bin/env python3
"""Unit tests for reconstructive recall."""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "scripts"))


class MockBrainEpisodicOnly:
    def episodic_memory_search(self, query):
        return [{
            "text": f"I remember {query} vividly",
            "timestamp": 1234567890,
            "valence": 0.5,
        }]


class MockBrainWithSchema:
    def episodic_memory_search(self, query):
        # Sparse gist only
        return [{"text": f"I remember a {query}"}]

    def semantic_memory_query(self, concept=None):
        # Rich schema for the concept
        return {
            "color": "varies",
            "typical_sound": "bark",
            "typical_size": "medium",
        }


class MockBrainNothing:
    pass


def test_gist_only_when_no_schema():
    from reconstructive_recall import ReconstructiveRecaller
    recaller = ReconstructiveRecaller(MockBrainEpisodicOnly())
    r = recaller.recall("dog")
    # Retrieved the gist with details
    assert "dog" in r.gist
    assert not r.reconstructed  # no schema query
    assert r.retrieved_details  # valence, timestamp present
    assert r.confidence > 0
    print(f"  PASS: gist-only recall works (confidence={r.confidence:.2f})")


def test_schema_fills_in_missing_details():
    from reconstructive_recall import ReconstructiveRecaller
    recaller = ReconstructiveRecaller(MockBrainWithSchema())
    r = recaller.recall("dog", concept_for_schema="dog")
    # Reconstruction happened
    assert r.reconstructed
    # Schema fillers present
    assert "color" in r.schema_fillers
    assert "typical_sound" in r.schema_fillers
    # Retrieved details preserved
    assert r.gist
    # Confidence penalized for schema content
    assert r.confidence < 1.0
    print(f"  PASS: schema fills missing details "
          f"(retrieved={len(r.retrieved_details)}, filled={len(r.schema_fillers)})")


def test_nothing_when_brain_has_no_episodic_api():
    from reconstructive_recall import ReconstructiveRecaller
    recaller = ReconstructiveRecaller(MockBrainNothing())
    r = recaller.recall("cat")
    assert "no memory" in r.gist.lower()
    assert r.confidence == 0.0
    assert not r.retrieved_details
    print(f"  PASS: graceful when no episodic API")


def test_narrative_formatting():
    from reconstructive_recall import ReconstructiveRecaller
    recaller = ReconstructiveRecaller(MockBrainWithSchema())
    r = recaller.recall("cat", concept_for_schema="cat")
    narr = r.as_narrative()
    assert "cat" in narr
    assert "schema" in narr.lower() or "bark" in narr
    print(f"  PASS: narrative formatting includes all pieces")


def test_stats_tracking():
    from reconstructive_recall import ReconstructiveRecaller
    recaller = ReconstructiveRecaller(MockBrainWithSchema())
    recaller.recall("dog", concept_for_schema="dog")
    recaller.recall("cat", concept_for_schema="cat")
    recaller.recall("bird")  # no schema → no reconstruction
    stats = recaller.stats()
    assert stats["total_recalls"] == 3
    assert stats["reconstructions"] == 2  # schema-based
    assert abs(stats["reconstruction_rate"] - 2/3) < 0.01
    print(f"  PASS: stats track reconstruction rate ({stats})")


def test_schema_does_not_override_retrieved():
    """Retrieved fields must take priority over schema fillers."""
    from reconstructive_recall import ReconstructiveRecaller

    class BrainWithBothDetailAndSchema:
        def episodic_memory_search(self, query):
            return [{
                "text": f"I remember a {query}",
                "color": "specific_to_this_memory",   # retrieved detail
                "valence": 0.5,
            }]
        def semantic_memory_query(self, concept=None):
            return {
                "color": "schema_default",   # would overwrite if allowed
                "size": "medium",            # not in retrieved
            }

    recaller = ReconstructiveRecaller(BrainWithBothDetailAndSchema())
    r = recaller.recall("dog", concept_for_schema="dog")
    # Retrieved 'color' should remain; schema 'color' should NOT appear in fillers
    assert r.retrieved_details.get("color") == "specific_to_this_memory"
    assert "color" not in r.schema_fillers, (
        "schema overwrote retrieved color (bug)")
    # Schema 'size' should fill in
    assert r.schema_fillers.get("size") == "medium"
    print(f"  PASS: retrieved details not overwritten by schema")


def main():
    failures = []
    for name, fn in [
        ("gist_only_when_no_schema", test_gist_only_when_no_schema),
        ("schema_fills_in_missing_details", test_schema_fills_in_missing_details),
        ("nothing_when_brain_has_no_episodic_api", test_nothing_when_brain_has_no_episodic_api),
        ("narrative_formatting", test_narrative_formatting),
        ("stats_tracking", test_stats_tracking),
        ("schema_does_not_override_retrieved", test_schema_does_not_override_retrieved),
    ]:
        print(f"[unit/reconstructive_recall] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        sys.exit(1)
    print("\nAll reconstructive recall unit tests passed.")


if __name__ == "__main__":
    main()
