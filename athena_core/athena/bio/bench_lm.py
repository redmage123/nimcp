"""Rung 3 benchmark (THEORY.md): next-word prediction on AO-CHILDES.

For each training budget (the first N words of the training stream), every
learner trains for up to `passes` passes; after each pass it is scored on a
dev slice, and the best pass is scored on a test slice. Reported: held-out
per-word perplexity (lower is better), next to the baselines.

Learners
  ngram      interpolated uni/bi/trigram (counts; weights tuned on dev)
  rnn        tanh recurrent net, the e-prop net's shapes (one-hot -> n -> n ->
             V), trained by backprop through time + Adam (conventional control)
  lstm       LSTM with the same hidden size, backprop + Adam (stronger control)
  snn:eprop  recurrent LIF network trained by e-prop (eprop_lm.py)
  hc:memory  hippocampal fast memory: sparse dentate codes of decaying word
             traces, one-shot Hebbian storage (hc_lm.py); its 4 mixture
             weights are fitted on dev, like the n-gram's interpolation
  diag:local      per-neuron (diagonal) gated recurrence, rate neurons, exact
                  local traces + RANDOM feedback (the admissible biological rule)
  diag:local-sym  same, symmetric feedback = the exact gradient (check)
  diag:bptt       same network, backprop through time + RMSprop (control)

Run: CUDA_VISIBLE_DEVICES= python3 -m athena.bio.bench_lm --budgets 10000,30000 --only ngram,rnn,snn:eprop
"""
from __future__ import annotations

import argparse
import math
import time

import torch

from athena import scoreboard
from athena.bio.childes import InterpolatedNgram, load, perplexity
from athena.bio.eprop_lm import EpropLM, EpropLMConfig
from athena.bio.hc_lm import HcLM, HcLMConfig
from athena.bio.diag_lm import DiagLM, DiagLMConfig, DiagLMTorch

TASK = "childes_nextword"


class _Recurrent(torch.nn.Module):
    def __init__(self, V: int, n: int, kind: str):
        super().__init__()
        self.emb = torch.nn.Embedding(V, n)
        self.rnn = (torch.nn.LSTM if kind == "lstm" else torch.nn.RNN)(n, n, batch_first=True)
        self.out = torch.nn.Linear(n, V)

    def forward(self, x, h=None):
        o, h = self.rnn(self.emb(x), h)
        return self.out(o), h


def _detach(h):
    return tuple(t.detach() for t in h) if isinstance(h, tuple) else h.detach()


def _bp_nll(m, ids, batch=64):
    S = EpropLM.streams(ids, batch)
    with torch.no_grad():
        logits, _ = m(S[:, :-1])
        nll = torch.nn.functional.cross_entropy(logits.reshape(-1, logits.shape[-1]),
                                                S[:, 1:].reshape(-1), reduction="sum")
    return float(nll), S[:, 1:].numel()


def run_backprop(kind, V, tr, dv, te, n, passes, seed, batch=16, bptt=32, lr=3e-3):
    torch.manual_seed(seed)
    m = _Recurrent(V, n, kind)
    opt = torch.optim.Adam(m.parameters(), lr=lr)
    S = EpropLM.streams(tr, batch)
    best = (math.inf, None)
    for _ in range(passes):
        h = None
        for s in range(0, S.shape[1] - 1, bptt):
            x, y = S[:, s:s + bptt], S[:, s + 1:s + 1 + bptt]
            L = min(x.shape[1], y.shape[1])
            logits, h = m(x[:, :L], h)
            h = _detach(h)
            loss = torch.nn.functional.cross_entropy(logits.reshape(-1, V), y[:, :L].reshape(-1))
            opt.zero_grad()
            loss.backward()
            torch.nn.utils.clip_grad_norm_(m.parameters(), 1.0)
            opt.step()
        d = perplexity(*_bp_nll(m, dv))
        if d < best[0]:
            best = (d, perplexity(*_bp_nll(m, te)))
    return best


def run_eprop(V, tr, dv, te, n, passes, seed, cfg_kw=None):
    net = EpropLM(V, EpropLMConfig(n_rec=n, seed=seed, **(cfg_kw or {})))
    best = (math.inf, None)
    for _ in range(passes):
        net.train_pass(tr)
        d = perplexity(*net.nll(dv))
        if d < best[0]:
            best = (d, perplexity(*net.nll(te)))
    return best


def run_hc(V, tr, dv, te, seed):
    m = HcLM(V, HcLMConfig(seed=seed))
    m.store(tr)
    m.tune(dv)
    return perplexity(*m.nll(dv)), perplexity(*m.nll(te))


