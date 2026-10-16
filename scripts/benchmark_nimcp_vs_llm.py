#!/usr/bin/env python3
"""
benchmark_nimcp_vs_llm.py — Compare 1M-neuron NIMCP brain vs frontier LLMs

Runs all benchmark categories:
  1. ML benchmarks:    Wine, Breast Cancer, Fashion-MNIST
  2. GenAI benchmarks: MMLU, ARC-Easy, HellaSwag, Winogrande
  3. Cognitive metrics: Working memory, oscillation coherence, ethics separation

Reference scores from published results:
  - GPT-4, Claude 3.5, Llama 70B (GenAI)
  - SVM, Random Forest, MLP, CNN (ML)

Usage:
  cd /home/bbrelin/nimcp
  python3 benchmark_nimcp_vs_llm.py [--neurons 1000000] [--epochs 20] [--strategy auto]
"""

import argparse
import random
import sys
import time

# ── Must be importable from project root ─────────────────────────────────────
sys.path.insert(0, "/home/bbrelin/nimcp/frontend/backend")

import nimcp
from benchmark_datasets import (
    BENCHMARK_DATASETS, GENAI_DATASETS, BENCHMARK_META, REFERENCE_SCORES,
    EthicsScenarios,
)


# ── Formatting helpers ───────────────────────────────────────────────────────

def bar(value, width=30, fill="█", empty="░"):
    n = int(value * width)
    return fill * n + empty * (width - n)


def fmt_pct(v):
    return f"{v * 100:6.2f}%"


def print_header(title):
    w = 72
    print()
    print("═" * w)
    print(f"  {title}")
    print("═" * w)


def print_separator():
    print("─" * 72)


# ── Dataset loading ──────────────────────────────────────────────────────────

def load_examples(benchmark_id):
    """Load examples for a benchmark."""
    if benchmark_id in BENCHMARK_DATASETS:
        return BENCHMARK_DATASETS[benchmark_id]().get_examples()
    if benchmark_id in GENAI_DATASETS:
        return GENAI_DATASETS[benchmark_id]().get_examples()
    return []


# ── Single benchmark runner ──────────────────────────────────────────────────

