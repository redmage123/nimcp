"""Wernicke predictor: a recurrent spiking network that listens continuously
and learns to predict the next sound with e-prop (THEORY.md algorithms 1+2).

Continuous listening: the network state is never reset while the stream
plays. Each 40 ms frame of auditory-nerve input is presented for
`sub` simulation steps of dt = 40/sub ms; at the end of the frame the
readout's value is the prediction of the change to the next frame. When the
next frame arrives, every neuron receives its own error signal and each
synapse changes by (its eligibility trace) x (its neuron's error): local in
space and time, no backpropagation, no autograd, no weight transport
(random fixed feedback B unless feedback="symmetric").

Neurons: ALIF (adaptive LIF, Bellec et al. 2018/2020)
  v_j(t) = a v_j(t-1) + sum_i W_in[i,j] x_i(t) + sum_i W_rec[i,j] z_i(t-1)
           - z_j(t-1) thr
  A_j(t) = thr + beta_j b_j(t),   b_j(t) = rho_j b_j(t-1) + z_j(t-1)
  z_j(t) = 1 if v_j(t) > A_j(t)
Adaptation has two timescales (tau_a 200 ms and 2 s) on `frac_adaptive`
of the neurons; the rest are plain LIF. beta_j is scaled so that at the
target rate the threshold rises by about `adapt_rise` x thr whatever tau_a.

Input: the auditory nerve's population spike count of each channel, as a
synaptic drive x_i in [0, 1] (the cochlea already made the spikes; no
extra Poisson sampling).

e-prop (Bellec et al. 2020, Nat Commun 11:3625, ALIF form)
  presynaptic trace  eps_i(t)   = a eps_i(t-1) + pre_i(t)
  pseudo-derivative  psi_j(t)   = gamma/thr max(0, 1 - |v_j - A_j|/thr)
  adaptation trace   epa_ij(t)  = psi_j(t-1) eps_i(t-1) + (rho_j - psi_j(t-1) beta_j) epa_ij(t-1)
  eligibility        e_ij(t)    = psi_j(t) (eps_i(t) - beta_j epa_ij(t))
  readout-filtered   ebar_ij(t) = k ebar_ij(t-1) + e_ij(t)
  at a frame end, with err = prediction - target:
      L_j = sum_k B[k,j] err_k                        (the neuron's own error)
      dW_ij   = -lr L_j ebar_ij
      dW_out  = -lr_out zbar_j err_k,  db = -lr_out err_k (bias: the leaky
                readout's steady-state input)
  Feedforward inhibition: the drive is x - xbar, xbar the input's running
  mean (ffi_tau). Without it the large tonic auditory-nerve input (mean
  ~0.66) gave every synapse a shared eligibility, e-prop drove all input
  weights negative together, and firing collapsed (measured 2026-11-22).
  Intrinsic plasticity: each neuron's bias current h_j moves by
  ip_lr (target - rate_j) per frame, independently of the error.
  The readout step is normalised by presynaptic activity (zbar.zbar + 1).
  A firing-rate homeostat adds reg (rate_j - target) to L_j (the
  regulariser's gradient through the same eligibilities).
"""
from __future__ import annotations

import math
from dataclasses import dataclass

import torch


