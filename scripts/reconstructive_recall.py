"""Reconstructive episodic recall (Bartlett 1932 schema theory).

Human episodic memory doesn't play back stored vectors — it reconstructs
plausible detail from a compact gist + semantic knowledge. You remember
that dinner was pasta; the fork, plate, and table are filled in from
schematic knowledge about dinners.

Athena's default recall returns stored vectors. This module wraps that
with reconstructive behavior:
    1. Retrieve gist (compact encoding)
    2. Query semantic knowledge for the concept involved
    3. Fill in plausible details from the schema
    4. Return composite "memory" — partly retrieved, partly constructed
"""
from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import Any, Optional

log = logging.getLogger("reconstructive_recall")


@dataclass
class ReconstructedMemory:
    gist: str
    retrieved_details: dict[str, Any] = field(default_factory=dict)
    schema_fillers: dict[str, Any] = field(default_factory=dict)
    confidence: float = 0.5
    reconstructed: bool = False
    sources: list[str] = field(default_factory=list)

    def as_narrative(self) -> str:
        """Return a narrative string combining retrieved + reconstructed parts."""
        parts = [self.gist]
        for k, v in self.retrieved_details.items():
            parts.append(f"({k}: {v})")
        if self.schema_fillers:
            plausible = "; ".join(f"{k}={v}" for k, v in self.schema_fillers.items())
            parts.append(f"[schema fillers: {plausible}]")
        return " ".join(parts)


class ReconstructiveRecaller:
    """Wraps brain's episodic retrieval with schema-driven reconstruction."""

    def __init__(self, brain, schema_confidence_weight: float = 0.3):
        """
        Args:
            brain: nimcp.Brain or proxy
            schema_confidence_weight: how much schema fillers lower confidence
                vs directly retrieved details
        """
        self.brain = brain
        self.schema_confidence_weight = schema_confidence_weight
        self._recall_count = 0
        self._reconstruction_count = 0

    def recall(self, query: str,
               concept_for_schema: Optional[str] = None) -> ReconstructedMemory:
        """Retrieve episode matching query, fill in schema details."""
        self._recall_count += 1

        # 1. Retrieve gist from episodic store
        gist, retrieved_details, sources = self._retrieve_gist(query)

        result = ReconstructedMemory(
            gist=gist or f"(no memory found for '{query}')",
            retrieved_details=retrieved_details,
            sources=sources,
        )

        # 2. If we have a concept hint, query semantic schema
        if concept_for_schema:
            fillers = self._get_schema_fillers(concept_for_schema)
            # Only keep fillers for attributes NOT already in retrieved
            for k, v in fillers.items():
                if k not in retrieved_details:
                    result.schema_fillers[k] = v

        # 3. Compute reconstruction confidence
        n_retrieved = len(retrieved_details)
        n_filled = len(result.schema_fillers)
        if n_retrieved + n_filled == 0:
            result.confidence = 0.0
        else:
            # Retrieved details worth more than schema fillers
            retrieved_contrib = n_retrieved
            filled_contrib = n_filled * self.schema_confidence_weight
            total = retrieved_contrib + filled_contrib
            max_possible = n_retrieved + n_filled
            result.confidence = total / max_possible if max_possible else 0

        result.reconstructed = n_filled > 0
        if result.reconstructed:
            self._reconstruction_count += 1

        return result

    def _retrieve_gist(self, query: str) -> tuple[Optional[str], dict, list[str]]:
        """Retrieve episode via any available API."""
        sources = []
        try:
            if hasattr(self.brain, "episodic_memory_search"):
                r = self.brain.episodic_memory_search(query)
                if r:
                    sources.append("episodic_search")
                    ep = r[0] if isinstance(r, list) else r
                    if isinstance(ep, dict):
                        return (ep.get("text", str(ep)),
                                {k: v for k, v in ep.items()
                                 if k not in {"text", "id"}},
                                sources)
                    return (str(ep), {}, sources)
            if hasattr(self.brain, "hippocampus_query"):
                r = self.brain.hippocampus_query(query=query)
                if r:
                    sources.append("hippocampus")
                    return (str(r.get("text", r)), {}, sources)
        except Exception as e:
            log.debug("retrieve_gist failed: %s", e)
        return (None, {}, sources)

    def _get_schema_fillers(self, concept: str) -> dict[str, Any]:
        """Query semantic memory for this concept's typical attributes."""
        try:
            if hasattr(self.brain, "semantic_memory_query"):
                r = self.brain.semantic_memory_query(concept=concept)
                if r and isinstance(r, dict):
                    return r
            if hasattr(self.brain, "kg_query"):
                facts = self.brain.kg_query(subject=concept)
                if facts:
                    return {f[0] if isinstance(f, tuple) else "fact":
                            (f[1] if isinstance(f, tuple) and len(f) > 1 else f)
                            for f in facts[:10]}
        except Exception as e:
            log.debug("schema_fillers failed: %s", e)
        return {}

    def stats(self) -> dict:
        return {
            "total_recalls": self._recall_count,
            "reconstructions": self._reconstruction_count,
            "reconstruction_rate": (
                self._reconstruction_count / self._recall_count
                if self._recall_count else 0.0),
        }
