#!/usr/bin/env python3
"""Build data/multimodal/kubric_sequences.pkl from Kubric MOVi-C (2026-11-05).

MOVi-C (google-research/kubric, Apache-2.0): 2-second clips, 24 frames at
12 fps, of 3-10 real scanned household objects (Google Scanned Objects)
thrown onto a photographic HDRI scene and left to fall, collide and roll.
Synthetic physics video: what infants learn early — motion, gravity,
collisions, objects persisting when they pass behind one another — with no
content risk, so it bypasses the film allowlist.

Reads the public TFDS shards over HTTPS (no TensorFlow): a TFRecord is a
sequence of [u64 length][u32 crc][tf.train.Example][u32 crc]; only the
"video" (24 PNG frames) and "instances/category" fields are decoded.

Output: a list of sequence dicts in the video-cache format read by
scripts/real_sensory.py, with frames re-encoded as 128x128 JPEG at the
source 12 fps (RealSensoryBank._fps_view thins them to the stage rate):
  {"item": "kubric:movi_c:<video name>", "title": "Kubric MOVi-C",
   "description": "...", "frames": [jpeg bytes], "fps": 12.0, "size": 128,
   "encoding": "jpeg", "source": "kubric", "maturity": 0}

Usage (build on Hetzner, then copy to the pod's data/multimodal/):
  python3 scripts/build_kubric_cache.py --shards 40 --out kubric_sequences.pkl
"""
import argparse
import io
import os
import pickle
import struct
import sys
import urllib.request

from PIL import Image

BASE = "https://storage.googleapis.com/kubric-public/tfds/movi_c/128x128/1.0.0"
N_TRAIN_SHARDS = 1024
LICENSE = ("Kubric MOVi-C, google-research/kubric (Apache-2.0); objects from "
           "Google Scanned Objects, backgrounds from Poly Haven HDRIs")

# instances-category.labels.txt order -> words for captions
CATEGORY_WORDS = {
    "Action Figures": "an action figure", "Bag": "a bag", "Board Games": "a board game",
    "Bottles and Cans and Cups": "a cup", "Camera": "a camera", "Car Seat": "a car seat",
    "Consumer Goods": "a box", "Hat": "a hat", "Headphones": "headphones",
    "Keyboard": "a keyboard", "Legos": "a toy brick", "Media Cases": "a case",
    "Mouse": "a computer mouse", "None": "an object", "Shoe": "a shoe",
    "Stuffed Toys": "a stuffed toy", "Toys": "a toy",
}


def tfrecords(data: bytes):
    i = 0
    while i + 12 <= len(data):
        n = struct.unpack_from("<Q", data, i)[0]
        yield data[i + 12:i + 12 + n]
        i += 12 + n + 4


def _varint(b, i):
    r = s = 0
    while True:
        c = b[i]
        i += 1
        r |= (c & 0x7F) << s
        s += 7
        if c < 0x80:
            return r, i


def _fields(b):
    """(field number, value) pairs of a protobuf message."""
    i = 0
    while i < len(b):
        key, i = _varint(b, i)
        fn, wt = key >> 3, key & 7
        if wt == 2:
            n, i = _varint(b, i)
            yield fn, b[i:i + n]
            i += n
        elif wt == 0:
            v, i = _varint(b, i)
            yield fn, v
        elif wt == 5:
            yield fn, b[i:i + 4]
            i += 4
        elif wt == 1:
            yield fn, b[i:i + 8]
            i += 8
        else:
            raise ValueError(f"unsupported wire type {wt}")


