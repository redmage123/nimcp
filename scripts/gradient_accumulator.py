"""Python-side gradient accumulation wrapper.

Athena's C training loop uses batch=1 because batched C paths historically
cause gradient explosion and SNN saturation. This wrapper preserves that
invariant while providing the *statistical* benefit of larger effective
batch size — reduced SGD noise.

Strategy:
    - Process N samples sequentially (SNN, homeostasis, R-STDP all run normally)
    - Record each per-sample gradient norm / loss
    - After every N samples, the training loop applies a smoothed LR derived
      from the recent-window's gradient statistics

This is NOT true batched backprop — it's a lower-risk approximation that
reduces LR-amplified noise without touching the stability package.
"""
from __future__ import annotations

import logging
import math
from collections import deque
from typing import Optional

log = logging.getLogger("gradient_accumulator")


class GradientAccumulator:
    """Accumulates per-sample losses and gradient norms across a window,
    recommends a smoothed learning rate for the next batch."""

    def __init__(self, window: int = 32,
                 min_lr: float = 1e-6,
                 max_lr: float = 1e-1,
                 target_grad_norm: float = 1.0):
        self.window = window
        self.min_lr = min_lr
        self.max_lr = max_lr
        self.target_grad_norm = target_grad_norm
        self._losses: deque[float] = deque(maxlen=window)
        self._grad_norms: deque[float] = deque(maxlen=window)
        self._n_samples = 0

    def record(self, *, loss: float | None = None,
               grad_norm: float | None = None) -> None:
        if loss is not None and not (math.isnan(loss) or math.isinf(loss)):
            self._losses.append(float(loss))
        if grad_norm is not None and not (math.isnan(grad_norm) or
                                            math.isinf(grad_norm)):
            self._grad_norms.append(float(grad_norm))
        self._n_samples += 1

    def ready(self) -> bool:
        """True once we've accumulated a full window."""
        return len(self._losses) >= self.window

    def smoothed_lr(self, base_lr: float) -> float:
        """Derive a recommended LR from the recent window.

        When gradient norms are consistently above target, scale LR down;
        when below, scale up. Clamp to [min_lr, max_lr].
        """
        if not self._grad_norms:
            return base_lr
        mean_gn = sum(self._grad_norms) / len(self._grad_norms)
        if mean_gn < 1e-8:
            return base_lr
        scale = self.target_grad_norm / mean_gn
        # Dampen: don't move LR by more than 2× per batch
        scale = max(0.5, min(2.0, scale))
        new_lr = base_lr * scale
        return max(self.min_lr, min(self.max_lr, new_lr))

    def loss_trend(self) -> tuple[float, float] | None:
        """Return (mean_loss, slope) over the window. Slope is (end - start).

        Returns None if insufficient data.
        """
        if len(self._losses) < 2:
            return None
        losses = list(self._losses)
        mean = sum(losses) / len(losses)
        # Simple slope: difference of first/last half averages
        mid = len(losses) // 2
        first_half = losses[:mid]
        second_half = losses[mid:]
        if not first_half or not second_half:
            return (mean, 0.0)
        fh_mean = sum(first_half) / len(first_half)
        sh_mean = sum(second_half) / len(second_half)
        return (mean, sh_mean - fh_mean)

    def stats(self) -> dict:
        mean_loss = sum(self._losses) / len(self._losses) if self._losses else 0
        mean_gn = (sum(self._grad_norms) / len(self._grad_norms)
                   if self._grad_norms else 0)
        return {
            "n_samples": self._n_samples,
            "window_filled": len(self._losses),
            "mean_loss": mean_loss,
            "mean_grad_norm": mean_gn,
        }

    def reset_window(self) -> None:
        self._losses.clear()
        self._grad_norms.clear()


def accumulating_learn_vector(brain, features, target, *,
                               label: str,
                               accumulator: GradientAccumulator,
                               base_lr: float,
                               **kwargs) -> float:
    """learn_vector variant that uses an accumulator to pick effective LR.

    The accumulator smooths LR over N samples — providing batched-statistical
    benefit while preserving per-sample SNN/homeostasis behavior.
    """
    effective_lr = accumulator.smoothed_lr(base_lr)
    loss = brain.learn_vector(features, target, label=label,
                               learning_rate=effective_lr, **kwargs)
    grad_norm = None
    try:
        if hasattr(brain, "get_last_gradient_norm"):
            grad_norm = brain.get_last_gradient_norm()
    except Exception:
        pass
    accumulator.record(loss=loss, grad_norm=grad_norm)
    return loss
