"""Batch-safe inhibitory plasticity.

Standard rule: strengthen I→E weights when both fire together; weaken
when target rate is exceeded.

Sequential per-synapse update:
    Δw[i,j] = -η · (fired[i] · fired[j] - 2·target^2)

Batch form (exact sum of per-sample deltas):
    Δw[i,j] = -η · ( Σ_b fired[i,b]·fired[j,b]  - B · 2·target^2 )
"""
from __future__ import annotations


class InhibitoryPlasticity:
    """Per-synapse update for inhibitory connections.

    Represented as a dense weight matrix for reference clarity. Real
    implementation would use CSR; the math is the same.
    """

    def __init__(self, n_pre: int, n_post: int,
                 eta_inh: float = 0.01,
                 target_rate: float = 0.03):
        self.n_pre = n_pre
        self.n_post = n_post
        self.eta = eta_inh
        self.target = target_rate
        # Weight matrix w[i,j] for pre i → post j; stored as list-of-lists
        self.w = [[0.0] * n_post for _ in range(n_pre)]

    def step_sequential(self, fired_pre: list[float],
                         fired_post: list[float]) -> None:
        target_sq = self.target * self.target
        for i in range(self.n_pre):
            f_i = fired_pre[i]
            for j in range(self.n_post):
                delta = -self.eta * (f_i * fired_post[j] - 2.0 * target_sq)
                self.w[i][j] += delta

    def step_batch(self, fired_pre_batch: list[list[float]],
                    fired_post_batch: list[list[float]]) -> None:
        B = len(fired_pre_batch)
        if B == 0:
            return
        target_sq = self.target * self.target
        for i in range(self.n_pre):
            for j in range(self.n_post):
                # Co-activity: sum over batch
                co = 0.0
                for b in range(B):
                    co += fired_pre_batch[b][i] * fired_post_batch[b][j]
                delta = -self.eta * (co - B * 2.0 * target_sq)
                self.w[i][j] += delta

    def snapshot(self) -> list[list[float]]:
        return [row[:] for row in self.w]

    def restore(self, w: list[list[float]]) -> None:
        self.w = [row[:] for row in w]
