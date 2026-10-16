#!/usr/bin/env python3
"""
ablation_study.py — Automated ablation study for NIMCP network types

Creates a small brain for each configuration, trains on identical data,
and compares loss convergence to determine which network types help.

Configurations tested:
  1. ANN-only (baseline)
  2. ANN + CNN
  3. ANN + SNN
  4. ANN + LNN
  5. All networks (ANN + CNN + SNN + LNN)

Usage:
    python scripts/ablation_study.py
    python scripts/ablation_study.py --neuron-count 50000 --steps 500
    python scripts/ablation_study.py --resume  # skip configs already in results file
"""

import argparse
import json
import os
import sys
import time
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

# Suppress noisy logging
os.environ["TOKENIZERS_PARALLELISM"] = "false"
os.environ["TQDM_DISABLE"] = "1"
os.environ["NIMCP_NO_COW_SIGNAL"] = "1"
import warnings
warnings.filterwarnings("ignore", category=FutureWarning)

import nimcp

# Try to import sentence-transformers for real embeddings, fall back to random
try:
    from claude_teacher import encode_text, batch_encode_texts
    HAS_ENCODER = True
except Exception:
    HAS_ENCODER = False

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
EMBED_DIM = 1024
BRAIN_INPUTS = 1024
BRAIN_OUTPUTS = 4096
RESULTS_FILE = "ablation_results.json"

# ---------------------------------------------------------------------------
# Dataset: temporal sequences, causal chains, and dynamic processes
#
# SNN excels at: spike-timing patterns, event sequences, temporal correlations
# LNN excels at: continuous dynamics, ODE-governed processes, time-series
# These samples exercise both — presented IN ORDER, not shuffled.
# ---------------------------------------------------------------------------

