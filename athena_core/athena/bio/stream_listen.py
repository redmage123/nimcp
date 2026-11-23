"""Listening to a simulated stream (LibriSpeech), then naming spoken words.

Stage 1, listening (unsupervised, one pass, in arrival order): a spiking STDP
cortex hears the audiobook stream as short windows (w frames of 40 ms,
default 5 = 200 ms, about a syllable or phoneme pair) and learns detectors
of recurring sound patterns. Each window is heard once; no epochs, no
shuffling, no labels. Optional hippocampal replay: a reservoir of heard
windows is replayed between new windows (complementary learning systems).

Stage 2, naming (rung 2 protocol, Speech Commands, 35 words, k clips per
word, 3 seeds): a clip is heard as the same short windows; its code is the
spike count of each detector, pooled over `segments` equal parts of the clip
(tolerant to when the word starts). Words are learned in one shot by
Hebbian word neurons or by the hippocampus over that code.

Controls: the same pipeline with a cortex that never listened (random
weights); a k-means "cortex" fitted on the same stream windows (conventional
learner, same experience); logistic regression on whole-clip features.

Run (live, nothing stored):
  curl -sL https://www.openslr.org/resources/12/train-clean-100.tar.gz | \
      python3 -m athena.data.librispeech --emit | \
      ssh <host> 'cd athena_core && python3 -m athena.bio.stream_listen --stdin'
(the sensor side decodes audio; only 40 ms log-mel frames cross the network)
Run (cached split, reproducible): python3 -m athena.bio.stream_listen --split dev-clean
"""
from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np
import torch

from athena import scoreboard
from athena.bio.bench_speech import NPZ, _hebb, _subset
from athena.bio.learners import Hippocampus, STDPCortex
from athena.data import librispeech

TASK = "speech_fewshot_after_listening"
CACHE = Path(__file__).resolve().parents[2] / "runs" / "bio"


def speech_commands_raw():
    """Speech Commands as (N, 32 bands, 25 frames) raw pooled log-mel."""
    d = np.load(NPZ)

    def pool(x):
        t = torch.tensor(x[:, 0, :, :100].astype(np.float32))
        return torch.nn.functional.avg_pool2d(t[:, None], (2, 4))[:, 0]
    return pool(d["xtr"]), torch.tensor(d["ytr"]), pool(d["xte"]), torch.tensor(d["yte"]), len(d["words"])


def to_unit(x, lo, hi):
    return ((x - lo) / (hi - lo)).clamp(0, 1)


def clip_windows(clips: torch.Tensor, w: int, stride: int) -> torch.Tensor:
    """(N, 32, F) -> (N, n_windows, 32*w) band-major windows."""
    win = clips.unfold(2, w, stride)                 # N, 32, n_win, w
    return win.permute(0, 2, 1, 3).reshape(clips.shape[0], win.shape[2], -1)


def clip_codes(c, clips, w, stride, segments, steps, chunk=4000):
    """Detector spike counts per clip, pooled over `segments` parts."""
    W = clip_windows(clips, w, stride)               # N, nw, d
    N, nw, d = W.shape
    flat = W.reshape(-1, d)
    R = torch.cat([c.respond(flat[i:i + chunk], steps=steps) for i in range(0, len(flat), chunk)])
    R = R.view(N, nw, -1)
    bounds = torch.linspace(0, nw, segments + 1).round().long()
    return torch.cat([R[:, bounds[i]:bounds[i + 1]].sum(1) for i in range(segments)], 1)


def kmeans_codes(C, clips, w, stride, segments):
    W = clip_windows(clips, w, stride)
    N, nw, d = W.shape
    D = torch.cdist(W.reshape(-1, d), C)
    A = (D.mean(1, keepdim=True) - D).clamp_min(0).view(N, nw, -1)
    bounds = torch.linspace(0, nw, segments + 1).round().long()
    return torch.cat([A[:, bounds[i]:bounds[i + 1]].sum(1) for i in range(segments)], 1)


def window_stream(utterances, lo, hi, w: int, stride: int):
    """Continuous stream of windows across utterance boundaries (as a live
    feed would be): yields unit-intensity (32*w,) windows in arrival order."""
    buf = torch.zeros(0, 32)
    for _, f in utterances:
        buf = torch.cat([buf, to_unit(torch.tensor(f.astype(np.float32)), lo, hi)])
        s = 0
        while s + w <= len(buf):
            yield buf[s:s + w].T.reshape(-1)
            s += stride
        buf = buf[s:]