def run_benchmark(brain, benchmark_id, epochs, strategy):
    """Train and evaluate brain on a single benchmark. Returns dict of results."""
    meta = BENCHMARK_META.get(benchmark_id)
    if not meta:
        return None

    examples = load_examples(benchmark_id)
    if not examples:
        print(f"    WARNING: No examples for {benchmark_id}")
        return None

    num_features = meta["num_features"]
    num_classes = meta["num_classes"]

    # Split train/test (70/30, fixed seed)
    rng = random.Random(42)
    shuffled = list(examples)
    rng.shuffle(shuffled)
    split = int(len(shuffled) * 0.7)
    train_examples = shuffled[:split]
    test_examples = shuffled[split:]

    # Build label mapping
    all_labels = sorted(set(str(ex.get("label", "0")) for ex in examples))
    label_to_idx = {label: i for i, label in enumerate(all_labels)}
    num_outputs = max(len(all_labels) * 2, len(all_labels) + 8, 10)

    # Determine actual strategy
    actual_strategy = strategy
    if strategy == "auto":
        n = len(train_examples)
        actual_strategy = "hebbian" if n < 100 else ("hybrid" if n < 500 else "gradient")

    # Configure training for gradient strategy
    if actual_strategy in ("gradient", "hybrid"):
        try:
            config = nimcp.TrainingConfig(
                loss_type=nimcp.LOSS_CROSS_ENTROPY,
                optimizer_type=nimcp.OPT_ADAM,
                scheduler_type=nimcp.SCHED_COSINE,
                learning_rate=0.01,
                enable_gradient_clipping=True,
                gradient_clip_value=1.0,
            )
            brain.configure_training(config)
        except Exception:
            pass

    # Train
    t0 = time.monotonic()
    epoch_losses = []

    for epoch in range(epochs):
        epoch_examples = list(train_examples)
        rng.shuffle(epoch_examples)
        losses = []

        for ex in epoch_examples:
            features = ex.get("features", ex.get("input", []))
            label = str(ex.get("label", "0"))

            if actual_strategy in ("gradient", "hybrid"):
                targets = [0.0] * num_outputs
                idx = label_to_idx.get(label, 0)
                if idx < len(targets):
                    targets[idx] = 1.0
                try:
                    result = brain.train_step(features, targets)
                    losses.append(float(result.loss))
                except Exception:
                    pass

            if actual_strategy in ("hebbian", "hybrid"):
                try:
                    brain.learn(features, label, 1.0)
                except Exception:
                    pass

        avg_loss = sum(losses) / len(losses) if losses else 0.0
        epoch_losses.append(avg_loss)

        if epoch == 0 or epoch == epochs - 1 or (epoch + 1) % 5 == 0:
            # Quick train accuracy sample
            sample = train_examples[:min(50, len(train_examples))]
            correct = 0
            for ex in sample:
                try:
                    pred = brain.predict(ex["features"])
                    pred_label = pred[0].split(" [")[0] if " [" in pred[0] else pred[0]
                    if pred_label == str(ex["label"]):
                        correct += 1
                except Exception:
                    pass
            train_acc = correct / len(sample) if sample else 0
            print(f"      Epoch {epoch+1:3d}/{epochs}: loss={avg_loss:.4f}  train_acc={train_acc:.3f}")

    train_time = time.monotonic() - t0

    # Evaluate on test set
    test_correct = 0
    test_total = 0
    inference_times = []

    for ex in test_examples:
        features = ex.get("features", ex.get("input", []))
        label = str(ex.get("label", "0"))
        try:
            t_inf = time.monotonic()
            pred = brain.predict(features)
            inf_us = (time.monotonic() - t_inf) * 1_000_000
            inference_times.append(inf_us)

            if pred is not None:
                pred_label = pred[0].split(" [")[0] if " [" in pred[0] else pred[0]
                test_total += 1
                if pred_label == label:
                    test_correct += 1
        except Exception:
            test_total += 1

    test_acc = test_correct / test_total if test_total > 0 else 0.0
    avg_inf_us = sum(inference_times) / len(inference_times) if inference_times else 0.0

    return {
        "benchmark_id": benchmark_id,
        "category": meta["category"],
        "accuracy": test_acc,
        "final_loss": epoch_losses[-1] if epoch_losses else 0.0,
        "train_time_s": train_time,
        "inference_us": avg_inf_us,
        "strategy": actual_strategy,
        "test_total": test_total,
        "test_correct": test_correct,
    }


# ── Cognitive benchmarks ─────────────────────────────────────────────────────

def run_cognitive(brain):
    """Run cognitive metrics on trained brain."""
    metrics = {}

    # Working memory
    try:
        wm_stats = brain.working_memory_stats()
        if isinstance(wm_stats, dict):
            metrics["wm_capacity"] = wm_stats.get("capacity", 0)
            metrics["wm_occupancy"] = wm_stats.get("occupancy", 0.0)
        elif isinstance(wm_stats, (tuple, list)) and len(wm_stats) >= 2:
            metrics["wm_capacity"] = int(wm_stats[0])
            metrics["wm_occupancy"] = float(wm_stats[1])
    except Exception:
        metrics["wm_capacity"] = 0
        metrics["wm_occupancy"] = 0.0

    # Oscillation coherence
    try:
        coherence = brain.get_phase_coherence()
        metrics["oscillation_coherence"] = float(coherence) if coherence else 0.0
    except Exception:
        metrics["oscillation_coherence"] = 0.0

    # PAC
    try:
        pac = brain.get_pac_modulation()
        metrics["pac_index"] = float(pac) if pac else 0.0
    except Exception:
        metrics["pac_index"] = 0.0

    # Ethics
    try:
        scenarios = EthicsScenarios.get_scenarios()
        harmful_scores = []
        beneficial_scores = []
        for s in scenarios:
            try:
                result = brain.predict(s["features"])
                if result:
                    confidence = float(result[1])
                    if s["category"] == "harmful":
                        harmful_scores.append(confidence)
                    elif s["category"] == "beneficial":
                        beneficial_scores.append(confidence)
            except Exception:
                pass

        metrics["ethics_harmful"] = (
            sum(harmful_scores) / len(harmful_scores) if harmful_scores else 0.0)
        metrics["ethics_beneficial"] = (
            sum(beneficial_scores) / len(beneficial_scores) if beneficial_scores else 0.0)
        metrics["ethics_separation"] = metrics["ethics_beneficial"] - metrics["ethics_harmful"]
    except Exception:
        metrics["ethics_harmful"] = 0.0
        metrics["ethics_beneficial"] = 0.0
        metrics["ethics_separation"] = 0.0

    return metrics