# Each "sequence" is a list of (text, category) steps that must be presented
# in order. The brain sees step 1, then step 2, etc. — building temporal context.
TEMPORAL_SEQUENCES = [
    # --- Causal chains: A causes B causes C (SNN spike timing) ---
    [
        ("Dark clouds gather on the horizon", "cause_effect"),
        ("The wind picks up and temperature drops", "cause_effect"),
        ("Rain begins to fall heavily", "cause_effect"),
        ("Puddles form on the ground", "cause_effect"),
        ("A rainbow appears as the sun returns", "cause_effect"),
    ],
    [
        ("A seed is planted in moist soil", "cause_effect"),
        ("Roots push down and a sprout emerges", "cause_effect"),
        ("Leaves unfold and begin photosynthesis", "cause_effect"),
        ("The stem grows taller toward sunlight", "cause_effect"),
        ("Flowers bloom and attract pollinators", "cause_effect"),
    ],
    [
        ("Hunger signals arise from the stomach", "cause_effect"),
        ("The brain decides to search for food", "cause_effect"),
        ("Hands reach out and grasp an apple", "cause_effect"),
        ("Teeth bite and jaw muscles chew", "cause_effect"),
        ("Nutrients absorb and hunger subsides", "cause_effect"),
    ],

    # --- Rhythmic patterns: repeating temporal structure (SNN periodicity) ---
    [
        ("The heart beats once with a strong pulse", "rhythm"),
        ("A brief pause follows the heartbeat", "rhythm"),
        ("The heart beats again with a strong pulse", "rhythm"),
        ("A brief pause follows the heartbeat", "rhythm"),
        ("The heart beats once more completing the cycle", "rhythm"),
    ],
    [
        ("Breathe in slowly filling the lungs with air", "rhythm"),
        ("Hold the breath for a moment of stillness", "rhythm"),
        ("Breathe out slowly releasing the air", "rhythm"),
        ("Breathe in slowly filling the lungs with air", "rhythm"),
        ("Breathe out slowly releasing the air", "rhythm"),
    ],
    [
        ("Day breaks with warm golden sunlight", "rhythm"),
        ("Noon arrives with the sun directly overhead", "rhythm"),
        ("Evening comes as shadows grow long", "rhythm"),
        ("Night falls and stars appear in darkness", "rhythm"),
        ("Day breaks again with warm golden sunlight", "rhythm"),
    ],

    # --- Continuous dynamics: smooth state evolution (LNN ODE trajectories) ---
    [
        ("A ball is released from the top of a hill", "dynamics"),
        ("It accelerates slowly gaining speed downward", "dynamics"),
        ("Halfway down it moves at moderate velocity", "dynamics"),
        ("Near the bottom it reaches maximum speed", "dynamics"),
        ("It rolls onto flat ground and gradually decelerates", "dynamics"),
    ],
    [
        ("Water is heated from room temperature", "dynamics"),
        ("Small bubbles form on the bottom of the pot", "dynamics"),
        ("The water begins to simmer with rising bubbles", "dynamics"),
        ("A rolling boil develops with vigorous motion", "dynamics"),
        ("Steam rises rapidly as water evaporates", "dynamics"),
    ],
    [
        ("A pendulum is pulled to the right and released", "dynamics"),
        ("It swings left through the center point", "dynamics"),
        ("It reaches maximum height on the left side", "dynamics"),
        ("It swings right back through the center", "dynamics"),
        ("It returns near its starting position slightly lower", "dynamics"),
    ],

    # --- Narrative sequences: story arcs with temporal progression ---
    [
        ("A child wakes up excited on a snowy morning", "narrative"),
        ("She puts on warm boots and a thick coat", "narrative"),
        ("She steps outside and feels cold air on her face", "narrative"),
        ("She builds a snowman with a carrot nose", "narrative"),
        ("She goes inside for hot chocolate feeling happy", "narrative"),
    ],
    [
        ("A bird discovers a good spot to build a nest", "narrative"),
        ("It gathers twigs and grass carrying them one by one", "narrative"),
        ("The nest takes shape over several days of work", "narrative"),
        ("Eggs are laid carefully in the soft center", "narrative"),
        ("Baby birds hatch and the parent feeds them", "narrative"),
    ],

    # --- Temporal prediction: what comes next (SNN + LNN both) ---
    [
        ("One two three four five six seven", "counting"),
        ("Eight nine ten eleven twelve thirteen", "counting"),
        ("Monday Tuesday Wednesday Thursday", "counting"),
        ("Friday Saturday Sunday then Monday again", "counting"),
        ("January February March April May June", "counting"),
    ],
    [
        ("Spring brings new growth and warming days", "seasons"),
        ("Summer follows with heat and long daylight", "seasons"),
        ("Autumn arrives bringing cool air and falling leaves", "seasons"),
        ("Winter comes with cold and shorter days", "seasons"),
        ("Spring returns again completing the yearly cycle", "seasons"),
    ],

    # --- Gradual state change: slow continuous evolution (LNN) ---
    [
        ("An ice cube sits in a warm room unchanged", "gradual"),
        ("The edges begin to soften and glisten with moisture", "gradual"),
        ("A pool of water forms around the shrinking cube", "gradual"),
        ("Only a small sliver of ice remains floating", "gradual"),
        ("The ice is completely gone leaving only water", "gradual"),
    ],
    [
        ("A caterpillar crawls slowly along a branch", "gradual"),
        ("It spins silk and forms a chrysalis around itself", "gradual"),
        ("Inside the chrysalis transformation occurs over days", "gradual"),
        ("The chrysalis cracks open revealing folded wings", "gradual"),
        ("A butterfly emerges and flies away on new wings", "gradual"),
    ],

    # --- Event-driven reactions: stimulus-response (SNN spike patterns) ---
    [
        ("A loud sudden noise echoes through the room", "stimulus"),
        ("The body startles with a rapid heartbeat increase", "stimulus"),
        ("Eyes scan the environment searching for the source", "stimulus"),
        ("The source is identified as a dropped book", "stimulus"),
        ("Heart rate returns to normal as calm is restored", "stimulus"),
    ],
    [
        ("A bright flash of lightning illuminates the sky", "stimulus"),
        ("Thunder follows after a short delay", "stimulus"),
        ("Another flash appears closer than before", "stimulus"),
        ("Thunder arrives more quickly this time", "stimulus"),
        ("The storm moves directly overhead with simultaneous flash and boom", "stimulus"),
    ],
]

# Flatten for the simple dataset interface but preserve sequence info
ABLATION_DATASET = []
for seq_idx, sequence in enumerate(TEMPORAL_SEQUENCES):
    for step_idx, (text, category) in enumerate(sequence):
        ABLATION_DATASET.append((text, category, seq_idx, step_idx))


