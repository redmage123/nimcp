"""Rung 3 data: next-word prediction on child-directed speech (AO-CHILDES).

Source: the BabyLM 2023 release of AO-CHILDES (Huebner & Willits'
age-ordered CHILDES transcripts), train / dev / test files from the
`original/{10M,dev,test}/aochildes.txt` splits of cambridge-climb/BabyLM on
Hugging Face. CHILDES is distributed by TalkBank for research use.

Words are lower-cased whitespace tokens; punctuation marks ("." "?" "!")
are tokens too and end each utterance. Lines are concatenated into one
stream. The vocabulary is the `vocab_size - 1` most frequent training words
plus <unk>, built from the full training file so every data budget uses the
same vocabulary.
"""
from __future__ import annotations

import collections
import math
from dataclasses import dataclass
from pathlib import Path

import torch

CORPUS = Path.home() / ".cache/athena/corpus/babylm"
FILES = {"train": "aochildes_10M.txt", "dev": "aochildes_dev.txt", "test": "aochildes_test.txt"}
URL = "https://huggingface.co/datasets/cambridge-climb/BabyLM/resolve/main/original/{}/aochildes.txt"
UNK = "<unk>"


def words(split: str, root: Path = CORPUS) -> list[str]:
    path = root / FILES[split]
    if not path.exists():
        raise FileNotFoundError(f"{path} missing; download {URL.format({'train': '10M'}.get(split, split))}")
    return path.read_text(encoding="utf-8").lower().split()


@dataclass
class Vocab:
    itos: list[str]

    def __post_init__(self):
        self.stoi = {w: i for i, w in enumerate(self.itos)}
        self.unk = self.stoi[UNK]

    def __len__(self) -> int:
        return len(self.itos)

    def encode(self, ws: list[str]) -> torch.Tensor:
        return torch.tensor([self.stoi.get(w, self.unk) for w in ws], dtype=torch.long)

    @classmethod
    def build(cls, ws: list[str], size: int) -> "Vocab":
        common = [w for w, _ in collections.Counter(ws).most_common(size - 1)]
        return cls(common + [UNK])


def load(vocab_size: int = 2000, root: Path = CORPUS):
    """(vocab, train ids, dev ids, test ids) as 1-D LongTensors."""
    tr, dv, te = words("train", root), words("dev", root), words("test", root)
    v = Vocab.build(tr, vocab_size)
    return v, v.encode(tr), v.encode(dv), v.encode(te)


def perplexity(nll_sum: float, n: int) -> float:
    """Per-word perplexity from a summed natural-log negative log-likelihood."""
    return math.exp(nll_sum / max(n, 1))


# --- n-gram baselines (no learning rule, just counts) ----------------------
class InterpolatedNgram:
    """Unigram/bigram/trigram with linear interpolation (weights chosen on
    a tuning stream) and add-one unigram, so no probability is zero."""

    def __init__(self, ids: torch.Tensor, V: int, order: int = 3):
        self.V, self.order = V, order
        x = ids.tolist()
        self.c1 = collections.Counter(x)
        self.n = len(x)
        self.c2 = collections.Counter(zip(x, x[1:]))
        self.c3 = collections.Counter(zip(x, x[1:], x[2:])) if order >= 3 else collections.Counter()
        self.h1 = collections.Counter(x[:-1])
        self.h2 = collections.Counter(zip(x, x[1:])) if order >= 3 else collections.Counter()
        self.lam = (1.0, 0.0, 0.0)

    def _probs(self, a, b, w):
        p1 = (self.c1[w] + 1) / (self.n + self.V)
        p2 = self.c2[(b, w)] / self.h1[b] if self.h1[b] else 0.0
        p3 = self.c3[(a, b, w)] / self.h2[(a, b)] if self.h2[(a, b)] else 0.0
        return p1, p2, p3

    def nll(self, ids: torch.Tensor, lam=None) -> tuple[float, int]:
        l1, l2, l3 = lam or self.lam
        x = ids.tolist()
        total, n = 0.0, 0
        for t in range(2, len(x)):
            p1, p2, p3 = self._probs(x[t - 2], x[t - 1], x[t])
            total -= math.log(l1 * p1 + l2 * p2 + l3 * p3)
            n += 1
        return total, n

    def tune(self, ids: torch.Tensor) -> None:
        best = None
        grid = [i / 10 for i in range(11)]
        for l2 in grid:
            for l3 in grid:
                l1 = 1.0 - l2 - l3
                if l1 < 0.05 or (self.order < 3 and l3 > 0):
                    continue
                s, n = self.nll(ids, (l1, l2, l3))
                if best is None or s < best[0]:
                    best = (s, (l1, l2, l3))
        self.lam = best[1]
