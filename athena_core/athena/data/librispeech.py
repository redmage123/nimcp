"""LibriSpeech as a simulated real-time listening stream.

Audiobook speech is converted once, with the same front end as Speech
Commands (16 kHz, 400-point FFT, 10 ms hop, 64 mel bands, log energy), then
averaged to 32 bands x 40 ms frames (as athena.bio.bench_speech feeds the
spiking cortex), so a cortex that has listened to the stream can be tested
on spoken-word naming directly.

Order is the order of listening: speaker by speaker, chapter by chapter,
utterance by utterance, concatenated into one stream of frames. Utterance
starts, speakers, chapters and transcripts are kept for later grounding.

Cache: ~/.cache/athena/librispeech/<split>.npz with
  frames  float16 (T, 32)   raw log-mel, pooled (map to [0, 1] downstream)
  start   int64 (U,)        first frame of each utterance
  speaker int32 (U,), chapter int32 (U,), utt (U,) ids, text (U,) transcripts

Build: python3 -m athena.data.librispeech dev-clean test-clean train-clean-100

Stream (nothing written to disk): stream_utterances(fileobj) reads a
LibriSpeech .tar.gz as it arrives and yields each utterance's pooled frames
as soon as its audio has been decoded. A live feed splits this in two:
  curl <url> | python3 -m athena.data.librispeech --emit | ssh <learner host> \
      'python3 -m athena.bio.stream_listen --stdin'
the sensor side decodes audio and sends only frames (32 x float16 per 40 ms);
the learner side reads them with read_frames.
"""
from __future__ import annotations

import io
import os
import sys
import tarfile
from pathlib import Path

import numpy as np
import torch

CACHE_DIR = Path(os.environ.get("ATHENA_CACHE", Path.home() / ".cache" / "athena")) / "librispeech"
SR, N_MELS, HOP = 16000, 64, 160
POOL_BANDS, POOL_FRAMES = 2, 4           # -> 32 bands, 40 ms frames


def _extract(split: str) -> Path:
    root = CACHE_DIR / "LibriSpeech" / split
    if not root.exists():
        with tarfile.open(CACHE_DIR / f"{split}.tar.gz") as t:
            t.extractall(CACHE_DIR, filter="data")
    return root


def build(split: str) -> Path:
    import soundfile as sf
    import torchaudio

    out = CACHE_DIR / f"{split}.npz"
    if out.exists():
        return out
    root = _extract(split)
    mel = torchaudio.transforms.MelSpectrogram(sample_rate=SR, n_fft=400, hop_length=HOP, n_mels=N_MELS)
    chunks, start, spk, chap, utt, text = [], [], [], [], [], []
    t = 0
    for sdir in sorted(root.iterdir(), key=lambda p: int(p.name)):
        for cdir in sorted(sdir.iterdir(), key=lambda p: int(p.name)):
            trans = dict(line.split(" ", 1) for line in
                         (cdir / f"{sdir.name}-{cdir.name}.trans.txt").read_text().splitlines())
            for f in sorted(cdir.glob("*.flac")):
                wav, sr = sf.read(f, dtype="float32")
                assert sr == SR, (f, sr)
                spec = torch.log(mel(torch.from_numpy(wav)) + 1e-6)              # 64 x F
                F = spec.shape[1] // POOL_FRAMES * POOL_FRAMES
                if F == 0:
                    continue
                p = torch.nn.functional.avg_pool2d(spec[None, None, :, :F], (POOL_BANDS, POOL_FRAMES))[0, 0]
                chunks.append(p.T.numpy().astype(np.float16))                    # frames x 32
                start.append(t)
                t += chunks[-1].shape[0]
                spk.append(int(sdir.name))
                chap.append(int(cdir.name))
                utt.append(f.stem)
                text.append(trans[f.stem].strip())
        print(f"{split}: speaker {sdir.name} done, {t} frames ({t * 0.04 / 3600:.2f} h)", flush=True)
    tmp = out.with_suffix(".tmp.npz")
    np.savez(tmp, frames=np.concatenate(chunks), start=np.asarray(start, np.int64),
             speaker=np.asarray(spk, np.int32), chapter=np.asarray(chap, np.int32),
             utt=np.asarray(utt), text=np.asarray(text))
    tmp.replace(out)
    return out


def _frontend():
    import torchaudio
    mel = torchaudio.transforms.MelSpectrogram(sample_rate=SR, n_fft=400, hop_length=HOP, n_mels=N_MELS)

    def frames(wav: np.ndarray) -> np.ndarray | None:
        spec = torch.log(mel(torch.from_numpy(wav)) + 1e-6)                      # 64 x F
        F = spec.shape[1] // POOL_FRAMES * POOL_FRAMES
        if F == 0:
            return None
        p = torch.nn.functional.avg_pool2d(spec[None, None, :, :F], (POOL_BANDS, POOL_FRAMES))[0, 0]
        return p.T.numpy().astype(np.float16)                                     # frames x 32
    return frames


def stream_wavs(fileobj):
    """Yield (utterance_id, mono float32 PCM at SR) from a LibriSpeech
    .tar.gz read sequentially from fileobj, in archive order, holding one
    audio file in memory at a time. A feed that stops mid-file just ends."""
    import gzip
    import zlib

    import soundfile as sf
    t = tarfile.open(fileobj=fileobj, mode="r|gz")
    members = iter(t)
    while True:
        try:
            m = next(members)
            data = t.extractfile(m).read() if m.isfile() and m.name.endswith(".flac") else None
        except (StopIteration, tarfile.ReadError, EOFError, gzip.BadGzipFile, zlib.error, OSError):
            return
        if data is None:
            continue
        wav, sr = sf.read(io.BytesIO(data), dtype="float32")
        assert sr == SR, (m.name, sr)
        yield Path(m.name).stem, wav


