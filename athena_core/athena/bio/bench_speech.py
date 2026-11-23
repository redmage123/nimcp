"""Phase B1, rung 2 (THEORY.md): name a spoken word from k examples.

Speech Commands v0.02, 35 words, one-second clips as log-mel spectrograms
(64 bands x 101 frames, athena.data.speech cache). Each clip is averaged to
32 bands x 25 frames (800 values) and mapped to [0, 1] with fixed percentiles
of the training pool: the intensities become Poisson rates, a cochlear-nerve
like code. The clip is presented whole (static), as in rung 1; presenting
it over time and predicting the next frame (algorithm 1) come later.

The ~85k training clips are the unlabelled pool (listening experience);
k labelled clips per word are drawn from it (3 seeds); the test clips are
held out. The STDP cortex is trained once per size on the pool and cached
under runs/bio/.

Learners (spiking, local rules): snn:hippocampus(features),
snn:cortex<n>+assign / +hebb / +hippocampus (as in bench_mnist).
Diagnostic: diag:cortex<n>+logreg.
Baselines: logreg, 1nn, mlp256 on the same features; kmeans<n>(pool)+logreg;
cnn: a small convolutional net on the full spectrogram trained on the k
labelled clips (the same-size conventional net of the rung's criterion).

Run: python3 -m athena.bio.bench_speech [--n 400] [--budgets 1,2,5,10]
"""
from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np
import torch

from athena import scoreboard
from athena.bio.bench_mnist import respond
from athena.bio.learners import Hippocampus, STDPCortex, assign_labels, predict_assigned

TASK = "speech_fewshot"
NPZ = Path.home() / ".cache" / "athena" / "speech_commands.npz"
CACHE = Path(__file__).resolve().parents[2] / "runs" / "bio"


def _pool(x: np.ndarray) -> torch.Tensor:
    t = torch.tensor(x[:, 0, :, :100].astype(np.float32))
    return torch.nn.functional.avg_pool2d(t[:, None], (2, 4)).flatten(1)


def load(max_test: int | None = None):
    d = np.load(NPZ)
    Xp, Xte = _pool(d["xtr"]), _pool(d["xte"])
    lo, hi = torch.quantile(Xp[::17].flatten()[::3], torch.tensor([0.02, 0.98]))
    Xp, Xte = ((Xp - lo) / (hi - lo)).clamp(0, 1), ((Xte - lo) / (hi - lo)).clamp(0, 1)
    yp, yte = torch.tensor(d["ytr"]), torch.tensor(d["yte"])
    if max_test:
        Xte, yte = Xte[:max_test], yte[:max_test]
    return Xp, yp, Xte, yte, [str(w) for w in d["words"]]


def cortex(n: int, Xpool: torch.Tensor, epochs: int, steps: int, rate: float) -> STDPCortex:
    path = CACHE / f"speech_cortex_n{n}_e{epochs}_pool{len(Xpool)}_r{rate}.pt"
    c = STDPCortex(Xpool.shape[1], n=n, seed=0, rate=rate, steps=steps,
                   theta_plus=0.05, theta_decay=0.999)
    if path.exists():
        st = torch.load(path)
        c.W, c.theta, c.updates = st["W"], st["theta"], st["updates"]
        print(f"loaded {path.name}", flush=True)
        return c
    t0 = time.time()
    c.train(Xpool, epochs=epochs)
    CACHE.mkdir(parents=True, exist_ok=True)
    torch.save({"W": c.W, "theta": c.theta, "updates": c.updates}, path)
    print(f"trained cortex n={n} on {len(Xpool)} clips x{epochs} in {time.time() - t0:.0f}s", flush=True)
    return c


class SmallCNN(torch.nn.Module):
    """Conventional baseline on the 32x25 features: two conv blocks, global
    average pooling, linear readout (~6k parameters)."""

    def __init__(self, n_words: int):
        super().__init__()
        self.f = torch.nn.Sequential(
            torch.nn.Conv2d(1, 16, 3, padding=1), torch.nn.ReLU(), torch.nn.MaxPool2d(2),
            torch.nn.Conv2d(16, 32, 3, padding=1), torch.nn.ReLU(), torch.nn.AdaptiveAvgPool2d(1))
        self.out = torch.nn.Linear(32, n_words)

    def forward(self, x):
        return self.out(self.f(x.view(-1, 1, 32, 25)).flatten(1))


