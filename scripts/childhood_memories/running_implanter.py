"""Running implanter — consolidates training experience into durable memory.

Training data IS the memory source. As the brain learns, each exposure
(stimulus, label, loss, reward) becomes a potential memory trace. The
running implanter:

  1. Observes each learn_vector call
  2. Scores events by salience (surprise ≈ |loss − running_mean|, plus
     reward signal if available)
  3. Buffers recent events
  4. Periodically consolidates top-N salient events into brain memory
     stores (semantic / KG / hippocampal episodic)
  5. Deduplicates — same label implanted recently → skip
  6. Runs throughout ALL stages, not just at init

This mirrors biological offline consolidation: not every experience
becomes a durable memory; salient/surprising ones preferentially do.

Usage:
    implanter = RunningImplanter(brain)
    # In training loop, after each learn_vector:
    implanter.observe(label="dog", description="a furry animal",
                      modality="visual", loss=0.3, baseline_loss=1.0)
    # Every N steps:
    n_consolidated = implanter.consolidate_batch(top_k=10)
"""
from __future__ import annotations

import logging
import time
from collections import deque, defaultdict
from dataclasses import dataclass, field
from typing import Any, Optional

log = logging.getLogger("childhood_memories.running_implanter")


@dataclass
class TrainingEvent:
    """One learning event captured for potential memory consolidation."""
    timestamp: float
    label: str
    description: str
    modality: str
    loss: float
    salience: float
    valence: float = 0.0      # reward signal if available
    consolidated: bool = False


