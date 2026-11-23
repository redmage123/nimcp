"""A residual stack of per-neuron-recurrence layers trained by a local rule
(THEORY.md algorithms 1 and 2; rung 3).

Architecture (one step per word w):
  x_0 = E[w]                                        word -> distributed code (d)
  block l = 1..L:
    z    = norm(x_{l-1})                            divisive normalisation (no gain)
    u    = z W_u + b_u,  g = sigmoid(z W_g + b_g)   drive and keep-gate (n neurons)
    h    = g h_prev + (1 - g) u                     per-neuron recurrence (diagonal)
    r    = relu(h)                                  firing rate
    x_l  = x_{l-1} + r W_o + b_o                    residual stream
  p = softmax(norm_affine(x_L) W_out + b_out)       next-word distribution

Learning, all from quantities local to each synapse plus an error signal
delivered to its neuron:
  readout and output synapses (W_out, W_o, biases, final gain/offset):
      pre-synaptic activity x post-synaptic error.
  drive and gate synapses (W_u, W_g): each neuron's state depends only on its
      own past, so a per-synapse eligibility trace carries the exact
      sensitivity of the state to the synapse through time:
        e_u[i, j] <- g_j e_u[i, j] + (1 - g_j) z_i
        e_g[i, j] <- g_j e_g[i, j] + (h_prev_j - u_j) g_j (1 - g_j) z_i
      update = sum_t L_j(t) e[i, j](t), with L_j the error at neuron j.
  errors travel down the readout, the output synapses and into lower blocks
      through feedback synapses F: "symmetric" (F = W^T, the exact gradient,
      a check) or "kp" (each feedback synapse learns with the same local
      update as its forward partner and both decay: Kolen-Pollack, so F comes
      to approximate W^T without copying).
  normalisation: its exact derivative, which needs only its own layer's mean
      and spread.
  embedding E: the error at x_0 at the current step.

With one block and symmetric feedback, the updates of every parameter except
E equal the gradient of the summed next-word loss (tested). With more blocks,
the error a lower block receives counts only the current step's path through
the blocks above, as in e-prop.

Optimiser: Adam on each synapse's accumulated update (an engineering choice,
used identically for the backprop control; not claimed as biological).
"""
from __future__ import annotations

from dataclasses import dataclass

import torch


def _norm(x, eps=1e-5):
    mu = x.mean(1, keepdim=True)
    sd = (x.var(1, unbiased=False, keepdim=True) + eps).sqrt()
    xh = (x - mu) / sd
    return xh, sd


def _norm_back(dy, xh, sd):
    """Exact derivative of (x - mean) / sd at the layer, given dL/dy."""
    return (dy - dy.mean(1, keepdim=True) - xh * (dy * xh).mean(1, keepdim=True)) / sd


@dataclass
class StackLMConfig:
    d: int = 128
    n: int = 256
    layers: int = 2
    feedback: str = "kp"           # "kp" | "symmetric"
    kp_decay: float = 1e-3
    lr: float = 2e-3
    betas: tuple = (0.9, 0.999)
    update_every: int = 8
    seed: int = 0


