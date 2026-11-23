"""Phase B1 benchmark: few-shot naming with spiking learners vs baselines.

Task: sklearn handwritten digits (8x8, 10 classes). Fixed split: 797
held-out images; the other 1000 are the pool. For each budget of k labelled
examples per class (3 seeds), every learner is scored on the held-out set.

Modes
  strict  the learner sees only the k labelled images.
  semi    the learner also sees the whole pool without labels (a child's
          unlabelled experience); conventional baselines then get the same
          pool (k-means features).

Run: python3 -m athena.bio.bench [--budgets 1,2,5] [--seeds 3]
"""
from __future__ import annotations

import argparse
import time

import numpy as np
import torch

from athena import scoreboard
from athena.bio.learners import Hippocampus, RSTDPReadout, STDPCortex

TASK = "digits_fewshot"


def load():
    from sklearn.datasets import load_digits
    from sklearn.model_selection import train_test_split
    X, y = load_digits(return_X_y=True)
    X = torch.tensor(X / 16.0, dtype=torch.float32)
    y = torch.tensor(y)
    Xp, Xte, yp, yte = train_test_split(X, y, test_size=797, stratify=y, random_state=0)
    return Xp, yp, Xte, yte


def subset(yp: torch.Tensor, k: int, seed: int) -> torch.Tensor:
    g = torch.Generator().manual_seed(seed)
    return torch.cat([torch.nonzero(yp == c).flatten()[
        torch.randperm(int((yp == c).sum()), generator=g)[:k]] for c in range(10)])


# --- learners: each returns held-out predictions --------------------------
def hippocampus_pixels(Xp, yp, idx, Xte, seed):
    h = Hippocampus(Xp.shape[1], 10, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=seed)
    h.store(h.code(Xp[idx]), yp[idx])
    return h.recall(h.code(Xte))


def _cortex(Xp, idx, seed, semi):
    c = STDPCortex(Xp.shape[1], n=100, seed=seed, theta_plus=0.05, theta_decay=0.999)
    c.train(Xp if semi else Xp[idx], epochs=3 if semi else 10)
    return c


def cortex_hippocampus(semi):
    def run(Xp, yp, idx, Xte, seed):
        c = _cortex(Xp, idx, seed, semi)
        Rl, Rte = c.respond(Xp[idx], steps=200), c.respond(Xte, steps=200)
        scale = max(1.0, float(Rl.max()))
        h = Hippocampus(c.n, 10, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=seed)
        h.store(h.code(Rl / scale), yp[idx])
        return h.recall(h.code((Rte / scale).clamp(0, 1)))
    return run


def rstdp_pixels(Xp, yp, idx, Xte, seed, epochs=60):
    r = RSTDPReadout(Xp.shape[1], 10, seed=seed)
    g = torch.Generator().manual_seed(seed)
    for _ in range(epochs):
        for i in idx[torch.randperm(len(idx), generator=g)]:
            r.learn(Xp[i], int(yp[i]))
    return r.predict(Xte)


# --- baselines ------------------------------------------------------------
def _sk(model):
    def run(Xp, yp, idx, Xte, seed):
        m = model(seed).fit(Xp[idx].numpy(), yp[idx].numpy())
        return torch.tensor(m.predict(Xte.numpy()))
    return run


def kmeans_logreg(Xp, yp, idx, Xte, seed, n=400):
    from sklearn.cluster import KMeans
    from sklearn.linear_model import LogisticRegression
    C = KMeans(n, n_init=1, random_state=seed).fit(Xp.numpy()).cluster_centers_

    def tri(A):
        d = np.linalg.norm(A[:, None] - C[None], axis=2)
        return np.maximum(0, d.mean(1, keepdims=True) - d)
    m = LogisticRegression(max_iter=3000, C=10).fit(tri(Xp[idx].numpy()), yp[idx].numpy())
    return torch.tensor(m.predict(tri(Xte.numpy())))


def learners():
    from sklearn.linear_model import LogisticRegression
    from sklearn.neighbors import KNeighborsClassifier
    from sklearn.neural_network import MLPClassifier
    return {
        # name: (kind, mode, fn)
        "logreg": ("baseline", "strict", _sk(lambda s: LogisticRegression(max_iter=3000, C=10))),
        "1nn": ("baseline", "strict", _sk(lambda s: KNeighborsClassifier(1))),
        "mlp256": ("baseline", "strict", _sk(lambda s: MLPClassifier((256,), max_iter=2000, random_state=s))),
        "kmeans400+logreg": ("baseline", "semi", kmeans_logreg),
        "snn:hippocampus": ("train", "strict", hippocampus_pixels),
        "snn:cortex+hippocampus": ("train", "strict", cortex_hippocampus(semi=False)),
        "snn:cortex(pool)+hippocampus": ("train", "semi", cortex_hippocampus(semi=True)),
        "snn:rstdp": ("train", "strict", rstdp_pixels),
    }


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--budgets", default="1,2,5,10")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--only", default="")
    a = ap.parse_args(argv)
    torch.set_num_threads(8)
    Xp, yp, Xte, yte = load()
    budgets = [int(b) for b in a.budgets.split(",")]
    table = {}
    for name, (kind, mode, fn) in learners().items():
        if a.only and name not in a.only.split(","):
            continue
        for k in budgets:
            t0 = time.time()
            accs = [float((fn(Xp, yp, subset(yp, k, s), Xte, s) == yte).float().mean())
                    for s in range(a.seeds)]
            table[(name, k)] = (np.mean(accs), np.std(accs))
            scoreboard.record(TASK, kind, name, {"held_top1": round(float(np.mean(accs)), 4),
                                                 "held_top1_std": round(float(np.std(accs)), 4)},
                              {"per_class": k, "seeds": a.seeds, "mode": mode}, time.time() - t0)
            print(f"{name:30s} {mode:6s} k={k:3d}  {np.mean(accs):.3f} ± {np.std(accs):.3f}", flush=True)
    names = list(dict.fromkeys(n for n, _ in table))
    print("\n" + "learner".ljust(30) + "".join(f"  k={k:<9d}" for k in budgets))
    for n in names:
        print(n.ljust(30) + "".join(f"  {table[(n, k)][0]:.3f}±{table[(n, k)][1]:.3f}" if (n, k) in table
                                     else "  " + "-".ljust(11) for k in budgets))


if __name__ == "__main__":
    main()
