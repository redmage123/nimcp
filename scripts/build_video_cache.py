#!/usr/bin/env python3
"""Build a cache of short real-motion frame sequences from public-domain films
on the Internet Archive (Prelinger Archives) for the developmental curriculum.

Source: archive.org advancedsearch (collection:prelinger AND mediatype:movies),
items whose licenseurl marks them public domain. The smallest MP4/OGV
derivative of each film is downloaded, frames are sampled with ffmpeg at
--fps (default 2) and scaled to 32x32 RGB (the visual cortex input), and the
frame stream is cut into fixed-length sequences at shot boundaries (large
frame-to-frame change) with near-static runs dropped.

Output: <out>/video_sequences.pkl — list of
    {"item": identifier, "title": str, "description": str,
     "start_s": float, "frames": [bytes 32*32*3, ...] (len == --seq-len)}
Sizes: one sequence of 8 frames is ~24 KB; 8,000 sequences ≈ 200 MB.

Usage (run on a host with good bandwidth; the pod's inbound is slow):
    python3 build_video_cache.py --out DIR [--films 60] [--seq-len 8] [--fps 2]
                                 [--max-seq-per-film 150] [--workdir /tmp/ia_films]
"""
import argparse
import os
import pickle
import subprocess
import sys
import time

import numpy as np

W = H = 32
FRAME_BYTES = W * H * 3


def chunk_frames(frames, seq_len=8, cut_thresh=40.0, min_motion=1.5, stride=None):
    """Split a (N, H, W, 3) uint8 frame stream into sequences of seq_len.

    A new sequence starts at a shot cut (mean abs diff between consecutive
    frames > cut_thresh) and otherwise every `stride` frames (default
    seq_len). Sequences spanning a cut or with almost no motion (mean
    consecutive diff < min_motion) are dropped. Returns list of lists of
    frame bytes.
    """
    n = len(frames)
    if n < seq_len:
        return []
    f = np.asarray(frames, dtype=np.float32)
    diffs = np.abs(f[1:] - f[:-1]).mean(axis=(1, 2, 3))  # len n-1
    stride = stride or seq_len
    out = []
    i = 0
    while i + seq_len <= n:
        d = diffs[i:i + seq_len - 1]
        if (d > cut_thresh).any():
            # jump to just after the cut
            i += int(np.argmax(d > cut_thresh)) + 1
            continue
        if d.mean() >= min_motion:
            out.append([np.asarray(frames[j], dtype=np.uint8).tobytes() for j in range(i, i + seq_len)])
        i += stride
    return out


def extract_frames(path, fps=2.0, max_seconds=1800):
    """Decode `path` with ffmpeg into a (N, 32, 32, 3) uint8 array."""
    cmd = ["ffmpeg", "-v", "error", "-i", path, "-t", str(max_seconds),
           "-vf", f"fps={fps},scale={W}:{H}", "-f", "rawvideo", "-pix_fmt", "rgb24", "-"]
    raw = subprocess.run(cmd, capture_output=True, timeout=1800).stdout
    n = len(raw) // FRAME_BYTES
    if n == 0:
        return np.zeros((0, H, W, 3), dtype=np.uint8)
    return np.frombuffer(raw[:n * FRAME_BYTES], dtype=np.uint8).reshape(n, H, W, 3)


def list_films(rows, session):
    r = session.get("https://archive.org/advancedsearch.php", params={
        "q": "collection:prelinger AND mediatype:movies",
        "fl[]": ["identifier", "title", "licenseurl", "description"],
        "rows": rows, "sort[]": "downloads desc", "output": "json"}, timeout=60)
    r.raise_for_status()
    docs = r.json()["response"]["docs"]
    return [d for d in docs if "publicdomain" in str(d.get("licenseurl", "")).lower()]


def smallest_video(identifier, session):
    m = session.get(f"https://archive.org/metadata/{identifier}", timeout=60).json()
    files = [(f["name"], int(f.get("size", 0))) for f in m.get("files", [])
             if f["name"].lower().endswith((".mp4", ".ogv")) and int(f.get("size", 0)) > 0]
    files.sort(key=lambda x: x[1])
    return files[0][0] if files else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", required=True)
    ap.add_argument("--films", type=int, default=60)
    ap.add_argument("--seq-len", type=int, default=8)
    ap.add_argument("--fps", type=float, default=2.0)
    ap.add_argument("--max-seq-per-film", type=int, default=150)
    ap.add_argument("--workdir", default="/tmp/ia_films")
    ap.add_argument("--keep-videos", action="store_true")
    a = ap.parse_args()
    import requests
    s = requests.Session()
    s.headers["User-Agent"] = "nimcp-curriculum-builder/1.0 (public-domain films for research)"
    os.makedirs(a.out, exist_ok=True)
    os.makedirs(a.workdir, exist_ok=True)
    films = list_films(a.films * 2, s)[:a.films]
    print(f"{len(films)} public-domain films selected")
    sequences = []
    t0 = time.time()
    for k, doc in enumerate(films, 1):
        ident = doc["identifier"]
        try:
            name = smallest_video(ident, s)
            if not name:
                print(f"[{k}/{len(films)}] {ident}: no video derivative"); continue
            local = os.path.join(a.workdir, f"{ident}_{os.path.basename(name)}")
            if not os.path.exists(local):
                with s.get(f"https://archive.org/download/{ident}/{name}", stream=True, timeout=120) as r:
                    r.raise_for_status()
                    with open(local, "wb") as fh:
                        for chunk in r.iter_content(1 << 20):
                            fh.write(chunk)
            frames = extract_frames(local, fps=a.fps)
            seqs = chunk_frames(frames, seq_len=a.seq_len)
            if len(seqs) > a.max_seq_per_film:
                idx = np.linspace(0, len(seqs) - 1, a.max_seq_per_film).astype(int)
                seqs = [seqs[i] for i in idx]
            title = str(doc.get("title", ident))
            desc = str(doc.get("description", ""))
            if isinstance(doc.get("description"), list):
                desc = " ".join(doc["description"])
            desc = desc.replace("\n", " ")[:200]
            for j, fr in enumerate(seqs):
                sequences.append({"item": ident, "title": title,
                                  "description": f"a moving scene from the film '{title}'",
                                  "summary": desc,   # the archive's own synopsis of the film
                                  "start_s": float(j * a.seq_len / a.fps), "frames": fr})
            print(f"[{k}/{len(films)}] {ident}: {len(frames)} frames -> {len(seqs)} sequences "
                  f"({os.path.getsize(local) / 1e6:.0f} MB, {time.time() - t0:.0f}s)", flush=True)
            if not a.keep_videos:
                os.remove(local)
        except Exception as e:
            print(f"[{k}/{len(films)}] {ident}: failed: {e}", flush=True)
    out = os.path.join(a.out, "video_sequences.pkl")
    with open(out, "wb") as fh:
        pickle.dump(sequences, fh, protocol=4)
    print(f"{out}: {len(sequences)} sequences from {len({q['item'] for q in sequences})} films, "
          f"{os.path.getsize(out) / 1e6:.0f} MB")


if __name__ == "__main__":
    main()
