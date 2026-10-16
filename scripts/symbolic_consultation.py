"""Symbolic consultation during inference.

The architectural gap: brain.decide_full(features) currently runs the vector
pipeline only. The knowledge graph, semantic memory, and inner dialogue
subsystems aren't queried at inference time.

This module adds a consultation layer: before returning a decision, query
the symbolic substrate for relevant facts, blend them into the output,
or flag conflicts.

Usage:
    consultant = SymbolicConsultant(brain)
    result = consultant.decide(features, concept_hint="dog")
    # result has vector_output + symbolic_facts + blended_decision
"""
from __future__ import annotations

import logging
from dataclasses import dataclass, field
from typing import Any, Optional

log = logging.getLogger("symbolic_consultation")


@dataclass
class ConsultationResult:
    vector_output: Any = None
    vector_confidence: float = 0.0
    symbolic_facts: list[dict] = field(default_factory=list)
    symbolic_confidence: float = 0.0
    blended_answer: Any = None
    blended_confidence: float = 0.0
    sources_consulted: list[str] = field(default_factory=list)
    disagreements: list[str] = field(default_factory=list)

    def summary(self) -> str:
        return (f"vector={self.vector_output}(c={self.vector_confidence:.2f}), "
                f"{len(self.symbolic_facts)} facts, "
                f"blended={self.blended_answer}(c={self.blended_confidence:.2f}), "
                f"sources={self.sources_consulted}")


class SymbolicConsultant:
    """Blends vector-pipeline output with symbolic-substrate queries."""

    def __init__(self, brain, vector_weight: float = 0.6,
                 symbolic_weight: float = 0.4):
        if abs((vector_weight + symbolic_weight) - 1.0) > 1e-6:
            raise ValueError("weights must sum to 1.0")
        self.brain = brain
        self.vector_weight = vector_weight
        self.symbolic_weight = symbolic_weight

    def decide(self, features: list[float],
               concept_hint: Optional[str] = None,
               query: Optional[str] = None) -> ConsultationResult:
        """Run vector pipeline + symbolic consultation, blend results."""
        result = ConsultationResult()

        # Vector pipeline
        self._consult_vector(features, result)

        # Symbolic consultation
        if concept_hint:
            self._consult_kg(concept_hint, result)
            self._consult_semantic_memory(concept_hint, result)
        if query:
            self._consult_episodic(query, result)

        # Blend
        self._blend(result)

        return result

    def _consult_vector(self, features, result: ConsultationResult) -> None:
        try:
            if hasattr(self.brain, "decide_full"):
                r = self.brain.decide_full(features)
                if isinstance(r, dict):
                    result.vector_output = r.get("output") or r.get("label")
                    result.vector_confidence = float(r.get("confidence", 0.5))
                else:
                    result.vector_output = r
                    result.vector_confidence = 0.5
                result.sources_consulted.append("vector_pipeline")
            elif hasattr(self.brain, "predict"):
                r = self.brain.predict(features)
                if isinstance(r, tuple) and len(r) == 2:
                    result.vector_output = r[0]
                    result.vector_confidence = float(r[1])
                else:
                    result.vector_output = r
                    result.vector_confidence = 0.5
                result.sources_consulted.append("predict")
        except Exception as e:
            log.debug("vector consult failed: %s", e)

    def _consult_kg(self, concept: str, result: ConsultationResult) -> None:
        try:
            if hasattr(self.brain, "kg_query"):
                facts = self.brain.kg_query(subject=concept)
                if facts:
                    for f in facts[:10]:
                        result.symbolic_facts.append({
                            "source": "kg",
                            "subject": concept,
                            "fact": f,
                        })
                    result.sources_consulted.append("kg_query")
        except Exception as e:
            log.debug("kg_query failed: %s", e)

    def _consult_semantic_memory(self, concept: str, result: ConsultationResult) -> None:
        try:
            if hasattr(self.brain, "semantic_memory_query"):
                r = self.brain.semantic_memory_query(concept=concept)
                if r:
                    result.symbolic_facts.append({
                        "source": "semantic_memory",
                        "concept": concept,
                        "attributes": r,
                    })
                    result.sources_consulted.append("semantic_memory")
        except Exception as e:
            log.debug("semantic_memory_query failed: %s", e)

    def _consult_episodic(self, query: str, result: ConsultationResult) -> None:
        try:
            if hasattr(self.brain, "episodic_memory_search"):
                r = self.brain.episodic_memory_search(query)
                if r:
                    for ep in r[:3]:
                        result.symbolic_facts.append({
                            "source": "episodic",
                            "episode": ep,
                        })
                    result.sources_consulted.append("episodic_memory")
        except Exception as e:
            log.debug("episodic_memory_search failed: %s", e)

    def _blend(self, result: ConsultationResult) -> None:
        """Combine vector output with symbolic facts to produce blended answer."""
        if not result.symbolic_facts:
            # Nothing to blend; pass through vector
            result.blended_answer = result.vector_output
            result.blended_confidence = result.vector_confidence
            return

        # Compute symbolic confidence: ratio of fact presence to max possible
        n_facts = len(result.symbolic_facts)
        result.symbolic_confidence = min(1.0, n_facts / 5.0)

        # Blended confidence is weighted average
        result.blended_confidence = (
            self.vector_weight * result.vector_confidence +
            self.symbolic_weight * result.symbolic_confidence
        )

        # Blended answer: prefer vector output but annotate with facts
        if isinstance(result.vector_output, str):
            fact_summary = "; ".join(
                f"{f.get('source')}={str(f)[:40]}"
                for f in result.symbolic_facts[:3])
            result.blended_answer = f"{result.vector_output} [facts: {fact_summary}]"
        else:
            result.blended_answer = result.vector_output
