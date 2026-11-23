"""One word list for every task, so the same word means the same thing
whether it was seen or heard."""
from __future__ import annotations

import torch

from .types import Task


def build_vocab(tasks: list[Task]) -> list[str]:
    """Union of the tasks' words, in first-seen order. Sets each task's
    word_ids (its label -> the joint index) in place."""
    vocab: list[str] = []
    index: dict[str, int] = {}
    for t in tasks:
        for w in t.words:
            if w not in index:
                index[w] = len(vocab)
                vocab.append(w)
        t.word_ids = torch.tensor([index[w] for w in t.words], dtype=torch.int64)
    return vocab


def shared_words(tasks: list[Task]) -> list[str]:
    """Words that every given task can name."""
    common = set(tasks[0].words)
    for t in tasks[1:]:
        common &= set(t.words)
    return [w for w in tasks[0].words if w in common]