def run_diag(V, tr, dv, te, n, passes, seed, feedback, lr):
    net = DiagLM(V, DiagLMConfig(n=n, feedback=feedback, lr=lr, seed=seed))
    best = (math.inf, None)
    for _ in range(passes):
        net.train_pass(tr)
        d = perplexity(*net.nll(dv))
        if d < best[0]:
            best = (d, perplexity(*net.nll(te)))
    return best


def run_diag_bptt(V, tr, dv, te, n, passes, seed, lr=3e-3, batch=16, bptt=32):
    torch.manual_seed(seed)
    net = DiagLM(V, DiagLMConfig(n=n, seed=seed))
    m = DiagLMTorch(net)
    opt = torch.optim.RMSprop(m.parameters(), lr=lr, alpha=0.99)
    S = DiagLM.streams(tr, batch)
    best = (math.inf, None)
    for _ in range(passes):
        h = None
        for s in range(0, S.shape[1] - 1, bptt):
            x, y = S[:, s:s + bptt], S[:, s + 1:s + 1 + bptt]
            L = min(x.shape[1], y.shape[1])
            logits, h = m(x[:, :L], h)
            h = h.detach()
            loss = torch.nn.functional.cross_entropy(logits.reshape(-1, V), y[:, :L].reshape(-1))
            opt.zero_grad()
            loss.backward()
            opt.step()
        for k in DiagLM.PARAMS:
            setattr(net, k, getattr(m, k).detach().clone())
        d = perplexity(*net.nll(dv))
        if d < best[0]:
            best = (d, perplexity(*net.nll(te)))
    return best


def run_ngram(V, tr, dv, te):
    g = InterpolatedNgram(tr, V)
    g.tune(dv)
    return perplexity(*g.nll(dv)), perplexity(*g.nll(te))


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--budgets", default="10000,30000,100000")
    ap.add_argument("--vocab", type=int, default=2000)
    ap.add_argument("--hidden", type=int, default=256)
    ap.add_argument("--passes", type=int, default=5)
    ap.add_argument("--eval_words", type=int, default=20000)
    ap.add_argument("--seeds", type=int, default=1)
    ap.add_argument("--only", default="")
    ap.add_argument("--eprop", default="", help="EpropLMConfig overrides, e.g. adaptive_frac=0.4,lr=2e-3")
    a = ap.parse_args(argv)
    torch.set_num_threads(8)
    V, tr_all, dv_all, te_all = load(a.vocab)
    Vn = len(V)
    dv, te = dv_all[: a.eval_words], te_all[: a.eval_words]
    ekw = {k: float(v) for k, v in (p.split("=") for p in a.eprop.split(",") if p)}
    learners = {
        "ngram": lambda tr, s: run_ngram(Vn, tr, dv, te),
        "rnn": lambda tr, s: run_backprop("rnn", Vn, tr, dv, te, a.hidden, a.passes, s),
        "lstm": lambda tr, s: run_backprop("lstm", Vn, tr, dv, te, a.hidden, a.passes, s),
        "snn:eprop": lambda tr, s: run_eprop(Vn, tr, dv, te, a.hidden, a.passes, s, ekw),
        "hc:memory": lambda tr, s: run_hc(Vn, tr, dv, te, s),
        "diag:local": lambda tr, s: run_diag(Vn, tr, dv, te, a.hidden, a.passes, s, "random", 1e-3),
        "diag:local-sym": lambda tr, s: run_diag(Vn, tr, dv, te, a.hidden, a.passes, s, "symmetric", 1e-3),
        "diag:bptt": lambda tr, s: run_diag_bptt(Vn, tr, dv, te, a.hidden, a.passes, s),
    }
    kinds = {"ngram": "baseline", "rnn": "baseline", "lstm": "baseline", "snn:eprop": "train",
             "hc:memory": "train", "diag:local": "train", "diag:local-sym": "check",
             "diag:bptt": "baseline"}
    for N in [int(b) for b in a.budgets.split(",")]:
        tr = tr_all[:N]
        for name, fn in learners.items():
            if a.only and name not in a.only.split(","):
                continue
            t0 = time.time()
            res = [fn(tr, s) for s in range(a.seeds)]
            dev_ppl = sum(r[0] for r in res) / len(res)
            test_ppl = sum(r[1] for r in res) / len(res)
            scoreboard.record(TASK, kinds[name], name,
                              {"dev_ppl": round(dev_ppl, 2), "test_ppl": round(test_ppl, 2)},
                              {"train_words": N, "vocab": Vn, "hidden": a.hidden, "passes": a.passes,
                               "seeds": a.seeds, "eprop": ekw if name == "snn:eprop" else None},
                              time.time() - t0)
            print(f"{name:10s} N={N:7d}  dev ppl {dev_ppl:8.2f}  test ppl {test_ppl:8.2f}  "
                  f"({time.time() - t0:.0f}s)", flush=True)


if __name__ == "__main__":
    main()