def parse_example(rec: bytes, wanted) -> dict:
    """tf.train.Example -> {key: list of bytes | list of ints} for wanted keys."""
    out = {}
    for _, features in _fields(rec):                 # Example.features
        for _, entry in _fields(features):           # map<string, Feature> entries
            key, feat = None, b""
            for fn, x in _fields(entry):
                if fn == 1:
                    key = x.decode()
                else:
                    feat = x
            if key not in wanted:
                continue
            for kind, lst in _fields(feat):          # 1 bytes_list, 3 int64_list
                if kind == 1:
                    out[key] = [v for _, v in _fields(lst)]
                elif kind == 3:
                    vals = []
                    for fn, v in _fields(lst):
                        if isinstance(v, int):
                            vals.append(v)
                        else:                         # packed
                            j = 0
                            while j < len(v):
                                x, j = _varint(v, j)
                                vals.append(x)
                    out[key] = vals
    return out


def caption(categories, labels) -> str:
    words = []
    for c in categories:
        w = CATEGORY_WORDS.get(labels[c] if 0 <= c < len(labels) else "None", "an object")
        if w not in words:
            words.append(w)
    words = words[:3]
    if not words:
        words = ["some objects"]
    things = words[0] if len(words) == 1 else ", ".join(words[:-1]) + " and " + words[-1]
    return f"{things} tumbling and rolling on the ground"


def clip_from_example(ex: dict, labels, size: int, quality: int):
    frames = []
    for png in ex.get("video", []):
        img = Image.open(io.BytesIO(png)).convert("RGB")
        if img.size != (size, size):
            img = img.resize((size, size), Image.BILINEAR)
        buf = io.BytesIO()
        img.save(buf, format="JPEG", quality=quality)
        frames.append(buf.getvalue())
    if len(frames) < 2:
        return None
    name = (ex.get("metadata/video_name") or [b"?"])[0]
    name = name.decode() if isinstance(name, bytes) else str(name)
    desc = caption(ex.get("instances/category", []), labels)
    return {"item": f"kubric:movi_c:{name}", "title": "Kubric MOVi-C", "description": desc,
            "summary": desc, "start_s": 0.0, "frames": frames, "fps": 12.0, "size": size,
            "encoding": "jpeg", "source": "kubric", "maturity": 0, "license": LICENSE}


def fetch(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=300) as r:
        return r.read()


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--shards", type=int, default=40, help="training shards to read (~10 clips each)")
    ap.add_argument("--first-shard", type=int, default=0)
    ap.add_argument("--size", type=int, default=128)
    ap.add_argument("--quality", type=int, default=90)
    ap.add_argument("--cache-dir", default=os.path.expanduser("~/nimcp_datasets/kubric"),
                    help="where downloaded shards are kept (reused on rerun)")
    ap.add_argument("--out", default="kubric_sequences.pkl")
    args = ap.parse_args()

    os.makedirs(args.cache_dir, exist_ok=True)
    labels = [l.strip() for l in fetch(f"{BASE}/instances-category.labels.txt").decode().splitlines()]
    wanted = {"video", "instances/category", "metadata/video_name"}
    clips = []
    for s in range(args.first_shard, min(N_TRAIN_SHARDS, args.first_shard + args.shards)):
        name = f"movi_c-train.tfrecord-{s:05d}-of-{N_TRAIN_SHARDS:05d}"
        local = os.path.join(args.cache_dir, name)
        if not os.path.exists(local):
            data = fetch(f"{BASE}/{name}")
            with open(local + ".part", "wb") as f:
                f.write(data)
            os.replace(local + ".part", local)
        with open(local, "rb") as f:
            data = f.read()
        n0 = len(clips)
        for rec in tfrecords(data):
            clip = clip_from_example(parse_example(rec, wanted), labels, args.size, args.quality)
            if clip:
                clips.append(clip)
        print(f"  shard {s}: {len(clips) - n0} clips (total {len(clips)})", flush=True)
    with open(args.out + ".tmp", "wb") as f:
        pickle.dump(clips, f, protocol=pickle.HIGHEST_PROTOCOL)
    os.replace(args.out + ".tmp", args.out)
    mb = os.path.getsize(args.out) / 1e6
    print(f"wrote {len(clips)} clips to {args.out} ({mb:.1f} MB)")
    return 0 if clips else 1


if __name__ == "__main__":
    sys.exit(main())
