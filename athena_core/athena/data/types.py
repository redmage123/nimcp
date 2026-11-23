"""Containers shared by every modality."""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable

import torch


@dataclass
class Split:
    inputs: torch.Tensor   # N x C x H x W (uint8 images or float16 spectrograms)
    labels: torch.Tensor   # int64, N, index into the task's own word list

    def __len__(self) -> int:
        return len(self.labels)

    def subset(self, idx: torch.Tensor) -> "Split":
        return Split(self.inputs[idx], self.labels[idx])


def _identity(x: torch.Tensor) -> torch.Tensor:
    return x


@dataclass
class Task:
    """One naming task: a modality, its data and its words.

    `prep` turns stored inputs into model inputs (on the model's device);
    `augment` perturbs a prepared training batch.
    """
    name: str
    modality: str                      # "vision" | "audio"
    train: Split
    test: Split
    words: list[str]
    val: Split | None = None
    prep: Callable[[torch.Tensor], torch.Tensor] = _identity
    augment: Callable[[torch.Tensor], torch.Tensor] = _identity
    in_channels: int = 3
    word_ids: torch.Tensor = field(default=None)   # task label -> joint vocabulary index

    def __post_init__(self):
        if self.word_ids is None:
            self.word_ids = torch.arange(len(self.words))