class StackLM:
    def __init__(self, V: int, cfg: StackLMConfig | None = None, dtype=torch.float32):
        self.cfg = c = cfg or StackLMConfig()
        self.V, self.dtype = V, dtype
        g = torch.Generator().manual_seed(c.seed)
        f = dict(dtype=dtype)
        rn = lambda *s, scale: (torch.randn(*s, generator=g) * scale).to(**f)
        d, n = c.d, c.n
        self.P = {"E": rn(V, d, scale=1.0)}
        keep = torch.linspace(0.5, 0.99, n, **f)
        for l in range(c.layers):
            self.P[f"Wu{l}"] = rn(d, n, scale=d ** -0.5)
            self.P[f"bu{l}"] = torch.zeros(n, **f)
            self.P[f"Wg{l}"] = rn(d, n, scale=d ** -0.5)
            self.P[f"bg{l}"] = torch.log(keep / (1 - keep))
            self.P[f"Wo{l}"] = rn(n, d, scale=n ** -0.5)
            self.P[f"bo{l}"] = torch.zeros(d, **f)
        self.P["gam"] = torch.ones(d, **f)
        self.P["bet"] = torch.zeros(d, **f)
        self.P["Wout"] = rn(d, V, scale=d ** -0.5)
        self.P["bout"] = torch.zeros(V, **f)
        # feedback synapses (used when feedback == "kp"); shapes are W^T
        self.Fb = {k: rn(*self.P[k].shape[::-1], scale=self.P[k].shape[0] ** -0.5)
                   for k in self.P if k.startswith(("Wu", "Wg", "Wo", "Wout"))}
        self.m = {k: torch.zeros_like(v) for k, v in self.P.items()}
        self.v = {k: torch.zeros_like(v) for k, v in self.P.items()}
        self.t = 0

    def F(self, k):
        return self.P[k].T if self.cfg.feedback == "symmetric" else self.Fb[k]

    # ------------------------------------------------------------------
    def init_state(self, batch: int, learn: bool) -> dict:
        c = self.cfg
        z = lambda *s: torch.zeros(*s, dtype=self.dtype)
        st = {"h": [z(batch, c.n) for _ in range(c.layers)]}
        if learn:
            st.update(eu=[z(batch, c.d, c.n) for _ in range(c.layers)],
                      eg=[z(batch, c.d, c.n) for _ in range(c.layers)],
                      ebu=[z(batch, c.n) for _ in range(c.layers)],
                      ebg=[z(batch, c.n) for _ in range(c.layers)])
        return st

    def zero_deltas(self) -> dict:
        return {k: torch.zeros_like(v) for k, v in self.P.items()}

    @torch.no_grad()
    def step(self, st, words, nxt=None, D=None):
        c, P = self.cfg, self.P
        x = P["E"][words]
        cache = []
        for l in range(c.layers):
            zl, sd = _norm(x)
            u = zl @ P[f"Wu{l}"] + P[f"bu{l}"]
            gt = torch.sigmoid(zl @ P[f"Wg{l}"] + P[f"bg{l}"])
            hp = st["h"][l]
            h = gt * hp + (1 - gt) * u
            st["h"][l] = h
            r = torch.relu(h)
            cache.append((zl, sd, u, gt, hp, h, r))
            x = x + r @ P[f"Wo{l}"] + P[f"bo{l}"]
        xf, sdf = _norm(x)
        zf = xf * P["gam"] + P["bet"]
        logp = torch.log_softmax(zf @ P["Wout"] + P["bout"], 1)
        if D is None:
            return logp
        Bsz = words.shape[0]
        err = logp.exp()
        err[torch.arange(Bsz), nxt] -= 1.0
        D["Wout"] += zf.T @ err
        D["bout"] += err.sum(0)
        dzf = err @ self.F("Wout")
        D["gam"] += (dzf * xf).sum(0)
        D["bet"] += dzf.sum(0)
        ex = _norm_back(dzf * P["gam"], xf, sdf)
        for l in reversed(range(c.layers)):
            zl, sd, u, gt, hp, h, r = cache[l]
            D[f"Wo{l}"] += r.T @ ex
            D[f"bo{l}"] += ex.sum(0)
            L = (ex @ self.F(f"Wo{l}")) * (h > 0).to(self.dtype)
            dgate = (hp - u) * gt * (1 - gt)
            G1 = gt.unsqueeze(1)
            st["eu"][l] = G1 * st["eu"][l] + zl.unsqueeze(2) * (1 - gt).unsqueeze(1)
            st["eg"][l] = G1 * st["eg"][l] + zl.unsqueeze(2) * dgate.unsqueeze(1)
            st["ebu"][l] = gt * st["ebu"][l] + (1 - gt)
            st["ebg"][l] = gt * st["ebg"][l] + dgate
            D[f"Wu{l}"] += torch.einsum("bj,bij->ij", L, st["eu"][l])
            D[f"Wg{l}"] += torch.einsum("bj,bij->ij", L, st["eg"][l])
            D[f"bu{l}"] += (L * st["ebu"][l]).sum(0)
            D[f"bg{l}"] += (L * st["ebg"][l]).sum(0)
            dz = (L * (1 - gt)) @ self.F(f"Wu{l}") + (L * dgate) @ self.F(f"Wg{l}")
            ex = ex + _norm_back(dz, zl, sd)
        D["E"].index_add_(0, words, ex)
        return logp

    @torch.no_grad()
    def apply(self, D: dict, scale: float) -> None:
        c = self.cfg
        b1, b2 = c.betas
        self.t += 1
        for k, p in self.P.items():
            gk = D[k] * scale
            self.m[k].mul_(b1).add_((1 - b1) * gk)
            self.v[k].mul_(b2).add_((1 - b2) * gk * gk)
            mh = self.m[k] / (1 - b1 ** self.t)
            vh = self.v[k] / (1 - b2 ** self.t)
            step = c.lr * mh / (vh.sqrt() + 1e-8)
            p.sub_(step)
            if c.feedback == "kp" and k in self.Fb:
                self.Fb[k].sub_(step.T)                       # same local update
                self.Fb[k].mul_(1 - c.kp_decay)
                p.mul_(1 - c.kp_decay)
            D[k].zero_()

    # ------------------------------------------------------------------
    @staticmethod
    def streams(ids, batch):
        L = len(ids) // batch
        return ids[: L * batch].view(batch, L)

    def train_pass(self, ids, batch: int = 16) -> float:
        S = self.streams(ids, batch)
        st = self.init_state(batch, learn=True)
        D = self.zero_deltas()
        nll, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step(st, S[:, t], S[:, t + 1], D)
            nll -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
            if (t + 1) % self.cfg.update_every == 0:
                self.apply(D, 1.0 / (batch * self.cfg.update_every))
        return nll / max(n, 1)

    @torch.no_grad()
    def nll(self, ids, batch: int = 64):
        S = self.streams(ids, batch)
        st = self.init_state(batch, learn=False)
        total, n = 0.0, 0
        for t in range(S.shape[1] - 1):
            logp = self.step(st, S[:, t])
            total -= float(logp[torch.arange(batch), S[:, t + 1]].sum())
            n += batch
        return total, n


class StackLMTorch(torch.nn.Module):
    """Same network for backprop through time (control) and the exactness check."""

    def __init__(self, src: StackLM):
        super().__init__()
        self.c = src.cfg
        self.P = torch.nn.ParameterDict({k: torch.nn.Parameter(v.clone()) for k, v in src.P.items()})

    def forward(self, x, hs=None):
        c, P = self.c, self.P
        B, T = x.shape
        hs = hs or [torch.zeros(B, c.n, dtype=P["E"].dtype) for _ in range(c.layers)]
        hs = list(hs)
        outs = []
        for t in range(T):
            y = P["E"][x[:, t]]
            for l in range(c.layers):
                zl, _ = _norm(y)
                u = zl @ P[f"Wu{l}"] + P[f"bu{l}"]
                gt = torch.sigmoid(zl @ P[f"Wg{l}"] + P[f"bg{l}"])
                hs[l] = gt * hs[l] + (1 - gt) * u
                y = y + torch.relu(hs[l]) @ P[f"Wo{l}"] + P[f"bo{l}"]
            xf, _ = _norm(y)
            outs.append((xf * P["gam"] + P["bet"]) @ P["Wout"] + P["bout"])
        return torch.stack(outs, 1), hs
