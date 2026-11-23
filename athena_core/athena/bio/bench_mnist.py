"""Phase B1, rung 2: few-shot MNIST with a large unlabelled pool.

The 60k training images are the unlabelled pool (a child's experience);
k labelled examples per class are drawn from it (3 seeds); the 10k test
images are held out. The STDP cortex is trained once per size on the pool
(no labels touch its weights) and cached under runs/bio/; every labelled
budget is then scored on the held-out set.

Learners (spiking, local rules)
  snn:hippocampus(pixels)        one-shot Hebbian memory on Poisson pixels
  snn:cortex<n>+assign           Diehl & Cook: neurons labelled by response
  snn:cortex<n>+hebb             one-shot Hebbian class prototypes of the
                                 cortex spike counts (word neuron per class)
  snn:cortex<n>+hippocampus      one-shot hippocampus over the cortex code
Diagnostic (not a biological learner)
  diag:cortex<n>+logreg          logistic regression on the cortex counts:
                                 how good the learned features are
Baselines: logreg, 1nn on pixels; kmeans<n>+logreg on the same pool.

Run: python3 -m athena.bio.bench_mnist [--n 400,1600] [--budgets 1,2,5,10,100]
"""
from __future__ import annotations

import argparse
import gzip
import time
from pathlib import Path

import numpy as np
import torch

from athena import scoreboard
from athena.bio.learners import (Hippocampus, STDPCortex, assign_labels,
                                 predict_assigned)

TASK = "mnist_fewshot"
DATA = Path.home() / ".cache" / "athena" / "mnist"
CACHE = Path(__file__).resolve().parents[2] / "runs" / "bio"


def _idx(name: str, offset: int) -> np.ndarray:
    return np.frombuffer(gzip.open(DATA / f"{name}.gz").read(), np.uint8, offset=offset)


def load():
    Xp = torch.tensor(_idx("train-images-idx3-ubyte", 16).reshape(-1, 784) / 255.0, dtype=torch.float32)
    yp = torch.tensor(_idx("train-labels-idx1-ubyte", 8).astype(np.int64))
    Xte = torch.tensor(_idx("t10k-images-idx3-ubyte", 16).reshape(-1, 784) / 255.0, dtype=torch.float32)
    yte = torch.tensor(_idx("t10k-labels-idx1-ubyte", 8).astype(np.int64))
    return Xp, yp, Xte, yte


def subset(yp, k, seed):
    g = torch.Generator().manual_seed(seed)
    return torch.cat([torch.nonzero(yp == c).flatten()[
        torch.randperm(int((yp == c).sum()), generator=g)[:k]] for c in range(10)])


def cortex(n: int, Xpool: torch.Tensor, epochs: int, steps: int) -> STDPCortex:
    path = CACHE / f"mnist_cortex_n{n}_e{epochs}_pool{len(Xpool)}.pt"
    c = STDPCortex(784, n=n, seed=0, rate=0.064, steps=steps, theta_plus=0.05, theta_decay=0.999)
    if path.exists():
        st = torch.load(path)
        c.W, c.theta, c.updates = st["W"], st["theta"], st["updates"]
        print(f"loaded {path.name}", flush=True)
        return c
    t0 = time.time()
    c.train(Xpool, epochs=epochs)
    CACHE.mkdir(parents=True, exist_ok=True)
    torch.save({"W": c.W, "theta": c.theta, "updates": c.updates}, path)
    print(f"trained cortex n={n} on {len(Xpool)} images x{epochs} in {time.time() - t0:.0f}s", flush=True)
    return c


def respond(c: STDPCortex, X: torch.Tensor, steps: int, chunk: int = 1000) -> torch.Tensor:
    return torch.cat([c.respond(X[i:i + chunk], steps=steps) for i in range(0, len(X), chunk)])


