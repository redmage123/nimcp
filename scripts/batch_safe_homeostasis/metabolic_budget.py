"""Metabolic budget — already stateless, batch-safe by construction.

Caps per-neuron sum-of-absolute-weights. Applied after any weight update.
"""
from __future__ import annotations


class MetabolicBudget:
    def __init__(self, n_neurons: int, cap_per_fan_in: float = 0.8):
        self.n = n_neurons
        self.cap_ratio = cap_per_fan_in

    def apply(self, w: list[list[float]], fan_in: list[int]) -> None:
        """Enforce per-neuron cap in-place.

        Args:
            w: weight matrix w[n][i] — row n is incoming weights to neuron n
            fan_in: fan_in[n] = number of incoming synapses for neuron n
        """
        for n in range(self.n):
            if fan_in[n] == 0:
                continue
            cap = self.cap_ratio * fan_in[n]
            total_abs = sum(abs(x) for x in w[n])
            if total_abs > cap:
                scale = cap / total_abs
                for i in range(len(w[n])):
                    w[n][i] *= scale

    def apply_flat(self, w: list[float], row_ptr: list[int]) -> None:
        """CSR-style: w is flat, row_ptr delimits per-neuron ranges."""
        for n in range(len(row_ptr) - 1):
            lo = row_ptr[n]
            hi = row_ptr[n + 1]
            if hi == lo:
                continue
            cap = self.cap_ratio * (hi - lo)
            total_abs = sum(abs(w[k]) for k in range(lo, hi))
            if total_abs > cap:
                scale = cap / total_abs
                for k in range(lo, hi):
                    w[k] *= scale
