"""Next-word prediction by a recurrent spiking network trained with e-prop
(THEORY.md, algorithms 1 and 2; rung 3).

Words arrive as a stream. Word w is presented for `steps_per_word` ms as a
spike on input line w every ms (one input line per vocabulary word). The
input-synapse eligibility is kept only for the last `ring` words of each
stream (exact up to a negligible decayed remainder; see step_word). The
recurrent LIF layer and the leaky readout are as in `eprop.EpropSNN`; during
word t the readout is trained toward word t+1, and the prediction for word
t+1 is the readout's softmax at the last ms of word t.

Nothing is reset between words or utterances: membrane potentials,
eligibility traces and the readout carry the context, as in a brain
listening to speech. Weights change every `update_every` words with the
mean of the accumulated e-prop changes over the parallel streams (no
backprop through time, no autograd).

Optional adaptive neurons (ALIF, Bellec et al. 2020): a fraction of neurons
raise their threshold after each spike and relax it with time constant
tau_a, which gives the network a memory longer than the membrane time
constant. Their eligibility includes the threshold-adaptation term of the
paper's ALIF rule.
"""
from __future__ import annotations

from dataclasses import dataclass

import torch

from athena.bio.snn import decay


@dataclass
class EpropLMConfig:
    n_rec: int = 256
    steps_per_word: int = 4
    tau_m: float = 20.0
    tau_out: float = 3.0       # short: the readout should reflect the current word's context
    threshold: float = 1.0
    gamma: float = 0.3
    adaptive_frac: float = 0.0  # fraction of ALIF neurons
    tau_a: float = 200.0
    beta: float = 1.0          # threshold increase per spike (ALIF)
    lr: float = 1e-3
    lr_out: float = 1e-3
    target_rate: float = 0.02
    reg: float = 0.5
    update_every: int = 4      # words between weight updates
    ring: int = 32             # recent words whose input-synapse traces are kept
    feedback: str = "random"
    seed: int = 0


