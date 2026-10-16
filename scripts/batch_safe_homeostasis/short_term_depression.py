"""Batch-safe short-term synaptic depression.

Each fire boosts depression by a fixed jump; between fires, depression
decays multiplicatively.

Sequential: depression[n] = 0.95 * depression[n] + 0.2 * fired[n]
Batch: direct vectorization preserving exact recurrence.
"""
from __future__ import annotations


class ShortTermDepression:
    def __init__(self, n_neurons: int,
                 decay: float = 0.95,
                 jump: float = 0.2,
                 cap: float = 0.5):
        self.n = n_neurons
        self.decay = decay
        self.jump = jump
        self.cap = cap
        self.depression = [0.0] * n_neurons

    def step_sequential(self, fired: list[float]) -> None:
        for n in range(self.n):
            self.depression[n] = (self.decay * self.depression[n]
                                    + self.jump * fired[n])
            if self.depression[n] > self.cap:
                self.depression[n] = self.cap

    def step_batch(self, fired_batch: list[list[float]]) -> None:
        """Batch update with cap-aware fallback.

        The cap is non-linear (min(x, cap)). Pure batched recurrence gives
        the UNCAPPED trajectory, which diverges from sequential when the
        cap activates mid-batch. We compute the batched value, and for any
        neuron where the result would hit the cap, fall back to exact
        sequential iteration to preserve equivalence.
        """
        B = len(fired_batch)
        if B == 0:
            return
        decay_B = self.decay ** B
        powers = [self.decay ** (B - 1 - b) for b in range(B)]

        for n in range(self.n):
            # Compute batched (uncapped) result
            jump_sum = 0.0
            for b in range(B):
                jump_sum += self.jump * powers[b] * fired_batch[b][n]
            uncapped = decay_B * self.depression[n] + jump_sum

            # If result is well under cap and initial was under cap,
            # batched formula is safe (capped branch never triggers
            # in any intermediate step).
            #
            # Check if cap would have been crossed: compute max possible
            # intermediate value = initial * decay + jump (single jump at
            # start of batch). If this > cap, iterate sequentially.
            max_intermediate = self.depression[n]
            capped_during_batch = False
            if uncapped > self.cap:
                capped_during_batch = True
            else:
                # Even if final < cap, intermediate might cross. Check by
                # walking through samples.
                cur = self.depression[n]
                for b in range(B):
                    cur = self.decay * cur + self.jump * fired_batch[b][n]
                    if cur > self.cap:
                        capped_during_batch = True
                        break
                if not capped_during_batch:
                    # Batched formula safe — use it
                    self.depression[n] = uncapped
                    continue

            # Fallback: iterate sequentially to get correct capping behavior
            cur = self.depression[n]
            for b in range(B):
                cur = self.decay * cur + self.jump * fired_batch[b][n]
                if cur > self.cap:
                    cur = self.cap
            self.depression[n] = cur

    def snapshot(self) -> list[float]:
        return list(self.depression)

    def restore(self, depression: list[float]) -> None:
        if len(depression) != self.n:
            raise ValueError(f"size mismatch: {len(depression)} vs {self.n}")
        self.depression = list(depression)