@dataclass
class WernickeConfig:
    n_rec: int = 256
    sub: int = 8                 # simulation steps per 40 ms frame (dt = 5 ms)
    tau_m: float = 20.0          # ms
    tau_out: float = 20.0        # ms, readout
    tau_a: tuple = (200.0, 2000.0)
    frac_adaptive: float = 0.5
    adapt_rise: float = 0.5      # threshold rise at target rate, x thr
    thr: float = 1.0
    gamma: float = 0.3
    target_rate: float = 10.0    # Hz
    reg: float = 0.1
    lr: float = 2e-3
    lr_out: float = 0.05
    in_gain: float = 4.0         # scale of the input weights
    feedback: str = "random"     # "random" | "symmetric"
    ip_lr: float = 0.1           # intrinsic plasticity: excitability toward target_rate
    out_bias: bool = False       # readout bias: redundant with tonic firing, so bias and
                                 # weights drift together along a cancelling direction and
                                 # fail on new speakers (|b| 0.67, held-out -0.86 at 75 min)
    out_decay: float = 0.0       # readout synaptic decay per frame
    ffi_tau: float = 2000.0      # ms; feedforward inhibition cancels the input's running
                                 # mean over this window (0 = off)
    # --- step-2 primitives (default off; each measured as an ablation) ---
    stp: bool = False            # Tsodyks-Markram short-term plasticity on recurrent synapses
    stp_depress: tuple = (0.5, 200.0, 20.0)    # (U, tau_d ms, tau_f ms): depressing half
    stp_facil: tuple = (0.15, 50.0, 500.0)     # facilitating half
    max_delay: float = 0.0       # ms; per-presynaptic-neuron axonal delay, uniform in [0, max]
    dale: bool = False           # E/I populations (80/20), weights keep their sign
    frac_exc: float = 0.8
    seed: int = 0