def stream_utterances(fileobj):
    """Yield (utterance_id, frames (F, 32) float16) log-mel frames."""
    frames = _frontend()
    for u, wav in stream_wavs(fileobj):
        f = frames(wav)
        if f is not None:
            yield u, f


def emit_frames(fileobj, out):
    """Sensor side of a live feed: decode a LibriSpeech .tar.gz arriving on
    fileobj and write each utterance's frames to out as uint32 n_frames then
    n_frames x 32 float16. Only these compact frames cross the network."""
    n = 0
    try:
        for _, f in stream_utterances(fileobj):
            out.write(np.uint32(f.shape[0]).tobytes())
            out.write(np.ascontiguousarray(f, dtype=np.float16).tobytes())
            n += 1
            if n % 200 == 0:
                out.flush()
        out.flush()
    except BrokenPipeError:                      # the learner has heard enough
        try:
            sys.stdout = None
        except Exception:
            pass


def read_frames(fileobj):
    """Learner side: yield (None, frames (F, 32) float16) from emit_frames
    output until the stream ends."""
    while True:
        head = fileobj.read(4)
        if len(head) < 4:
            return
        n = int(np.frombuffer(head, np.uint32)[0])
        body = fileobj.read(n * 32 * 2)
        if len(body) < n * 32 * 2:
            return
        yield None, np.frombuffer(body, np.float16).reshape(n, 32)


def emit_pcm(fileobj, out):
    """Sensor side for the cochlea front end: decode a LibriSpeech .tar.gz
    on fileobj and write uint32 n_samples + float32 PCM per utterance, the
    input format of nimcp's tools/cochlea_emit."""
    try:
        for _, wav in stream_wavs(fileobj):
            out.write(np.uint32(len(wav)).tobytes())
            out.write(np.ascontiguousarray(wav, dtype=np.float32).tobytes())
        out.flush()
    except BrokenPipeError:
        try:
            sys.stdout = None
        except Exception:
            pass


COCHLEA_CHANNELS = 64


def read_cochlea(fileobj, nch: int = COCHLEA_CHANNELS):
    """Learner side for the cochlea front end: yield (None, counts (F, nch)
    uint16) per utterance from cochlea_emit output (auditory-nerve spike
    counts per 40 ms frame and channel) until the stream ends."""
    while True:
        head = fileobj.read(4)
        if len(head) < 4:
            return
        n = int(np.frombuffer(head, np.uint32)[0])
        body = fileobj.read(n * nch * 2)
        if len(body) < n * nch * 2:
            return
        yield None, np.frombuffer(body, np.uint16).reshape(n, nch)


COCHLEA_EMIT = Path(os.environ.get("NIMCP_COCHLEA_EMIT", "/home/bbrelin/nimcp/build/cochlea_emit"))


def build_cochlea(split: str) -> Path:
    """Run a cached LibriSpeech split through nimcp's cochlea once:
    ~/.cache/athena/librispeech/<split>_cochlea.npz with frames uint16
    (T, 64) spike counts per 40 ms, start int64 (U,) utterance starts."""
    import subprocess
    import threading
    out = CACHE_DIR / f"{split}_cochlea.npz"
    if out.exists():
        return out
    tar = CACHE_DIR / f"{split}.tar.gz"
    proc = subprocess.Popen([str(COCHLEA_EMIT)], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    chunks, starts = [], []

    def reader():
        t = 0
        for _, f in read_cochlea(proc.stdout):
            starts.append(t)
            chunks.append(f.copy())
            t += len(f)
    th = threading.Thread(target=reader)
    th.start()
    with open(tar, "rb") as fh:
        emit_pcm(fh, proc.stdin)
    proc.stdin.close()
    th.join()
    proc.wait()
    tmp = out.with_suffix(".tmp.npz")
    np.savez(tmp, frames=np.concatenate(chunks), start=np.asarray(starts, np.int64))
    tmp.replace(out)
    return out


def load(split: str) -> dict:
    z = np.load(build(split))
    return {k: z[k] for k in z.files}


def windows(frames: torch.Tensor, win: int = 25, stride: int = 5):
    """The stream as overlapping windows of `win` frames (25 = 1 s), each
    flattened band-major like a pooled Speech Commands clip (32 x 25), in
    stream order. Yields (window_start_frame, (32*win,) tensor)."""
    T = frames.shape[0]
    for s in range(0, T - win + 1, stride):
        yield s, frames[s:s + win].T.reshape(-1)


if __name__ == "__main__":
    torch.set_num_threads(int(os.environ.get("ATHENA_THREADS", "4")))
    if sys.argv[1:2] == ["--emit"]:            # .tar.gz on stdin -> log-mel frames on stdout
        emit_frames(sys.stdin.buffer, sys.stdout.buffer)
    elif sys.argv[1:2] == ["--emit-pcm"]:      # .tar.gz on stdin -> PCM for cochlea_emit
        emit_pcm(sys.stdin.buffer, sys.stdout.buffer)
    elif sys.argv[1:2] == ["--build-cochlea"]:  # cached split -> cochlea spike counts
        for sp in sys.argv[2:] or ["test-clean"]:
            print(build_cochlea(sp), flush=True)
    else:
        for sp in sys.argv[1:] or ["dev-clean"]:
            print(build(sp), flush=True)
