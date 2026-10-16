#!/usr/bin/env python3
"""Biologically-appropriate eval for the hybrid NIMCP brain.

Cosine similarity to BERT targets is a poor metric for a brain where the
SNN core uses biological plasticity (R-STDP, STDP, homeostasis) rather
than gradient descent. The SNN doesn't optimize for BERT alignment —
it forms sparse population codes that don't have to live in BERT's
geometric space to be meaningful.

This eval uses five biologically-appropriate measurements:

  1. DISCRIMINATION  — within-class cosine vs between-class cosine.
                       Ratio > 1 means the brain tells classes apart,
                       regardless of absolute cosine values.

  2. LINEAR DECODABILITY — freeze the brain, train logistic regression
                           on its outputs to classify items. Accuracy
                           above chance = information is present.

  3. RSA (Representational Similarity Analysis)
                       — for N items, compute NxN similarity matrix
                         from brain responses; compare to NxN matrix
                         from BERT via Spearman correlation. Measures
                         whether the brain *organizes* concepts like
                         humans do, even if absolute directions differ.

  4. CONSISTENCY     — show the same input twice; cosine between the
                       two outputs. 1.0 = perfect reliability.

  5. STANDARD cosine — to-target cosine + Top-K retrieval, for
                       comparison with prior evals.

USAGE (on pod):
    python3 /workspace/nimcp/scripts/eval_brain_biological.py

Runtime: ~45 min for default 15 items + 3 consistency retries.
Brain should be idle (pause athena-training if possible).
"""

import argparse
import os
import sys
import time

