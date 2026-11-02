#!/usr/bin/env python3
"""Build the high-resolution photo caches for the visual acuity schedule.

Stage 0 sees CIFAR photos at their native 32x32. From stage 1 the curriculum
sharpens vision (real_sensory.STAGE_ACUITY: 64x64, then 128x128), and
upscaled CIFAR adds no detail, so stages 1+ draw photos from Caltech-256
(Griffin, Holub & Perona, 2007; CC BY 4.0, data.caltech.edu record
nyy15-4j048): ~30k photos in 256 object categories at roughly 300 px.

Each photo is centre-cropped to a square, resized to --size (default 128)
with area averaging, and stored as JPEG. Categories are mapped onto the
labels real_sensory.SYNONYMS already uses where they overlap (dog, horse,
airplane, ...), so existing vocabulary words find the sharper photos; other
categories keep their own name (e.g. giraffe). Weapons and a few other
categories are excluded (EXCLUDED). About 1 photo in 10 per category is held
out for evaluation.

Output (into --out):
    photos_hires_by_label.pkl          {label: [JPEG bytes, ...]}
    photos_hires_heldout_by_label.pkl  same layout, evaluation only

Usage:
    python3 build_hires_photos.py --tar 256_ObjectCategories.tar --out DIR
                                  [--size 128] [--max-per-label 200] [--quality 90]
"""
import argparse
import io
import os
import pickle
import re
import sys
import tarfile
import zlib

# Weapons, and categories that are not objects a young learner should be
# shown by name (clutter is the dataset's background class).
EXCLUDED = {
    "ak47", "sword", "rifle", "revolver-101", "cannon", "fighter-jet", "knife",  # weapons
    "coffin", "tombstone", "human-skeleton",                                    # death imagery
    "syringe", "beer-mug", "wine-bottle", "roulette-wheel", "smokestack",       # adult habits
    "clutter",                                                                  # background class
}

# Caltech-256 category -> label used by real_sensory (CIFAR names where the
# vocabulary already maps words to them).
TO_LABEL = {
    "dog": "dog", "house-fly": "fly", "horse": "horse", "elephant-101": "elephant",
    "bear": "bear", "camel": "camel", "kangaroo-101": "kangaroo", "chimp": "chimpanzee",
    "gorilla": "chimpanzee", "leopards-101": "leopard", "raccoon": "raccoon",
    "skunk": "skunk", "porcupine": "porcupine", "snail": "snail", "spider": "spider",
    "butterfly": "butterfly", "cockroach": "cockroach", "frog": "frog", "snake": "snake",
    "crab-101": "crab", "dolphin-101": "dolphin", "goldfish": "aquarium_fish",
    "killer-whale": "whale", "duck": "bird", "goose": "bird", "ostrich": "bird",
    "owl": "bird", "hummingbird": "bird", "swan": "bird", "penguin": "bird",
    "cormorant": "bird", "iguana": "lizard", "airplanes-101": "airplane",
    "car-side-101": "automobile", "motorbikes-101": "motorcycle", "school-bus": "bus",
    "fire-truck": "truck", "pickup-truck": "pickup_truck", "train": "train",
    "tricycle": "bicycle", "mountain-bike": "bicycle", "touring-bike": "bicycle",
    "speed-boat": "ship", "canoe": "ship", "kayak": "ship", "ketch-101": "ship",
    "sailboat": "ship", "tractor": "tractor", "mushroom": "mushroom", "sunflower-101": "sunflower",
    "teapot": "teapot", "coffee-mug": "cup", "bathtub": "bathtub", "couch": "couch",
    "chair": "chair", "desk-globe": "globe", "telephone-box": "telephone",
    "cereal-box": "box", "rainbow": "rainbow", "lightning": "lightning",
    "waterfall": "waterfall", "galaxy": "galaxy", "mars": "planet", "saturn": "planet",
    "skyscraper": "skyscraper", "lighthouse": "lighthouse", "windmill": "windmill",
    "bridge": "bridge", "covered-wagon": "wagon", "teddy-bear": "teddy_bear",
    "faces-easy-101": "face", "grapes": "grapes", "watermelon": "watermelon", "strawberry": "strawberry",
    "pineapple": "pineapple", "banana": "banana", "tomato": "tomato",
}


def category_label(dirname: str) -> str:
    """'056.dog' -> 'dog' (via TO_LABEL), else the name with '-' -> '_' and a
    trailing '-101' dropped."""
    name = dirname.split(".", 1)[-1]
    if name in TO_LABEL:
        return TO_LABEL[name]
    name = re.sub(r"-101$", "", name)
    return name.replace("-", "_")


def square_jpeg(data: bytes, size: int, quality: int) -> bytes:
    from PIL import Image
    img = Image.open(io.BytesIO(data)).convert("RGB")
    w, h = img.size
    side = min(w, h)
    left, top = (w - side) // 2, (h - side) // 2
    img = img.crop((left, top, left + side, top + side)).resize((size, size), Image.BOX)
    buf = io.BytesIO()
    img.save(buf, format="JPEG", quality=quality)
    return buf.getvalue()


def is_heldout(member_name: str) -> bool:
    return zlib.crc32(member_name.encode()) % 10 == 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tar", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--size", type=int, default=128)
    ap.add_argument("--quality", type=int, default=90)
    ap.add_argument("--max-per-label", type=int, default=200,
                    help="training photos kept per label (held-out photos are extra)")
    ap.add_argument("--list", action="store_true", help="print categories and labels, then exit")
    a = ap.parse_args()

    train, held, skipped = {}, {}, set()
    with tarfile.open(a.tar) as tf:
        for m in tf:
            if not m.isfile() or not m.name.lower().endswith((".jpg", ".jpeg")):
                continue
            parts = m.name.split("/")
            if len(parts) < 3:
                continue
            cat = parts[-2]
            name = cat.split(".", 1)[-1]
            if name in EXCLUDED:
                skipped.add(name)
                continue
            label = category_label(cat)
            if a.list:
                train.setdefault(label, set()).add(name)
                continue
            dest = held if is_heldout(m.name) else train
            if dest is train and len(train.get(label, [])) >= a.max_per_label:
                continue
            try:
                jpg = square_jpeg(tf.extractfile(m).read(), a.size, a.quality)
            except Exception as e:
                print(f"skip {m.name}: {e}", file=sys.stderr)
                continue
            dest.setdefault(label, []).append(jpg)
    if a.list:
        for label in sorted(train):
            print(f"{label:24s} <- {', '.join(sorted(train[label]))}")
        print(f"{len(train)} labels; excluded: {', '.join(sorted(skipped))}")
        return
    os.makedirs(a.out, exist_ok=True)
    for fname, d in (("photos_hires_by_label.pkl", train), ("photos_hires_heldout_by_label.pkl", held)):
        path = os.path.join(a.out, fname)
        with open(path, "wb") as fh:
            pickle.dump(d, fh, protocol=4)
        print(f"{path}: {len(d)} labels, {sum(len(v) for v in d.values())} photos, "
              f"{os.path.getsize(path) / 1e6:.0f} MB")
    print(f"excluded categories: {', '.join(sorted(skipped))}")


if __name__ == "__main__":
    main()
