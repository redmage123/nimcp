#!/usr/bin/env python3
"""Debug script to inspect Athena's brain state and diagnose output collapse."""

import sys
import os
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'build', 'lib', 'python'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '.venv', 'lib', 'python3.12', 'site-packages'))
import nimcp

CHECKPOINT = "checkpoints/athena/athena_immersive.bin"

print("=== Loading brain from checkpoint ===")
brain = nimcp.Brain("athena", checkpoint=CHECKPOINT)
brain.set_fast_training(False)

print(f"\n=== Brain stats ===")
try:
    stats = brain.get_stats()
    for k, v in sorted(stats.items()):
        print(f"  {k}: {v}")
except Exception as e:
    print(f"  get_stats error: {e}")

# Check output labels
print(f"\n=== Output label map ===")
try:
    labels = brain.get_output_labels()
    if labels:
        print(f"  Total labels: {len(labels)}")
        # Show first 20 and last 20
        for i, lbl in enumerate(labels[:20]):
            print(f"  [{i}] = '{lbl}'")
        if len(labels) > 40:
            print(f"  ... ({len(labels) - 40} more) ...")
            for i in range(max(20, len(labels) - 20), len(labels)):
                print(f"  [{i}] = '{labels[i]}'")
        elif len(labels) > 20:
            for i in range(20, len(labels)):
                print(f"  [{i}] = '{labels[i]}'")
    else:
        print("  No labels (None returned)")
except Exception as e:
    print(f"  get_output_labels error: {e}")

# Run a few test inferences with different inputs
print(f"\n=== Test inferences (output diversity) ===")
test_texts = [
    "The sun is shining brightly in the blue sky",
    "A small brown dog is running through the grass",
    "Mathematics involves numbers and equations",
    "Music fills the room with beautiful melodies",
    "The ocean waves crash against the rocky shore",
    "A child laughs while playing with colorful blocks",
    "Rain falls gently on the green leaves",
    "Stars twinkle in the dark night sky",
]

# Use encode_text if available, otherwise make random vectors
try:
    from claude_teacher import encode_text
    has_encoder = True
except:
    try:
        from sentence_transformers import SentenceTransformer
        model = SentenceTransformer('all-MiniLM-L6-v2')
        def encode_text(t):
            return model.encode(t).tolist()
        has_encoder = True
    except:
        has_encoder = False

for text in test_texts:
    if has_encoder:
        emb = np.array(encode_text(text), dtype=np.float64)
        fvec = np.zeros(1024, dtype=np.float64)
        fvec[0] = 1.0  # text modality
        # primary [16:528]
        n = min(len(emb), 512)
        fvec[16:16+n] = emb[:n]
        # text [528:912]
        n2 = min(len(emb), 384)
        fvec[528:528+n2] = emb[:n2]
        features = [float(x) for x in fvec]
    else:
        np.random.seed(hash(text) % 2**31)
        features = [float(x) for x in np.random.randn(1024)]

    result = brain.decide_full(features)
    label = result.get("label", "???")
    conf = result.get("confidence", 0)
    # Check output vector diversity
    ov = result.get("output_vector", [])
    if ov:
        ov_arr = np.array(ov)
        top5_idx = np.argsort(ov_arr)[-5:][::-1]
        top5_vals = ov_arr[top5_idx]
        print(f"  '{text[:50]}...'")
        print(f"    label='{label}' conf={conf:.4f}")
        print(f"    top5 neurons: {list(zip(top5_idx.tolist(), [f'{v:.4f}' for v in top5_vals]))}")
        print(f"    output stats: mean={ov_arr.mean():.6f} std={ov_arr.std():.6f} "
              f"max={ov_arr.max():.4f} min={ov_arr.min():.4f} "
              f"nonzero={np.count_nonzero(ov_arr)}/{len(ov_arr)}")
    else:
        print(f"  '{text[:50]}' → label='{label}' conf={conf:.4f} (no output vector)")

# Check if the same neurons always win
print(f"\n=== Output collapse test (100 random inputs) ===")
winners = {}
for i in range(100):
    if has_encoder:
        texts = ["apple", "ocean", "music", "fire", "tree", "book", "sky", "love",
                 "rain", "dance", "mountain", "river", "cloud", "sand", "wind",
                 "flower", "night", "sun", "bird", "fish"]
        text = texts[i % len(texts)]
        emb = np.array(encode_text(text), dtype=np.float64)
        fvec = np.zeros(1024, dtype=np.float64)
        fvec[0] = 1.0
        n = min(len(emb), 512)
        fvec[16:16+n] = emb[:n]
        n2 = min(len(emb), 384)
        fvec[528:528+n2] = emb[:n2]
        features = [float(x) for x in fvec]
    else:
        np.random.seed(i * 7 + 42)
        features = [float(x) for x in np.random.randn(1024)]

    result = brain.decide_full(features)
    label = result.get("label", "???")
    winners[label] = winners.get(label, 0) + 1

print(f"  Unique labels across 100 inputs: {len(winners)}")
for label, count in sorted(winners.items(), key=lambda x: -x[1])[:15]:
    print(f"    '{label}': {count} times ({count}%)")

# Check weight statistics
print(f"\n=== Network weight statistics ===")
try:
    # Use predict_fast to get raw outputs for a zero vector
    zero_label, zero_conf = brain.predict([0.0] * 1024)
    print(f"  Zero-input: label='{zero_label}' conf={zero_conf:.4f}")

    one_label, one_conf = brain.predict([1.0] * 1024)
    print(f"  Ones-input: label='{one_label}' conf={one_conf:.4f}")

    # Use decide_full for one detailed check
    zero_result = brain.decide_full([0.0] * 1024)
    zero_ov = np.array(zero_result.get("output_vector", []))
    print(f"  Zero-input output vec: mean={zero_ov.mean():.6f} std={zero_ov.std():.6f} "
          f"max={zero_ov.max():.4f} argmax={zero_ov.argmax()}")

    one_result = brain.decide_full([1.0] * 1024)
    one_ov = np.array(one_result.get("output_vector", []))
    print(f"  Ones-input output: mean={one_ov.mean():.6f} std={one_ov.std():.6f} "
          f"max={one_ov.max():.4f} argmax={one_ov.argmax()}")

    # Compare
    diff = one_ov - zero_ov
    print(f"  Output sensitivity (ones-zero): mean_diff={diff.mean():.6f} "
          f"std_diff={diff.std():.6f} max_diff={np.abs(diff).max():.6f}")
    if np.abs(diff).max() < 0.001:
        print("  ** WARNING: Outputs barely change with input — weights may be dead! **")
except Exception as e:
    print(f"  Error: {e}")

# Plasticity stats
print(f"\n=== Plasticity stats ===")
try:
    pstats = brain.get_plasticity_stats()
    for k, v in sorted(pstats.items()):
        print(f"  {k}: {v}")
except Exception as e:
    print(f"  get_plasticity_stats error: {e}")

# BG state
print(f"\n=== Basal Ganglia state ===")
try:
    print(f"  dopamine: {brain.bg_get_dopamine()}")
    print(f"  RPE: {brain.bg_get_rpe()}")
    print(f"  conflict: {brain.bg_get_conflict()}")
    print(f"  mode: {brain.bg_get_mode()}")
except Exception as e:
    print(f"  BG error: {e}")

print("\n=== Done ===")
