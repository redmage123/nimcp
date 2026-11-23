"""Video clips as 32x32 frame sequences, for next-frame prediction.

Sources (already collected for nimcp):
  kubric      Kubric MOVi-C: 571 clips x 24 frames, 12 fps, rendered objects
              tumbling and rolling (128 px JPEG, downsampled here)
  homemovies  2077 clips x 8 frames, 2 fps, archival amateur film (32 px RGB)
Clips are split by clip (10% validation, 10% test), so no held-out clip is
ever trained on.
"""
from __future__ import annotations

import io
import os
import pickle
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import torch

from .cifar import normalize

CACHE_DIR = Path(os.environ.get("ATHENA_CACHE", Path.home() / ".cache" / "athena"))
VIDEO_DIR = Path(os.environ.get("ATHENA_VIDEO_DIR", "/home/bbrelin/nimcp_datasets"))
SOURCES = {"kubric": "kubric_sequences.pkl", "homemovies": "hm_cache/hm_sequences.pkl"}


@dataclass
class Clips:
    frames: torch.Tensor   # uint8, N x T x 3 x 32 x 32
    source: str

    def __len__(self) -> int:
        return len(self.frames)


@dataclass
class VideoData:
    train: dict[str, Clips]
    val: dict[str, Clips]
    test: dict[str, Clips]


def _frame(raw, encoding: str, size: int) -> np.ndarray:
    from PIL import Image

    if encoding == "jpeg":
        img = Image.open(io.BytesIO(raw)).convert("RGB")
    else:
        img = Image.fromarray(np.frombuffer(raw, dtype=np.uint8).reshape(size, size, 3))
    if img.size != (32, 32):
        img = img.resize((32, 32), Image.BILINEAR)
    return np.asarray(img)


def _build(source: str) -> np.ndarray:
    with open(VIDEO_DIR / SOURCES[source], "rb") as f:
        seqs = pickle.load(f)
    t = min(len(s["frames"]) for s in seqs)
    out = np.empty((len(seqs), t, 32, 32, 3), dtype=np.uint8)
    for i, s in enumerate(seqs):
        for j, raw in enumerate(s["frames"][:t]):
            out[i, j] = _frame(raw, s["encoding"], s["size"])
    return out.transpose(0, 1, 4, 2, 3).copy()


def load_video(sources=("kubric", "homemovies"), seed: int = 0) -> VideoData:
    parts: dict[str, dict[str, Clips]] = {"train": {}, "val": {}, "test": {}}
    for src in sources:
        cache = CACHE_DIR / f"video_{src}.npy"
        if not cache.exists():
            CACHE_DIR.mkdir(parents=True, exist_ok=True)
            tmp = CACHE_DIR / f"video_{src}.tmp.npy"
            np.save(tmp, _build(src))
            tmp.replace(cache)
        x = torch.from_numpy(np.load(cache))
        perm = torch.randperm(len(x), generator=torch.Generator().manual_seed(seed))
        n_hold = max(1, len(x) // 10)
        parts["val"][src] = Clips(x[perm[:n_hold]], src)
        parts["test"][src] = Clips(x[perm[n_hold:2 * n_hold]], src)
        parts["train"][src] = Clips(x[perm[2 * n_hold:]], src)
    return VideoData(**parts)


def windows(clips: Clips, ctx: int) -> torch.Tensor:
    """Every (clip, t) such that frames t-ctx+1..t predict frame t+1."""
    n, t = clips.frames.shape[:2]
    c = torch.arange(n).repeat_interleave(t - ctx)
    s = torch.arange(ctx - 1, t - 1).repeat(n)
    return torch.stack([c, s], 1)


def gather(clips: Clips, win: torch.Tensor, ctx: int, device: str) -> tuple[torch.Tensor, torch.Tensor]:
    """Context frames (B x ctx x 3 x 32 x 32) and the next frame (B x 3 x 32 x 32),
    prepared (standardised) on `device`."""
    offs = torch.arange(-ctx + 1, 2)
    idx = win[:, 1:2] + offs[None, :]
    seq = clips.frames[win[:, 0:1], idx]                       # B x (ctx+1) x 3 x 32 x 32
    seq = normalize(seq.flatten(0, 1).to(device)).view(len(win), ctx + 1, 3, 32, 32)
    return seq[:, :ctx], seq[:, ctx]