def cnn_predict(Xl, yl, Xte, n_words, seed, epochs=200):
    torch.manual_seed(seed)
    m = SmallCNN(n_words)
    opt = torch.optim.Adam(m.parameters(), lr=3e-3, weight_decay=1e-4)
    for _ in range(epochs):
        opt.zero_grad()
        torch.nn.functional.cross_entropy(m(Xl), yl).backward()
        opt.step()
    with torch.no_grad():
        return torch.cat([m(Xte[i:i + 4096]).argmax(1) for i in range(0, len(Xte), 4096)])


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", default="400")
    ap.add_argument("--budgets", default="1,2,5,10")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--pool", type=int, default=0, help="0 = every training clip")
    ap.add_argument("--max-test", type=int, default=0)
    ap.add_argument("--epochs", type=int, default=1)
    ap.add_argument("--rate", type=float, default=0.064)
    ap.add_argument("--steps", type=int, default=100)
    ap.add_argument("--eval-steps", type=int, default=200)
    ap.add_argument("--threads", type=int, default=8)
    ap.add_argument("--skip-baselines", action="store_true")
    a = ap.parse_args(argv)
    torch.set_num_threads(a.threads)
    from sklearn.linear_model import LogisticRegression
    from sklearn.neighbors import KNeighborsClassifier
    from sklearn.neural_network import MLPClassifier
    Xp, yp, Xte, yte, words = load(a.max_test or None)
    nw = len(words)
    Xpool = Xp[:a.pool] if a.pool else Xp
    budgets = [int(b) for b in a.budgets.split(",")]
    subs = {(k, s): _subset(yp, k, s, nw) for k in budgets for s in range(a.seeds)}
    print(f"words {nw}, pool {len(Xpool)}, held {len(Xte)}, chance {1 / nw:.3f}", flush=True)
    table = {}

    def score(name, kind, k, preds, cfg=None, t0=None):
        accs = [float((p == yte).float().mean()) for p in preds]
        table[(name, k)] = (np.mean(accs), np.std(accs))
        scoreboard.record(TASK, kind, name, {"held_top1": round(float(np.mean(accs)), 4),
                                             "held_top1_std": round(float(np.std(accs)), 4)},
                          {"per_class": k, "seeds": a.seeds, **(cfg or {})},
                          None if t0 is None else time.time() - t0)
        print(f"{name:28s} k={k:3d}  {np.mean(accs):.3f} ± {np.std(accs):.3f}", flush=True)

    def sk(model, F, Fte, idx):
        return torch.tensor(model.fit(F[idx].numpy(), yp[idx].numpy()).predict(Fte.numpy()))

    if not a.skip_baselines:
        for k in budgets:
            t0 = time.time()
            S = [subs[(k, s)] for s in range(a.seeds)]
            score("logreg", "baseline", k, [sk(LogisticRegression(max_iter=2000), Xp, Xte, i) for i in S], t0=t0)
            score("1nn", "baseline", k, [sk(KNeighborsClassifier(1), Xp, Xte, i) for i in S], t0=t0)
            score("mlp256", "baseline", k, [sk(MLPClassifier((256,), max_iter=1000, random_state=s), Xp, Xte, i)
                                            for s, i in enumerate(S)], t0=t0)
            score("cnn", "baseline", k, [cnn_predict(Xp[i], yp[i], Xte, nw, s) for s, i in enumerate(S)], t0=t0)
            h = []
            for s, i in enumerate(S):
                hc = Hippocampus(Xp.shape[1], nw, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=s)
                hc.store(hc.code(Xp[i]), yp[i])
                h.append(torch.cat([hc.recall(hc.code(Xte[j:j + 2000])) for j in range(0, len(Xte), 2000)]))
            score("snn:hippocampus(features)", "train", k, h, t0=t0)

    for n in [int(v) for v in a.n.split(",")]:
        if not a.skip_baselines:
            from sklearn.cluster import MiniBatchKMeans
            Ct = torch.tensor(MiniBatchKMeans(n, random_state=0, n_init=3, batch_size=4096)
                              .fit(Xpool.numpy()).cluster_centers_, dtype=torch.float32)

            def tri(A):
                return torch.cat([(lambda d: (d.mean(1, keepdim=True) - d).clamp_min(0))(torch.cdist(A[j:j + 8192], Ct))
                                  for j in range(0, len(A), 8192)])
            Fp, Fte = tri(Xp), tri(Xte)
            for k in budgets:
                score(f"kmeans{n}+logreg", "baseline", k,
                      [sk(LogisticRegression(max_iter=2000), Fp, Fte, subs[(k, s)]) for s in range(a.seeds)],
                      {"mode": "semi"})

        c = cortex(n, Xpool, a.epochs, a.steps, a.rate)
        t0 = time.time()
        Rte = respond(c, Xte, a.eval_steps)
        need = torch.unique(torch.cat(list(subs.values())))
        Rneed = respond(c, Xp[need], a.eval_steps)
        pos = {int(i): j for j, i in enumerate(need)}
        print(f"cortex n={n}: held spikes/clip {Rte.sum(1).mean():.1f}, active "
              f"{int((Rte.sum(0) > 0).sum())}/{n}, respond {time.time() - t0:.0f}s", flush=True)
        cfg = {"mode": "semi", "n": n, "pool": len(Xpool), "epochs": a.epochs, "rate": a.rate}
        for k in budgets:
            P = {"assign": [], "hebb": [], "hippo": [], "logreg": []}
            for s in range(a.seeds):
                idx = subs[(k, s)]
                Rl, yl = Rneed[[pos[int(i)] for i in idx]], yp[idx]
                P["assign"].append(predict_assigned(Rte, assign_labels(Rl, yl, nw), nw))
                P["hebb"].append(_hebb(Rl, yl, Rte, nw))
                scale = max(1.0, float(Rl.max()))
                hc = Hippocampus(n, nw, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=s)
                hc.store(hc.code(Rl / scale), yl)
                P["hippo"].append(torch.cat([hc.recall(hc.code((Rte[j:j + 2000] / scale).clamp(0, 1)))
                                             for j in range(0, len(Rte), 2000)]))
                P["logreg"].append(torch.tensor(LogisticRegression(max_iter=2000)
                                                .fit(Rl.numpy(), yl.numpy()).predict(Rte.numpy())))
            score(f"snn:cortex{n}+assign", "train", k, P["assign"], cfg)
            score(f"snn:cortex{n}+hebb", "train", k, P["hebb"], cfg)
            score(f"snn:cortex{n}+hippocampus", "train", k, P["hippo"], cfg)
            score(f"diag:cortex{n}+logreg", "diagnostic", k, P["logreg"], cfg)

    names = list(dict.fromkeys(nm for nm, _ in table))
    print("\n" + "learner".ljust(28) + "".join(f"  k={k:<9d}" for k in budgets))
    for nm in names:
        print(nm.ljust(28) + "".join(f"  {table[(nm, k)][0]:.3f}±{table[(nm, k)][1]:.3f}"
                                      if (nm, k) in table else "  " + "-".ljust(11) for k in budgets))


def _subset(yp, k, seed, n_words):
    g = torch.Generator().manual_seed(seed)
    return torch.cat([torch.nonzero(yp == c).flatten()[
        torch.randperm(int((yp == c).sum()), generator=g)[:k]] for c in range(n_words)])


def _hebb(Rl, yl, Rte, n_words):
    M = torch.stack([Rl[yl == c].sum(0) for c in range(n_words)])
    M = M / M.norm(dim=1, keepdim=True).clamp_min(1e-9)
    return (Rte / Rte.norm(dim=1, keepdim=True).clamp_min(1e-9) @ M.T).argmax(1)


if __name__ == "__main__":
    main()
