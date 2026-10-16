#!/usr/bin/env python3
"""
Pod-side distributional-collapse diagnostic.

For each word in three categories (concrete / actions / abstract), calls
comprehend(word) and captures the returned semantic_vector. Then computes
the full pairwise cosine matrix and reports:

  - Mean cosine WITHIN each category   (should be moderately positive,
                                        these words share semantic field)
  - Mean cosine BETWEEN categories     (should be ~0 or weakly positive
                                        if distributional space is healthy)
  - Top-5 most-similar cross-category pairs
                                        (if abstract words dominate, we'll
                                        see concrete words with cos > 0.9
                                        to abstract anchors)

Collapse signature (per memory distributional-collapse-2026-05-18):
  - All pairs ≥ 0.85, very narrow spread → distributional space has
    homogenized. Reset required.

Healthy signature:
  - Within-category 0.6-0.9; between-category < 0.3; clear cluster
    separation.

Usage on pod:
    python3 /workspace/nimcp/scripts/probe_pod_cosines.py
"""

from __future__ import annotations

import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from brain_client import BrainProxy, is_daemon_running  # noqa: E402


WORDS_CONCRETE = ["cat", "dog", "bird", "tree", "ball"]
WORDS_ACTIONS  = ["ran", "ate", "jumped", "flew", "swam"]
WORDS_ABSTRACT = ["love", "peace", "idea", "hope", "truth"]

CATEGORIES = {
    "concrete": WORDS_CONCRETE,
    "actions":  WORDS_ACTIONS,
    "abstract": WORDS_ABSTRACT,
}


def cosine(a, b):
    if len(a) != len(b):
        return float("nan")
    dot = 0.0
    na = 0.0
    nb = 0.0
    for x, y in zip(a, b):
        dot += x * y
        na  += x * x
        nb  += y * y
    if na <= 0.0 or nb <= 0.0:
        return 0.0
    return dot / math.sqrt(na * nb)


def main():
    if not is_daemon_running():
        print("ERROR: brain daemon not running", file=sys.stderr)
        return 2
    b = BrainProxy()

    all_words = []
    word_cat = {}
    for cat, ws in CATEGORIES.items():
        for w in ws:
            all_words.append(w)
            word_cat[w] = cat

    # Comprehend each word, capture semantic_vector
    vecs = {}
    confs = {}
    for w in all_words:
        r = b.comprehend(w)
        v = r.get("semantic_vector")
        c = r.get("confidence", 0.0)
        if not isinstance(v, list) or not v:
            print(f"WARN: comprehend({w}) returned no vector")
            continue
        # Check vector isn't all zeros
        nz = sum(1 for x in v if abs(x) > 1e-9)
        vecs[w] = v
        confs[w] = c
        norm = math.sqrt(sum(x * x for x in v))
        print(f"  comprehend({w:<8s}) conf={c:.2f} dim={len(v)} nonzero={nz}/{len(v)} ||v||={norm:.3f}")

    print()
    print("=" * 72)
    print(" PAIRWISE COSINE MATRIX")
    print("=" * 72)

    # Build matrix
    pairs = []   # (w1, w2, cos, same_cat)
    for i, w1 in enumerate(all_words):
        if w1 not in vecs:
            continue
        for w2 in all_words[i+1:]:
            if w2 not in vecs:
                continue
            c = cosine(vecs[w1], vecs[w2])
            same = (word_cat[w1] == word_cat[w2])
            pairs.append((w1, w2, c, same))

    # Header row
    header = "          " + " ".join(f"{w[:7]:>7s}" for w in all_words)
    print(header)
    for w1 in all_words:
        if w1 not in vecs:
            print(f"  {w1:<8s}  [no vector]")
            continue
        row = [f"  {w1:<8s}"]
        for w2 in all_words:
            if w2 not in vecs:
                row.append("    n/a")
            elif w1 == w2:
                row.append("   1.00")
            else:
                c = cosine(vecs[w1], vecs[w2])
                row.append(f"  {c:+5.2f}")
        print(" ".join(row))

    if not pairs:
        print("\nNo valid pairs — all comprehend calls failed.")
        return 1

    # Stats
    within = [p[2] for p in pairs if p[3]]
    between = [p[2] for p in pairs if not p[3]]
    all_cos = [p[2] for p in pairs]

    print()
    print("=" * 72)
    print(" STATISTICS")
    print("=" * 72)
    def stats(name, xs):
        if not xs:
            print(f"  {name}: empty")
            return
        mn, mx = min(xs), max(xs)
        mu = sum(xs) / len(xs)
        print(f"  {name:<22s} n={len(xs):>3d}  min={mn:+.3f}  mean={mu:+.3f}  max={mx:+.3f}")
    stats("ALL pairs",     all_cos)
    stats("WITHIN category", within)
    stats("BETWEEN category", between)

    # Top cross-category similar pairs — collapse signature
    cross_pairs = sorted([p for p in pairs if not p[3]], key=lambda p: -p[2])
    print()
    print("  Top-10 cross-category cosines (collapse if all > 0.85):")
    for w1, w2, c, _ in cross_pairs[:10]:
        cat1 = word_cat[w1]
        cat2 = word_cat[w2]
        print(f"    {w1:<8s} ({cat1}) <-> {w2:<8s} ({cat2}):  cos = {c:+.3f}")

    # Verdict
    print()
    print("=" * 72)
    print(" VERDICT")
    print("=" * 72)
    mean_within  = sum(within)/len(within) if within else 0
    mean_between = sum(between)/len(between) if between else 0
    spread = max(all_cos) - min(all_cos)
    print(f"  mean within   = {mean_within:+.3f}")
    print(f"  mean between  = {mean_between:+.3f}")
    print(f"  separation    = {mean_within - mean_between:+.3f}  (positive = clusters separate)")
    print(f"  total spread  = {spread:.3f}    (small = collapse)")
    print()
    if mean_between > 0.85 and spread < 0.15:
        print("  STATUS: DISTRIBUTIONAL COLLAPSE — all words look identical in semantic space")
        print("          Reset required (reset_lexicon_distributional RPC)")
    elif mean_within - mean_between < 0.10:
        print("  STATUS: WEAK SEPARATION — clusters barely distinguishable")
        print("          Soft collapse; reset recommended")
    elif mean_within - mean_between > 0.30:
        print("  STATUS: HEALTHY DISTRIBUTIONAL SPACE — clusters cleanly separated")
    else:
        print(f"  STATUS: BORDERLINE separation={mean_within-mean_between:+.3f}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
