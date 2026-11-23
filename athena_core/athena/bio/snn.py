"""Spiking primitives: rate-coded Poisson input and leaky integrate-and-fire
dynamics in discrete time (1 step = 1 ms)."""
from __future__ import annotations

import math

import torch


def poisson(x: torch.Tensor, steps: int, rate: float, gen: torch.Generator | None = None) -> torch.Tensor:
    """Spike trains for intensities x in [0, 1]: (steps, *x.shape) of 0/1,
    each step firing with probability x * rate."""
    p = (x.clamp(0, 1) * rate).expand(steps, *x.shape)
    return (torch.rand(p.shape, generator=gen) < p).float()


def decay(tau: float) -> float:
    """Per-step multiplier of a trace or membrane with time constant tau steps."""
    return math.exp(-1.0 / tau)
