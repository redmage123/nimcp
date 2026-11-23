"""Rung 3 data-efficiency curve (THEORY.md): the complete biological system
against a tuned LSTM at each training budget.

For each budget N (first N training words):
  lstm        backprop LSTM; hidden size (LSTM_SIZES) and weight decay (WD_GRID)
              chosen on dev, best pass on dev
  memory      hippocampal fast memory (hc_lm), one exposure; mixture weights
              fitted on dev
  bio-slow    residual per-neuron-recurrence stack trained by the local rule
              with learned (Kolen-Pollack) feedback (stack_lm); feedback decay
              chosen on dev from KP_GRID, best pass on dev
  BIO         memory + bio-slow, mixing weight fitted on dev
  hybrid      memory + lstm, mixing weight fitted on dev (shows what the
              memory would give a conventional model)
  trigram     interpolated trigram, weights fitted on dev
Selection and fitting use dev[:SEL]; every reported number is per-word
perplexity on test[:TEST] (held out from everything).

Run: CUDA_VISIBLE_DEVICES= python3 -m athena.bio.bench_curve --budgets 3000,10000,30000
"""
from __future__ import annotations

import argparse
import copy
import math
import time

import torch

from athena import scoreboard
from athena.bio.bench_lm import _Recurrent
from athena.bio.childes import InterpolatedNgram, load, perplexity
from athena.bio.hc_lm import HcLM, HcLMConfig
from athena.bio.stack_lm import StackLM, StackLMConfig

TASK = "childes_nextword_curve"
SEL, TEST = 10000, 20000
WD_GRID = (0.0, 1e-3, 3e-3, 1e-2, 3e-2)
LSTM_SIZES = (128, 256)
KP_GRID = (3e-3, 1e-2, 3e-2)
PROFILES = ((1.0,), (1.0, 1.0), (1.0, 0.8, 0.64, 0.51, 0.41, 0.33, 0.26, 0.21))


def passes_for(N: int) -> int:
    """About 300k presented words, between 5 and 30 passes."""
    return int(min(30, max(5, 300000 // N)))


def ppl(p: torch.Tensor) -> float:
    return math.exp(-float(torch.log(p.clamp_min(1e-12)).mean()))


# --- per-position probability of the true next word -----------------------
def lstm_probs(m, ids):
    with torch.no_grad():
        lg, _ = m(ids[:-1].unsqueeze(0))
        return torch.softmax(lg[0], 1)[torch.arange(len(ids) - 1), ids[1:]]


def stack_probs(net, ids):
    st = net.init_state(1, learn=False)
    out = torch.empty(len(ids) - 1)
    for i in range(len(ids) - 1):
        out[i] = net.step(st, ids[i:i + 1])[0, ids[i + 1]].exp()
    return out


def hc_probs(m, ids):
    return torch.cat([P @ m.lam for P in m._probs(ids, 2048)])


# --- learners --------------------------------------------------------------
def train_lstm(V, tr, sel, passes, wd, seed, n=256, lr=3e-3, batch=16, L=32):
    torch.manual_seed(seed)
    m = _Recurrent(V, n, "lstm")
    dec = [p for k, p in m.named_parameters() if "weight" in k]
    opt = torch.optim.Adam(m.parameters(), lr=lr)
    S = tr[: len(tr) // batch * batch].view(batch, -1)
    best = (math.inf, None)
    for _ in range(passes):
        h = None
        for s in range(0, S.shape[1] - 1, L):
            x, y = S[:, s:s + L], S[:, s + 1:s + 1 + L]
            k = min(x.shape[1], y.shape[1])
            if k == 0:
                continue
            lg, h = m(x[:, :k], h)
            h = tuple(z.detach() for z in h)
            loss = torch.nn.functional.cross_entropy(lg.reshape(-1, V), y[:, :k].reshape(-1))
            opt.zero_grad()
            loss.backward()
            opt.step()
            with torch.no_grad():
                for p in dec:
                    p.mul_(1 - wd)
        d = ppl(lstm_probs(m, sel))
        if d < best[0]:
            best = (d, copy.deepcopy(m))
    return best


def train_stack(V, tr, sel, passes, kp_decay, seed):
    net = StackLM(V, StackLMConfig(d=128, n=256, layers=2, feedback="kp", kp_decay=kp_decay,
                                   update_every=32, seed=seed))
    best = (math.inf, None)
    for _ in range(passes):
        net.train_pass(tr)
        d = perplexity(*net.nll(sel))
        if d < best[0]:
            best = (d, copy.deepcopy(net))
    return best


def fit_mix(a_sel, b_sel):
    return min((ppl(x * a_sel + (1 - x) * b_sel), x) for x in [i / 20 for i in range(21)])[1]


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--budgets", default="3000,10000,30000,100000,439000")
    ap.add_argument("--seed", type=int, default=0)
    a = ap.parse_args(argv)
    torch.set_num_threads(8)
    Vo, tr_all, dv, te = load(2000)
    V = len(Vo)
    sel, test = dv[:SEL], te[:TEST]
    for N in [int(b) for b in a.budgets.split(",")]:
        t0 = time.time()
        tr, P = tr_all[:N], passes_for(N)
        lstm = min((train_lstm(V, tr, sel, P, wd, a.seed, n=hs) + ((wd, hs),) for wd in WD_GRID
                    for hs in LSTM_SIZES), key=lambda r: r[0])
        L_sel, L_te = lstm_probs(lstm[1], sel), lstm_probs(lstm[1], test)
        stack = min((train_stack(V, tr, sel, P, kd, a.seed) + (kd,) for kd in KP_GRID), key=lambda r: r[0])
        S_sel, S_te = stack_probs(stack[1], sel), stack_probs(stack[1], test)
        hc = HcLM(V, HcLMConfig(profiles=PROFILES, combine="and", seed=a.seed))
        hc.store(tr)
        hc.tune(sel)
        H_sel, H_te = hc_probs(hc, sel), hc_probs(hc, test)
        n_s, n_t = min(len(H_sel), len(S_sel), len(L_sel)), min(len(H_te), len(S_te), len(L_te))
        H_sel, S_sel, L_sel = H_sel[:n_s], S_sel[:n_s], L_sel[:n_s]
        H_te, S_te, L_te = H_te[:n_t], S_te[:n_t], L_te[:n_t]
        wb, wh = fit_mix(H_sel, S_sel), fit_mix(H_sel, L_sel)
        g = InterpolatedNgram(tr, V)
        g.tune(sel)
        res = {"lstm": ppl(L_te), "memory": ppl(H_te), "bio_slow": ppl(S_te),
               "BIO": ppl(wb * H_te + (1 - wb) * S_te), "hybrid": ppl(wh * H_te + (1 - wh) * L_te),
               "trigram": perplexity(*g.nll(test))}
        res = {k: round(v, 2) for k, v in res.items()}
        cfg = {"train_words": N, "passes": P, "lstm_wd": lstm[2], "kp_decay": stack[2],
               "mix_bio": wb, "mix_hybrid": wh, "seed": a.seed}
        scoreboard.record(TASK, "train", "curve", {f"test_ppl_{k}": v for k, v in res.items()}, cfg,
                          time.time() - t0)
        gain = 100 * (1 - res["BIO"] / res["lstm"])
        print(f"N={N:7d} test ppl  " + "  ".join(f"{k} {v:.1f}" for k, v in res.items())
              + f"   BIO vs LSTM {gain:+.1f}%   ({time.time() - t0:.0f}s; {cfg})", flush=True)


if __name__ == "__main__":
    main()