class WernickePredictor:
    def __init__(self, dim: int, cfg: WernickeConfig | None = None):
        self.cfg = c = cfg or WernickeConfig()
        g = torch.Generator().manual_seed(c.seed)
        n = self.n = c.n_rec
        self.dim = dim
        dt = 40.0 / c.sub
        self.dt = dt
        self.a = math.exp(-dt / c.tau_m)
        self.k = math.exp(-dt / c.tau_out)
        # adaptation: per-neuron rho and beta (0 for non-adaptive neurons)
        n_ad = int(round(c.frac_adaptive * n))
        tau_a = torch.full((n,), float("inf"))
        for i, t in enumerate(c.tau_a):
            lo = i * n_ad // len(c.tau_a)
            hi = (i + 1) * n_ad // len(c.tau_a)
            tau_a[lo:hi] = t
        self.rho = torch.where(torch.isinf(tau_a), torch.zeros(n), torch.exp(-dt / tau_a))
        r_step = c.target_rate * dt / 1000.0
        steady_b = r_step / (1.0 - self.rho).clamp_min(1e-6)      # b at target rate
        self.beta = torch.where(torch.isinf(tau_a), torch.zeros(n), c.adapt_rise * c.thr / steady_b)
        self.r_step = r_step
        # weights
        self.W_in = torch.randn(dim, n, generator=g) * (c.in_gain / dim ** 0.5)
        self.W_rec = torch.randn(n, n, generator=g) * (0.5 / n ** 0.5)
        self.W_rec.fill_diagonal_(0.0)
        if c.dale:
            # Dale's law: each presynaptic neuron is excitatory or inhibitory;
            # inhibitory weights are scaled so excitation and inhibition balance
            n_e = int(round(c.frac_exc * n))
            self.sign = torch.ones(n)
            self.sign[n_e:] = -1.0
            scale = torch.ones(n)
            scale[n_e:] = c.frac_exc / (1.0 - c.frac_exc)
            self.W_rec = self.W_rec.abs() * (self.sign * scale)[:, None]
        if c.stp:
            half = n // 2
            U, td, tf = (torch.empty(n) for _ in range(3))
            for sl, (u0, d0, f0) in ((slice(0, half), c.stp_depress), (slice(half, n), c.stp_facil)):
                U[sl], td[sl], tf[sl] = u0, d0, f0
            self.stp_U, self.stp_rd, self.stp_rf = U, dt / td, dt / tf
        self.delay = None
        if c.max_delay > 0:
            dmax = int(round(c.max_delay / dt))
            self.delay = torch.randint(0, dmax + 1, (n,), generator=g)
            self.dmax = dmax
        self.W_out = torch.zeros(n, dim)
        self.b_out = torch.zeros(dim)
        self.B = torch.randn(dim, n, generator=g) * (1.0 / dim ** 0.5)
        self.h = torch.zeros(n)                   # intrinsic excitability (bias current)
        self.ffi = math.exp(-40.0 / c.ffi_tau) if c.ffi_tau > 0 else None
        self.state = self._zero_state()
        self.frames = 0

    # ------------------------------------------------------------------
    def _zero_state(self):
        n, d = self.n, self.dim
        return {
            "v": torch.zeros(n), "z": torch.zeros(n), "b": torch.zeros(n), "y": torch.zeros(d),
            "eps_in": torch.zeros(d), "eps_rec": torch.zeros(n),
            "psi": torch.zeros(n),
            "epa_in": torch.zeros(d, n), "epa_rec": torch.zeros(n, n),
            "ebar_in": torch.zeros(d, n), "ebar_rec": torch.zeros(n, n),
            "zbar": torch.zeros(n), "zsum": torch.zeros(n),
            "xbar": None,                    # running mean input (feedforward inhibition)
            "stp_u": self.stp_U.clone() if self.cfg.stp else None,
            "stp_x": torch.ones(n) if self.cfg.stp else None,
            "zbuf": torch.zeros(self.dmax + 1, n) if self.delay is not None else None,
            "pending": None,                 # prediction awaiting the next frame
            "x_prev": None,
        }

    @torch.no_grad()
    def _present(self, s, x: torch.Tensor, learn: bool, h: torch.Tensor | None = None) -> torch.Tensor:
        """Run one frame (sub steps) of drive x; returns the readout at its
        end. h: excitability to use (default the live self.h)."""
        c = self.cfg
        a, k, thr = self.a, self.k, c.thr
        s["zsum"].zero_()
        if self.ffi is not None:
            # feedforward inhibition: interneurons driven by the same afferents
            # cancel the input's slow running mean; cortex receives the change
            s["xbar"] = x.clone() if s["xbar"] is None else self.ffi * s["xbar"] + (1 - self.ffi) * x
            x = x - s["xbar"]
        I_in = x @ self.W_in + (self.h if h is None else h)
        for _ in range(c.sub):
            z_prev = s["z"]
            pre = self._presyn(s, z_prev)          # what arrives at the recurrent synapses
            s["b"] = self.rho * s["b"] + z_prev
            A = thr + self.beta * s["b"]
            s["v"] = a * s["v"] + I_in + pre @ self.W_rec - z_prev * thr
            z = (s["v"] > A).float()
            s["z"] = z
            s["y"] = k * s["y"] + z @ self.W_out + self.b_out * (1.0 - k)
            s["zsum"] += z
            if not learn:
                continue
            psi_prev = s["psi"]
            # adaptation traces use the previous step's psi and presynaptic traces
            decay_a = self.rho - psi_prev * self.beta                       # (n,)
            s["epa_in"] = psi_prev * s["eps_in"][:, None] + decay_a * s["epa_in"]
            s["epa_rec"] = psi_prev * s["eps_rec"][:, None] + decay_a * s["epa_rec"]
            s["eps_in"] = a * s["eps_in"] + x
            s["eps_rec"] = a * s["eps_rec"] + pre
            psi = c.gamma / thr * torch.clamp(1.0 - (s["v"] - A).abs() / thr, min=0.0)
            s["psi"] = psi
            e_in = psi * (s["eps_in"][:, None] - self.beta * s["epa_in"])
            e_rec = psi * (s["eps_rec"][:, None] - self.beta * s["epa_rec"])
            s["ebar_in"] = k * s["ebar_in"] + e_in
            s["ebar_rec"] = k * s["ebar_rec"] + e_rec
            s["zbar"] = k * s["zbar"] + z
        return s["y"].clone()

    def _presyn(self, s, z_prev: torch.Tensor) -> torch.Tensor:
        """Recurrent presynaptic signal: spikes after axonal delay, scaled by
        short-term plasticity (efficacy 1 at low rates)."""
        p = z_prev
        if self.delay is not None:
            buf = torch.roll(s["zbuf"], 1, dims=0)
            buf[0] = z_prev
            s["zbuf"] = buf
            p = buf[self.delay, torch.arange(self.n)]
        if self.cfg.stp:
            u, xr, U = s["stp_u"], s["stp_x"], self.stp_U
            # recovery between spikes
            xr = xr + (1.0 - xr) * self.stp_rd
            u = u + (U - u) * self.stp_rf
            # on a spike: release a fraction u of resources, then facilitate
            # (u relaxes to U between spikes, so release must use the
            # pre-spike u - Tsodyks, Pawelzik & Markram 1998). Before
            # 2026-11-23 u jumped first, so a rested synapse transmitted at
            # efficacy 2 - U (1.5x depressing, 1.85x facilitating), not 1;
            # the ep9 STP ablation ran with that bug.
            rel = u * xr * p
            u = torch.where(p > 0, u + U * (1.0 - u), u)
            s["stp_x"], s["stp_u"] = xr - rel, u
            p = rel / U
        return p

    @torch.no_grad()
    def _learn(self, s, err: torch.Tensor) -> None:
        c = self.cfg
        Fb = self.W_out.T if c.feedback == "symmetric" else self.B
        L = err @ Fb                                                         # (n,) per-neuron error
        rate = s["zsum"] / c.sub                                             # spikes/step this frame
        r_err = c.reg * (rate - self.r_step)
        self.W_in -= c.lr * (L + r_err) * s["ebar_in"]
        self.W_rec -= c.lr * (L + r_err) * s["ebar_rec"]
        self.W_rec.fill_diagonal_(0.0)
        if c.dale:                                   # a synapse cannot change sign
            self.W_rec = torch.where(self.W_rec * self.sign[:, None] < 0, torch.zeros_like(self.W_rec), self.W_rec)
        # readout: normalised by presynaptic activity (NLMS), so the step does
        # not grow with the number of active neurons
        zb = s["zbar"]
        self.W_out -= c.lr_out * torch.outer(zb, err) / (1.0 + float(zb @ zb))
        if c.out_decay:
            self.W_out *= 1.0 - c.out_decay
        if c.out_bias:
            self.b_out -= 0.1 * c.lr_out * err

    # ------------------------------------------------------------------ stream
    def step_frame(self, x: torch.Tensor) -> None:
        """Hear one frame: learn from the error of the last prediction, then
        present this frame and predict the next."""
        s = self.state
        if s["pending"] is not None:
            err = s["pending"] - (x - s["x_prev"])
            self._learn(s, err)
            # intrinsic plasticity (homeostatic, independent of the error):
            # each neuron nudges its excitability toward the target rate
            self.h += self.cfg.ip_lr * (self.r_step - s["zsum"] / self.cfg.sub)
        s["pending"] = self._present(s, x, learn=True)
        s["x_prev"] = x
        self.frames += 1

    @torch.no_grad()
    def predict_utterance(self, F: torch.Tensor) -> torch.Tensor:
        """Hear F[:-1] without learning from error, starting from a copy of
        the live listening state; return predictions of F[1:].
        Intrinsic plasticity keeps running (on a copy of h): it uses only the
        neuron's own activity, never the target, and a listening brain does
        not switch it off. Measured 2026-11-22 (readout-only, frozen weights,
        30 test-clean utterances): skill +0.117 with it, +0.023 without."""
        s = self._zero_state()
        for key in ("v", "z", "b", "y"):
            s[key] = self.state[key].clone()
        for key in ("xbar", "stp_u", "stp_x", "zbuf"):
            s[key] = None if self.state[key] is None else self.state[key].clone()
        h = self.h.clone()
        out = []
        for t in range(len(F) - 1):
            out.append(self._present(s, F[t], learn=False, h=h) + F[t])
            h += self.cfg.ip_lr * (self.r_step - s["zsum"] / self.cfg.sub)
        return torch.stack(out)

    def rate_hz(self) -> float:
        return float(self.state["zsum"].mean()) / self.cfg.sub / (self.dt / 1000.0)
