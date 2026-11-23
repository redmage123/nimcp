"""Three spiking learners, each trained only by a local rule.

STDPCortex   unsupervised feature learning: LIF neurons with winner-take-all
             lateral inhibition, spike-timing-dependent plasticity on the input
             weights and an adaptive threshold (homeostasis), after Diehl &
             Cook (2015). Labels are attached afterwards by which neurons
             respond to which labelled examples (no weight sees a label).
Hippocampus  one-shot memory: a dentate-gyrus-like expansion with sparse
             firing (pattern separation) and a Hebbian association from that
             code to word neurons clamped by the teacher, learned in a single
             exposure (complementary learning systems).
RSTDPReadout reward-modulated STDP (three-factor rule): eligibility traces
             from pre/post spike coincidences, turned into weight change only
             by a dopamine-like reward (+1 right word, -1 wrong).

All inputs are intensities in [0, 1], presented as Poisson spike trains.
"""
from __future__ import annotations

import torch

from athena.bio.snn import decay, poisson


def _gen(seed: int) -> torch.Generator:
    return torch.Generator().manual_seed(seed)


# ---------------------------------------------------------------------------
class STDPCortex:
    """n LIF excitatory neurons over n_in Poisson inputs.

    Each step the neuron furthest above its threshold fires and the lateral
    inhibition resets every membrane (winner-take-all). On a spike of neuron
    j its input weights move toward the inputs that fired recently:
    dw_ij = lr * (pre_trace_i - mean_i pre_trace), then w is clipped to
    [0, w_max] and the column renormalised to w_sum (synaptic scaling).
    The firing neuron's threshold rises by theta_plus and relaxes by
    theta_decay per presentation, so no neuron takes every input.
    """

    def __init__(self, n_in: int, n: int = 100, steps: int = 50, rate: float = 0.25,
                 tau_m: float = 10.0, v_th: float = 2.0, lr: float = 0.02,
                 tau_pre: float = 20.0, theta_plus: float = 0.2, theta_decay: float = 0.98,
                 w_max: float = 1.0, seed: int = 0):
        self.n_in, self.n, self.steps, self.rate = n_in, n, steps, rate
        self.dm, self.dpre = decay(tau_m), decay(tau_pre)
        self.v_th, self.lr, self.theta_plus, self.theta_decay = v_th, lr, theta_plus, theta_decay
        self.w_max = w_max
        self.gen = _gen(seed)
        self.w_sum = 0.1 * n_in
        self.W = torch.rand(n_in, n, generator=self.gen) * 0.3
        self._normalise()
        self.theta = torch.zeros(n)
        self.updates = 0

    def _normalise(self, cols=None):
        W = self.W if cols is None else self.W[:, cols]
        W = W.clamp(0, self.w_max)
        W = W * (self.w_sum / W.sum(0, keepdim=True).clamp_min(1e-8))
        if cols is None:
            self.W = W
        else:
            self.W[:, cols] = W

    def present(self, x: torch.Tensor, learn: bool) -> torch.Tensor:
        """One image (n_in,) -> spike counts (n,). Learns when learn=True."""
        rate = self.rate
        for _ in range(3):                      # too few spikes: raise the input rate
            s_in = poisson(x, self.steps, rate, self.gen)
            v = torch.zeros(self.n)
            pre = torch.zeros(self.n_in)
            counts = torch.zeros(self.n)
            for t in range(self.steps):
                pre = pre * self.dpre + s_in[t]
                v = v * self.dm + s_in[t] @ self.W
                over = v - (self.v_th + self.theta)
                j = int(torch.argmax(over))
                if over[j] > 0:
                    counts[j] += 1
                    v.zero_()                   # lateral inhibition
                    if learn:
                        self.W[:, j] += self.lr * (pre - pre.mean())
                        self._normalise([j])
                        self.theta[j] += self.theta_plus
                        self.updates += 1
            if counts.sum() >= 3 or not learn:
                break
            rate *= 1.5
        if learn:
            self.theta *= self.theta_decay
        return counts

    def train(self, X: torch.Tensor, epochs: int = 1):
        for _ in range(epochs):
            for i in torch.randperm(len(X), generator=self.gen):
                self.present(X[i], learn=True)

    def respond(self, X: torch.Tensor, winners: int = 1, steps: int | None = None) -> torch.Tensor:
        """Spike counts (len(X), n) with learning and thresholds frozen.
        Batched: each image runs its own competition, in which up to
        `winners` neurons above threshold may fire per step (weaker lateral
        inhibition gives a distributed code); firing neurons reset."""
        steps = steps or self.steps
        s_in = poisson(X, steps, self.rate, self.gen)             # (steps, B, n_in)
        B = len(X)
        v = torch.zeros(B, self.n)
        counts = torch.zeros(B, self.n)
        for t in range(steps):
            v = v * self.dm + s_in[t] @ self.W
            over = v - (self.v_th + self.theta)
            top = over.topk(min(winners, self.n), dim=1)
            fire = torch.zeros_like(v).scatter_(1, top.indices, (top.values > 0).float())
            counts += fire
            v = v * (1 - fire)
            if winners == 1:
                v = v * (1 - fire.sum(1, keepdim=True))           # one winner resets all
        return counts


def assign_labels(R: torch.Tensor, y: torch.Tensor, n_classes: int) -> torch.Tensor:
    """Each neuron's class: the one whose labelled examples drove it most
    (-1 for neurons no labelled example drove)."""
    M = torch.zeros(n_classes, R.shape[1])
    for c in range(n_classes):
        if (y == c).any():
            M[c] = R[y == c].mean(0)
    a = M.argmax(0)
    a[M.max(0).values <= 0] = -1
    return a