class EpropLM:
    def __init__(self, V: int, cfg: EpropLMConfig | None = None, device: str = "cpu"):
        self.cfg = c = cfg or EpropLMConfig()
        self.V, self.n, self.dev = V, c.n_rec, torch.device(device)
        g = torch.Generator().manual_seed(c.seed)
        self.a, self.k, self.rho = decay(c.tau_m), decay(c.tau_out), decay(c.tau_a)
        n = self.n
        self.W_in = (torch.randn(V, n, generator=g) * 0.8).to(self.dev)
        self.W_rec = (torch.randn(n, n, generator=g) * (0.5 / n ** 0.5)).to(self.dev)
        self.W_rec.fill_diagonal_(0.0)
        self.W_out = (torch.randn(n, V, generator=g) * (1.0 / n ** 0.5)).to(self.dev)
        self.b_out = torch.zeros(V, device=self.dev)
        self.B = (torch.randn(V, n, generator=g) * (1.0 / n ** 0.5)).to(self.dev)
        n_ad = int(round(c.adaptive_frac * n))
        self.beta = torch.zeros(n, device=self.dev)
        self.beta[:n_ad] = c.beta
        self.gen = g

    # ------------------------------------------------------------------
    def init_state(self, batch: int, learn: bool) -> dict:
        z = lambda *s: torch.zeros(*s, device=self.dev)
        st = {"v": z(batch, self.n), "s": z(batch, self.n), "ath": z(batch, self.n),
              "y": z(batch, self.V)}
        if learn:
            K = self.cfg.ring
            st.update(ring_ids=torch.zeros(batch, K, dtype=torch.long, device=self.dev), pos=0,
                      eps_in=z(batch, K), eps_rec=z(batch, self.n),
                      epa_in=z(batch, K, self.n), epa_rec=z(batch, self.n, self.n),
                      ebar_in=z(batch, K, self.n), ebar_rec=z(batch, self.n, self.n),
                      rbar_in=z(batch, K, self.n), rbar_rec=z(batch, self.n, self.n),
                      zbar=z(batch, self.n), bbar=torch.zeros((), device=self.dev),
                      psi_prev=z(batch, self.n), rate=z(batch, self.n), rate_steps=0)
        return st

    def _zero_deltas(self):
        return [torch.zeros_like(self.W_in), torch.zeros_like(self.W_rec),
                torch.zeros_like(self.W_out), torch.zeros_like(self.b_out)]

    @torch.no_grad()
    def step_word(self, st: dict, words: torch.Tensor, nxt: torch.Tensor | None,
                  deltas: list | None) -> torch.Tensor:
        """Present one word per stream; return log-probabilities of the next
        word (B, V) read at the word's last ms. With `nxt` and `deltas`,
        accumulate e-prop changes toward the next word."""
        c = self.cfg
        B = words.shape[0]
        drive = self.W_in[words]                      # one-hot input: its W_in row
        learn = deltas is not None
        if learn:
            # Input-synapse traces live in a ring of the last `ring` words per
            # stream. The traces are linear in the input, so one slot per
            # presentation sums to the exact trace of each input line; a slot
            # is recycled after `ring` words, when what is left of it
            # (a^(ring*steps_per_word)) is negligible.
            slot = st["pos"] % c.ring
            st["pos"] += 1
            st["ring_ids"][:, slot] = words
            for key in ("eps_in", "epa_in", "ebar_in", "rbar_in"):
                st[key][:, slot] = 0.0
            x_slot = torch.zeros(B, c.ring, device=self.dev)
            x_slot[:, slot] = 1.0
            rows = st["ring_ids"].reshape(-1)
            tgt = torch.zeros(B, self.V, device=self.dev)
            tgt[torch.arange(B), nxt] = 1.0
            Fb = self.W_out.T if c.feedback == "symmetric" else self.B
        logp = None
        for _ in range(c.steps_per_word):
            z_prev = st["s"]
            thr = c.threshold + self.beta * st["ath"]
            v = self.a * st["v"] + drive + z_prev @ self.W_rec - z_prev * thr
            z = (v > thr).to(v.dtype)
            st["ath"] = self.rho * st["ath"] + z
            st["v"], st["s"] = v, z
            st["y"] = self.k * st["y"] + z @ self.W_out + self.b_out
            logp = torch.log_softmax(st["y"], dim=1)
            if not learn:
                continue
            psi = c.gamma * torch.clamp(1.0 - (v - thr).abs() / c.threshold, min=0.0) / c.threshold
            st["eps_in"] = self.a * st["eps_in"] + x_slot
            st["eps_rec"] = self.a * st["eps_rec"] + z_prev
            ad = (self.beta > 0).any()
            if ad:   # ALIF: eps_a = psi_prev * eps_v_prev + (rho - psi_prev * beta) * eps_a
                pp = st["psi_prev"].unsqueeze(1)
                st["epa_in"] = pp * st["eps_in"].unsqueeze(2) + (self.rho - pp * self.beta) * st["epa_in"]
                st["epa_rec"] = pp * st["eps_rec"].unsqueeze(2) + (self.rho - pp * self.beta) * st["epa_rec"]
                e_in = psi.unsqueeze(1) * (st["eps_in"].unsqueeze(2) - self.beta * st["epa_in"])
                e_rec = psi.unsqueeze(1) * (st["eps_rec"].unsqueeze(2) - self.beta * st["epa_rec"])
            else:
                e_in = st["eps_in"].unsqueeze(2) * psi.unsqueeze(1)
                e_rec = st["eps_rec"].unsqueeze(2) * psi.unsqueeze(1)
            st["psi_prev"] = psi
            st["ebar_in"] = self.k * st["ebar_in"] + e_in
            st["ebar_rec"] = self.k * st["ebar_rec"] + e_rec
            st["rbar_in"] += e_in
            st["rbar_rec"] += e_rec
            st["zbar"] = self.k * st["zbar"] + z
            st["bbar"] = self.k * st["bbar"] + 1.0
            st["rate"] += z
            st["rate_steps"] += 1
            err = logp.exp() - tgt
            L = err @ Fb
            deltas[0].index_add_(0, rows, (L.unsqueeze(1) * st["ebar_in"]).reshape(-1, self.n))
            deltas[1] += torch.einsum("bj,bij->ij", L, st["ebar_rec"])
            deltas[2] += st["zbar"].T @ err
            deltas[3] += st["bbar"] * err.sum(0)
        return logp

    @torch.no_grad()
    def apply(self, st: dict, deltas: list, batch: int) -> None:
        c = self.cfg
        if st["rate_steps"]:
            rate = st["rate"] / st["rate_steps"]
            r_err = (rate - c.target_rate) * c.reg / st["rate_steps"]
            deltas[0].index_add_(0, st["ring_ids"].reshape(-1),
                                 (r_err.unsqueeze(1) * st["rbar_in"]).reshape(-1, self.n))
            deltas[1] += torch.einsum("bj,bij->ij", r_err, st["rbar_rec"])
        st["rate"].zero_()
        st["rate_steps"] = 0
        st["rbar_in"].zero_()
        st["rbar_rec"].zero_()
        deltas[1].fill_diagonal_(0.0)
        self.W_in -= c.lr * deltas[0] / batch
        self.W_rec -= c.lr * deltas[1] / batch
        self.W_out -= c.lr_out * deltas[2] / batch
        self.b_out -= c.lr_out * deltas[3] / batch
        for d in deltas:
            d.zero_()

    # ------------------------------------------------------------------
    @staticmethod
    def streams(ids: torch.Tensor, batch: int) -> torch.Tensor:
        """Cut a 1-D id stream into `batch` parallel contiguous streams (B, L)."""
        L = len(ids) // batch
        return ids[: L * batch].view(batch, L)

    def train_pass(self, ids: torch.Tensor, batch: int = 16) -> float:
        """One pass over the stream; returns the training per-word NLL."""
        S = self.streams(ids, batch).to(self.dev)
        st = self.init_state(batch, learn=True)
        deltas = self._zero_deltas()
        nll, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step_word(st, S[:, t], S[:, t + 1], deltas)
            nll -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
            if (t + 1) % self.cfg.update_every == 0:
                self.apply(st, deltas, batch)
        return nll / max(n, 1)

    def nll(self, ids: torch.Tensor, batch: int = 64) -> tuple[float, int]:
        """Summed next-word NLL on a held-out stream (no learning)."""
        S = self.streams(ids, batch).to(self.dev)
        st = self.init_state(batch, learn=False)
        total, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step_word(st, S[:, t], None, None)
            total -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
        return total, n
