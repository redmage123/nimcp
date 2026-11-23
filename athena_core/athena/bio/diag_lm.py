"""Next-word prediction with per-neuron (diagonal) recurrence and exact local
learning (THEORY.md, algorithms 1 and 2; rung 3).

Network, one step per word w(t):
  gate     g_j(t) = sigmoid(G[w(t), j] + c_j)          how much neuron j keeps
  drive    u_j(t) = W_in[w(t), j]
  state    h_j(t) = g_j(t) h_j(t-1) + (1 - g_j(t)) u_j(t)
  rate     r_j(t) = relu(h_j(t))                       firing rate (a rate model of
                                                        a spiking neuron)
  readout  p(t)   = softmax(W_out^T r(t) + b)          next-word distribution
Each neuron's state depends only on its own past and the current word, so
the recurrence is diagonal. For such networks the forward ("online")
sensitivity of every state to each of its own synapses can be carried
exactly by a local trace, and the update  sum_t L_j(t) * trace(t)  is the
exact gradient of the summed next-word loss when L_j is the true error at
neuron j (real-time recurrent learning made local by the diagonal
structure).

Local traces (per synapse, per stream):
  W_in[i, j]:  e(t) = g_j e(t-1) + (1 - g_j) [w(t) = i]
  G[i, j]:     e(t) = g_j e(t-1) + (h_j(t-1) - u_j(t)) g_j (1 - g_j) [w(t) = i]
  c_j:         e(t) = g_j e(t-1) + (h_j(t-1) - u_j(t)) g_j (1 - g_j)
Learning signal at neuron j (third factor):
  L_j(t) = relu'(h_j(t)) * sum_k F[k, j] (p_k(t) - [w(t+1) = k])
with F = W_out^T ("symmetric": the exact gradient, used as a correctness
check), F = B fixed random ("random": no weight transport), or F = B learned
("kp", Kolen-Pollack; Akrout et al. 2019): each backward synapse B[k, j]
receives the same local update as its forward partner W_out[j, k] (the
product of the same pre-synaptic rate and post-synaptic error, available at
both ends) and both decay by `kp_decay`; the difference B - W_out^T then
shrinks every update, so the feedback comes to carry the true error without
any weight being copied.
Readout: dW_out = r (p - target)^T, db = p - target (local: pre-rate times
post-error).

The W_in and G traces are nonzero only for words seen recently; they are
kept in a ring of the last `ring` words per stream (exact while every slot's
trace still matters; with gates near 1 a word's trace lasts longer, so the
ring must cover the memory span in use).
"""
from __future__ import annotations

from dataclasses import dataclass

import torch


@dataclass
class DiagLMConfig:
    n: int = 256
    feedback: str = "random"       # "random" | "kp" (learned, Kolen-Pollack) | "symmetric"
    kp_decay: float = 1e-3         # kp: weight decay on W_out and B per update
    lr: float = 0.05
    optimizer: str = "rms"         # "sgd" | "rms" (per-synapse running-scale normalisation)
    rms_decay: float = 0.99
    update_every: int = 8          # words between weight updates
    ring: int = 64
    decay_min: float = 0.5         # initial keep-gates spread over [decay_min, decay_max]
    decay_max: float = 0.99
    seed: int = 0


