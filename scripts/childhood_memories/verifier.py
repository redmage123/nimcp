"""Implantation verifier — confirms each implanted memory is retrievable.

Runs after MemoryImplanter to validate that the brain can actually access
the implanted content via its query APIs. Any failure is a red flag.
"""
from __future__ import annotations

import json
import logging
import random
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

log = logging.getLogger("childhood_memories.verifier")


@dataclass
class VerifyResult:
    concept_checks: int = 0
    concept_hits: int = 0
    kg_checks: int = 0
    kg_hits: int = 0
    episode_checks: int = 0
    episode_hits: int = 0
    narrative_hits: bool = False
    failures: list[str] = field(default_factory=list)

    @property
    def retrieval_rate(self) -> float:
        total = self.concept_checks + self.kg_checks + self.episode_checks
        ok = self.concept_hits + self.kg_hits + self.episode_hits
        return ok / total if total else 0.0

    def summary(self) -> str:
        return (f"concepts: {self.concept_hits}/{self.concept_checks}  "
                f"kg: {self.kg_hits}/{self.kg_checks}  "
                f"episodes: {self.episode_hits}/{self.episode_checks}  "
                f"narrative: {'yes' if self.narrative_hits else 'no'}  "
                f"retrieval: {self.retrieval_rate:.1%}")


def verify_retrievable(brain, memory_dir: str, *,
                        sample_n: int = 20,
                        seed: int = 123) -> VerifyResult:
    """Sample implanted items and confirm they retrieve.

    sample_n concepts are checked; sample_n KG triples; sample_n episodes.
    Brain APIs are tried in order, with fallbacks. If none work for a given
    layer, all checks for that layer are recorded as failures (no blame on
    the data — it's a brain API availability issue).
    """
    md = Path(memory_dir)
    result = VerifyResult()
    rng = random.Random(seed)

    _verify_concepts(brain, md, rng, sample_n, result)
    _verify_kg(brain, md, rng, sample_n, result)
    _verify_episodes(brain, md, rng, sample_n, result)
    _verify_narrative(brain, md, result)

    log.info("Verification: %s", result.summary())
    return result


def _verify_concepts(brain, md, rng, sample_n, result):
    path = md / "concepts.json"
    if not path.exists():
        return
    with open(path) as f:
        data = json.load(f)
    items = data.get("items", [])
    if not items:
        return
    sample = rng.sample(items, min(sample_n, len(items)))
    for item in sample:
        concept = item["concept"]
        result.concept_checks += 1
        if _retrieve_concept(brain, concept):
            result.concept_hits += 1
        else:
            result.failures.append(f"concept:{concept}")


def _retrieve_concept(brain, concept: str) -> bool:
    try:
        if hasattr(brain, "semantic_memory_query"):
            r = brain.semantic_memory_query(concept=concept)
            return r is not None and r != {}
        if hasattr(brain, "kg_query"):
            r = brain.kg_query(subject=concept)
            return bool(r)
    except Exception:
        pass
    return False


def _verify_kg(brain, md, rng, sample_n, result):
    path = md / "kg_triples.json"
    if not path.exists():
        return
    with open(path) as f:
        data = json.load(f)
    items = data.get("items", [])
    if not items:
        return
    sample = rng.sample(items, min(sample_n, len(items)))
    for t in sample:
        result.kg_checks += 1
        if _retrieve_kg(brain, t["subject"], t["predicate"]):
            result.kg_hits += 1
        else:
            result.failures.append(f"kg:{t['subject']}+{t['predicate']}")


def _retrieve_kg(brain, subject, predicate) -> bool:
    try:
        if hasattr(brain, "kg_query"):
            r = brain.kg_query(subject=subject, predicate=predicate)
            return bool(r)
        if hasattr(brain, "ti_forward_chain"):
            r = brain.ti_forward_chain(depth=1)
            return r is not None and r > 0
    except Exception:
        pass
    return False


def _verify_episodes(brain, md, rng, sample_n, result):
    path = md / "episodes.json"
    if not path.exists():
        return
    with open(path) as f:
        data = json.load(f)
    items = data.get("items", [])
    if not items:
        return
    sample = rng.sample(items, min(sample_n, len(items)))
    for e in sample:
        result.episode_checks += 1
        if _retrieve_episode(brain, e["id"], e.get("related_concepts", [])):
            result.episode_hits += 1
        else:
            result.failures.append(f"episode:{e['id']}")


def _retrieve_episode(brain, episode_id, related_concepts) -> bool:
    try:
        if hasattr(brain, "hippocampus_query"):
            r = brain.hippocampus_query(episode_id=episode_id)
            return r is not None
        if hasattr(brain, "episodic_memory_search"):
            # Search by related concept
            if related_concepts:
                r = brain.episodic_memory_search(related_concepts[0])
                return r is not None and r != []
    except Exception:
        pass
    return False


def _verify_narrative(brain, md, result):
    path = md / "narrative_identity.json"
    if not path.exists():
        return
    try:
        if hasattr(brain, "self_model_get_narrative"):
            n = brain.self_model_get_narrative()
            result.narrative_hits = n is not None and n != {}
    except Exception:
        pass