def tile_to_brain_input(embedding):
    """Tile/truncate embedding to BRAIN_INPUTS dimensions."""
    e = np.asarray(embedding, dtype=np.float32).ravel()
    e = (e + 1.0) * 0.5  # normalize [-1,1] -> [0,1]
    import math
    reps = math.ceil(BRAIN_INPUTS / len(e))
    tiled = np.tile(e, reps)[:BRAIN_INPUTS]
    return tiled.tolist()


def make_semantic_target(text, target_dim=BRAIN_OUTPUTS):
    """Create a semantic target vector from text embedding."""
    if HAS_ENCODER:
        emb = encode_text(text)
    else:
        # Deterministic hash-based embedding for reproducibility
        import hashlib
        h = hashlib.sha256(text.encode()).digest()
        rng = np.random.RandomState(int.from_bytes(h[:4], 'big'))
        emb = rng.randn(EMBED_DIM).astype(np.float32)
        emb = emb / (np.linalg.norm(emb) + 1e-8)

    emb_norm = (emb + 1.0) * 0.5
    import math
    reps = math.ceil(target_dim / len(emb_norm))
    target = np.tile(emb_norm, reps)[:target_dim]
    return target.tolist()


def prepare_dataset():
    """Pre-encode all training pairs, preserving sequence structure."""
    print("Preparing dataset...")
    if HAS_ENCODER:
        texts = [text for text, *_ in ABLATION_DATASET]
        print(f"  Encoding {len(texts)} texts with sentence-transformers...")
        embeddings = batch_encode_texts(texts)
    else:
        print("  WARNING: No sentence-transformers available, using hash-based embeddings")
        embeddings = []
        for text, *_ in ABLATION_DATASET:
            import hashlib
            h = hashlib.sha256(text.encode()).digest()
            rng = np.random.RandomState(int.from_bytes(h[:4], 'big'))
            emb = rng.randn(EMBED_DIM).astype(np.float32)
            emb = emb / (np.linalg.norm(emb) + 1e-8)
            embeddings.append(emb)

    dataset = []
    for i, item in enumerate(ABLATION_DATASET):
        text, category = item[0], item[1]
        seq_idx = item[2] if len(item) > 2 else 0
        step_idx = item[3] if len(item) > 3 else i
        features = tile_to_brain_input(embeddings[i])
        target = make_semantic_target(text)
        dataset.append({
            "text": text,
            "category": category,
            "seq_idx": seq_idx,
            "step_idx": step_idx,
            "features": features,
            "target": target,
            "label": f"{category}:{text[:30]}",
        })

    categories = set(d['category'] for d in dataset)
    num_sequences = len(set(d['seq_idx'] for d in dataset))
    print(f"  Dataset ready: {len(dataset)} samples, {num_sequences} sequences, "
          f"{len(categories)} categories ({', '.join(sorted(categories))})")
    return dataset


# ---------------------------------------------------------------------------
# Ablation configurations
# ---------------------------------------------------------------------------
CONFIGS = [
    {"name": "ANN-only",      "train_cnn": 0, "train_snn": 0, "train_lnn": 0, "lnn": False},
    {"name": "ANN+CNN",        "train_cnn": 1, "train_snn": 0, "train_lnn": 0, "lnn": False},
    {"name": "ANN+SNN",        "train_cnn": 0, "train_snn": 1, "train_lnn": 0, "lnn": False},
    {"name": "ANN+LNN",        "train_cnn": 0, "train_snn": 0, "train_lnn": 1, "lnn": True},
    {"name": "All-networks",   "train_cnn": 1, "train_snn": 1, "train_lnn": 1, "lnn": True},
]


