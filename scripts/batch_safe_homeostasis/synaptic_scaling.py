"""Batch-safe synaptic scaling (Turrigiano homeostasis).

Maintains per-neuron firing-rate EMA, applies weight scaling when the
population mean drifts from target.

Sequential recurrence (batch=1):
    rate_ema[n] ← α · rate_ema[n] + (1-α) · fired[n]

Batch recurrence (batch=B):
    α_B = α^B
    rate_ema[n] ← α_B · rate_ema[n] + Σ_{b=0..B-1} (1-α) · α^(B-1-b) · fired[n, b]

Key property: batch=1 reduces exactly to sequential; batch=N produces
the same rate_ema trajectory as N sequential applications (modulo
floating-point reassociation).
"""
from __future__ import annotations


class SynapticScaling:
    def __init__(self, n_neurons: int, tau_rate: float = 100.0,
                 target_rate: float = 0.03,
                 scale_eta: float = 0.01):
        import math
        self.n = n_neurons
        self.alpha = math.exp(-1.0 / tau_rate)
        self.target = target_rate
        self.eta = scale_eta
        # Seeded at target (not zero) to avoid spurious scaling at startup
        self.rate_ema = [target_rate] * n_neurons

    # --- Sequential (reference) ---

    def step_sequential(self, fired: list[float]) -> None:
        """One sample: update EMA per neuron."""
        for n in range(self.n):
            self.rate_ema[n] = (self.alpha * self.rate_ema[n]
                                 + (1.0 - self.alpha) * fired[n])

    # --- Batched (equivalent) ---

    def step_batch(self, fired_batch: list[list[float]]) -> None:
        """Batch of samples. fired_batch[b][n] ∈ {0, 1}."""
        B = len(fired_batch)
        if B == 0:
            return
        alpha = self.alpha
        one_minus_alpha = 1.0 - alpha
        alpha_B = alpha ** B

        # Precompute powers alpha^(B-1-b) for each b
        # alpha^(B-1) ... alpha^0
        powers = [alpha ** (B - 1 - b) for b in range(B)]

        for n in range(self.n):
            # accumulated contribution from batch
            contrib = 0.0
            for b in range(B):
                contrib += one_minus_alpha * powers[b] * fired_batch[b][n]
            self.rate_ema[n] = alpha_B * self.rate_ema[n] + contrib

    # --- Scaling decision (stateless — always identical) ---

    def scale_factor(self) -> float:
        """Compute the weight-scale factor from current EMAs.

        Returns multiplier (1.0 means no change).
        """
        mean_ema = sum(self.rate_ema) / max(1, self.n)
        err = self.target - mean_ema
        return 1.0 + self.eta * err

    def snapshot(self) -> list[float]:
        """Return a copy of rate_ema for comparison."""
        return list(self.rate_ema)

    def restore(self, rate_ema: list[float]) -> None:
        if len(rate_ema) != self.n:
            raise ValueError(f"size mismatch: {len(rate_ema)} vs {self.n}")
        self.rate_ema = list(rate_ema)
