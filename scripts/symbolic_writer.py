"""Symbolic-layer updates on every learn_vector call.

Addresses the architectural gap where training only updates the vector
pipeline, leaving the knowledge graph and semantic memory under-populated.

Usage:
    sw = SymbolicWriter(brain)
    loss = brain.learn_vector(features, target, label="dog")
    sw.record(label="dog", modality="visual",
              context={"description": "a friendly furry animal"})

The writer pushes propositions to the knowledge graph, seeds hippocampal
episodes, and updates semantic memory — all in one call, all best-effort.
"""
from __future__ import annotations

import logging
import time
from typing import Any, Optional

log = logging.getLogger("symbolic_writer")


class SymbolicWriter:
    """Writes to symbolic layers alongside neural learning.

    Safe to use even if the brain doesn't expose all APIs — each write
    is guarded individually.
    """

    def __init__(self, brain, episode_valence: float = 0.1,
                 verbose: bool = False):
        self.brain = brain
        self.episode_valence = episode_valence
        self.verbose = verbose
        self._kg_writes = 0
        self._episode_writes = 0
        self._semantic_writes = 0
        self._errors = 0

    def record(self, *, label: str, modality: str = "text",
               context: Optional[dict] = None,
               valence: Optional[float] = None) -> dict:
        """Record one learning event at all available symbolic layers.

        Returns a dict of what succeeded / failed (for diagnostics).
        """
        context = context or {}
        result = {"label": label, "modality": modality,
                  "kg": False, "episode": False, "semantic": False}

        # 1. Knowledge graph — assert (label, observed_with, modality)
        try:
            if hasattr(self.brain, "kg_add_fact"):
                self.brain.kg_add_fact(
                    subject=label,
                    predicate="observed_with",
                    object_=modality,
                    confidence=0.9)
                self._kg_writes += 1
                result["kg"] = True
            elif hasattr(self.brain, "ti_add_fact"):
                self.brain.ti_add_fact(label, "observed_with", modality)
                self._kg_writes += 1
                result["kg"] = True
        except Exception as e:
            self._errors += 1
            if self.verbose:
                log.debug("kg_add_fact failed: %s", e)

        # 2. Hippocampal episode — timestamped experience
        try:
            if hasattr(self.brain, "hippocampus_seed_episode"):
                ep_text = context.get("description") or f"observed {label}"
                v = valence if valence is not None else self.episode_valence
                self.brain.hippocampus_seed_episode(
                    text=ep_text,
                    valence=v,
                    timestamp=time.time(),
                    modality=modality)
                self._episode_writes += 1
                result["episode"] = True
        except Exception as e:
            self._errors += 1
            if self.verbose:
                log.debug("hippocampus_seed_episode failed: %s", e)

        # 3. Semantic memory — concept node + attributes
        try:
            if hasattr(self.brain, "semantic_memory_insert"):
                self.brain.semantic_memory_insert(
                    concept=label,
                    attributes=context)
                self._semantic_writes += 1
                result["semantic"] = True
        except Exception as e:
            self._errors += 1
            if self.verbose:
                log.debug("semantic_memory_insert failed: %s", e)

        return result

    def stats(self) -> dict:
        return {
            "kg_writes": self._kg_writes,
            "episode_writes": self._episode_writes,
            "semantic_writes": self._semantic_writes,
            "errors": self._errors,
        }


def symbolic_learn_vector(brain, features, target, *,
                           label: str,
                           context: Optional[dict] = None,
                           writer: Optional[SymbolicWriter] = None,
                           **kwargs) -> float:
    """Drop-in replacement for brain.learn_vector that also updates
    symbolic layers.

    Returns the same loss value as brain.learn_vector.
    """
    loss = brain.learn_vector(features, target, label=label, **kwargs)
    if writer is None:
        writer = SymbolicWriter(brain)
    writer.record(label=label, modality="text", context=context)
    return loss