sys.path.insert(0, "/workspace/nimcp/scripts"
                if os.path.exists("/workspace/nimcp/scripts")
                else os.path.join(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

from brain_client import BrainProxy
from claude_teacher import encode_text
from talk_to_athena import extract_embedding_from_output


# 5 classes × 3 instances each.  Instances vary in wording to test
# generalization within a concept.
PROBE_SET = [
    ("dog",   "A friendly dog with soft fur that wags its tail"),
    ("dog",   "A playful puppy chasing a ball across the yard"),
    ("dog",   "A golden retriever running toward its owner"),

    ("cat",   "A cat with soft fur that purrs in sunlight"),
    ("cat",   "A black cat stalking silently through tall grass"),
    ("cat",   "A kitten batting at a dangling string"),

    ("rain",  "Rain tapping on a roof steadily"),
    ("rain",  "A heavy downpour soaking the garden"),
    ("rain",  "Drizzle on the window at dawn"),

    ("sun",   "The bright warm sun that lights up the day"),
    ("sun",   "Sunlight streaming through tall trees"),
    ("sun",   "The afternoon sun casting long shadows"),

    ("tree",  "A tall tree with green leaves reaching up"),
    ("tree",  "An oak tree standing alone in a field"),
    ("tree",  "A pine tree covered in snow"),
]

# 3 consistency probes — same description, shown twice.
CONSISTENCY_PROBES = [
    ("dog",  "A friendly dog with soft fur that wags its tail"),
    ("rain", "Rain tapping on a roof steadily"),
    ("tree", "A tall tree with green leaves reaching up"),
]


def cos_sim(a, b):
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    m = min(len(a), len(b))
    na = np.linalg.norm(a[:m])
    nb = np.linalg.norm(b[:m])
    if na < 1e-8 or nb < 1e-8:
        return 0.0
    return float(np.dot(a[:m], b[:m]) / (na * nb))


def probe_brain(b, desc, timeout_s=600):
    """Run one forward pass, return (brain_output_embedding, target_embedding)."""
    features = encode_text(desc).tolist()
    target = np.array(encode_text(desc), dtype=np.float32)
    t0 = time.time()
    resp = b.decide_full(features)
    dt = time.time() - t0
    out = resp.get("output_vector")
    if out is None:
        return None, None, dt
    out_arr = np.array(out, dtype=np.float32)
    out_emb = extract_embedding_from_output(out_arr)
    return out_emb, target, dt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=600,
                    help="Per-call RPC timeout seconds (default 600)")
    args = ap.parse_args()

    b = BrainProxy(timeout=args.timeout)

    # =================================================================
    # Phase 1: collect brain outputs for the main probe set
    # =================================================================
    print(f"[eval] Collecting brain outputs for {len(PROBE_SET)} probes...",
          flush=True)
    outputs = []   # list of brain embedding vectors
    targets = []   # list of BERT target vectors
    labels = []    # list of class labels

    for i, (label, desc) in enumerate(PROBE_SET):
        out, tgt, dt = probe_brain(b, desc, args.timeout)
        if out is None:
            print(f"  [{i+1}/{len(PROBE_SET)}] {label:8s}: NO OUTPUT ({dt:.0f}s)",
                  flush=True)
            continue
        outputs.append(out)
        targets.append(tgt)
        labels.append(label)
        # Quick per-probe cosine for live feedback
        c = cos_sim(out, tgt)
        print(f"  [{i+1}/{len(PROBE_SET)}] {label:8s}: cos_to_target={c:+.4f} ({dt:.0f}s)",
              flush=True)

    if len(outputs) < 6:
        print("[eval] Too few successful probes — aborting", file=sys.stderr)
        sys.exit(1)

    outputs = np.array(outputs, dtype=np.float32)
    targets = np.array(targets, dtype=np.float32)
    labels = np.array(labels)

    # =================================================================
    # Phase 2: consistency — re-run 3 probes and compare
    # =================================================================
    print(f"\n[eval] Consistency probes ({len(CONSISTENCY_PROBES)} re-runs)...",
          flush=True)
    consistency_cosines = []
    for label, desc in CONSISTENCY_PROBES:
        out, _, dt = probe_brain(b, desc, args.timeout)
        if out is None:
            continue
        # Find original output for this description
        orig_idx = None
        for i, (lab, d) in enumerate(PROBE_SET):
            if d == desc:
                orig_idx = i
                break
        if orig_idx is None or orig_idx >= len(outputs):
            continue
        c = cos_sim(out, outputs[orig_idx])
        consistency_cosines.append(c)
        print(f"  {label:8s}: consistency_cos={c:+.4f} ({dt:.0f}s)", flush=True)

    # =================================================================
    # Metric 1: Discrimination (within-class vs between-class)
    # =================================================================
    print("\n" + "=" * 60)
    print("METRIC 1 — DISCRIMINATION")
    print("=" * 60)
    within_cosines = []
    between_cosines = []
    n = len(outputs)
    for i in range(n):
        for j in range(i + 1, n):
            c = cos_sim(outputs[i], outputs[j])
            if labels[i] == labels[j]:
                within_cosines.append(c)
            else:
                between_cosines.append(c)
    mean_within = np.mean(within_cosines) if within_cosines else 0.0
    mean_between = np.mean(between_cosines) if between_cosines else 0.0
    ratio = mean_within / mean_between if mean_between > 1e-6 else float("inf")
    print(f"  Within-class mean cosine:  {mean_within:+.4f}  (n={len(within_cosines)})")
    print(f"  Between-class mean cosine: {mean_between:+.4f}  (n={len(between_cosines)})")
    print(f"  Discrimination ratio (within/between): {ratio:.3f}")
    if ratio > 1.2:
        print("  ✓ Brain DISCRIMINATES — within-class responses more similar")
    elif ratio > 1.05:
        print("  ~ WEAK discrimination — some class-specific structure")
    else:
        print("  ✗ NO discrimination — classes not distinguished")

    # =================================================================
    # Metric 2: Linear decodability
    # =================================================================
    print("\n" + "=" * 60)
    print("METRIC 2 — LINEAR DECODABILITY")
    print("=" * 60)
    try:
        from sklearn.linear_model import LogisticRegression
        from sklearn.model_selection import cross_val_score
        from sklearn.preprocessing import StandardScaler
        scaler = StandardScaler()
        X = scaler.fit_transform(outputs)
        clf = LogisticRegression(max_iter=1000, C=1.0)
        # 5-fold CV won't work with only 3 samples/class; use leave-one-out
        from sklearn.model_selection import LeaveOneOut
        loo = LeaveOneOut()
        scores = cross_val_score(clf, X, labels, cv=loo)
        acc = float(np.mean(scores))
        chance = 1.0 / len(set(labels))
        print(f"  LOOCV classifier accuracy: {acc:.3f}  (chance={chance:.3f})")
        print(f"  Above chance by:           {acc - chance:+.3f}")
        if acc > chance + 0.3:
            print("  ✓ STRONG linear decodability — labels recoverable")
        elif acc > chance + 0.1:
            print("  ~ WEAK decodability — some signal")
        else:
            print("  ✗ NO decodability — brain output carries no class info")
    except ImportError:
        print("  (sklearn not available — skipping)")
    except Exception as e:
        print(f"  Decoder eval failed: {e}")

    # =================================================================
    # Metric 3: RSA — do brain and BERT organize concepts similarly?
    # =================================================================
    print("\n" + "=" * 60)
    print("METRIC 3 — REPRESENTATIONAL SIMILARITY ANALYSIS")
    print("=" * 60)
    # Build NxN cosine matrices
    n = len(outputs)
    brain_sim = np.zeros((n, n))
    bert_sim = np.zeros((n, n))
    for i in range(n):
        for j in range(n):
            brain_sim[i, j] = cos_sim(outputs[i], outputs[j])
            bert_sim[i, j] = cos_sim(targets[i], targets[j])
    # Flatten upper triangle and compute Spearman
    iu = np.triu_indices(n, k=1)
    brain_flat = brain_sim[iu]
    bert_flat = bert_sim[iu]
    # Spearman without scipy: rank-order then Pearson
    def rank(v):
        idx = np.argsort(v)
        r = np.empty_like(idx, dtype=np.float32)
        r[idx] = np.arange(len(v))
        return r
    br = rank(brain_flat)
    tr = rank(bert_flat)
    if br.std() > 1e-8 and tr.std() > 1e-8:
        spearman = float(np.corrcoef(br, tr)[0, 1])
    else:
        spearman = 0.0
    print(f"  Spearman rank correlation (brain vs BERT similarity): {spearman:+.4f}")
    if spearman > 0.5:
        print("  ✓ STRONG topology alignment — brain organizes concepts like BERT")
    elif spearman > 0.2:
        print("  ~ MODERATE alignment")
    elif spearman > 0.05:
        print("  ~ WEAK but positive alignment")
    else:
        print("  ✗ NO topology alignment — brain uses a different organization")

    # =================================================================
    # Metric 4: Consistency
    # =================================================================
    print("\n" + "=" * 60)
    print("METRIC 4 — RESPONSE CONSISTENCY")
    print("=" * 60)
    if consistency_cosines:
        mean_cons = np.mean(consistency_cosines)
        print(f"  Mean same-input cosine: {mean_cons:+.4f}  (n={len(consistency_cosines)})")
        if mean_cons > 0.9:
            print("  ✓ HIGH reliability — same input produces same representation")
        elif mean_cons > 0.7:
            print("  ~ MODERATE reliability")
        elif mean_cons > 0.3:
            print("  ~ LOW reliability — stochastic firing may dominate")
        else:
            print("  ✗ NO reliability — outputs essentially random")
    else:
        print("  No consistency probes succeeded")

    # =================================================================
    # Metric 5: Traditional — for comparison with old evals
    # =================================================================
    print("\n" + "=" * 60)
    print("METRIC 5 — TRADITIONAL (for comparison)")
    print("=" * 60)
    to_target = [cos_sim(outputs[i], targets[i]) for i in range(len(outputs))]
    print(f"  Mean cosine to target: {np.mean(to_target):+.4f}")
    print(f"  Min/Max:               {min(to_target):+.4f} / {max(to_target):+.4f}")
    pos = sum(1 for c in to_target if c > 0.05)
    strong = sum(1 for c in to_target if c > 0.10)
    print(f"  Positive (>0.05):      {pos}/{len(to_target)}")
    print(f"  Strong   (>0.10):      {strong}/{len(to_target)}")

    # =================================================================
    # Overall verdict
    # =================================================================
    print("\n" + "=" * 60)
    print("VERDICT")
    print("=" * 60)
    signals = 0
    if ratio > 1.05:
        signals += 1
        print("  ✓ Discrimination present")
    if 'acc' in dir() and acc > chance + 0.1:
        signals += 1
        print("  ✓ Linear decodability above chance")
    if spearman > 0.05:
        signals += 1
        print("  ✓ Topology alignment positive")
    if consistency_cosines and np.mean(consistency_cosines) > 0.3:
        signals += 1
        print("  ✓ Response consistency non-random")
    print(f"\n  {signals}/4 biological learning signals present")
    if signals >= 3:
        print("  → BRAIN IS LEARNING (even if cosine-to-target is low)")
    elif signals >= 2:
        print("  → PARTIAL LEARNING — some representational structure")
    else:
        print("  → MINIMAL LEARNING detected by biological metrics")


if __name__ == "__main__":
    main()
