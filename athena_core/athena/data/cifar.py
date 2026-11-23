"""CIFAR-10 / CIFAR-100 as naming tasks: 32x32 RGB images labelled with an
object word."""
from __future__ import annotations

import io
import json
import os
from pathlib import Path

import numpy as np
import torch

from .types import Split, Task

PARQUET_DIR = Path(os.environ.get("ATHENA_CIFAR_DIR", "/home/bbrelin/nimcp_datasets/parquet"))
CACHE_DIR = Path(os.environ.get("ATHENA_CACHE", Path.home() / ".cache" / "athena"))

# Per-channel statistics of the CIFAR-100 training set (0-1 scale); CIFAR-10's
# are within 0.01 of these.
MEAN = (0.5071, 0.4865, 0.4409)
STD = (0.2673, 0.2564, 0.2762)

_LABEL_COLUMN = {"cifar100": "fine_label", "cifar10": "label"}
_TYPOS = {"cra": "crab"}          # the Hugging Face CIFAR-100 label names misspell crab


def _decode(parquet: Path, label_col: str) -> tuple[np.ndarray, np.ndarray, list[str]]:
    import pyarrow.parquet as pq
    from PIL import Image

    table = pq.read_table(parquet)
    meta = json.loads(table.schema.metadata[b"huggingface"])
    names = meta["info"]["features"][label_col]["names"]
    img_col = "img" if "img" in table.column_names else "image"
    imgs = table.column(img_col).to_pylist()
    labels = np.asarray(table.column(label_col).to_pylist(), dtype=np.int64)
    out = np.empty((len(imgs), 32, 32, 3), dtype=np.uint8)
    for i, rec in enumerate(imgs):
        out[i] = np.asarray(Image.open(io.BytesIO(rec["bytes"])).convert("RGB"))
    return out.transpose(0, 3, 1, 2).copy(), labels, names


def _load(which: str) -> Task:
    """Decode the parquet files once, then load from an .npz cache."""
    cache = CACHE_DIR / f"{which}.npz"
    if not cache.exists():
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        col = _LABEL_COLUMN[which]
        xtr, ytr, names = _decode(PARQUET_DIR / f"{which}_train.parquet", col)
        xte, yte, _ = _decode(PARQUET_DIR / f"{which}_test.parquet", col)
        tmp = cache.with_suffix(".tmp.npz")
        np.savez(tmp, xtr=xtr, ytr=ytr, xte=xte, yte=yte, names=np.asarray(names))
        tmp.replace(cache)
    z = np.load(cache)
    words = [_TYPOS.get(str(n), str(n)).replace("_", " ") for n in z["names"]]
    return Task(
        name=which, modality="vision",
        train=Split(torch.from_numpy(z["xtr"]), torch.from_numpy(z["ytr"])),
        test=Split(torch.from_numpy(z["xte"]), torch.from_numpy(z["yte"])),
        words=words, prep=normalize, augment=augment, in_channels=3,
    )


def load_cifar100() -> Task:
    return _load("cifar100")


def load_cifar10() -> Task:
    return _load("cifar10")


def normalize(x: torch.Tensor) -> torch.Tensor:
    """uint8 images -> float, standardised per channel."""
    mean = torch.tensor(MEAN, device=x.device).view(1, 3, 1, 1)
    std = torch.tensor(STD, device=x.device).view(1, 3, 1, 1)
    return (x.float() / 255.0 - mean) / std


def augment(x: torch.Tensor) -> torch.Tensor:
    """Random crop (4-pixel reflection padding) and horizontal flip."""
    n = x.shape[0]
    padded = torch.nn.functional.pad(x, (4, 4, 4, 4), mode="reflect")
    dx = torch.randint(0, 9, (n,), device=x.device)
    dy = torch.randint(0, 9, (n,), device=x.device)
    idx = torch.arange(32, device=x.device)
    rows = (dy[:, None] + idx[None, :])[:, None, :, None].expand(n, 3, 32, 32)
    cols = (dx[:, None] + idx[None, :])[:, None, None, :].expand(n, 3, 32, 32)
    out = padded[torch.arange(n, device=x.device)[:, None, None, None],
                 torch.arange(3, device=x.device)[None, :, None, None],
                 rows, cols]
    flip = torch.rand(n, device=x.device) < 0.5
    out[flip] = out[flip].flip(3)
    return out
