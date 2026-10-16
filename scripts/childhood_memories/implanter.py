"""Memory implanter — loads generated JSON into brain substrates.

Every write is best-effort: if the brain doesn't expose a particular API,
that layer is skipped with a clear log message. This keeps implantation
robust across brain versions.
"""
from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

log = logging.getLogger("childhood_memories.implanter")


@dataclass
class ImplantResult:
    concepts_attempted: int = 0
    concepts_implanted: int = 0
    kg_triples_attempted: int = 0
    kg_triples_implanted: int = 0
    episodes_attempted: int = 0
    episodes_implanted: int = 0
    phono_attempted: int = 0
    phono_implanted: int = 0
    narrative_implanted: bool = False
    errors: list[str] = field(default_factory=list)

    @property
    def success_rate(self) -> float:
        total = (self.concepts_attempted + self.kg_triples_attempted +
                 self.episodes_attempted + self.phono_attempted)
        ok = (self.concepts_implanted + self.kg_triples_implanted +
              self.episodes_implanted + self.phono_implanted)
        return ok / total if total else 0.0

    def summary(self) -> str:
        return (f"concepts: {self.concepts_implanted}/{self.concepts_attempted}  "
                f"kg: {self.kg_triples_implanted}/{self.kg_triples_attempted}  "
                f"episodes: {self.episodes_implanted}/{self.episodes_attempted}  "
                f"phono: {self.phono_implanted}/{self.phono_attempted}  "
                f"narrative: {'yes' if self.narrative_implanted else 'no'}  "
                f"errors: {len(self.errors)}  "
                f"success_rate: {self.success_rate:.1%}")


class MemoryImplanter:
    """Loads a generated memory directory into a brain."""

    def __init__(self, brain, memory_dir: str, verbose: bool = False):
        self.brain = brain
        self.memory_dir = Path(memory_dir)
        self.verbose = verbose
        if not self.memory_dir.exists():
            raise FileNotFoundError(f"Memory dir not found: {self.memory_dir}")

    def implant_all(self) -> ImplantResult:
        result = ImplantResult()
        self._implant_concepts(result)
        self._implant_kg(result)
        self._implant_episodes(result)
        self._implant_phono(result)
        self._implant_narrative(result)
        log.info("Implantation done — %s", result.summary())
        return result

    # ---- per-layer ----

    def _implant_concepts(self, result: ImplantResult) -> None:
        path = self.memory_dir / "concepts.json"
        if not path.exists():
            return
        with open(path) as f:
            data = json.load(f)
        for item in data.get("items", []):
            result.concepts_attempted += 1
            if self._write_semantic(item):
                result.concepts_implanted += 1

    def _implant_kg(self, result: ImplantResult) -> None:
        path = self.memory_dir / "kg_triples.json"
        if not path.exists():
            return
        with open(path) as f:
            data = json.load(f)
        for t in data.get("items", []):
            result.kg_triples_attempted += 1
            if self._write_kg(t):
                result.kg_triples_implanted += 1

    def _implant_episodes(self, result: ImplantResult) -> None:
        path = self.memory_dir / "episodes.json"
        if not path.exists():
            return
        with open(path) as f:
            data = json.load(f)
        for e in data.get("items", []):
            result.episodes_attempted += 1
            if self._write_episode(e):
                result.episodes_implanted += 1

    def _implant_phono(self, result: ImplantResult) -> None:
        path = self.memory_dir / "phonological.json"
        if not path.exists():
            return
        with open(path) as f:
            data = json.load(f)
        for p in data.get("items", []):
            result.phono_attempted += 1
            if self._write_phono(p):
                result.phono_implanted += 1

    def _implant_narrative(self, result: ImplantResult) -> None:
        path = self.memory_dir / "narrative_identity.json"
        if not path.exists():
            return
        with open(path) as f:
            data = json.load(f)
        narrative = data.get("item")
        if narrative and self._write_narrative(narrative):
            result.narrative_implanted = True

    # ---- write adapters — each tries multiple API names ----

    def _write_semantic(self, item: dict) -> bool:
        concept = item.get("concept", "")
        attrs = item.get("relations", {})
        try:
            if hasattr(self.brain, "semantic_memory_insert"):
                self.brain.semantic_memory_insert(concept=concept, attributes=attrs)
                return True
            if hasattr(self.brain, "kg_add_fact"):
                # Fallback: represent as (concept, has_attributes, summary)
                self.brain.kg_add_fact(
                    subject=concept, predicate="has_attributes",
                    object_=str(attrs)[:200], confidence=0.9)
                return True
        except Exception as e:
            if self.verbose:
                log.debug("_write_semantic %s failed: %s", concept, e)
        return False

    def _write_kg(self, triple: dict) -> bool:
        try:
            if hasattr(self.brain, "kg_add_fact"):
                self.brain.kg_add_fact(
                    subject=triple["subject"],
                    predicate=triple["predicate"],
                    object_=str(triple["object"]),
                    confidence=float(triple.get("confidence", 0.9)))
                return True
            if hasattr(self.brain, "ti_add_fact"):
                self.brain.ti_add_fact(
                    triple["subject"], triple["predicate"], str(triple["object"]))
                return True
        except Exception as e:
            if self.verbose:
                log.debug("_write_kg failed: %s", e)
        return False

    def _write_episode(self, episode: dict) -> bool:
        try:
            if hasattr(self.brain, "hippocampus_seed_episode"):
                self.brain.hippocampus_seed_episode(
                    text=episode["text"],
                    valence=float(episode.get("valence", 0.0)),
                    timestamp=float(episode.get("timestamp", time.time())),
                    modality=episode.get("modality", "text"))
                return True
            if hasattr(self.brain, "episodic_memory_store"):
                self.brain.episodic_memory_store(
                    text=episode["text"],
                    timestamp=float(episode.get("timestamp", time.time())))
                return True
        except Exception as e:
            if self.verbose:
                log.debug("_write_episode failed: %s", e)
        return False

    def _write_phono(self, phono: dict) -> bool:
        try:
            if hasattr(self.brain, "vocabulary_add_word"):
                self.brain.vocabulary_add_word(
                    word=phono["word"],
                    phonemes=phono.get("phonemes", []),
                    syllables=int(phono.get("syllables", 1)))
                return True
        except Exception as e:
            if self.verbose:
                log.debug("_write_phono failed: %s", e)
        return False

    def _write_narrative(self, narrative: dict) -> bool:
        try:
            if hasattr(self.brain, "self_model_set_narrative"):
                self.brain.self_model_set_narrative(
                    opening=narrative.get("opening", ""),
                    values=narrative.get("values", []),
                    memories=narrative.get("early_memories", []),
                    voice=narrative.get("voice", {}))
                return True
        except Exception as e:
            if self.verbose:
                log.debug("_write_narrative failed: %s", e)
        return False