# ── Comparison table rendering ───────────────────────────────────────────────

def print_comparison_table(results, category_filter):
    """Print a formatted comparison table for one category."""
    filtered = [r for r in results if r["category"] == category_filter]
    if not filtered:
        return

    # Determine reference columns
    ref_models = set()
    for r in filtered:
        refs = REFERENCE_SCORES.get(r["benchmark_id"], {})
        ref_models.update(refs.keys())

    # Sort reference models for consistent column order
    if category_filter == "generative_ai":
        col_order = ["gpt4", "claude35", "llama_70b", "random"]
        ref_cols = [m for m in col_order if m in ref_models]
    else:
        col_order = ["svm", "random_forest", "knn", "mlp", "cnn"]
        ref_cols = [m for m in col_order if m in ref_models]

    # Column names
    col_names = {
        "gpt4": "GPT-4", "claude35": "Claude 3.5", "llama_70b": "Llama-70B",
        "random": "Random", "svm": "SVM", "random_forest": "RF",
        "knn": "KNN", "mlp": "MLP", "cnn": "CNN",
    }

    # Header
    header = f"  {'Benchmark':<16} {'NIMCP':>8}"
    for col in ref_cols:
        header += f" {col_names.get(col, col):>10}"
    header += f"  {'vs Best':>8}  {'Time':>6}"
    print(header)
    print_separator()

    total_nimcp = 0
    total_best = 0
    count = 0

    for r in filtered:
        bid = r["benchmark_id"]
        acc = r["accuracy"]
        refs = REFERENCE_SCORES.get(bid, {})
        best_ref = max(refs.values()) if refs else 0

        row = f"  {bid:<16} {fmt_pct(acc)}"
        for col in ref_cols:
            val = refs.get(col)
            if val is not None:
                row += f" {fmt_pct(val)}"
            else:
                row += f" {'---':>10}"

        vs_best = acc / best_ref if best_ref > 0 else 0
        row += f"  {fmt_pct(vs_best)}  {r['train_time_s']:5.1f}s"
        print(row)

        total_nimcp += acc
        total_best += best_ref
        count += 1

    print_separator()
    avg_nimcp = total_nimcp / count if count > 0 else 0
    avg_best = total_best / count if count > 0 else 0
    avg_vs = avg_nimcp / avg_best if avg_best > 0 else 0
    print(f"  {'AVERAGE':<16} {fmt_pct(avg_nimcp)}"
          + " " * (11 * len(ref_cols))
          + f"  {fmt_pct(avg_vs)}")

    return avg_nimcp, avg_best


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="NIMCP vs Frontier LLM Benchmark")
    parser.add_argument("--neurons", type=int, default=1_000_000,
                        help="Target neuron count (default: 1000000)")
    parser.add_argument("--epochs", type=int, default=20,
                        help="Training epochs per benchmark (default: 20)")
    parser.add_argument("--strategy", default="auto",
                        choices=["auto", "gradient", "hebbian", "hybrid"],
                        help="Training strategy (default: auto)")
    parser.add_argument("--benchmarks", nargs="*", default=None,
                        help="Specific benchmarks to run (default: all)")
    args = parser.parse_args()

    print_header("NIMCP vs Frontier LLM — Comprehensive Benchmark")
    print(f"  Target neurons:  {args.neurons:,}")
    print(f"  Epochs:          {args.epochs}")
    print(f"  Strategy:        {args.strategy}")
    print()

    # Initialize NIMCP
    nimcp.init()
    print(f"  NIMCP version:   {nimcp.version()}")

    # ── Phase 1: Create brain and resize to target ───────────────────────────
    print_header("Phase 1: Brain Creation")

    t_create = time.monotonic()
    # Start with BRAIN_LARGE (100K), then resize up
    initial_size = nimcp.BRAIN_LARGE
    brain = nimcp.Brain(
        name="benchmark_1m",
        size=initial_size,
        task=nimcp.TASK_CLASSIFICATION,
        num_inputs=784,   # max feature count across benchmarks
        num_outputs=20,   # max output count across benchmarks
    )

    initial_count = brain.get_neuron_count()
    print(f"  Initial neurons: {initial_count:,}")

    if args.neurons > initial_count:
        print(f"  Resizing to {args.neurons:,} neurons...")
        t_resize = time.monotonic()
        ok = brain.resize(args.neurons)
        resize_time = time.monotonic() - t_resize
        if ok:
            final_count = brain.get_neuron_count()
            print(f"  Resize complete: {final_count:,} neurons ({resize_time:.1f}s)")
        else:
            print(f"  WARNING: Resize failed, continuing with {initial_count:,} neurons")

    create_time = time.monotonic() - t_create
    neuron_count = brain.get_neuron_count()
    print(f"  Final neuron count: {neuron_count:,}")
    print(f"  Creation time:      {create_time:.1f}s")

    # Enable cognitive features
    try:
        brain.enable_complex_oscillations(True)
        print(f"  Oscillations:       enabled")
    except Exception:
        print(f"  Oscillations:       not available")

    # Brain probe for memory stats
    try:
        probe = brain.probe()
        if isinstance(probe, dict):
            mem_mb = probe.get("memory_bytes", 0) / (1024 * 1024)
            print(f"  Memory usage:       {mem_mb:.0f} MB")
    except Exception:
        pass

    # ── Phase 2: Run benchmarks ──────────────────────────────────────────────
    # Determine which benchmarks to run
    if args.benchmarks:
        benchmark_ids = args.benchmarks
    else:
        # All benchmarks that have datasets
        benchmark_ids = list(BENCHMARK_DATASETS.keys()) + list(GENAI_DATASETS.keys())

    print_header("Phase 2: Training & Evaluation")

    results = []
    for i, bid in enumerate(benchmark_ids):
        meta = BENCHMARK_META.get(bid)
        if not meta:
            print(f"  [{i+1}/{len(benchmark_ids)}] {bid}: UNKNOWN — skipping")
            continue

        print(f"\n  [{i+1}/{len(benchmark_ids)}] {meta['name']} ({meta['category']})")
        print(f"    Features: {meta['num_features']}, Classes: {meta['num_classes']}")

        result = run_benchmark(brain, bid, args.epochs, args.strategy)
        if result:
            results.append(result)
            acc = result["accuracy"]
            refs = REFERENCE_SCORES.get(bid, {})
            best_ref = max(refs.values()) if refs else 0
            vs_best = acc / best_ref if best_ref > 0 else 0

            print(f"    Result: {acc*100:.2f}% accuracy  "
                  f"({result['test_correct']}/{result['test_total']})  "
                  f"vs best: {vs_best*100:.1f}%  "
                  f"infer: {result['inference_us']:.0f}μs")

    # ── Phase 3: Cognitive metrics ───────────────────────────────────────────
    print_header("Phase 3: Cognitive Metrics")
    cognitive = run_cognitive(brain)

    print(f"  Working Memory Capacity:    {cognitive.get('wm_capacity', 0)}")
    print(f"  Working Memory Occupancy:   {cognitive.get('wm_occupancy', 0):.4f}")
    print(f"  Oscillation Coherence:      {cognitive.get('oscillation_coherence', 0):.4f}")
    print(f"  PAC Index:                  {cognitive.get('pac_index', 0):.4f}")
    print(f"  Ethics Harmful Score:       {cognitive.get('ethics_harmful', 0):.4f}")
    print(f"  Ethics Beneficial Score:    {cognitive.get('ethics_beneficial', 0):.4f}")
    print(f"  Ethics Separation:          {cognitive.get('ethics_separation', 0):.4f}")

    # ── Phase 4: Results comparison ──────────────────────────────────────────
    print_header("Phase 4: ML Benchmark Results")
    ml_summary = print_comparison_table(results, "ml")

    print_header("Phase 5: Generative AI Benchmark Results (vs Frontier LLMs)")
    genai_summary = print_comparison_table(results, "generative_ai")

    # ── Phase 5: Visual comparison bars ──────────────────────────────────────
    print_header("Visual Comparison: NIMCP vs Frontier LLMs")

    genai_results = [r for r in results if r["category"] == "generative_ai"]
    for r in genai_results:
        bid = r["benchmark_id"]
        acc = r["accuracy"]
        refs = REFERENCE_SCORES.get(bid, {})

        print(f"\n  {bid.upper()}")
        print(f"    NIMCP (1M)   {bar(acc)} {acc*100:5.1f}%")
        for model in ["gpt4", "claude35", "llama_70b"]:
            val = refs.get(model)
            if val is not None:
                names = {"gpt4": "GPT-4     ", "claude35": "Claude 3.5",
                         "llama_70b": "Llama-70B "}
                print(f"    {names[model]} {bar(val)} {val*100:5.1f}%")

    # ── Summary ──────────────────────────────────────────────────────────────
    print_header("SUMMARY")

    ml_accs = [r["accuracy"] for r in results if r["category"] == "ml"]
    genai_accs = [r["accuracy"] for r in results if r["category"] == "generative_ai"]
    all_accs = [r["accuracy"] for r in results]

    avg_ml = sum(ml_accs) / len(ml_accs) if ml_accs else 0
    avg_genai = sum(genai_accs) / len(genai_accs) if genai_accs else 0
    avg_all = sum(all_accs) / len(all_accs) if all_accs else 0

    total_train = sum(r["train_time_s"] for r in results)
    avg_infer = (sum(r["inference_us"] for r in results) / len(results)) if results else 0

    print(f"  Brain size:              {neuron_count:,} neurons")
    print(f"  Benchmarks completed:    {len(results)}")
    print(f"  Total training time:     {total_train:.1f}s")
    print(f"  Avg inference latency:   {avg_infer:.0f}μs")
    print()
    print(f"  ML accuracy (avg):       {avg_ml*100:.2f}%")
    print(f"  GenAI accuracy (avg):    {avg_genai*100:.2f}%")
    print(f"  Overall accuracy (avg):  {avg_all*100:.2f}%")
    print()

    # Compare against best published scores
    if genai_accs:
        ref_gpt4 = []
        ref_claude = []
        for r in genai_results:
            refs = REFERENCE_SCORES.get(r["benchmark_id"], {})
            if "gpt4" in refs:
                ref_gpt4.append(refs["gpt4"])
            if "claude35" in refs:
                ref_claude.append(refs["claude35"])

        avg_gpt4 = sum(ref_gpt4) / len(ref_gpt4) if ref_gpt4 else 0
        avg_claude = sum(ref_claude) / len(ref_claude) if ref_claude else 0

        print(f"  vs GPT-4 avg:            {avg_genai/avg_gpt4*100:.1f}%" if avg_gpt4 else "")
        print(f"  vs Claude 3.5 avg:       {avg_genai/avg_claude*100:.1f}%" if avg_claude else "")

    # Cognitive health score
    cog_scores = []
    if cognitive.get("oscillation_coherence", 0) > 0:
        cog_scores.append(cognitive["oscillation_coherence"])
    if cognitive.get("wm_capacity", 0) > 0:
        cog_scores.append(min(cognitive["wm_capacity"] / 7.0, 1.0))
    if cognitive.get("ethics_separation", 0) > 0:
        cog_scores.append(min(cognitive["ethics_separation"] / 2.0, 1.0))
    cog_health = sum(cog_scores) / len(cog_scores) if cog_scores else 0
    print(f"\n  Cognitive health score:   {cog_health:.4f}")

    print()
    print("  NOTE: GenAI benchmarks use text→feature-vector encoding, not")
    print("  native language understanding. NIMCP operates on numeric patterns,")
    print("  not token sequences. Direct comparison shows pattern-learning")
    print("  capability, not language comprehension.")
    print()

    del brain
    return 0


if __name__ == "__main__":
    sys.exit(main())