def run_config(config, dataset, neuron_count, num_steps, num_epochs):
    """Run a single ablation configuration and return metrics."""
    name = config["name"]
    print(f"\n{'='*60}")
    print(f"  CONFIG: {name}")
    print(f"{'='*60}")

    # Create fresh brain with FAST init for speed
    print(f"  Creating brain ({neuron_count} neurons, FAST init)...")
    t0 = time.time()
    brain = nimcp.Brain(f"ablation_{name.replace('+','_').lower()}",
                        num_inputs=BRAIN_INPUTS,
                        num_outputs=BRAIN_OUTPUTS,
                        neuron_count=neuron_count,
                        init_mode='fast')
    init_time = time.time() - t0
    print(f"  Brain created in {init_time:.1f}s")

    # Enable training-mode fast path (skip cognitive modules)
    brain.set_training_mode(True)

    # Enable multi-network training (creates SNN + LNN + CNN + dispatch)
    try:
        brain.enable_multi_network()
        print("  Multi-network training: enabled (SNN + LNN + CNN created)")
    except Exception as e:
        print(f"  Multi-network training: failed ({e})")

    # Set ablation flags
    brain.set_network_ablation(
        train_cnn=config["train_cnn"],
        train_snn=config["train_snn"],
        train_lnn=config["train_lnn"],
    )
    print(f"  Ablation: CNN={bool(config['train_cnn'])}, "
          f"SNN={bool(config['train_snn'])}, LNN={bool(config['train_lnn'])}")

    # Training loop — present sequences IN ORDER to exercise temporal learning
    # Shuffle sequence order each epoch, but steps within each sequence stay ordered
    loss_history = []
    epoch_losses = []
    step = 0
    t_train_start = time.time()

    # Group dataset by sequence
    from collections import defaultdict
    sequences = defaultdict(list)
    for i, sample in enumerate(dataset):
        sequences[sample["seq_idx"]].append(i)
    # Sort each sequence by step_idx
    for seq_id in sequences:
        sequences[seq_id].sort(key=lambda i: dataset[i]["step_idx"])
    seq_ids = list(sequences.keys())

    for epoch in range(num_epochs):
        epoch_loss_sum = 0.0
        epoch_count = 0

        # Shuffle which sequences come first, but keep steps within each in order
        np.random.shuffle(seq_ids)

        for seq_id in seq_ids:
            for idx in sequences[seq_id]:
                if step >= num_steps:
                    break

                sample = dataset[idx]
                features = sample["features"]
                target = sample["target"]
                label = sample["label"]

                try:
                    loss = brain.learn_vector(features, target, label=label,
                                              confidence=0.7, learning_rate=0.001)
                    if loss is not None and np.isfinite(loss):
                        loss_history.append({"step": step, "loss": float(loss),
                                             "epoch": epoch, "seq": seq_id,
                                             "category": sample["category"]})
                        epoch_loss_sum += float(loss)
                        epoch_count += 1
                    else:
                        loss_history.append({"step": step, "loss": None, "note": "non-finite"})
                except Exception as e:
                    loss_history.append({"step": step, "loss": None, "error": str(e)})

                step += 1

            if step >= num_steps:
                break

            # Present a decide_full between sequences — forces SNN/LNN to process
            # the temporal context before the next sequence begins
            try:
                brain.decide_full(dataset[sequences[seq_id][0]]["features"])
            except Exception:
                pass

        # Progress every epoch
        if epoch_count > 0:
            epoch_avg = epoch_loss_sum / epoch_count
            rate = step / (time.time() - t_train_start) if step > 0 else 0
            print(f"  Epoch {epoch}: avg_loss={epoch_avg:.6f} ({epoch_count} samples, "
                  f"{rate:.1f} steps/s)")

        if epoch_count > 0:
            epoch_avg = epoch_loss_sum / epoch_count
            epoch_losses.append({"epoch": epoch, "avg_loss": epoch_avg, "samples": epoch_count})

        if step >= num_steps:
            break

    train_time = time.time() - t_train_start

    # Evaluation: run decide on all samples, measure output similarity to target
    print(f"  Evaluating...")
    eval_results = []
    for sample in dataset:
        try:
            result = brain.decide_full(sample["features"])
            output_vec = result.get("output_vector") if result else None
            if output_vec is not None:
                out = np.array(output_vec, dtype=np.float32)
                tgt = np.array(sample["target"], dtype=np.float32)
                # Cosine similarity
                dot = np.dot(out, tgt)
                norm_out = np.linalg.norm(out)
                norm_tgt = np.linalg.norm(tgt)
                if norm_out > 1e-8 and norm_tgt > 1e-8:
                    cosine_sim = float(dot / (norm_out * norm_tgt))
                else:
                    cosine_sim = 0.0
                # MSE
                mse = float(np.mean((out - tgt) ** 2))
                eval_results.append({
                    "text": sample["text"],
                    "category": sample["category"],
                    "cosine_sim": cosine_sim,
                    "mse": mse,
                })
        except Exception as e:
            eval_results.append({"text": sample["text"], "error": str(e)})

    # Get per-network metrics
    metrics = brain.get_network_metrics()

    # Compute summary stats
    valid_losses = [h["loss"] for h in loss_history if h.get("loss") is not None]
    valid_evals = [e for e in eval_results if "cosine_sim" in e]

    first_quarter = valid_losses[:len(valid_losses)//4] if valid_losses else []
    last_quarter = valid_losses[3*len(valid_losses)//4:] if valid_losses else []

    summary = {
        "config": name,
        "neuron_count": neuron_count,
        "num_steps": step,
        "num_epochs": num_epochs,
        "init_time_s": init_time,
        "train_time_s": train_time,
        "steps_per_second": step / train_time if train_time > 0 else 0,
        "final_avg_loss": float(np.mean(last_quarter)) if last_quarter else None,
        "initial_avg_loss": float(np.mean(first_quarter)) if first_quarter else None,
        "loss_reduction_pct": None,
        "avg_cosine_sim": float(np.mean([e["cosine_sim"] for e in valid_evals])) if valid_evals else None,
        "avg_mse": float(np.mean([e["mse"] for e in valid_evals])) if valid_evals else None,
        "per_category_sim": {},
        "network_metrics": metrics,
        "loss_history": loss_history,
        "epoch_losses": epoch_losses,
        "eval_results": eval_results,
    }

    if summary["initial_avg_loss"] and summary["final_avg_loss"] and summary["initial_avg_loss"] > 0:
        summary["loss_reduction_pct"] = (
            (summary["initial_avg_loss"] - summary["final_avg_loss"])
            / summary["initial_avg_loss"] * 100
        )

    # Per-category cosine similarity
    categories = set(e["category"] for e in valid_evals)
    for cat in sorted(categories):
        cat_sims = [e["cosine_sim"] for e in valid_evals if e["category"] == cat]
        summary["per_category_sim"][cat] = float(np.mean(cat_sims))

    # Print results
    print(f"\n  --- {name} Results ---")
    print(f"  Training: {step} steps in {train_time:.1f}s ({summary['steps_per_second']:.1f} steps/s)")
    print(f"  Loss: {summary['initial_avg_loss']:.6f} → {summary['final_avg_loss']:.6f} "
          f"({summary['loss_reduction_pct']:.1f}% reduction)" if summary['loss_reduction_pct'] else
          f"  Loss: {summary.get('final_avg_loss', 'N/A')}")
    print(f"  Avg cosine similarity: {summary['avg_cosine_sim']:.4f}" if summary['avg_cosine_sim'] else
          "  Avg cosine similarity: N/A")
    print(f"  Avg MSE: {summary['avg_mse']:.6f}" if summary['avg_mse'] else "  Avg MSE: N/A")
    if summary["per_category_sim"]:
        print(f"  Per-category similarity:")
        for cat, sim in sorted(summary["per_category_sim"].items(), key=lambda x: -x[1]):
            print(f"    {cat:12s}: {sim:.4f}")
    if metrics:
        print(f"  Network metrics: {json.dumps(metrics, indent=2)}")

    # Clean up
    del brain
    return summary


def print_comparison(results):
    """Print a comparison table of all configurations."""
    print(f"\n{'='*80}")
    print(f"  ABLATION STUDY RESULTS — COMPARISON")
    print(f"{'='*80}")
    print(f"\n  {'Config':<16s} {'Final Loss':>12s} {'Loss Red%':>10s} "
          f"{'Cos Sim':>10s} {'MSE':>12s} {'Steps/s':>10s}")
    print(f"  {'-'*16} {'-'*12} {'-'*10} {'-'*10} {'-'*12} {'-'*10}")

    for r in results:
        final = f"{r['final_avg_loss']:.6f}" if r['final_avg_loss'] is not None else "N/A"
        red = f"{r['loss_reduction_pct']:.1f}%" if r['loss_reduction_pct'] is not None else "N/A"
        sim = f"{r['avg_cosine_sim']:.4f}" if r['avg_cosine_sim'] is not None else "N/A"
        mse = f"{r['avg_mse']:.6f}" if r['avg_mse'] is not None else "N/A"
        rate = f"{r['steps_per_second']:.1f}"
        print(f"  {r['config']:<16s} {final:>12s} {red:>10s} {sim:>10s} {mse:>12s} {rate:>10s}")

    # Best config by cosine similarity
    valid = [r for r in results if r['avg_cosine_sim'] is not None]
    if valid:
        best = max(valid, key=lambda x: x['avg_cosine_sim'])
        worst = min(valid, key=lambda x: x['avg_cosine_sim'])
        print(f"\n  Best by similarity:  {best['config']} ({best['avg_cosine_sim']:.4f})")
        print(f"  Worst by similarity: {worst['config']} ({worst['avg_cosine_sim']:.4f})")

    # Best by loss reduction
    valid_loss = [r for r in results if r['loss_reduction_pct'] is not None]
    if valid_loss:
        best_lr = max(valid_loss, key=lambda x: x['loss_reduction_pct'])
        print(f"  Best loss reduction: {best_lr['config']} ({best_lr['loss_reduction_pct']:.1f}%)")

    # Fastest training
    fastest = max(results, key=lambda x: x['steps_per_second'])
    print(f"  Fastest training:    {fastest['config']} ({fastest['steps_per_second']:.1f} steps/s)")

    print(f"\n  Per-category breakdown:")
    all_cats = sorted(set(
        cat for r in results for cat in r.get("per_category_sim", {}).keys()
    ))
    if all_cats:
        header = f"  {'Category':<12s}" + "".join(f" {r['config']:>14s}" for r in results)
        print(header)
        print(f"  {'-'*12}" + "".join(f" {'-'*14}" for _ in results))
        for cat in all_cats:
            row = f"  {cat:<12s}"
            for r in results:
                sim = r.get("per_category_sim", {}).get(cat)
                row += f" {sim:>14.4f}" if sim is not None else f" {'N/A':>14s}"
            print(row)


def main():
    parser = argparse.ArgumentParser(description="NIMCP Network Ablation Study")
    parser.add_argument("--neuron-count", type=int, default=100000,
                        help="Neurons per brain (default: 100K for speed)")
    parser.add_argument("--steps", type=int, default=400,
                        help="Training steps per config (default: 400)")
    parser.add_argument("--epochs", type=int, default=20,
                        help="Max epochs (default: 20, capped by --steps)")
    parser.add_argument("--resume", action="store_true",
                        help="Skip configs already present in results file")
    parser.add_argument("--configs", nargs="+",
                        help="Only run specific configs (e.g., ANN-only ANN+CNN)")
    args = parser.parse_args()

    print(f"NIMCP Ablation Study")
    print(f"  Neurons: {args.neuron_count}")
    print(f"  Steps per config: {args.steps}")
    print(f"  Max epochs: {args.epochs}")
    print(f"  Encoder: {'sentence-transformers' if HAS_ENCODER else 'hash-based (fallback)'}")

    # Prepare dataset once
    dataset = prepare_dataset()

    # Load previous results if resuming
    existing_results = {}
    if args.resume and os.path.exists(RESULTS_FILE):
        with open(RESULTS_FILE) as f:
            prev = json.load(f)
            for r in prev.get("results", []):
                existing_results[r["config"]] = r
            print(f"  Loaded {len(existing_results)} previous results from {RESULTS_FILE}")

    # Filter configs if specified
    configs = CONFIGS
    if args.configs:
        configs = [c for c in CONFIGS if c["name"] in args.configs]
        if not configs:
            print(f"  ERROR: No matching configs. Available: {[c['name'] for c in CONFIGS]}")
            sys.exit(1)

    # Run each configuration
    results = []
    for config in configs:
        if args.resume and config["name"] in existing_results:
            print(f"\n  Skipping {config['name']} (already in results)")
            results.append(existing_results[config["name"]])
            continue

        try:
            result = run_config(config, dataset, args.neuron_count, args.steps, args.epochs)
            results.append(result)
        except Exception as e:
            print(f"\n  ERROR running {config['name']}: {e}")
            import traceback
            traceback.print_exc()
            results.append({
                "config": config["name"],
                "error": str(e),
                "final_avg_loss": None,
                "loss_reduction_pct": None,
                "avg_cosine_sim": None,
                "avg_mse": None,
                "steps_per_second": 0,
                "per_category_sim": {},
            })

        # Save incrementally
        output = {
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
            "neuron_count": args.neuron_count,
            "steps_per_config": args.steps,
            "dataset_size": len(dataset),
            "encoder": "sentence-transformers" if HAS_ENCODER else "hash-based",
            "results": results,
        }
        with open(RESULTS_FILE, "w") as f:
            json.dump(output, f, indent=2, default=str)
        print(f"  Results saved to {RESULTS_FILE}")

    # Final comparison
    print_comparison(results)


if __name__ == "__main__":
    main()
