"""E-prop: a recurrent spiking network trained by a local three-factor rule.

After Bellec et al. (2020), "A solution to the learning dilemma for recurrent
networks of spiking neurons", Nature Communications 11:3625.

Network (1 step = 1 ms)
  input      Poisson spike trains x(t) from intensities in [0, 1]
  recurrent  n LIF neurons:  v_j(t) = a v_j(t-1) + sum_i W_in[i,j] x_i(t)
                                      + sum_i W_rec[i,j] z_i(t-1) - z_j(t-1) th
             z_j(t) = 1 if v_j(t) > th (reset by subtraction next step)
  readout    leaky output neurons: y_k(t) = k y_k(t-1) + sum_j W_out[j,k] z_j(t) + b_k
             pi(t) = softmax(y(t)); the decision is the mean pi over the last
             `read_steps` steps.

Learning (no backpropagation, no autograd; everything a synapse uses is local
to it or broadcast to its neuron)
  eligibility trace of synapse i->j:
      eps_i(t) = a eps_i(t-1) + pre_i(t)               (presynaptic, local)
      e_ij(t)  = psi_j(t) eps_i(t)                     (psi: postsynaptic
                 pseudo-derivative gamma * max(0, 1 - |v_j - th| / th) / th)
      ebar_ij(t) = k ebar_ij(t-1) + e_ij(t)            (filtered like the readout)
  learning signal of neuron j (third factor, broadcast):
      L_j(t) = sum_k B[k,j] (pi_k(t) - target_k)       B fixed and random
                                                       ("random e-prop": no
                                                       weight transport)
  update:  dW_ij = -lr * sum_t L_j(t) ebar_ij(t)       (input and recurrent)
           dW_out[j,k] = -lr * sum_t (pi_k(t) - target_k) zbar_j(t)
           db_k = -lr * sum_t (pi_k(t) - target_k) bbar(t)   (bbar: the
           readout-filtered constant 1, since the bias accumulates in y)
  plus a firing-rate regulariser that pulls each neuron's rate toward
  `target_rate` through the same eligibility traces (homeostasis).

Examples in a batch are simulated in parallel; the applied change is the mean
of the per-example updates computed with the batch's starting weights
(batch_size=1 is fully online).
"""
from __future__ import annotations

from dataclasses import dataclass, field

import torch

from athena.bio.snn import decay, poisson


@dataclass
class EpropConfig:
    n_rec: int = 256
    steps: int = 40            # ms per presentation
    read_steps: int = 20       # decision window: the last read_steps ms
    rate: float = 0.3          # input spike probability per ms at intensity 1
    tau_m: float = 20.0        # membrane time constant (ms)
    tau_out: float = 20.0      # readout time constant (ms)
    threshold: float = 1.0
    gamma: float = 0.3         # pseudo-derivative height
    recurrent: bool = True
    feedback: str = "random"   # "random" (B fixed random) or "symmetric" (B = W_out^T)
    lr: float = 1e-3
    lr_out: float = 1e-3
    target_rate: float = 0.01  # spikes per ms (10 Hz)
    reg: float = 0.5           # firing-rate regulariser strength
    seed: int = 0
    extras: dict = field(default_factory=dict)