def listen(windows, n: int, w: int, rate: float, replay: int, stride: int, kmeans=None,
           seed: int = 0, max_windows: int = 0) -> tuple[STDPCortex, int]:
    """One pass over a window stream. The STDP cortex hears each window once;
    with replay>0 a reservoir of past windows is replayed between new ones.
    The conventional control (MiniBatchKMeans) gets the same single pass."""
    c = STDPCortex(32 * w, n=n, seed=seed, rate=rate, steps=100, theta_plus=0.05, theta_decay=0.999)
    g = torch.Generator().manual_seed(seed)
    reservoir, batch, i = [], [], 0
    t0 = time.time()
    for i, x in enumerate(windows):
        c.present(x, learn=True)
        if kmeans is not None:
            batch.append(x)
            if len(batch) == 4096:
                kmeans.partial_fit(torch.stack(batch).numpy())
                batch = []
        if replay:
            if len(reservoir) < 20000:
                reservoir.append(x)
            else:
                j = int(torch.randint(i + 1, (1,), generator=g))
                if j < len(reservoir):
                    reservoir[j] = x
            if i % 10 == 9:                          # between new windows, replay old ones
                for j in torch.randint(len(reservoir), (replay,), generator=g):
                    c.present(reservoir[int(j)], learn=True)
        if i % 50000 == 0:
            print(f"  listened {i} windows ({i * stride * 0.04 / 3600:.2f} h) in {time.time() - t0:.0f}s",
                  flush=True)
        if max_windows and i + 1 >= max_windows:
            break
    if kmeans is not None and len(batch) >= kmeans.n_clusters:
        kmeans.partial_fit(torch.stack(batch).numpy())
    return c, i + 1


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--split", default="dev-clean")
    ap.add_argument("--stdin", action="store_true", help="read a LibriSpeech .tar.gz stream from stdin")
    ap.add_argument("--hours", type=float, default=0, help="stop after H hours of speech (0 = all)")
    ap.add_argument("--n", type=int, default=400)
    ap.add_argument("--w", type=int, default=5)
    ap.add_argument("--stride", type=int, default=2, help="stream stride while listening (frames)")
    ap.add_argument("--clip-stride", type=int, default=1)
    ap.add_argument("--segments", type=int, default=3)
    ap.add_argument("--rate", type=float, default=0.064)
    ap.add_argument("--replay", type=int, default=0, help="replayed windows per 10 new ones")
    ap.add_argument("--budgets", default="1,2,5,10")
    ap.add_argument("--seeds", type=int, default=3)
    ap.add_argument("--max-test", type=int, default=0)
    ap.add_argument("--threads", type=int, default=8)
    a = ap.parse_args(argv)
    torch.set_num_threads(a.threads)
    from sklearn.linear_model import LogisticRegression

    Xp_raw, yp, Xte_raw, yte, nw = speech_commands_raw()
    if a.max_test:
        Xte_raw, yte = Xte_raw[:a.max_test], yte[:a.max_test]
    lo, hi = torch.quantile(Xp_raw[::17].flatten()[::3], torch.tensor([0.02, 0.98]))
    Xp, Xte = to_unit(Xp_raw, lo, hi), to_unit(Xte_raw, lo, hi)
    from sklearn.cluster import MiniBatchKMeans
    if a.stdin:                                  # live: the archive arrives on stdin, never stored
        source = librispeech.read_frames(sys.stdin.buffer)
        a.split = "stdin"
    else:
        z = librispeech.load(a.split)
        source = ((u, f) for u, f in zip(z["utt"], np.split(z["frames"], z["start"][1:])))
    max_windows = int(a.hours * 3600 / 0.04 / a.stride) if a.hours else 0
    km = MiniBatchKMeans(a.n, random_state=0, batch_size=4096)
    t0 = time.time()
    c, heard = listen(window_stream(source, lo, hi, a.w, a.stride), a.n, a.w, a.rate, a.replay,
                      a.stride, kmeans=km, max_windows=max_windows)
    hours = heard * a.stride * 0.04 / 3600
    print(f"listening done: {heard} windows = {hours:.1f} h of speech in {time.time() - t0:.0f}s, "
          f"{c.updates} plastic events", flush=True)
    tag = f"{a.split}_h{hours:.1f}_n{a.n}_w{a.w}_s{a.stride}_r{a.rate}_rp{a.replay}"
    CACHE.mkdir(parents=True, exist_ok=True)
    torch.save({"W": c.W, "theta": c.theta, "updates": c.updates, "hours": hours,
                "kmeans": torch.tensor(km.cluster_centers_)}, CACHE / f"listen_{tag}.pt")
    naive = STDPCortex(32 * a.w, n=a.n, seed=0, rate=a.rate, steps=100)
    naive.theta = c.theta.mean().expand(a.n).clone()           # same excitability, never listened

    budgets = [int(b) for b in a.budgets.split(",")]
    subs = {(k, s): _subset(yp, k, s, nw) for k in budgets for s in range(a.seeds)}
    need = torch.unique(torch.cat(list(subs.values())))
    pos = {int(i): j for j, i in enumerate(need)}

    C = torch.tensor(km.cluster_centers_, dtype=torch.float32)
    codes = {}
    for name, f in (("snn:listened", lambda X: clip_codes(c, X, a.w, a.clip_stride, a.segments, 200)),
                    ("snn:never-listened", lambda X: clip_codes(naive, X, a.w, a.clip_stride, a.segments, 200)),
                    ("kmeans(stream)", lambda X: kmeans_codes(C, X, a.w, a.clip_stride, a.segments))):
        t0 = time.time()
        codes[name] = (f(Xp[need]), f(Xte))
        print(f"codes {name}: {time.time() - t0:.0f}s", flush=True)

    table = {}

    def score(name, kind, k, preds, cfg=None):
        accs = [float((p == yte).float().mean()) for p in preds]
        table[(name, k)] = (np.mean(accs), np.std(accs))
        scoreboard.record(TASK, kind, name, {"held_top1": round(float(np.mean(accs)), 4),
                                             "held_top1_std": round(float(np.std(accs)), 4)},
                          {"per_class": k, "seeds": a.seeds, "stream": a.split, "hours": a.hours,
                           "n": a.n, "w": a.w, "replay": a.replay, **(cfg or {})})
        print(f"{name:34s} k={k:3d}  {np.mean(accs):.3f} ± {np.std(accs):.3f}", flush=True)

    for k in budgets:
        for name, (Cl_all, Cte) in codes.items():
            hebb, hippo, lr = [], [], []
            for s in range(a.seeds):
                idx = subs[(k, s)]
                Cl, yl = Cl_all[[pos[int(i)] for i in idx]], yp[idx]
                hebb.append(_hebb(Cl, yl, Cte, nw))
                scale = max(1e-6, float(Cl.max()))
                hc = Hippocampus(Cl.shape[1], nw, n_dg=8000, k_frac=0.1, fan_in=0.25, steps=1000, seed=s)
                hc.store(hc.code(Cl / scale), yl)
                hippo.append(torch.cat([hc.recall(hc.code((Cte[j:j + 2000] / scale).clamp(0, 1)))
                                        for j in range(0, len(Cte), 2000)]))
                lr.append(torch.tensor(LogisticRegression(max_iter=2000).fit(Cl.numpy(), yl.numpy())
                                       .predict(Cte.numpy())))
            bio = name.startswith("snn")
            score(f"{name}+hebb", "train" if bio else "baseline", k, hebb)
            score(f"{name}+hippocampus", "train" if bio else "baseline", k, hippo)
            score(f"{name}+logreg", "diagnostic" if bio else "baseline", k, lr)
        score("logreg(whole-clip features)", "baseline", k,
              [torch.tensor(LogisticRegression(max_iter=2000).fit(Xp[subs[(k, s)]].flatten(1).numpy(),
                                                                  yp[subs[(k, s)]].numpy())
                            .predict(Xte.flatten(1).numpy())) for s in range(a.seeds)])

    names = list(dict.fromkeys(nm for nm, _ in table))
    print("\n" + "learner".ljust(34) + "".join(f"  k={k:<9d}" for k in budgets))
    for nm in names:
        print(nm.ljust(34) + "".join(f"  {table[(nm, k)][0]:.3f}±{table[(nm, k)][1]:.3f}"
                                      if (nm, k) in table else "  " + "-".ljust(11) for k in budgets))


if __name__ == "__main__":
    main()
