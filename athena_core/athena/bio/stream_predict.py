"""Listening by prediction (THEORY.md algorithm 1) on a live speech stream.

Every learner hears the same stream once, frame by frame (40 ms frames,
intensities in [0, 1]) and, at every frame, predicts the
next frame from the last w frames (default 5 = 200 ms) and then learns from
its error. All predict the change from the current frame, so the zero
predictor is "the sound stays as it is" (copy).

Front ends (--front)
  logmel   32 log-mel bands (athena.data.librispeech --emit), mapped to [0, 1]
           with percentiles of the held-out set.
  cochlea  nimcp's cochlea (nimcp/tools/cochlea_emit): 64 auditory-nerve
           channels, spike counts per 40 ms frame across each channel's
           fibres; intensity = count / COCHLEA_SAT (a fixed physiological
           scale: ~302 is saturation, ~25 the spontaneous rate).

Learners
  snn:stdp+delta   spiking STDP cortex (LIF, winner-take-all, adaptive
                   threshold) hears the context window as Poisson spikes and
                   learns its detectors from the stream; D output neurons
                   read the cortex spike counts and learn with the delta rule
                   (presynaptic activity x their own prediction error:
                   a per-neuron error, THEORY algorithm 2, one layer).
  linear           the same context, raw, through a linear map learned by
                   normalised LMS (conventional, also a local rule).
  lstm             an LSTM over the frame sequence, trained online with
                   truncated backprop through time and Adam (conventional).
  copy             no learning: predicts no change.

Score on held-out speakers (LibriSpeech test-clean, never streamed):
  skill = 1 - MSE(learner) / MSE(copy)   (0 = no better than copy, 1 = exact)
reported at several points along the stream (a learning curve in hours heard).
Recurrent learners (lstm, eprop) hear each held-out utterance starting from
a copy of their live listening state, without learning.

Live with nimcp's cochlea as the front end (nothing stored):
  curl -sL https://www.openslr.org/resources/12/train-clean-100.tar.gz | \\
      python3 -m athena.data.librispeech --emit-pcm | nimcp/build/cochlea_emit | \\
      ssh <host> 'cd athena_core && python3 -m athena.bio.stream_predict --stdin --front cochlea'

Live with log-mel frames (nothing stored):
  curl -sL https://www.openslr.org/resources/12/train-clean-100.tar.gz | \\
      python3 -m athena.data.librispeech --emit | \\
      ssh <host> 'cd athena_core && python3 -m athena.bio.stream_predict --stdin --heldout test-clean.npz'
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np
import torch

from athena import scoreboard
from athena.bio.learners import STDPCortex
from athena.bio.wernicke import WernickeConfig, WernickePredictor
from athena.data import librispeech

TASK = "speech_next_frame_stream"
CACHE = Path(__file__).resolve().parents[2] / "runs" / "bio"


def unit_range(frames: np.ndarray) -> tuple[float, float]:
    """Fixed intensity map from the held-out set's percentiles (a property of
    the front end, identical for every learner)."""
    x = torch.tensor(frames[::7].astype(np.float32)).flatten()
    q = torch.quantile(x[::5], torch.tensor([0.02, 0.98]))
    return float(q[0]), float(q[1])


COCHLEA_SAT = 320.0


class Front:
    """Maps raw frames (log-mel or cochlear spike counts) to [0, 1]."""

    def __init__(self, kind: str, heldout_frames: np.ndarray):
        self.kind = kind
        self.dim = heldout_frames.shape[1]
        if kind == "cochlea":
            self.lo, self.hi = 0.0, COCHLEA_SAT
        else:
            self.lo, self.hi = unit_range(heldout_frames)

    def __call__(self, f: np.ndarray) -> torch.Tensor:
        return torch.tensor(((f.astype(np.float32) - self.lo) / (self.hi - self.lo)).clip(0, 1))


class SNNPredictor:
    def __init__(self, w: int, n: int, rate: float, lr: float, dim: int = 32, seed: int = 0):
        self.w = w
        self.cortex = STDPCortex(dim * w, n=n, seed=seed, rate=rate, steps=50,
                                 theta_plus=0.05, theta_decay=0.999)
        self.R = torch.zeros(n, dim)              # cortex -> output synapses
        self.b = torch.zeros(dim)
        self.lr = lr

    def _code(self, ctx: torch.Tensor, learn: bool) -> torch.Tensor:
        h = self.cortex.present(ctx, learn=learn)
        return h / max(1.0, float(h.sum()))      # firing-rate code

    def step(self, ctx: torch.Tensor, target_delta: torch.Tensor):
        h = self._code(ctx, learn=True)
        err = target_delta - (h @ self.R + self.b)
        self.R += self.lr * torch.outer(h, err) / (float(h @ h) + 0.05)
        self.b += 0.01 * self.lr * err

    def predict(self, C: torch.Tensor) -> torch.Tensor:
        Hs = self.cortex.respond(C, steps=self.cortex.steps)
        Hs = Hs / Hs.sum(1, keepdim=True).clamp_min(1.0)
        return Hs @ self.R + self.b


class LinearPredictor:
    def __init__(self, w: int, lr: float, dim: int = 32):
        self.W = torch.zeros(dim * w, dim)
        self.b = torch.zeros(dim)
        self.lr = lr
        self.eps = 0.05 * dim * w                 # ~ energy of a quiet context in [0, 1]

    def step(self, ctx, target_delta):
        err = target_delta - (ctx @ self.W + self.b)
        # NLMS with a floor on the normaliser: near-silent contexts have
        # almost no energy, and dividing by it alone made the steps explode.
        self.W += self.lr * torch.outer(ctx, err) / (float(ctx @ ctx) + self.eps)
        self.b += 0.01 * self.lr * err

    def predict(self, C):
        return C @ self.W + self.b


class LSTMPredictor:
    def __init__(self, hidden: int, lr: float, tbptt: int, dim: int = 32, seed: int = 0):
        torch.manual_seed(seed)
        self.rnn = torch.nn.LSTM(dim, hidden, batch_first=True)
        self.out = torch.nn.Linear(hidden, dim)
        self.opt = torch.optim.Adam(list(self.rnn.parameters()) + list(self.out.parameters()), lr=lr)
        self.tbptt, self.buf, self.state = tbptt, [], None

    def step_frame(self, x: torch.Tensor):
        self.buf.append(x)
        if len(self.buf) == self.tbptt + 1:
            seq = torch.stack(self.buf)[None]               # 1, T+1, D
            y, self.state = self.rnn(seq[:, :-1], self.state)
            pred = self.out(y) + seq[:, :-1]
            loss = torch.nn.functional.mse_loss(pred, seq[:, 1:])
            self.opt.zero_grad()
            loss.backward()
            self.opt.step()
            self.state = tuple(s.detach() for s in self.state)
            self.buf = self.buf[-1:]

    @torch.no_grad()
    def predict_utterance(self, F: torch.Tensor) -> torch.Tensor:
        # from the live (carried) state, as for the spiking predictor
        y, _ = self.rnn(F[None, :-1], self.state)
        return (self.out(y) + F[None, :-1])[0]             # predictions of F[1:]


def heldout_set(path: str, front: "Front", w: int, n_utts: int, seed: int = 0):
    z = np.load(path)
    starts = list(z["start"]) + [len(z["frames"])]
    g = np.random.default_rng(seed)
    pick = sorted(g.choice(len(starts) - 1, size=min(n_utts, len(starts) - 1), replace=False))
    utts = [front(z["frames"][starts[i]:starts[i + 1]]) for i in pick]
    C, T, P = [], [], []
    for F in utts:                                  # windows inside each utterance
        for t in range(w - 1, len(F) - 1):
            C.append(F[t - w + 1:t + 1].T.reshape(-1))
            T.append(F[t + 1])
            P.append(F[t])
    return utts, torch.stack(C), torch.stack(T), torch.stack(P)


def evaluate(learners, utts, C, T, P, w):
    mse_copy = float(((T - P) ** 2).mean())
    out = {"copy_mse": mse_copy}
    for name, L in learners.items():
        if name in ("lstm", "eprop"):
            se, n = 0.0, 0
            for F in utts:
                pr = L.predict_utterance(F)[w - 1:]
                se += float(((pr - F[w:]) ** 2).sum())
                n += pr.numel()
            mse = se / n
        else:
            pred = torch.cat([L.predict(C[i:i + 4000]) for i in range(0, len(C), 4000)]) + P
            mse = float(((pred - T) ** 2).mean())
        out[name] = 1 - mse / mse_copy
    return out


def frame_stream(source, front: "Front"):
    for _, f in source:
        for x in front(f):
            yield x


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--stdin", action="store_true")
    ap.add_argument("--split", default="dev-clean")
    ap.add_argument("--front", choices=["logmel", "cochlea"], default="logmel")
    ap.add_argument("--heldout", default="", help="held-out frames .npz (default: test-clean for the front)")
    ap.add_argument("--hours", type=float, default=0)
    ap.add_argument("--eval-at", default="0.1,0.25,0.5,1,2,4,8,16,32,64,100")
    ap.add_argument("--eval-utts", type=int, default=150)
    ap.add_argument("--w", type=int, default=5)
    ap.add_argument("--n", type=int, default=400)
    ap.add_argument("--rate", type=float, default=0.064)
    ap.add_argument("--snn-lr", type=float, default=0.05)
    ap.add_argument("--lin-lr", type=float, default=0.01)
    ap.add_argument("--lstm-hidden", type=int, default=128)
    ap.add_argument("--lstm-lr", type=float, default=1e-3)
    ap.add_argument("--threads", type=int, default=4)
    ap.add_argument("--only", default="snn,linear,lstm")
    ap.add_argument("--ep-n", type=int, default=256)
    ap.add_argument("--ep-lr", type=float, default=2e-3)
    ap.add_argument("--ep-lr-out", type=float, default=0.05)
    ap.add_argument("--ep-feedback", choices=["random", "symmetric"], default="random")
    ap.add_argument("--ep-in-gain", type=float, default=4.0)
    ap.add_argument("--ep-ip", type=float, default=0.1)
    ap.add_argument("--ep-reg", type=float, default=0.1)
    ap.add_argument("--ep-ffi", type=float, default=2000.0, help="feedforward-inhibition window ms (0 off)")
    ap.add_argument("--ep-seed", type=int, default=0)
    ap.add_argument("--ep-out-bias", action="store_true")
    ap.add_argument("--ep-out-decay", type=float, default=0.0)
    ap.add_argument("--ep-stp", action="store_true")
    ap.add_argument("--ep-delay", type=float, default=0.0, help="max axonal delay ms")
    ap.add_argument("--ep-dale", action="store_true")
    ap.add_argument("--lstm-seed", type=int, default=0)
    a = ap.parse_args(argv)
    torch.set_num_threads(a.threads)

    if not a.heldout:
        a.heldout = str(librispeech.CACHE_DIR / ("test-clean_cochlea.npz" if a.front == "cochlea"
                                                 else "test-clean.npz"))
    front = Front(a.front, np.load(a.heldout)["frames"])
    utts, C, T, P = heldout_set(a.heldout, front, a.w, a.eval_utts)
    D = front.dim
    print(f"front {a.front} ({D} channels); held-out: {len(utts)} utterances, {len(T)} predicted frames; "
          f"intensity map [{front.lo:.2f}, {front.hi:.2f}]", flush=True)
    if a.stdin:
        reader = librispeech.read_cochlea if a.front == "cochlea" else librispeech.read_frames
        source, a.split = reader(sys.stdin.buffer), "stdin"
    else:
        if a.front == "cochlea":
            z = np.load(librispeech.build_cochlea(a.split))
            source = ((None, f) for f in np.split(z["frames"], z["start"][1:]))
        else:
            z = librispeech.load(a.split)
            source = ((u, f) for u, f in zip(z["utt"], np.split(z["frames"], z["start"][1:])))

    only = a.only.split(",")
    learners = {}
    if "snn" in only:
        learners["snn"] = SNNPredictor(a.w, a.n, a.rate, a.snn_lr, dim=D)
    if "linear" in only:
        learners["linear"] = LinearPredictor(a.w, a.lin_lr, dim=D)
    if "lstm" in only:
        learners["lstm"] = LSTMPredictor(a.lstm_hidden, a.lstm_lr, tbptt=25, dim=D, seed=a.lstm_seed)
    if "eprop" in only:
        learners["eprop"] = WernickePredictor(D, WernickeConfig(
            n_rec=a.ep_n, lr=a.ep_lr, lr_out=a.ep_lr_out, feedback=a.ep_feedback, in_gain=a.ep_in_gain,
            ip_lr=a.ep_ip, reg=a.ep_reg, ffi_tau=a.ep_ffi, seed=a.ep_seed,
            out_bias=a.ep_out_bias, out_decay=a.ep_out_decay,
            stp=a.ep_stp, max_delay=a.ep_delay, dale=a.ep_dale))
    marks = [float(h) for h in a.eval_at.split(",")]
    hist, ctx = [], []
    t0, frames_heard = time.time(), 0
    cfg = {"w": a.w, "n": a.n, "rate": a.rate, "snn_lr": a.snn_lr, "lin_lr": a.lin_lr,
           "lstm_hidden": a.lstm_hidden, "stream": a.split, "front": a.front,
           "ep_n": a.ep_n, "ep_lr": a.ep_lr, "ep_lr_out": a.ep_lr_out, "ep_feedback": a.ep_feedback,
           "ep_ip": a.ep_ip, "ep_reg": a.ep_reg, "ep_ffi": a.ep_ffi, "ep_seed": a.ep_seed, "ep_out_bias": a.ep_out_bias, "ep_out_decay": a.ep_out_decay,
           "ep_stp": a.ep_stp, "ep_delay": a.ep_delay, "ep_dale": a.ep_dale, "lstm_seed": a.lstm_seed, "lstm_lr": a.lstm_lr}

    def report(hours):
        r = evaluate(learners, utts, C, T, P, a.w)
        hist.append((hours, r))
        print(f"[{hours:6.2f} h heard, {time.time() - t0:6.0f}s] skill vs copy: " +
              "  ".join(f"{k} {v:+.3f}" for k, v in r.items() if k != "copy_mse") +
              (f"  [eprop {learners['eprop'].rate_hz():.1f} Hz]" if "eprop" in learners else ""), flush=True)
        scoreboard.record(TASK, "train", "stream_predict",
                          {k: round(v, 5) for k, v in r.items()}, {**cfg, "hours": round(hours, 3)},
                          time.time() - t0)

    report(0.0)
    for x in frame_stream(source, front):
        if "lstm" in learners:
            learners["lstm"].step_frame(x)
        if "eprop" in learners:
            learners["eprop"].step_frame(x)
        if len(ctx) == a.w:
            c = torch.stack(ctx).T.reshape(-1)
            d = x - ctx[-1]
            for name in ("snn", "linear"):
                if name in learners:
                    learners[name].step(c, d)
        ctx = (ctx + [x])[-a.w:]
        frames_heard += 1
        hours = frames_heard * 0.04 / 3600
        if marks and hours >= marks[0]:
            report(marks.pop(0))
        if a.hours and hours >= a.hours:
            break
    report(frames_heard * 0.04 / 3600)
    if "snn" in learners:
        CACHE.mkdir(parents=True, exist_ok=True)
        s = learners["snn"]
        torch.save({"W": s.cortex.W, "theta": s.cortex.theta, "R": s.R, "b": s.b, "hours": hours},
                   CACHE / f"predict_snn_{a.front}_{a.split}_h{hours:.1f}_n{a.n}_w{a.w}.pt")


if __name__ == "__main__":
    main()