class DiagLM:
    PARAMS = ("W_in", "G", "c", "W_out", "b")

    def __init__(self, V: int, cfg: DiagLMConfig | None = None, dtype=torch.float32):
        self.cfg = c = cfg or DiagLMConfig()
        self.V, self.n, self.dtype = V, c.n, dtype
        g = torch.Generator().manual_seed(c.seed)
        n = c.n
        f = dict(dtype=dtype)
        self.W_in = torch.randn(V, n, generator=g).to(**f)
        self.G = torch.zeros(V, n, **f)
        keep = torch.linspace(c.decay_min, c.decay_max, n, **f)
        self.c = torch.log(keep / (1 - keep))
        self.W_out = (torch.randn(n, V, generator=g) / n ** 0.5).to(**f)
        self.b = torch.zeros(V, **f)
        self.B = (torch.randn(V, n, generator=g) / n ** 0.5).to(**f)
        self.ms = {k: torch.zeros_like(getattr(self, k)) for k in self.PARAMS}

    # ------------------------------------------------------------------
    def init_state(self, batch: int, learn: bool) -> dict:
        z = lambda *s: torch.zeros(*s, dtype=self.dtype)
        st = {"h": z(batch, self.n)}
        if learn:
            K = self.cfg.ring
            st.update(ring_ids=torch.zeros(batch, K, dtype=torch.long), pos=0,
                      e_in=z(batch, K, self.n), e_G=z(batch, K, self.n), e_c=z(batch, self.n))
        return st

    def zero_deltas(self) -> dict:
        return {k: torch.zeros_like(getattr(self, k)) for k in self.PARAMS}

    @torch.no_grad()
    def step(self, st: dict, words: torch.Tensor, nxt: torch.Tensor | None = None,
             deltas: dict | None = None) -> torch.Tensor:
        """One word per stream; returns log p(next word) (B, V). With `nxt` and
        `deltas`, accumulates the local updates toward the next word."""
        Bsz = words.shape[0]
        gate = torch.sigmoid(self.G[words] + self.c)
        u = self.W_in[words]
        h_prev = st["h"]
        h = gate * h_prev + (1 - gate) * u
        st["h"] = h
        r = torch.relu(h)
        logp = torch.log_softmax(r @ self.W_out + self.b, dim=1)
        if deltas is None:
            return logp
        c = self.cfg
        K = c.ring
        slot = st["pos"] % K
        st["pos"] += 1
        st["ring_ids"][:, slot] = words
        st["e_in"][:, slot] = 0.0
        st["e_G"][:, slot] = 0.0
        cur = torch.zeros(Bsz, K, 1, dtype=self.dtype)
        cur[:, slot] = 1.0
        gg = gate.unsqueeze(1)
        dg = (h_prev - u) * gate * (1 - gate)                     # d h / d (gate pre-activation)
        st["e_in"] = gg * st["e_in"] + cur * (1 - gate).unsqueeze(1)
        st["e_G"] = gg * st["e_G"] + cur * dg.unsqueeze(1)
        st["e_c"] = gate * st["e_c"] + dg
        err = logp.exp()
        err[torch.arange(Bsz), nxt] -= 1.0                        # p - target
        F = self.W_out.T if c.feedback == "symmetric" else self.B
        L = (err @ F) * (h > 0).to(self.dtype)                    # (B, n) learning signal
        rows = st["ring_ids"].reshape(-1)
        deltas["W_in"].index_add_(0, rows, (L.unsqueeze(1) * st["e_in"]).reshape(-1, self.n))
        deltas["G"].index_add_(0, rows, (L.unsqueeze(1) * st["e_G"]).reshape(-1, self.n))
        deltas["c"] += (L * st["e_c"]).sum(0)
        deltas["W_out"] += r.T @ err
        deltas["b"] += err.sum(0)
        return logp

    @torch.no_grad()
    def apply(self, deltas: dict, scale: float) -> None:
        c = self.cfg
        for k in self.PARAMS:
            d = deltas[k] * scale
            if c.optimizer == "rms":
                self.ms[k].mul_(c.rms_decay).add_((1 - c.rms_decay) * d * d)
                d = d / (self.ms[k].sqrt() + 1e-8)
            getattr(self, k).sub_(c.lr * d)
            if k == "W_out" and c.feedback == "kp":
                self.B.sub_(c.lr * d.T)                    # same local update as its partner
                self.B.mul_(1 - c.kp_decay)
                self.W_out.mul_(1 - c.kp_decay)
            deltas[k].zero_()

    # ------------------------------------------------------------------
    @staticmethod
    def streams(ids: torch.Tensor, batch: int) -> torch.Tensor:
        L = len(ids) // batch
        return ids[: L * batch].view(batch, L)

    def train_pass(self, ids: torch.Tensor, batch: int = 16) -> float:
        S = self.streams(ids, batch)
        st = self.init_state(batch, learn=True)
        deltas = self.zero_deltas()
        nll, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step(st, S[:, t], S[:, t + 1], deltas)
            nll -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
            if (t + 1) % self.cfg.update_every == 0:
                self.apply(deltas, 1.0 / (batch * self.cfg.update_every))
        return nll / max(n, 1)

    @torch.no_grad()
    def nll(self, ids: torch.Tensor, batch: int = 64) -> tuple[float, int]:
        S = self.streams(ids, batch)
        st = self.init_state(batch, learn=False)
        total, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step(st, S[:, t])
            total -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
        return total, n


class DiagLMTorch(torch.nn.Module):
    """The same network for backprop-through-time training (the conventional
    control) and for checking that the local traces equal the gradient."""

    def __init__(self, src: DiagLM):
        super().__init__()
        for k in DiagLM.PARAMS:
            setattr(self, k, torch.nn.Parameter(getattr(src, k).clone()))

    def forward(self, x: torch.Tensor, h: torch.Tensor | None = None):
        B, T = x.shape
        h = torch.zeros(B, self.W_in.shape[1], dtype=self.W_in.dtype) if h is None else h
        out = []
        for t in range(T):
            gate = torch.sigmoid(self.G[x[:, t]] + self.c)
            h = gate * h + (1 - gate) * self.W_in[x[:, t]]
            out.append(torch.relu(h) @ self.W_out + self.b)
        return torch.stack(out, 1), h
