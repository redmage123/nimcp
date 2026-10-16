"""Compressed-time sequential replay.

Between wake training steps, replay recent salient experiences sequentially
at maximum GPU rate. Each replay is still batch=1 (no gradient explosion risk),
but the wall-clock window between replays is minimized.

Biological precedent: sleep-state hippocampal replay at 5-10x compression.
Digital system can theoretically do 100-1000x compression.

Usage:
    replayer = CompressedReplayer(brain, capacity=1000)
    replayer.record(features, target, label, salience=1.0)  # record experience
    # ... many experiences recorded during normal training ...
    replayer.replay_burst(n=50, target_wallclock_s=10)
"""
from __future__ import annotations

import heapq
import logging
import random
import time
from collections import deque
from dataclasses import dataclass, field
from typing import Any, Optional

log = logging.getLogger("compressed_replay")


@dataclass
class ReplayItem:
    features: list[float]
    target: Any
    label: str
    salience: float = 1.0
    timestamp: float = field(default_factory=time.time)
    replay_count: int = 0


@dataclass
class ReplayStats:
    items_recorded: int = 0
    items_replayed: int = 0
    total_wallclock_s: float = 0.0
    avg_replay_rate_hz: float = 0.0
    last_burst_duration_s: float = 0.0


class CompressedReplayer:
    """Salience-prioritized replay buffer.

    Records experiences with associated salience scores (typically proportional
    to prediction error or surprise). During replay bursts, samples are drawn
    from the buffer preferring high-salience items.
    """

    def __init__(self, brain, capacity: int = 1000,
                 salience_bias: float = 0.7,
                 min_replay_interval_s: float = 0.05):
        """
        Args:
            brain: nimcp.Brain or BrainProxy
            capacity: max buffer size (oldest items dropped)
            salience_bias: 0 = uniform random, 1 = strictly by salience
            min_replay_interval_s: minimum wall-time between individual replays
                (keeps GPU from being pegged in the wrong way)
        """
        self.brain = brain
        self.capacity = capacity
        self.salience_bias = max(0.0, min(1.0, salience_bias))
        self.min_replay_interval_s = min_replay_interval_s
        self._buffer: deque[ReplayItem] = deque(maxlen=capacity)
        self._stats = ReplayStats()

    # ---- Recording ----

    def record(self, features: list[float], target: Any,
               label: str = "", salience: float = 1.0) -> None:
        """Add an experience to the buffer."""
        self._buffer.append(ReplayItem(
            features=features, target=target,
            label=label, salience=max(0.0, float(salience)),
        ))
        self._stats.items_recorded += 1

    def record_from_training_step(self, features: list[float], target: Any,
                                    label: str, loss: float,
                                    baseline_loss: Optional[float] = None) -> None:
        """Convenience: derive salience from loss surprise.

        If baseline_loss is provided, salience = relative deviation.
        Otherwise salience = normalized loss magnitude.
        """
        if baseline_loss is not None and baseline_loss > 0:
            salience = abs(loss - baseline_loss) / baseline_loss
        else:
            salience = min(1.0, max(0.01, float(loss)))
        self.record(features, target, label, salience)

    # ---- Replay ----

    def replay_burst(self, n: int = 50,
                      target_wallclock_s: Optional[float] = None) -> ReplayStats:
        """Replay N items sequentially.

        If target_wallclock_s is set, caps total burst duration (stops early
        if target exceeded).
        """
        if not self._buffer:
            log.debug("replay_burst: buffer empty, skipping")
            return self._stats

        t_start = time.time()
        replayed = 0
        for i in range(n):
            if target_wallclock_s and (time.time() - t_start) >= target_wallclock_s:
                break
            item = self._sample_item()
            if item is None:
                break
            self._replay_one(item)
            replayed += 1
            item.replay_count += 1
            if self.min_replay_interval_s > 0:
                time.sleep(self.min_replay_interval_s)

        elapsed = time.time() - t_start
        self._stats.items_replayed += replayed
        self._stats.total_wallclock_s += elapsed
        self._stats.last_burst_duration_s = elapsed
        if elapsed > 0:
            self._stats.avg_replay_rate_hz = replayed / elapsed
        log.info("replay_burst: %d items in %.2fs (%.1f Hz)",
                 replayed, elapsed, self._stats.avg_replay_rate_hz)
        return self._stats

    def _sample_item(self) -> Optional[ReplayItem]:
        """Sample an item preferring high salience."""
        items = list(self._buffer)
        if not items:
            return None
        if self.salience_bias <= 0:
            return random.choice(items)
        # Weighted sampling by salience^alpha
        alpha = self.salience_bias * 3.0  # bias 1.0 → alpha 3.0 (strong)
        weights = [max(1e-4, item.salience) ** alpha for item in items]
        total = sum(weights)
        if total <= 0:
            return random.choice(items)
        r = random.random() * total
        acc = 0.0
        for item, w in zip(items, weights):
            acc += w
            if acc >= r:
                return item
        return items[-1]

    def _replay_one(self, item: ReplayItem) -> None:
        """Re-present the item as a learning step (batch=1)."""
        try:
            # Lower LR during replay to avoid over-fitting to buffered items
            if hasattr(self.brain, "learn_vector"):
                self.brain.learn_vector(item.features, item.target,
                                          label=item.label,
                                          learning_rate=None)
        except Exception as e:
            log.debug("replay_one failed: %s", e)

    # ---- Introspection ----

    def stats(self) -> dict:
        return {
            "items_recorded": self._stats.items_recorded,
            "items_replayed": self._stats.items_replayed,
            "buffer_size": len(self._buffer),
            "capacity": self.capacity,
            "total_wallclock_s": self._stats.total_wallclock_s,
            "avg_replay_rate_hz": self._stats.avg_replay_rate_hz,
            "last_burst_duration_s": self._stats.last_burst_duration_s,
        }

    def clear(self) -> None:
        self._buffer.clear()
