"""Next-word prediction by a hippocampal fast memory (THEORY.md, algorithm 3;
rung 3).

Context. Each vocabulary word has an input line; a heard word leaves
decaying traces on its line. There are several trace populations, one per
decay factor (like presynaptic traces with different time constants): decay
0 holds only the current word, larger decays remember further back with
weights falling by `decay` per word, which keeps word order.

Pattern separation. Each trace population drives its own dentate-gyrus-like
layer of `n_dg` units through fixed random synapses; lateral inhibition lets
only the `k` most driven units fire (k-winners-take-all). With the default
dense projection every input line reaches every unit, so the winners respond
to the combination of recent words (conjunctive codes); the same context
gives the same sparse code every time, and similar contexts share units.
Sparse codes (k = 5 of 10000) worked best on AO-CHILDES dev (2026-11-21:
k 50 -> 158, 20 -> 153, 10 -> 150, 5 -> 146; trigram 154.5).

One-shot storage. When the next word arrives, the synapses from each
layer's active units to that word's neuron are strengthened by one
(Hebbian, a single exposure per event: no error, no replay).

Recall. Each layer reads a next-word distribution from its active units:
    p_l(w) = (sum over active units of A_l[w, unit] + alpha k u(w))
             / (sum over active units of all A_l + alpha k),
where u is the word-frequency distribution held by the same memory (alpha
is in units of whole events, since one event adds k counts). Units shared
by similar contexts pool their events, which is how the memory generalises.
The answer is the mixture  sum_l lambda_l p_l + lambda_0 u  with mixture
weights fitted on a tuning stream (`tune`), like the interpolation weights
of an n-gram model.
"""
from __future__ import annotations

from dataclasses import dataclass

import torch


@dataclass
class HcLMConfig:
    decays: tuple = (0.0, 0.5, 0.8)   # trace decay per word, one layer each
    profiles: tuple | None = None     # explicit trace weight per word age, one tuple
                                      # per layer (overrides decays); e.g. (1,) is the
                                      # current word, (1, 1) current and previous
                                      # equally (conjunctive), (0, 1) the previous only
    combine: str = "sum"              # multi-word layers: "sum" of weighted inputs, or
                                      # "and": dendritic coincidence detection, a unit's
                                      # drive is the smallest of its weighted inputs, so
                                      # it fires only for that combination of words
    n_dg: int = 10000                 # dentate units per layer
    k: int = 5                        # active units per layer (k-WTA)
    fan_in: int = 64                  # synapses from each input line to its layer
    alpha: float = 10.0               # smoothing, in events
    horizon: int = 12                 # words of trace kept (decay^12 is negligible)
    projection: str = "dense"         # "sparse": fan_in units per line; "dense": every
                                      # line reaches every unit (Gaussian weights), so
                                      # the k winners respond to combinations of words
    sim_mix: float = 0.0              # with word_vectors: share of each word's input
                                      # pattern carried by its similarity vector (the
                                      # rest is a word-specific random pattern)
    seed: int = 0


