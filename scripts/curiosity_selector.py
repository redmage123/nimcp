"""Curiosity-driven stimulus selector.

Augments the fixed training stimulus schedule with curiosity-aware selection:
when the brain reports knowledge gaps on specific concepts, we preferentially
present items targeting those gaps.

Falls back to random selection if curiosity API is unavailable or quiet.
"""
from __future__ import annotations

import logging
import random
from collections import deque
from typing import Any, Callable

log = logging.getLogger("curiosity_selector")


class CuriositySelector:
    """Wraps a source.get_object() / source.get_fact() source with curiosity.

    Protocol:
        selector = CuriositySelector(brain, source, bias=0.4)
        name, description = selector.pick_object()

    Each call:
      - With probability `bias` (default 0.4), query curiosity and target a gap
      - Otherwise fall through to the underlying source

    The selector also tracks recently presented concepts to avoid
    repetition when the brain's curiosity reports the same gap repeatedly.
    """

    def __init__(self, brain, source, bias: float = 0.4,
                 recent_history_n: int = 20):
        self.brain = brain
        self.source = source
        self.bias = bias
        self._recent: deque = deque(maxlen=recent_history_n)
        self._curiosity_hits = 0
        self._fallback_hits = 0

    def __getattr__(self, name):
        """Transparently forward any attribute the selector doesn't define to
        the wrapped source, so CuriositySelector is a true drop-in for a raw
        stimulus source (e.g. callers that read source.ADVANCED_TOPICS).

        __getattr__ only fires on normal-lookup failure. Read `source` from
        __dict__ directly to avoid recursion if it's accessed before __init__
        finishes setting it.
        """
        if name.startswith('__') and name.endswith('__'):
            raise AttributeError(name)
        source = self.__dict__.get('source')
        if source is None:
            raise AttributeError(name)
        return getattr(source, name)

    # ---- Public API ----

    def get_object(self) -> tuple[str, str]:
        """Alias for pick_object — allows CuriositySelector to be used
        anywhere a raw stimulus source (.get_object) is expected."""
        return self.pick_object()

    def get_fact(self, preferred_domain=None) -> tuple:
        """Alias for pick_fact — allows drop-in replacement of source."""
        return self.pick_fact(preferred_domain=preferred_domain)

    def get_sensory(self):
        """Pass-through for sensory queries — curiosity does not bias these."""
        return self.source.get_sensory()

    def get_codebase_snippet(self):
        return self.source.get_codebase_snippet()

    def get_advanced(self, domain=None):
        return self.source.get_advanced(domain)

    def get_question(self):
        return self.source.get_question()

    def pick_object(self) -> tuple[str, str]:
        """Return (name, description) for an object-type stimulus."""
        if random.random() < self.bias:
            picked = self._curiosity_pick_object()
            if picked:
                self._curiosity_hits += 1
                return picked
        self._fallback_hits += 1
        name, desc = self.source.get_object()
        self._recent.append(name)
        return name, desc

    def pick_fact(self, preferred_domain: str | None = None) -> tuple[str, Any]:
        """Return (description, expected) for a fact-type stimulus."""
        if random.random() < self.bias:
            picked = self._curiosity_pick_fact(preferred_domain)
            if picked:
                self._curiosity_hits += 1
                return picked
        self._fallback_hits += 1
        return self.source.get_fact(preferred_domain=preferred_domain)

    def stats(self) -> dict:
        total = self._curiosity_hits + self._fallback_hits
        return {
            "curiosity_hits": self._curiosity_hits,
            "fallback_hits": self._fallback_hits,
            "curiosity_rate": (self._curiosity_hits / total) if total else 0.0,
        }

    # ---- Internals ----

    def _curiosity_pick_object(self) -> tuple[str, str] | None:
        """Ask brain what it wants to learn about, match against source."""
        gaps = self._detect_gaps(topic="object")
        if not gaps:
            return None

        # Try to find an object in the source that matches a gap.
        # Strategy: pick a random gap concept and try source.get_object()
        # with a filter. If source doesn't expose filtering, sample N times
        # and pick the one closest to a gap concept.
        for concept in gaps:
            if concept in self._recent:
                continue
            matched = self._try_match_object(concept)
            if matched:
                self._recent.append(matched[0])
                return matched
        return None

    def _curiosity_pick_fact(self, preferred_domain) -> tuple[str, Any] | None:
        gaps = self._detect_gaps(topic="fact")
        if not gaps:
            return None
        for concept in gaps:
            if concept in self._recent:
                continue
            try:
                desc, expected = self.source.get_fact(
                    preferred_domain=preferred_domain)
                if desc and concept.lower() in desc.lower():
                    self._recent.append(concept)
                    return desc, expected
            except Exception:
                continue
        return None

    def _detect_gaps(self, topic: str) -> list[str]:
        """Query brain's curiosity module for gap concepts."""
        try:
            gaps = self.brain.curiosity_detect_gaps(topic=topic)
        except Exception as e:
            log.debug("curiosity_detect_gaps failed: %s", e)
            return []
        if gaps is None or gaps == {} or gaps == []:
            return []
        # Extract concept names from whatever structure the API returns
        if isinstance(gaps, list):
            concepts = [g if isinstance(g, str) else g.get("concept", "")
                        for g in gaps]
        elif isinstance(gaps, dict):
            concepts = list(gaps.get("gaps", [])) or list(gaps.keys())
        else:
            concepts = []
        return [c for c in concepts if c]

    def _try_match_object(self, concept: str,
                          max_attempts: int = 20) -> tuple[str, str] | None:
        """Sample from source.get_object() up to max_attempts, return first
        match to concept (case-insensitive substring)."""
        concept_lower = concept.lower()
        for _ in range(max_attempts):
            try:
                name, desc = self.source.get_object()
            except Exception:
                return None
            if (concept_lower in name.lower() or
                concept_lower in desc.lower()):
                return name, desc
        return None


def wrap_source(brain, source, bias: float = 0.4) -> CuriositySelector:
    """Factory helper — wraps a source object with a curiosity selector."""
    return CuriositySelector(brain, source, bias=bias)
