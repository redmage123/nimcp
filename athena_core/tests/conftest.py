import pytest
import torch

from athena import scoreboard
from athena.data import Split, Task


@pytest.fixture(autouse=True)
def tmp_scoreboard(tmp_path, monkeypatch):
    """Keep test runs out of the real scoreboard."""
    path = tmp_path / "scoreboard.jsonl"
    monkeypatch.setattr(scoreboard, "SCOREBOARD", path)
    return path


def _prep_uint8(x: torch.Tensor) -> torch.Tensor:
    return x.float() / 127.5 - 1.0


def _synthetic(name, modality, channels, words, seed, n_tr=200, n_te=100) -> Task:
    """Each word is a fixed random pattern plus noise, so any working learner
    (and the linear baselines) should name it."""
    g = torch.Generator().manual_seed(seed)
    v = len(words)
    patterns = torch.randint(0, 256, (v, channels, 32, 32), generator=g).float()

    def make(n):
        y = torch.arange(n) % v
        x = (patterns[y] + torch.randn(n, channels, 32, 32, generator=g) * 40).clamp(0, 255)
        return Split(x.to(torch.uint8), y)

    return Task(name=name, modality=modality, train=make(n_tr), test=make(n_te), words=list(words),
                prep=_prep_uint8, in_channels=channels)


@pytest.fixture
def pattern_task() -> Task:
    return _synthetic("seen", "vision", 3, [f"w{i}" for i in range(5)], seed=0)


@pytest.fixture
def two_senses() -> list[Task]:
    """A seen task and a heard task sharing three of their words."""
    seen = _synthetic("seen", "vision", 3, ["cat", "dog", "bed", "car", "cup"], seed=1)
    heard = _synthetic("heard", "audio", 1, ["cat", "dog", "bed", "yes", "no"], seed=2)
    return [seen, heard]