class HcLM:
    def __init__(self, V: int, cfg: HcLMConfig | None = None,
                 word_vectors: torch.Tensor | None = None):
        """`word_vectors` (V, d), optional: words with similar vectors get
        overlapping input patterns (dense projection only), so a context
        recalls what followed similar contexts."""
        self.cfg = c = cfg or HcLMConfig()
        self.prof = [tuple(p) for p in c.profiles] if c.profiles else [
            tuple(d ** a for a in range(c.horizon if d > 0 else 1) if d ** a >= 1e-3) if d > 0 else (1.0,)
            for d in c.decays]
        self.V, self.L = V, len(self.prof)
        g = torch.Generator().manual_seed(c.seed)
        if c.projection == "dense":
            self.Wd = torch.randn(self.L, V, c.n_dg, generator=g)
            if word_vectors is not None and c.sim_mix > 0:
                E = word_vectors - word_vectors.mean(0, keepdim=True)
                E = E / E.norm(dim=1, keepdim=True).clamp_min(1e-8)
                for l in range(self.L):
                    R = torch.randn(E.shape[1], c.n_dg, generator=g)
                    sim = E @ R                                # unit variance per entry
                    self.Wd[l] = (1 - c.sim_mix) ** 0.5 * self.Wd[l] + c.sim_mix ** 0.5 * sim
        else:
            if c.fan_in < c.k:
                raise ValueError("fan_in must be >= k, or units with no input would win")
            self.tgt = torch.randint(0, c.n_dg, (self.L, V, c.fan_in), generator=g)
            self.w = torch.rand(self.L, V, c.fan_in, generator=g)
        self.A = torch.zeros(self.L, V, c.n_dg)     # dentate -> word associations per layer
        self.unigram = torch.ones(V)                 # word-frequency memory (add-one start)
        self.lam = torch.full((self.L + 1,), 1.0 / (self.L + 1))

    def _codes(self, ids: torch.Tensor, layer: int) -> torch.Tensor:
        """Indices of the k winners of one layer for the context ending at each
        position of `ids`: (T, k)."""
        c = self.cfg
        T = len(ids)
        prof = self.prof[layer]
        if c.combine == "and" and c.projection == "dense" and len(prof) > 1:
            drive = torch.full((T, c.n_dg), float("inf"))
            for age, weight in enumerate(prof):
                if weight == 0.0:
                    continue
                part = torch.full((T, c.n_dg), -1e9)  # before the stream starts: no input
                if age < T:
                    part[age:] = weight * self.Wd[layer, ids[: T - age]]
                drive = torch.minimum(drive, part)
            return drive.topk(c.k, dim=1).indices
        drive = torch.zeros(T, c.n_dg)
        for age, weight in enumerate(prof[:T]):
            if weight == 0.0:
                continue
            src = ids[: T - age]                       # the word `age` steps back
            if c.projection == "dense":
                drive[age:] += weight * self.Wd[layer, src]
            else:
                rows = torch.arange(age, T)
                drive.index_put_((rows.unsqueeze(1).expand(-1, c.fan_in), self.tgt[layer, src]),
                                 weight * self.w[layer, src], accumulate=True)
        return drive.topk(c.k, dim=1).indices

    def _chunks(self, ids: torch.Tensor, chunk: int):
        """(codes per layer, next words) for consecutive chunks of the stream,
        each with `horizon` words of preceding context."""
        h = self.cfg.horizon
        for s in range(0, len(ids) - 1, chunk):
            lo = max(0, s - h)
            seg = ids[lo: s + chunk + 1]
            off = s - lo
            nxt = seg[off + 1:]
            codes = [self._codes(seg, l)[off: off + len(nxt)] for l in range(self.L)]
            yield codes, nxt

    def store(self, ids: torch.Tensor, chunk: int = 2048) -> None:
        """One exposure to the stream: bind each context to the word that followed."""
        k = self.cfg.k
        for codes, nxt in self._chunks(ids, chunk):
            for l, cd in enumerate(codes):
                self.A[l].index_put_((nxt.unsqueeze(1).expand(-1, k), cd),
                                     torch.ones(cd.shape), accumulate=True)
            self.unigram += torch.bincount(nxt, minlength=self.V).float()

    def _probs(self, ids: torch.Tensor, chunk: int):
        """Per chunk: (component probabilities of the true next word, (T, L+1))."""
        c = self.cfg
        u = self.unigram / self.unigram.sum()
        a = c.alpha * c.k
        for codes, nxt in self._chunks(ids, chunk):
            T = len(nxt)
            cols = [u[nxt]]
            rows = torch.arange(T).repeat_interleave(c.k)
            for l, cd in enumerate(codes):
                C = torch.sparse_coo_tensor(torch.stack([rows, cd.reshape(-1)]),
                                            torch.ones(T * c.k), (T, c.n_dg))
                counts = torch.sparse.mm(C, self.A[l].T)                 # (T, V)
                p = (counts[torch.arange(T), nxt] + a * u[nxt]) / (counts.sum(1) + a)
                cols.append(p)
            yield torch.stack(cols, dim=1)

    def tune(self, ids: torch.Tensor, iters: int = 30, chunk: int = 2048) -> None:
        """Fit the mixture weights on a tuning stream (EM on the mixture)."""
        P = torch.cat(list(self._probs(ids, chunk)))
        lam = torch.full((self.L + 1,), 1.0 / (self.L + 1))
        for _ in range(iters):
            r = P * lam
            r = r / r.sum(1, keepdim=True)
            lam = r.mean(0)
        self.lam = lam

    def nll(self, ids: torch.Tensor, chunk: int = 2048) -> tuple[float, int]:
        """Summed next-word negative log-likelihood on a held-out stream."""
        total, n = 0.0, 0
        for P in self._probs(ids, chunk):
            total -= float(torch.log(P @ self.lam).sum())
            n += len(P)
        return total, n
