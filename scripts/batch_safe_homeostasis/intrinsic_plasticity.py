"""Batch-safe intrinsic plasticity (IP threshold adaptation).

Each time a neuron fires, adjust its threshold slightly based on the
deviation of its rate EMA from the target. This drives the rate toward
the target over a long timescale.

Key design choice: because rate_ema is slow-changing (τ ≈ 100), using the
batch-averaged rate_ema gives an update that is approximately equal to
applying each sample's update sequentially, as long as B << τ.

Sequential: per-fire update with current rate_ema
Batch: sum-of-fires × IP update using batch-avg rate_ema

Exact equivalence for B=1; approximate (within 1/B·τ · rate_change) for
B > 1.
"""
from __future__ import annotations


class IntrinsicPlasticity:
    def __init__(self, n_neurons: int,
                 eta_ip: float = 0.5,
                 target_rate: float = 0.03,
                 delta_max: float = 0.010):
        """
        Args:
            delta_max: maximum absolute threshold offset (volts)
        """
        self.n = n_neurons
        self.eta = eta_ip
        self.target = target_rate
        self.delta_max = delta_max
        self.threshold_offset = [0.0] * n_neurons

    def step_sequential(self, fired: list[float], rate_ema: list[float]) -> None:
        """One sample: per-neuron per-fire IP update."""
        for n in range(self.n):
            if fired[n] > 0.5:  # fired
                err = rate_ema[n] - self.target
                self.threshold_offset[n] += self.eta * err
                # Clip
                if self.threshold_offset[n] > self.delta_max:
                    self.threshold_offset[n] = self.delta_max
                elif self.threshold_offset[n] < -self.delta_max:
                    self.threshold_offset[n] = -self.delta_max

    def step_batch(self, fired_batch: list[list[float]],
                    rate_ema_batch: list[list[float]]) -> None:
        """Batch update.

        Args:
            fired_batch[b][n]: 1 if neuron n fired on sample b
            rate_ema_batch[b][n]: rate_ema at each sample b within the batch
        """
        B = len(fired_batch)
        if B == 0:
            return
        for n in range(self.n):
            # Count fires
            n_fires = 0
            for b in range(B):
                if fired_batch[b][n] > 0.5:
                    n_fires += 1
            if n_fires == 0:
                continue
            # Average rate_ema over the batch (or use current if single rate provided)
            if isinstance(rate_ema_batch[0], list):
                avg_rate = sum(rate_ema_batch[b][n] for b in range(B)) / B
            else:
                # If caller passed a single rate_ema vector (valid for small batches)
                avg_rate = rate_ema_batch[n]
            err = avg_rate - self.target
            delta = n_fires * self.eta * err
            self.threshold_offset[n] += delta
            if self.threshold_offset[n] > self.delta_max:
                self.threshold_offset[n] = self.delta_max
            elif self.threshold_offset[n] < -self.delta_max:
                self.threshold_offset[n] = -self.delta_max

    def snapshot(self) -> list[float]:
        return list(self.threshold_offset)

    def restore(self, threshold_offset: list[float]) -> None:
        if len(threshold_offset) != self.n:
            raise ValueError(f"size mismatch: {len(threshold_offset)} vs {self.n}")
        self.threshold_offset = list(threshold_offset)
