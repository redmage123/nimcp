"""Batch-safe R-STDP (reward-modulated STDP).

R-STDP is fundamentally temporal — pre-post spike timing determines
eligibility trace direction; reward modulates the weight update.

Batch-safe strategy: accumulate eligibility across all samples in the
batch (preserving per-sample temporal ordering within its own trace
update), then apply a SINGLE weight update at batch end using the
batch-averaged reward.

This preserves:
  - Temporal causality (trace updated per-sample in order)
  - Biological interpretation (reward modulates consolidated eligibility)

This changes:
  - Frequency of weight updates (once per batch instead of per sample)
  - This is the KEY stability property: fewer weight changes = less
    cross-network gradient amplification
"""
from __future__ import annotations


class BatchRSTDP:
    def __init__(self, n_pre: int, n_post: int,
                 trace_decay: float = 0.9,
                 ltp_rate: float = 0.01,
                 ltd_rate: float = 0.01,
                 learning_rate: float = 0.0005):
        self.n_pre = n_pre
        self.n_post = n_post
        self.trace_decay = trace_decay
        self.ltp = ltp_rate
        self.ltd = ltd_rate
        self.lr = learning_rate
        # Per-synapse eligibility trace e[i][j]
        self.trace = [[0.0] * n_post for _ in range(n_pre)]
        # Weight matrix (small dense for ref; CSR in production)
        self.w = [[0.0] * n_post for _ in range(n_pre)]

    # --- Sequential reference ---

    def step_sequential(self, pre_fired: list[float], post_fired: list[float],
                         reward: float) -> None:
        """One sample: update trace, apply weighted update immediately."""
        for i in range(self.n_pre):
            for j in range(self.n_post):
                # Decay trace
                self.trace[i][j] *= self.trace_decay
                # LTP: pre-before-post (here simplified: pre current, post-was-recent)
                if pre_fired[i] > 0.5 and post_fired[j] > 0.5:
                    self.trace[i][j] += self.ltp
                # LTD: post-before-pre
                elif post_fired[j] > 0.5 and pre_fired[i] < 0.5:
                    self.trace[i][j] -= self.ltd
                # Apply weight update per-sample (sequential style)
                self.w[i][j] += self.lr * reward * self.trace[i][j]

    # --- Batch-safe: mathematically equivalent to sequential ---

    def step_batch(self, pre_fired_batch: list[list[float]],
                    post_fired_batch: list[list[float]],
                    rewards: list[float]) -> None:
        """Batch: EXACTLY equivalent to sequential.

        Sequential sum:
            Δw[i,j] = lr · Σ_b r_b · trace[i,j]_b

        We compute this by accumulating (r_b · trace_b) per-sample into a
        delta buffer, then applying the total delta to weights once at
        the end. No averaging — direct summation preserves exact equivalence.
        """
        B = len(pre_fired_batch)
        if B == 0:
            return

        # Accumulate per-synapse delta over the batch
        delta = [[0.0] * self.n_post for _ in range(self.n_pre)]

        for b in range(B):
            pre_b = pre_fired_batch[b]
            post_b = post_fired_batch[b]
            r_b = rewards[b] if b < len(rewards) else 0.0
            # Update trace per-sample (temporal ordering preserved)
            for i in range(self.n_pre):
                for j in range(self.n_post):
                    self.trace[i][j] *= self.trace_decay
                    if pre_b[i] > 0.5 and post_b[j] > 0.5:
                        self.trace[i][j] += self.ltp
                    elif post_b[j] > 0.5 and pre_b[i] < 0.5:
                        self.trace[i][j] -= self.ltd
                    # Accumulate this sample's contribution to the weight update
                    delta[i][j] += r_b * self.trace[i][j]

        # Apply total delta once at the end of the batch
        for i in range(self.n_pre):
            for j in range(self.n_post):
                self.w[i][j] += self.lr * delta[i][j]

    def snapshot(self) -> tuple[list[list[float]], list[list[float]]]:
        return ([row[:] for row in self.trace],
                [row[:] for row in self.w])

    def restore(self, trace: list[list[float]], w: list[list[float]]) -> None:
        self.trace = [row[:] for row in trace]
        self.w = [row[:] for row in w]