def predict_assigned(R: torch.Tensor, a: torch.Tensor, n_classes: int) -> torch.Tensor:
    """Class whose assigned neurons fired most on average."""
    S = torch.full((len(R), n_classes), -1.0)
    for c in range(n_classes):
        m = a == c
        if m.any():
            S[:, c] = R[:, m].mean(1)
    return S.argmax(1)


# ---------------------------------------------------------------------------
class Hippocampus:
    """Pattern separation + one-shot Hebbian word association.

    Dentate gyrus: n_dg cells, each wired to a random fan_in fraction of the
    inputs with positive weights. Strong feedback inhibition lets only the
    k most driven cells fire (modelled at the rate level as top-k of the
    summed input spikes; an abstraction of LIF cells under global inhibition).
    Word neurons: when a word is heard with an input, the teacher clamps its
    neuron on and every active DG synapse onto it is potentiated once
    (Hebb, one shot). Recall: the word neuron with the largest normalised
    input from the current DG code wins.
    """

    def __init__(self, n_in: int, n_classes: int, n_dg: int = 2000, k_frac: float = 0.03,
                 fan_in: float = 0.25, steps: int = 50, rate: float = 0.25, seed: int = 0):
        self.gen = _gen(seed)
        self.steps, self.rate = steps, rate
        mask = torch.rand(n_in, n_dg, generator=self.gen) < fan_in
        self.P = torch.rand(n_in, n_dg, generator=self.gen) * mask
        self.P = self.P / self.P.sum(0, keepdim=True).clamp_min(1e-8)
        self.k = max(1, int(round(k_frac * n_dg)))
        self.A = torch.zeros(n_classes, n_dg)     # DG -> word synapses

    def code(self, X: torch.Tensor) -> torch.Tensor:
        """Sparse binary DG code (len(X), n_dg) from Poisson input counts."""
        # Spike counts over the window: the sum of `steps` Bernoulli(x*rate)
        # spikes is Binomial(steps, x*rate), drawn directly (same
        # distribution, without holding every time step in memory).
        p = (X.clamp(0, 1) * self.rate).clamp(0, 1)
        counts = torch.binomial(torch.full_like(p, float(self.steps)), p, generator=self.gen)
        counts = counts / counts.sum(1, keepdim=True).clamp_min(1.0)         # input gain control
        drive = counts @ self.P
        drive = drive - drive.mean(1, keepdim=True)
        idx = drive.topk(self.k, dim=1).indices
        return torch.zeros_like(drive).scatter_(1, idx, 1.0)

    def store(self, codes: torch.Tensor, y: torch.Tensor):
        """One-shot Hebbian: word neuron y (clamped) x active DG cells."""
        self.A.index_add_(0, y, codes)

    def recall(self, codes: torch.Tensor) -> torch.Tensor:
        norm = self.A.sum(1, keepdim=True).clamp_min(1.0)
        return (codes @ (self.A / norm).T).argmax(1)


# ---------------------------------------------------------------------------
class RSTDPReadout:
    """n_out LIF word neurons with winner-take-all, trained by reward only.

    During a presentation every output spike adds the presynaptic traces to
    that neuron's eligibility. Afterwards a reward r = +1 (the neuron that
    fired most is the right word) or -1 (wrong word, or silence) arrives and
    dW = lr * (r - r_mean) * E (dopamine-gated plasticity), then weights are
    clipped at 0 and columns renormalised. The network is never told which
    neuron was right; it only learns whether it was right.
    """

    def __init__(self, n_in: int, n_out: int, steps: int = 50, rate: float = 0.25,
                 tau_m: float = 10.0, v_th: float = 1.0, lr: float = 0.05, tau_pre: float = 20.0,
                 noise: float = 0.3, seed: int = 0):
        self.gen = _gen(seed)
        self.n_in, self.n_out, self.steps, self.rate = n_in, n_out, steps, rate
        self.dm, self.dpre = decay(tau_m), decay(tau_pre)
        self.v_th, self.lr, self.noise = v_th, lr, noise
        self.w_sum = 0.1 * n_in
        self.W = torch.rand(n_in, n_out, generator=self.gen)
        self.W = self.W * (self.w_sum / self.W.sum(0, keepdim=True))
        self.r_mean = 0.0

    def _run(self, x: torch.Tensor, explore: bool):
        s_in = poisson(x, self.steps, self.rate, self.gen)
        v = torch.zeros(self.n_out)
        pre = torch.zeros(self.n_in)
        E = torch.zeros(self.n_in, self.n_out)
        counts = torch.zeros(self.n_out)
        for t in range(self.steps):
            pre = pre * self.dpre + s_in[t]
            v = v * self.dm + s_in[t] @ self.W
            if explore:                          # membrane noise: exploration
                v = v + self.noise * torch.randn(self.n_out, generator=self.gen)
            over = v - self.v_th
            j = int(torch.argmax(over))
            if over[j] > 0:
                counts[j] += 1
                E[:, j] += pre
                v.zero_()
        return counts, E

    def learn(self, x: torch.Tensor, label: int) -> bool:
        counts, E = self._run(x, explore=True)
        right = bool(counts.sum() > 0 and int(counts.argmax()) == label)
        r = 1.0 if right else -1.0
        self.W += self.lr * (r - self.r_mean) * E / max(1.0, float(E.max()))
        self.W = self.W.clamp_min(0)
        self.W = self.W * (self.w_sum / self.W.sum(0, keepdim=True).clamp_min(1e-8))
        self.r_mean = 0.95 * self.r_mean + 0.05 * r
        return right

    def predict(self, X: torch.Tensor) -> torch.Tensor:
        return torch.stack([self._run(x, explore=False)[0] for x in X]).argmax(1)
