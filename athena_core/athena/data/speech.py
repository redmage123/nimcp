"""Speech Commands v0.02 as a naming task: one-second clips of 35 spoken words.

Clips become 64-band log-mel spectrograms (10 ms hop, 101 frames), cached once.
Splits follow the dataset's own validation/testing lists, which keep each
speaker in one split only.
"""
from __future__ import annotations

import os
from pathlib import Path

import numpy as np
import torch

from .types import Split, Task

CACHE_DIR = Path(os.environ.get("ATHENA_CACHE", Path.home() / ".cache" / "athena"))
SPEECH_DIR = Path(os.environ.get("ATHENA_SPEECH_DIR", CACHE_DIR / "speech_commands"))
SR = 16000
N_MELS = 64
FRAMES = 101


def _features(paths: list[Path], batch: int = 512) -> np.ndarray:
    import soundfile as sf
    import torchaudio

    mel = torchaudio.transforms.MelSpectrogram(sample_rate=SR, n_fft=400, hop_length=160, n_mels=N_MELS)
    out = np.empty((len(paths), 1, N_MELS, FRAMES), dtype=np.float16)
    for i in range(0, len(paths), batch):
        wav = np.zeros((min(batch, len(paths) - i), SR), dtype=np.float32)
        for j, p in enumerate(paths[i:i + batch]):
            d, sr = sf.read(p, dtype="float32")
            assert sr == SR, (p, sr)
            wav[j, :min(SR, len(d))] = d[:SR]
        spec = torch.log(mel(torch.from_numpy(wav)) + 1e-6)          # B x 64 x 101
        out[i:i + len(wav), 0] = spec.numpy().astype(np.float16)
    return out


def _build_cache(cache: Path) -> None:
    words = sorted(p.name for p in SPEECH_DIR.iterdir() if p.is_dir() and not p.name.startswith("_"))
    val = set((SPEECH_DIR / "validation_list.txt").read_text().split())
    test = set((SPEECH_DIR / "testing_list.txt").read_text().split())
    parts = {"tr": ([], []), "va": ([], []), "te": ([], [])}
    for label, w in enumerate(words):
        for f in sorted((SPEECH_DIR / w).glob("*.wav")):
            rel = f"{w}/{f.name}"
            key = "va" if rel in val else "te" if rel in test else "tr"
            parts[key][0].append(f)
            parts[key][1].append(label)
    arrays = {}
    for key, (paths, labels) in parts.items():
        arrays[f"x{key}"] = _features(paths)
        arrays[f"y{key}"] = np.asarray(labels, dtype=np.int64)
    x = arrays["xtr"].astype(np.float32)
    arrays["mean"] = np.float32(x.mean())
    arrays["std"] = np.float32(x.std())
    tmp = cache.with_suffix(".tmp.npz")
    np.savez(tmp, words=np.asarray(words), **arrays)
    tmp.replace(cache)


def load_speech() -> Task:
    cache = CACHE_DIR / "speech_commands.npz"
    if not cache.exists():
        _build_cache(cache)
    z = np.load(cache)
    mean, std = float(z["mean"]), float(z["std"])

    def prep(x: torch.Tensor) -> torch.Tensor:
        return (x.float() - mean) / std

    def split(key: str) -> Split:
        return Split(torch.from_numpy(z[f"x{key}"]), torch.from_numpy(z[f"y{key}"]))

    return Task(name="speech", modality="audio", train=split("tr"), val=split("va"),
                test=split("te"), words=[str(w) for w in z["words"]],
                prep=prep, augment=augment, in_channels=1)


def augment(x: torch.Tensor, max_shift: int = 10, max_f: int = 8, max_t: int = 12) -> torch.Tensor:
    """Random time shift (up to 100 ms, zero fill) plus one frequency mask and
    one time mask (SpecAugment), on a prepared batch (N x 1 x 64 x 101)."""
    n, _, f, t = x.shape
    dev = x.device
    shift = torch.randint(-max_shift, max_shift + 1, (n,), device=dev)
    cols = torch.arange(t, device=dev)[None, :] - shift[:, None]          # N x T
    valid = (cols >= 0) & (cols < t)
    cols = cols.clamp(0, t - 1)
    out = x.gather(3, cols[:, None, None, :].expand(n, 1, f, t))
    fill = x.amin(dim=(1, 2, 3), keepdim=True)                            # silence level
    out = torch.where(valid[:, None, None, :], out, fill)
    f0 = torch.randint(0, f, (n,), device=dev)
    fw = torch.randint(0, max_f + 1, (n,), device=dev)
    t0 = torch.randint(0, t, (n,), device=dev)
    tw = torch.randint(0, max_t + 1, (n,), device=dev)
    fi = torch.arange(f, device=dev)[None, :]
    ti = torch.arange(t, device=dev)[None, :]
    fmask = (fi >= f0[:, None]) & (fi < (f0 + fw)[:, None])               # N x F
    tmask = (ti >= t0[:, None]) & (ti < (t0 + tw)[:, None])               # N x T
    mask = fmask[:, None, :, None] | tmask[:, None, None, :]
    return torch.where(mask, fill, out)
