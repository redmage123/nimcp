"""Progressive curriculum — start with few categories, expand gradually.

A child learns about 5 objects first, then 10, then 20. Presenting the full
variety immediately is the hardest possible case. This module wraps a source
with a curriculum that expands categorical scope with training progress.
"""
from __future__ import annotations

import logging
import random
from typing import Any, Callable

log = logging.getLogger("curriculum")


class ProgressiveCurriculum:
    """Expand stimulus scope from narrow-to-broad across training.

    Stages (configurable):
        steps 0-200:    5 categories
        steps 200-500:  10 categories
        steps 500-1000: 20 categories
        steps 1000+:    full variety

    Wraps a source.get_object(). When the source's category list is
    introspectable, filters by current-window categories; else samples
    with rejection until a matching category appears.
    """

    DEFAULT_STAGES: list[tuple[int, int]] = [
        # (end_step_exclusive, n_categories)
        (200, 5),
        (500, 10),
        (1000, 20),
        (10 ** 9, 10 ** 9),  # unbounded past 1000
    ]

    def __init__(self, source, stages: list[tuple[int, int]] | None = None,
                 category_extractor: Callable | None = None):
        self.source = source
        self.stages = stages or self.DEFAULT_STAGES
        self.step = 0
        self._allowed: set[str] | None = None
        self._category_extractor = (
            category_extractor or (lambda name, desc: name.split()[0].lower()
                                    if name else ""))
        self._rejected = 0
        self._presented = 0

    # ---- Public API ----

    def advance(self, step: int | None = None) -> int:
        """Inform curriculum of current step; returns active category count."""
        if step is not None:
            self.step = step
        else:
            self.step += 1
        return self._current_n()

    def get_object(self) -> tuple[str, str]:
        """Alias for pick_object() so ProgressiveCurriculum can substitute
        for a raw source (which uses .get_object()) in chained wrappings."""
        return self.pick_object()

    def get_fact(self, preferred_domain=None) -> tuple:
        """Pass-through for fact queries — curriculum does not filter facts."""
        if hasattr(self.source, "get_fact"):
            return self.source.get_fact(preferred_domain=preferred_domain)
        return ("", None)

    def get_sensory(self):
        """Pass-through for sensory queries — curriculum does not filter these."""
        return self.source.get_sensory()

    def get_codebase_snippet(self):
        return self.source.get_codebase_snippet()

    def get_advanced(self, domain=None):
        return self.source.get_advanced(domain)

    def get_question(self):
        return self.source.get_question()

    def pick_object(self, max_reject: int = 50) -> tuple[str, str]:
        """Pick an object, filtering to currently-allowed categories."""
        n_allowed = self._current_n()
        if n_allowed >= 10 ** 8:
            # Past all stages — unrestricted
            self._presented += 1
            return self.source.get_object()

        for _ in range(max_reject):
            name, desc = self.source.get_object()
            cat = self._category_extractor(name, desc)
            if self._allowed is None:
                # Lazily build allowed set from first N unique categories
                self._allowed = {cat}
                self._presented += 1
                return name, desc
            if cat in self._allowed:
                self._presented += 1
                return name, desc
            # Try to grow the allowed set if we haven't hit the cap
            if len(self._allowed) < n_allowed:
                self._allowed.add(cat)
                self._presented += 1
                return name, desc
            self._rejected += 1
        # Fallback after too many rejections — accept whatever
        name, desc = self.source.get_object()
        self._presented += 1
        return name, desc

    def stats(self) -> dict:
        return {
            "step": self.step,
            "active_n_categories": self._current_n(),
            "allowed_set_size": len(self._allowed or []),
            "presented": self._presented,
            "rejected": self._rejected,
        }

    # ---- Internals ----

    def _current_n(self) -> int:
        for end_step, n in self.stages:
            if self.step < end_step:
                return n
        return 10 ** 9  # fallback: unlimited


def wrap_source(source, stages=None) -> ProgressiveCurriculum:
    return ProgressiveCurriculum(source, stages=stages)