class RunningImplanter:
    """Observes training, consolidates salient events into brain memory.

    Design:
      - Unbounded observation rate (every learn_vector)
      - Bounded buffer (keeps most recent N events)
      - Consolidation is periodic (caller controls cadence)
      - Deduplication by label within a rolling window

    The brain need not expose any specific memory API — each write is
    best-effort via existing `symbolic_writer` patterns.
    """

    def __init__(self, brain, *,
                 buffer_capacity: int = 2000,
                 min_salience_for_consolidation: float = 0.15,
                 dedup_window_s: float = 3600.0,
                 verbose: bool = False):
        self.brain = brain
        self.buffer: deque[TrainingEvent] = deque(maxlen=buffer_capacity)
        self.min_salience = min_salience_for_consolidation
        self.dedup_window_s = dedup_window_s
        self.verbose = verbose

        # Tracking
        self._recent_loss_window: deque[float] = deque(maxlen=100)
        self._last_implanted_at: dict[str, float] = {}
        self._n_observed = 0
        self._n_consolidated = 0
        self._n_skipped_dedup = 0
        self._n_skipped_salience = 0

    # ---- Observation (called every learn step) ----

    def observe(self, *, label: str, description: str = "",
                modality: str = "text",
                loss: float = 0.0,
                baseline_loss: Optional[float] = None,
                reward: Optional[float] = None) -> None:
        """Record one learning event. Cheap — just buffers + tracks."""
        self._n_observed += 1
        if loss is not None and loss >= 0:
            self._recent_loss_window.append(float(loss))

        salience = self._compute_salience(loss, baseline_loss)
        valence = float(reward) if reward is not None else 0.0

        self.buffer.append(TrainingEvent(
            timestamp=time.time(),
            label=label,
            description=description,
            modality=modality,
            loss=float(loss) if loss is not None else 0.0,
            salience=salience,
            valence=valence,
        ))

    def _compute_salience(self, loss: Optional[float],
                           baseline: Optional[float]) -> float:
        """Salience = relative surprise. Higher = more memory-worthy."""
        if loss is None or loss < 0:
            return 0.0
        if baseline is None:
            if not self._recent_loss_window:
                return min(1.0, float(loss))
            baseline = sum(self._recent_loss_window) / len(self._recent_loss_window)
        if baseline <= 1e-6:
            return min(1.0, float(loss))
        rel = abs(float(loss) - float(baseline)) / (float(baseline) + 1e-6)
        return max(0.0, min(1.0, rel))

    # ---- Consolidation (called periodically) ----

    def consolidate_batch(self, top_k: int = 10) -> int:
        """Consolidate top-k salient events from buffer into brain memory.

        Returns the number of events actually consolidated (excluding
        dedup and salience skips).
        """
        if not self.buffer:
            return 0

        # Score: salience × (1 + |valence|)
        candidates = list(self.buffer)
        scored = sorted(
            ((e, e.salience * (1.0 + abs(e.valence))) for e in candidates
             if not e.consolidated),
            key=lambda x: -x[1],
        )
        if not scored:
            return 0

        now = time.time()
        n = 0
        for event, score in scored[:top_k * 3]:  # over-consider to account for dedup/salience
            if n >= top_k:
                break
            if event.salience < self.min_salience:
                self._n_skipped_salience += 1
                continue
            # Dedup
            last_at = self._last_implanted_at.get(event.label, 0)
            if now - last_at < self.dedup_window_s:
                self._n_skipped_dedup += 1
                continue
            if self._consolidate_one(event):
                event.consolidated = True
                self._last_implanted_at[event.label] = now
                self._n_consolidated += 1
                n += 1
        return n

    def _consolidate_one(self, event: TrainingEvent) -> bool:
        """Write one event to brain memory stores (best-effort)."""
        wrote_any = False

        # Semantic memory — concept node
        try:
            if hasattr(self.brain, "semantic_memory_insert"):
                self.brain.semantic_memory_insert(
                    concept=event.label,
                    attributes={
                        "description": event.description,
                        "first_observed_modality": event.modality,
                        "valence": event.valence,
                    })
                wrote_any = True
        except Exception as e:
            if self.verbose:
                log.debug("semantic_memory_insert %s failed: %s", event.label, e)

        # Knowledge graph — fact
        try:
            if hasattr(self.brain, "kg_add_fact"):
                self.brain.kg_add_fact(
                    subject=event.label,
                    predicate="experienced_as",
                    object_=event.modality,
                    confidence=min(1.0, 0.5 + event.salience * 0.5))
                wrote_any = True
            elif hasattr(self.brain, "ti_add_fact"):
                self.brain.ti_add_fact(event.label, "experienced_as", event.modality)
                wrote_any = True
        except Exception as e:
            if self.verbose:
                log.debug("kg_add_fact %s failed: %s", event.label, e)

        # Hippocampal episode
        try:
            if hasattr(self.brain, "hippocampus_seed_episode"):
                narrative = (event.description
                              or f"experienced {event.label}")
                self.brain.hippocampus_seed_episode(
                    text=narrative,
                    valence=event.valence,
                    timestamp=event.timestamp,
                    modality=event.modality)
                wrote_any = True
        except Exception as e:
            if self.verbose:
                log.debug("hippocampus_seed_episode %s failed: %s",
                          event.label, e)

        return wrote_any

    # ---- Introspection ----

    def stats(self) -> dict:
        return {
            "observed": self._n_observed,
            "consolidated": self._n_consolidated,
            "skipped_dedup": self._n_skipped_dedup,
            "skipped_salience": self._n_skipped_salience,
            "buffer_size": len(self.buffer),
            "unique_labels_implanted": len(self._last_implanted_at),
        }

    def set_stage(self, stage: int) -> None:
        """Called by training loop at stage boundary. Widens dedup window
        so later stages can re-visit earlier labels under a different context."""
        # Simple heuristic: shorter dedup in early stages (fast churn),
        # longer in later stages (more durable memories)
        if stage <= 1:
            self.dedup_window_s = 600.0    # 10 min
        elif stage == 2:
            self.dedup_window_s = 1800.0   # 30 min
        else:
            self.dedup_window_s = 3600.0   # 1 hour

    def reset(self) -> None:
        self.buffer.clear()
        self._recent_loss_window.clear()
        self._last_implanted_at.clear()
        self._n_observed = 0
        self._n_consolidated = 0
        self._n_skipped_dedup = 0
        self._n_skipped_salience = 0
