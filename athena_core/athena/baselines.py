"""Reference scores a learner has to beat before its number means anything.
Each baseline scores one task on that task's own words."""
from __future__ import annotations

import torch

from .data import Task


def topk_accuracy(scores: torch.Tensor, labels: torch.Tensor, ks=(1, 5)) -> dict[str, float]:
    k_max = min(max(ks), scores.shape[1])
    top = scores.topk(k_max, dim=1).indices
    hit = top == labels[:, None]
    return {f"top{k}": hit[:, :min(k, k_max)].any(1).float().mean().item() for k in ks}


def _flat(task: Task, x: torch.Tensor) -> torch.Tensor:
    return task.prep(x).float().flatten(1)


def chance(task: Task) -> dict[str, float]:
    v = len(task.words)
    return {"top1": 1.0 / v, "top5": min(5, v) / v}


def majority(task: Task) -> dict[str, float]:
    counts = torch.bincount(task.train.labels, minlength=len(task.words)).float()
    return topk_accuracy(counts.expand(len(task.test), -1), task.test.labels)


def ridge(task: Task, lams=(1.0, 10.0, 100.0, 1e3, 1e4, 1e5), val_frac: float = 0.1,
          chunk: int = 5000) -> dict[str, float]:
    """Closed-form linear map from raw inputs to one-hot words. The
    regularisation weight is chosen on a validation slice of the training
    split, so the baseline is as strong as a linear map fairly gets."""
    v = len(task.words)
    n = len(task.train)
    perm = torch.randperm(n, generator=torch.Generator().manual_seed(0))
    n_val = max(1, int(n * val_frac))
    va, tr = perm[:n_val], perm[n_val:]
    xs, ys = task.train.inputs[tr], task.train.labels[tr]
    d = _flat(task, xs[:1]).shape[1]

    mu = torch.zeros(d, dtype=torch.float64)
    for i in range(0, len(tr), chunk):
        mu += _flat(task, xs[i:i + chunk]).double().sum(0)
    mu /= len(tr)
    xtx = torch.zeros(d, d, dtype=torch.float64)
    xty = torch.zeros(d, v, dtype=torch.float64)
    for i in range(0, len(tr), chunk):
        x = _flat(task, xs[i:i + chunk]).double() - mu
        xtx += x.t() @ x
        xty += x.t() @ torch.nn.functional.one_hot(ys[i:i + chunk], v).double()

    def scores(inputs: torch.Tensor, w: torch.Tensor) -> torch.Tensor:
        return torch.cat([(_flat(task, inputs[i:i + chunk]).double() - mu) @ w
                          for i in range(0, len(inputs), chunk)])

    eye = torch.eye(d, dtype=torch.float64)
    best_lam, best_acc, best_w = None, -1.0, None
    for lam in lams:
        w = torch.linalg.solve(xtx + lam * eye, xty)
        acc = topk_accuracy(scores(task.train.inputs[va], w), task.train.labels[va])["top1"]
        if acc > best_acc:
            best_lam, best_acc, best_w = lam, acc, w
    out = topk_accuracy(scores(task.test.inputs, best_w), task.test.labels)
    out["lambda"] = best_lam
    return out


def knn(task: Task, k: int = 1, chunk: int = 1000) -> dict[str, float]:
    """Nearest training items by cosine similarity of the prepared inputs;
    for k > 1, votes are summed per word."""
    v = len(task.words)
    train = torch.cat([torch.nn.functional.normalize(_flat(task, task.train.inputs[i:i + 10000]), dim=1)
                       for i in range(0, len(task.train), 10000)])
    out = []
    for i in range(0, len(task.test), chunk):
        q = torch.nn.functional.normalize(_flat(task, task.test.inputs[i:i + chunk]), dim=1)
        nn_idx = (q @ train.t()).topk(k, dim=1).indices
        votes = torch.zeros(len(q), v)
        votes.scatter_add_(1, task.train.labels[nn_idx], torch.ones(len(q), k))
        votes += 1e-3 * torch.rand_like(votes)   # break ties for top-5
        out.append(votes)
    return topk_accuracy(torch.cat(out), task.test.labels)


BASELINES = {
    "chance": chance,
    "majority": majority,
    "ridge": ridge,
    "knn1": lambda t: knn(t, k=1),
    "knn10": lambda t: knn(t, k=10),
}


def text_baselines(corpus) -> dict[str, dict[str, float]]:
    """Held-out test perplexity (per word, like the Text objective) of n-gram
    models over the same vocabulary, counted on the canonical books' training
    text: add-one unigram, and a bigram interpolated with the unigram (weight
    chosen on validation text)."""
    import math

    v = len(corpus.vocab)
    starts = corpus.vocab.word_starts()
    tr = corpus.train[:corpus.n_canonical_train].long()
    uni = torch.bincount(tr, minlength=v).double() + 1.0
    p_uni = uni / uni.sum()
    pair = tr[:-1] * v + tr[1:]
    keys, counts = pair.unique(return_counts=True)
    prev_counts = torch.bincount(tr[:-1], minlength=v).double()
    table = dict(zip(keys.tolist(), counts.double().tolist()))

    def nll(ids: torch.Tensor, lam: float | None) -> float:
        ids = ids.long()
        words = int(starts[ids[1:]].sum())
        if lam is None:
            return -p_uni[ids[1:]].log().sum().item() / words
        prev, nxt = ids[:-1], ids[1:]
        k = (prev * v + nxt).tolist()
        c = torch.tensor([table.get(x, 0.0) for x in k], dtype=torch.float64)
        p_bi = c / prev_counts[prev].clamp(min=1.0)
        p = lam * p_bi + (1 - lam) * p_uni[nxt]
        return -p.log().sum().item() / words

    best = min((nll(corpus.val, lam), lam) for lam in (0.3, 0.5, 0.6, 0.7, 0.8, 0.9))[1]
    return {"unigram": {"ppl": math.exp(nll(corpus.test, None))},
            "bigram": {"ppl": math.exp(nll(corpus.test, best)), "lambda": best}}