class EpropSNN:
    def __init__(self, n_in: int, n_out: int, cfg: EpropConfig | None = None):
        self.cfg = cfg or EpropConfig()
        c = self.cfg
        self.n_in, self.n_out, self.n = n_in, n_out, c.n_rec
        self.gen = torch.Generator().manual_seed(c.seed)
        g = self.gen
        self.a, self.k = decay(c.tau_m), decay(c.tau_out)
        self.W_in = torch.randn(n_in, self.n, generator=g) * (1.0 / n_in ** 0.5)
        self.W_rec = (torch.randn(self.n, self.n, generator=g) * (0.5 / self.n ** 0.5)
                      if c.recurrent else torch.zeros(self.n, self.n))
        self.W_rec.fill_diagonal_(0.0)
        self.W_out = torch.randn(self.n, n_out, generator=g) * (1.0 / self.n ** 0.5)
        self.b_out = torch.zeros(n_out)
        self.B = torch.randn(n_out, self.n, generator=g) * (1.0 / self.n ** 0.5)
        self.updates = 0

    # ------------------------------------------------------------------
    def encode(self, X: torch.Tensor) -> torch.Tensor:
        """(steps, batch, n_in) Poisson spikes."""
        return poisson(X, self.cfg.steps, self.cfg.rate, self.gen)

    def _feedback(self) -> torch.Tensor:
        return self.W_out.T if self.cfg.feedback == "symmetric" else self.B

    @torch.no_grad()
    def run(self, spikes: torch.Tensor, target: torch.Tensor | None = None):
        """Simulate one presentation for a batch. With a one-hot `target`
        (batch, n_out), also return the e-prop weight changes (unscaled).
        Returns (mean decision probabilities over the read window, deltas|None,
        mean firing rate)."""
        c = self.cfg
        T, B, _ = spikes.shape
        n, th = self.n, c.threshold
        v = torch.zeros(B, n)
        z = torch.zeros(B, n)
        y = torch.zeros(B, self.n_out)
        learn = target is not None
        if learn:
            eps_in = torch.zeros(B, self.n_in)        # presynaptic traces
            eps_rec = torch.zeros(B, n)
            ebar_in = torch.zeros(B, self.n_in, n)    # filtered eligibility
            ebar_rec = torch.zeros(B, n, n)
            zbar = torch.zeros(B, n)                  # readout-filtered spikes
            bbar = 0.0                                # readout-filtered constant (bias input)
            rbar_in = torch.zeros(B, self.n_in, n)    # unfiltered, for the regulariser
            rbar_rec = torch.zeros(B, n, n)
            d_in = torch.zeros_like(self.W_in)
            d_rec = torch.zeros_like(self.W_rec)
            d_out = torch.zeros_like(self.W_out)
            d_b = torch.zeros_like(self.b_out)
            Fb = self._feedback()
        pi_sum = torch.zeros(B, self.n_out)
        rate_sum = torch.zeros(B, n)
        for t in range(T):
            x = spikes[t]
            z_prev = z
            v = self.a * v + x @ self.W_in + z_prev @ self.W_rec - z_prev * th
            z = (v > th).to(v.dtype)
            y = self.k * y + z @ self.W_out + self.b_out
            pi = torch.softmax(y, dim=1)
            rate_sum += z
            if t >= T - c.read_steps:
                pi_sum += pi
            if not learn:
                continue
            psi = c.gamma * torch.clamp(1.0 - (v - th).abs() / th, min=0.0) / th   # (B, n)
            eps_in = self.a * eps_in + x
            eps_rec = self.a * eps_rec + z_prev
            e_in = eps_in.unsqueeze(2) * psi.unsqueeze(1)                          # (B, n_in, n)
            e_rec = eps_rec.unsqueeze(2) * psi.unsqueeze(1)
            ebar_in = self.k * ebar_in + e_in
            ebar_rec = self.k * ebar_rec + e_rec
            rbar_in += e_in
            rbar_rec += e_rec
            zbar = self.k * zbar + z
            bbar = self.k * bbar + 1.0
            err = pi - target                                                    # (B, n_out)
            L = err @ Fb                                                         # (B, n)
            d_in += torch.einsum("bj,bij->ij", L, ebar_in)
            d_rec += torch.einsum("bj,bij->ij", L, ebar_rec)
            d_out += zbar.T @ err
            d_b += bbar * err.sum(0)
        rate = rate_sum / T
        deltas = None
        if learn:
            # Rate regulariser: d/dW of reg/2 * sum_j (rate_j - target)^2, with
            # d rate_j / dW_ij approximated by the eligibility (e-prop's form).
            r_err = (rate - c.target_rate) * c.reg / T                           # (B, n)
            d_in += torch.einsum("bj,bij->ij", r_err, rbar_in)
            d_rec += torch.einsum("bj,bij->ij", r_err, rbar_rec)
            if not c.recurrent:
                d_rec.zero_()
            d_rec.fill_diagonal_(0.0)
            deltas = (d_in, d_rec, d_out, d_b)
        return pi_sum / c.read_steps, deltas, rate.mean().item()

    @torch.no_grad()
    def apply(self, deltas, batch: int = 1) -> None:
        """Apply the summed e-prop changes of `batch` examples, averaged."""
        d_in, d_rec, d_out, d_b = (d / batch for d in deltas)
        c = self.cfg
        self.W_in -= c.lr * d_in
        self.W_rec -= c.lr * d_rec
        self.W_out -= c.lr_out * d_out
        self.b_out -= c.lr_out * d_b
        self.updates += 1

    # ------------------------------------------------------------------
    def fit(self, X: torch.Tensor, y: torch.Tensor, epochs: int = 30, batch: int = 16) -> list[float]:
        """Train on (X in [0,1], integer labels). Returns training accuracy per epoch."""
        hist = []
        n = len(X)
        for _ in range(epochs):
            order = torch.randperm(n, generator=self.gen)
            correct = 0
            for s in range(0, n, batch):
                idx = order[s:s + batch]
                tgt = torch.nn.functional.one_hot(y[idx], self.n_out).to(self.W_out.dtype)
                pi, deltas, _ = self.run(self.encode(X[idx]), tgt)
                self.apply(deltas, len(idx))
                correct += int((pi.argmax(1) == y[idx]).sum())
            hist.append(correct / n)
        return hist

    @torch.no_grad()
    def predict(self, X: torch.Tensor, batch: int = 256, repeats: int = 1) -> torch.Tensor:
        """Integer predictions; `repeats` averages over independent spike trains."""
        out = []
        for s in range(0, len(X), batch):
            p = sum(self.run(self.encode(X[s:s + batch]))[0] for _ in range(repeats))
            out.append(p.argmax(1))
        return torch.cat(out)
