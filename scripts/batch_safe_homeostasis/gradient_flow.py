"""Batch-safe cross-network gradient budget.

Instead of per-network independent clipping (which produces
cross-network amplification under batching), enforce a global norm
budget across all networks simultaneously.

Preserves relative magnitudes between networks (no one network dominates)
while capping total gradient norm.
"""
from __future__ import annotations

import math


class GlobalGradientBudget:
    def __init__(self, budget: float = 1.0,
                 per_network_cap: float | None = None):
        """
        Args:
            budget: total gradient L2-norm budget across all networks
            per_network_cap: optional max-norm per individual network
                (safety net to prevent any single network from
                dominating even within the global budget)
        """
        self.budget = budget
        self.per_network_cap = per_network_cap

    def _norm(self, g: list[float]) -> float:
        return math.sqrt(sum(x * x for x in g))

    def clip_sequential(self, per_network_grads: dict[str, list[float]]) -> dict[str, list[float]]:
        """Independent per-network clip (legacy behavior)."""
        out = {}
        for name, g in per_network_grads.items():
            n = self._norm(g)
            scale = min(1.0, self.budget / max(n, 1e-12))
            out[name] = [x * scale for x in g]
        return out

    def clip_global(self, per_network_grads: dict[str, list[float]]) -> dict[str, list[float]]:
        """Global norm clip.

        Treats all networks' gradients as one concatenated vector;
        clips total L2 norm to `budget`. Preserves relative magnitudes.
        """
        # Compute total norm across all networks
        total_sq = 0.0
        for g in per_network_grads.values():
            for x in g:
                total_sq += x * x
        total = math.sqrt(total_sq)
        global_scale = 1.0 if total <= self.budget else (self.budget / max(total, 1e-12))

        out = {}
        for name, g in per_network_grads.items():
            scaled = [x * global_scale for x in g]
            # Per-network safety cap
            if self.per_network_cap is not None:
                n = self._norm(scaled)
                if n > self.per_network_cap:
                    per_scale = self.per_network_cap / max(n, 1e-12)
                    scaled = [x * per_scale for x in scaled]
            out[name] = scaled
        return out
