"""Complementary learning systems on rung 1 (THEORY.md, algorithms 2 + 3).

The hippocampus (Phase B1 `Hippocampus`) stores each labelled episode in one
exposure as a sparse, pattern-separated dentate code. A slow spiking learner
(`EpropSNN`, algorithm 2) then learns from those stored codes by replay:
the episodes are re-presented offline many times, so each experience trains
the slow learner repeatedly. The hippocampus keeps answering by recall.

Decision: the two systems vote with their confidence. The hippocampus
answers from the word neurons its code activates most; the slow learner
from its readout. Each prediction is a probability vector; the combined
answer is their weighted sum (weight `w_cortex` on the slow learner).
"""
from __future__ import annotations

import torch

from athena.bio.eprop import EpropConfig, EpropSNN
from athena.bio.learners import Hippocampus


class CLS:
    def __init__(self, n_in: int, n_classes: int, seed: int = 0, n_dg: int = 8000,
                 k_frac: float = 0.1, fan_in: float = 0.25, steps: int = 1000,
                 eprop: EpropConfig | None = None, w_cortex: float = 0.5):
        self.hc = Hippocampus(n_in, n_classes, n_dg=n_dg, k_frac=k_frac, fan_in=fan_in,
                              steps=steps, seed=seed)
        self.cortex = EpropSNN(n_dg, n_classes, eprop or EpropConfig(n_rec=256, seed=seed, rate=0.3))
        self.n_classes, self.w_cortex = n_classes, w_cortex

    def learn(self, X: torch.Tensor, y: torch.Tensor, replays: int = 30, batch: int = 8) -> None:
        codes = self.hc.code(X)
        self.hc.store(codes, y)                       # one exposure
        self.codes, self.y = codes, y                 # the stored episodes
        self.cortex.fit(codes, y, epochs=replays, batch=batch)   # offline replay

    def _hc_probs(self, codes: torch.Tensor) -> torch.Tensor:
        drive = codes @ self.hc.A.T                   # word-neuron input from the code
        return torch.softmax(drive / drive.std(1, keepdim=True).clamp_min(1e-6), dim=1)

    @torch.no_grad()
    def predict(self, X: torch.Tensor, repeats: int = 3) -> torch.Tensor:
        codes = self.hc.code(X)
        p_hc = self._hc_probs(codes)
        p_cx = torch.zeros(len(X), self.n_classes)
        for s in range(0, len(X), 256):
            p_cx[s:s + 256] = sum(self.cortex.run(self.cortex.encode(codes[s:s + 256]))[0]
                                  for _ in range(repeats)) / repeats
        return ((1 - self.w_cortex) * p_hc + self.w_cortex * p_cx).argmax(1)