def hebb_prototypes(Rl, yl, Rte):
    """Word neuron c gets one Hebbian increment per labelled example of c;
    recall by normalised overlap (one-shot, local)."""
    M = torch.stack([Rl[yl == c].sum(0) for c in range(10)])
    M = M / M.norm(dim=1, keepdim=True).clamp_min(1e-9)
    return (Rte / Rte.norm(dim=1, keepdim=True).clamp_min(1e-9) @ M.T).argmax(1)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", default="400")
    ap.add_argument("--budgets", default="1,2,5,10,100")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--pool", type=int, default=60000)
    ap.add_argument("--epochs", type=int, default=1)
    ap.add_argument("--steps", type=int, default=100, help="presentation length while learning")
    ap.add_argument("--eval-steps", type=int, default=200)
    ap.add_argument("--skip-baselines", action="store_true")
    a = ap.parse_args(argv)
    torch.set_num_threads(8)
    from sklearn.linear_model import LogisticRegression
    from sklearn.neighbors import KNeighborsClassifier
    Xp, yp, Xte, yte = load()
    Xpool = Xp[:a.pool]
    budgets = [int(b) for b in a.budgets.split(",")]
    subs = {(k, s): subset(yp, k, s) for k in budgets for s in range(a.seeds)}
    table = {}

    def score(name, kind, k, preds, cfg=None, t0=None):
        accs = [float((p == yte).float().mean()) for p in preds]
        table[(name, k)] = (np.mean(accs), np.std(accs))
        scoreboard.record(TASK, kind, name, {"held_top1": round(float(np.mean(accs)), 4),
                                             "held_top1_std": round(float(np.std(accs)), 4)},
                          {"per_class": k, "seeds": a.seeds, **(cfg or {})},
                          None if t0 is None else time.time() - t0)
        print(f"{name:28s} k={k:3d}  {np.mean(accs):.3f} ± {np.std(accs):.3f}", flush=True)

    def logreg(F, Fte, idx):
        m = LogisticRegression(max_iter=2000, C=1.0).fit(F[idx].numpy(), yp[idx].numpy())
        return torch.tensor(m.predict(Fte.numpy()))

    if not a.skip_baselines:
        for k in budgets:
            t0 = time.time()
            score("logreg", "baseline", k, [logreg(Xp, Xte, subs[(k, s)]) for s in range(a.seeds)], t0=t0)
            score("1nn", "baseline", k, [torch.tensor(KNeighborsClassifier(1).fit(
                Xp[subs[(k, s)]].numpy(), yp[subs[(k, s)]].numpy()).predict(Xte.numpy()))
                for s in range(a.seeds)], t0=t0)
            h = []
            for s in range(a.seeds):
                hc = Hippocampus(784, 10, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=s)
                hc.store(hc.code(Xp[subs[(k, s)]]), yp[subs[(k, s)]])
                h.append(torch.cat([hc.recall(hc.code(Xte[i:i + 2000])) for i in range(0, len(Xte), 2000)]))
            score("snn:hippocampus(pixels)", "train", k, h, t0=t0)

    for n in [int(v) for v in a.n.split(",")]:
        from sklearn.cluster import MiniBatchKMeans
        if not a.skip_baselines:
            C = MiniBatchKMeans(n, random_state=0, n_init=3, batch_size=4096).fit(Xpool.numpy()).cluster_centers_
            Ct = torch.tensor(C, dtype=torch.float32)

            def tri(A):
                d = torch.cdist(A, Ct)
                return (d.mean(1, keepdim=True) - d).clamp_min(0)
            Fp_all, Fte_k = tri(Xp), tri(Xte)
            for k in budgets:
                score(f"kmeans{n}+logreg", "baseline", k,
                      [logreg(Fp_all, Fte_k, subs[(k, s)]) for s in range(a.seeds)], {"mode": "semi"})

        c = cortex(n, Xpool, a.epochs, a.steps)
        t0 = time.time()
        Rte = respond(c, Xte, a.eval_steps)
        need = torch.unique(torch.cat(list(subs.values())))
        Rneed = respond(c, Xp[need], a.eval_steps)
        pos = {int(i): j for j, i in enumerate(need)}
        print(f"cortex n={n}: held spikes/img {Rte.sum(1).mean():.1f}, active neurons "
              f"{int((Rte.sum(0) > 0).sum())}/{n}, respond {time.time() - t0:.0f}s", flush=True)
        cfg = {"mode": "semi", "n": n, "pool": len(Xpool), "epochs": a.epochs}
        for k in budgets:
            P = {"assign": [], "hebb": [], "hippo": [], "logreg": []}
            for s in range(a.seeds):
                idx = subs[(k, s)]
                Rl = Rneed[[pos[int(i)] for i in idx]]
                yl = yp[idx]
                P["assign"].append(predict_assigned(Rte, assign_labels(Rl, yl, 10), 10))
                P["hebb"].append(hebb_prototypes(Rl, yl, Rte))
                scale = max(1.0, float(Rl.max()))
                hc = Hippocampus(n, 10, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=s)
                hc.store(hc.code(Rl / scale), yl)
                P["hippo"].append(torch.cat([hc.recall(hc.code((Rte[i:i + 2000] / scale).clamp(0, 1)))
                                             for i in range(0, len(Rte), 2000)]))
                m = LogisticRegression(max_iter=2000, C=1.0).fit(Rl.numpy(), yl.numpy())
                P["logreg"].append(torch.tensor(m.predict(Rte.numpy())))
            score(f"snn:cortex{n}+assign", "train", k, P["assign"], cfg)
            score(f"snn:cortex{n}+hebb", "train", k, P["hebb"], cfg)
            score(f"snn:cortex{n}+hippocampus", "train", k, P["hippo"], cfg)
            score(f"diag:cortex{n}+logreg", "diagnostic", k, P["logreg"], cfg)

    names = list(dict.fromkeys(nm for nm, _ in table))
    print("\n" + "learner".ljust(28) + "".join(f"  k={k:<9d}" for k in budgets))
    for nm in names:
        print(nm.ljust(28) + "".join(f"  {table[(nm, k)][0]:.3f}±{table[(nm, k)][1]:.3f}"
                                      if (nm, k) in table else "  " + "-".ljust(11) for k in budgets))


if __name__ == "__main__":
    main()
