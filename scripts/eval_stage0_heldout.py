#!/usr/bin/env python3
"""Stage 0 held-out generalization eval.

Samples a deterministic held-out slice of the Stage 0 sensory corpus
(SENSORY descriptions from immerse_athena.StimulusSource) and runs
inference-only predictions against it. Compares reconstruction quality
vs. training loss plateau to decide whether Stage 0 should advance.

USAGE (on RunPod):
    python3 /workspace/nimcp/scripts/eval_stage0_heldout.py
    python3 /workspace/nimcp/scripts/eval_stage0_heldout.py --n 50 --seed 42

DECISION RULE (suggestion):
    - If held-out mean MSE ≤ 1.2 × training plateau  → ADVANCE to Stage 1
    - If held-out mean MSE > 2.0 × training plateau  → CONTINUE Stage 0
    - In between: inspect per-category breakdown, advance if all categories
      pass, extend curriculum on the weakest category otherwise.
"""

import argparse
import os
import random
import statistics
import sys
import time

sys.path.insert(0, "/workspace/nimcp/scripts")

import numpy as np

from brain_client import BrainProxy
from claude_teacher import encode_text
from immerse_athena import StimulusSource


# Stage 0 corpus is pure sensory descriptions. Group by prefix-category
# so we can see which perceptual channel is weak vs strong.
CATEGORY_SPANS = [
    ("Visual",     0, 25),   # indices into StimulusSource.SENSORY
    ("Tactile",    25, 40),
    ("Auditory",   40, 57),
    ("Olfactory",  57, 68),
    ("Emotional",  68, 80),
]


def categorize(idx: int) -> str:
    for name, lo, hi in CATEGORY_SPANS:
        if lo <= idx < hi:
            return name
    return "Unknown"


def mse(a, b) -> float:
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    if a.shape != b.shape:
        # Trim/pad to min length so eval isn't killed by a shape surprise
        m = min(a.size, b.size)
        a, b = a.flat[:m], b.flat[:m]
    return float(np.mean((a - b) ** 2))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n", type=int, default=50,
                    help="Number of held-out items to eval (default 50)")
    ap.add_argument("--seed", type=int, default=42,
                    help="Random seed for held-out selection (default 42)")
    ap.add_argument("--plateau", type=float, default=0.29,
                    help="Current training-loss plateau for comparison")
    args = ap.parse_args()

    corpus = StimulusSource.SENSORY
    n_corpus = len(corpus)
    print(f"[eval] Stage 0 corpus size: {n_corpus}")

    # Deterministic held-out selection: same seed always picks same items.
    # Training uses a different seed (or draws live), so this slice is
    # statistically disjoint from what the brain saw this session.
    rng = random.Random(args.seed)
    indices = list(range(n_corpus))
    rng.shuffle(indices)
    heldout = indices[: min(args.n, n_corpus)]
    print(f"[eval] Held-out sample: {len(heldout)} items (seed={args.seed})")

    brain = BrainProxy()
    # Ping to confirm daemon is reachable before a long eval.
    resp = brain._send({"cmd": "ping"})
    if not resp or not resp.get("ok", False):
        print(f"[eval] Brain ping failed: {resp}", file=sys.stderr)
        sys.exit(1)

    losses = []
    per_cat = {name: [] for name, _, _ in CATEGORY_SPANS}
    failures = 0

    t0 = time.time()
    for i, idx in enumerate(heldout):
        desc = corpus[idx]
        cat = categorize(idx)
        try:
            features = np.asarray(encode_text(desc), dtype=np.float32)
            resp = brain.predict(features)
            # predict() returns a dict; the prediction/output vector is
            # the most common field. Accept whichever the daemon returned.
            pred = (resp.get("prediction")
                    or resp.get("output")
                    or resp.get("output_vector")
                    or resp.get("decision"))
            if pred is None:
                failures += 1
                if failures <= 3:
                    print(f"[eval] No prediction field in response for "
                          f"'{desc[:40]}'; keys={list(resp.keys())}")
                continue
            # Stage 0 training is self-supervised reconstruction:
            # target == input embedding. Loss = MSE(prediction, features).
            loss = mse(pred, features)
            losses.append(loss)
            per_cat[cat].append(loss)
        except Exception as e:
            failures += 1
            if failures <= 3:
                print(f"[eval] Failed on '{desc[:40]}': {e}")
        if (i + 1) % 10 == 0:
            print(f"[eval] progress: {i + 1}/{len(heldout)}")

    elapsed = time.time() - t0

    if not losses:
        print("[eval] No successful predictions. Abort.", file=sys.stderr)
        sys.exit(2)

    overall_mean = statistics.mean(losses)
    overall_p50 = statistics.median(losses)
    overall_p95 = sorted(losses)[int(len(losses) * 0.95)] if len(losses) >= 2 else losses[0]

    print()
    print(f"=== Stage 0 Held-out Eval ({len(losses)}/{len(heldout)} successful, "
          f"{failures} failed, {elapsed:.1f}s) ===")
    print()
    print(f"{'Category':<12} {'N':>4} {'Mean':>9} {'P50':>9} {'P95':>9}")
    print("-" * 50)
    for name, lo, hi in CATEGORY_SPANS:
        cat_losses = per_cat[name]
        if not cat_losses:
            print(f"{name:<12} {'-':>4} {'-':>9} {'-':>9} {'-':>9}")
            continue
        n = len(cat_losses)
        m = statistics.mean(cat_losses)
        p50 = statistics.median(cat_losses)
        p95 = sorted(cat_losses)[int(n * 0.95)] if n >= 2 else cat_losses[0]
        print(f"{name:<12} {n:>4} {m:>9.4f} {p50:>9.4f} {p95:>9.4f}")
    print("-" * 50)
    print(f"{'OVERALL':<12} {len(losses):>4} "
          f"{overall_mean:>9.4f} {overall_p50:>9.4f} {overall_p95:>9.4f}")
    print()

    ratio = overall_mean / args.plateau
    print(f"Training plateau: {args.plateau:.4f}")
    print(f"Held-out mean:    {overall_mean:.4f}  (ratio {ratio:.2f}× plateau)")
    print()

    if ratio <= 1.2:
        print(">>> DECISION: ADVANCE to Stage 1")
        print("    Held-out generalization is within 20% of training plateau.")
    elif ratio > 2.0:
        print(">>> DECISION: CONTINUE Stage 0")
        print(f"    Held-out loss is {ratio:.1f}× the plateau — not generalizing.")
    else:
        # Find the worst per-category mean
        cat_means = [(n, statistics.mean(per_cat[n]))
                     for n, _, _ in CATEGORY_SPANS if per_cat[n]]
        cat_means.sort(key=lambda x: -x[1])
        weakest = cat_means[0] if cat_means else ("?", 0.0)
        print(">>> DECISION: MARGINAL — inspect per-category")
        print(f"    Weakest category: {weakest[0]} (mean {weakest[1]:.4f})")
        print("    Consider advancing if all categories < 1.5× plateau,")
        print(f"    or bias curriculum toward '{weakest[0]}' for another 2-5K steps.")


if __name__ == "__main__":
    main()
