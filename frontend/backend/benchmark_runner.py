"""Benchmark runner — creates ephemeral brains, trains, evaluates, measures cognitive metrics.

Ephemeral brains are created per-benchmark and destroyed after evaluation.
They don't appear in the sidebar brain list.
"""
import asyncio
import random
import time
from typing import Optional

import nimcp

import nimcp_logger
from benchmark_datasets import (
    BENCHMARK_DATASETS, GENAI_DATASETS, BENCHMARK_META, REFERENCE_SCORES,
    EthicsScenarios,
)
from models.benchmark import BenchmarkResult, BenchmarkSummary, CognitiveMetrics

_log = nimcp_logger.get("benchmark_runner")


class BenchmarkRunner:
    """Runs benchmarks on ephemeral brains."""

    def __init__(self):
        self._running = False
        self._should_stop = False
        self._current_benchmark: Optional[str] = None
        self._progress_queue: Optional[asyncio.Queue] = None
        self._last_results: Optional[BenchmarkSummary] = None

    @property
    def is_running(self) -> bool:
        return self._running

    @property
    def current_benchmark(self) -> Optional[str]:
        return self._current_benchmark

    @property
    def last_results(self) -> Optional[BenchmarkSummary]:
        return self._last_results

    def stop(self):
        self._should_stop = True

    def _notify(self, msg: dict):
        if self._progress_queue:
            try:
                self._progress_queue.put_nowait(msg)
            except asyncio.QueueFull:
                pass

    async def run_benchmark(self, benchmark_id: str, brain_size: int = 1,
                            strategy: str = "auto", epochs: int = 10,
                            include_cognitive: bool = True,
                            progress_queue: Optional[asyncio.Queue] = None
                            ) -> BenchmarkSummary:
        """Run one or all benchmarks."""
        self._running = True
        self._should_stop = False
        self._progress_queue = progress_queue

        try:
            benchmarks = list(BENCHMARK_META.keys()) if benchmark_id == "all" else [benchmark_id]

            results = []
            for i, bid in enumerate(benchmarks):
                if self._should_stop:
                    break
                self._current_benchmark = bid
                self._notify({
                    "type": "benchmark_progress",
                    "benchmark_id": bid,
                    "progress": i / len(benchmarks),
                    "status": f"Running {bid}...",
                })

                try:
                    result = await self._run_single(
                        bid, brain_size, strategy, epochs, include_cognitive,
                    )
                    results.append(result)
                    _log.info("Benchmark %s: accuracy=%.1f%% time=%.1fs",
                              bid, result.accuracy * 100, result.train_time_seconds)
                except Exception as exc:
                    _log.error("Benchmark %s failed: %s", bid, exc, exc_info=True)
                    meta = BENCHMARK_META.get(bid, {})
                    results.append(BenchmarkResult(
                        benchmark_id=bid,
                        category=meta.get("category", "ml"),
                        strategy=strategy,
                        epochs=epochs,
                        brain_size=brain_size,
                        reference_scores=REFERENCE_SCORES.get(bid, {}),
                    ))

            # Compute summary
            ml_accs = [r.accuracy for r in results if r.category == "ml" and r.accuracy > 0]
            genai_accs = [r.accuracy for r in results
                          if r.category == "generative_ai" and r.accuracy > 0]

            cog_scores = []
            for r in results:
                if r.cognitive:
                    c = r.cognitive
                    if c.oscillation_coherence > 0:
                        cog_scores.append(c.oscillation_coherence)
                    if c.working_memory_capacity > 0:
                        cog_scores.append(min(c.working_memory_capacity / 7.0, 1.0))
                    if c.ethics_separation > 0:
                        cog_scores.append(min(c.ethics_separation / 2.0, 1.0))

            summary = BenchmarkSummary(
                results=results,
                overall_ml_accuracy=sum(ml_accs) / len(ml_accs) if ml_accs else 0.0,
                overall_genai_accuracy=sum(genai_accs) / len(genai_accs) if genai_accs else 0.0,
                cognitive_health_score=sum(cog_scores) / len(cog_scores) if cog_scores else 0.0,
                timestamp=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            )

            self._last_results = summary
            self._notify({"type": "benchmark_complete", "num_results": len(results)})
            return summary

        finally:
            self._running = False
            self._current_benchmark = None

    async def _run_single(self, benchmark_id: str, brain_size: int,
                          strategy: str, epochs: int,
                          include_cognitive: bool) -> BenchmarkResult:
        """Run a single benchmark."""
        meta = BENCHMARK_META.get(benchmark_id)
        if meta is None:
            raise ValueError(f"Unknown benchmark: {benchmark_id}")

        category = meta["category"]
        num_features = meta["num_features"]
        num_classes = meta["num_classes"]

        # Load examples
        examples = await asyncio.to_thread(self._load_examples, benchmark_id)
        if not examples:
            raise ValueError(f"No examples for benchmark: {benchmark_id}")

        # Split train/test
        rng = random.Random(42)
        shuffled = list(examples)
        rng.shuffle(shuffled)
        split = int(len(shuffled) * 0.7)
        train_examples = shuffled[:split]
        test_examples = shuffled[split:]

        # Determine actual strategy
        actual_strategy = strategy
        if strategy == "auto":
            n = len(train_examples)
            actual_strategy = "hebbian" if n < 100 else ("hybrid" if n < 500 else "gradient")

        # Build label mapping
        all_labels = sorted(set(str(ex.get("label", "0")) for ex in examples))
        label_to_idx = {label: i for i, label in enumerate(all_labels)}
        num_outputs = max(len(all_labels) * 2, len(all_labels) + 8, 10)

        # Create ephemeral brain
        brain_name = f"_bench_{benchmark_id}_{int(time.time())}"
        t0 = time.monotonic()

        brain = await asyncio.to_thread(
            nimcp.Brain,
            name=brain_name, size=brain_size, task=0,
            num_inputs=num_features, num_outputs=num_outputs,
        )

        # Configure training
        try:
            config = nimcp.TrainingConfig(
                loss_type=nimcp.LOSS_CROSS_ENTROPY,
                optimizer_type=nimcp.OPT_ADAM,
                scheduler_type=nimcp.SCHED_COSINE,
                learning_rate=0.01,
                enable_gradient_clipping=True,
                gradient_clip_value=1.0,
            )
            await asyncio.to_thread(brain.configure_training, config)
        except Exception as exc:
            _log.warning("configure_training failed for %s: %s", benchmark_id, exc)

        # Try to enable cognitive features
        try:
            await asyncio.to_thread(brain.enable_complex_oscillations, True)
        except Exception:
            pass

        # Train
        steps_to_70 = None
        total_correct = 0
        total_seen = 0
        epoch_losses = []

        for epoch in range(epochs):
            if self._should_stop:
                break
            epoch_examples = list(train_examples)
            rng.shuffle(epoch_examples)

            for ex in epoch_examples:
                if self._should_stop:
                    break
                features = ex.get("features", ex.get("input", []))
                label = str(ex.get("label", "0"))

                if actual_strategy in ("gradient", "hybrid"):
                    targets = [0.0] * num_outputs
                    idx = label_to_idx.get(label, 0)
                    if idx < len(targets):
                        targets[idx] = 1.0
                    try:
                        result = await asyncio.to_thread(brain.train_step, features, targets)
                        epoch_losses.append(float(result.loss))
                    except Exception:
                        pass

                if actual_strategy in ("hebbian", "hybrid"):
                    try:
                        await asyncio.to_thread(brain.learn, features, label, 1.0)
                    except Exception:
                        pass

                # Track training accuracy
                total_seen += 1
                try:
                    pred = await asyncio.to_thread(brain.predict, features)
                    if pred is not None:
                        pred_label = pred[0].split(" [")[0] if " [" in pred[0] else pred[0]
                        if pred_label == label:
                            total_correct += 1
                except Exception:
                    pass

                if steps_to_70 is None and total_seen >= 10:
                    acc = total_correct / total_seen
                    if acc >= 0.70:
                        steps_to_70 = total_seen

        train_time = time.monotonic() - t0

        # Evaluate on test set
        test_correct = 0
        test_total = 0
        per_class_correct: dict[str, int] = {}
        per_class_total: dict[str, int] = {}
        confusion: dict[str, dict[str, int]] = {}
        inference_times: list[float] = []

        for ex in test_examples:
            features = ex.get("features", ex.get("input", []))
            label = str(ex.get("label", "0"))
            per_class_total[label] = per_class_total.get(label, 0) + 1

            try:
                t_inf = time.monotonic()
                pred = await asyncio.to_thread(brain.predict, features)
                inf_us = (time.monotonic() - t_inf) * 1_000_000
                inference_times.append(inf_us)

                if pred is not None:
                    pred_label = pred[0].split(" [")[0] if " [" in pred[0] else pred[0]
                    test_total += 1
                    confusion.setdefault(label, {})
                    confusion[label][pred_label] = confusion[label].get(pred_label, 0) + 1
                    if pred_label == label:
                        test_correct += 1
                        per_class_correct[label] = per_class_correct.get(label, 0) + 1
            except Exception:
                test_total += 1

        test_accuracy = test_correct / test_total if test_total > 0 else 0.0
        avg_inference_us = sum(inference_times) / len(inference_times) if inference_times else 0.0

        per_class_accuracy = {}
        for label in per_class_total:
            c = per_class_correct.get(label, 0)
            t = per_class_total[label]
            per_class_accuracy[label] = c / t if t > 0 else 0.0

        # Get brain metrics
        neuron_count = 0
        utilization = 0.0
        try:
            neuron_count = await asyncio.to_thread(brain.get_neuron_count)
        except Exception:
            pass
        try:
            utilization, _ = await asyncio.to_thread(brain.get_utilization_metrics)
        except Exception:
            pass

        # Run cognitive benchmarks
        cognitive = None
        if include_cognitive:
            cognitive = await self._run_cognitive_benchmarks(brain)

        # Compute vs. best reference
        refs = REFERENCE_SCORES.get(benchmark_id, {})
        best_ref = max(refs.values()) if refs else 0.0
        vs_best = test_accuracy / best_ref if best_ref > 0 else 0.0

        # Destroy ephemeral brain
        del brain

        return BenchmarkResult(
            benchmark_id=benchmark_id,
            category=category,
            accuracy=round(test_accuracy, 4),
            loss=round(epoch_losses[-1] if epoch_losses else 0.0, 6),
            per_class_accuracy=per_class_accuracy,
            confusion=confusion if confusion else None,
            train_time_seconds=round(train_time, 2),
            inference_time_us=round(avg_inference_us, 2),
            memory_bytes=0,
            sparsity=round(utilization, 4),
            active_neuron_ratio=round(utilization, 4),
            steps_to_70_pct=steps_to_70,
            strategy=actual_strategy,
            epochs=epochs,
            brain_size=brain_size,
            cognitive=cognitive,
            reference_scores=refs,
            nimcp_vs_best=round(vs_best, 4),
        )

    def _load_examples(self, benchmark_id: str) -> list[dict]:
        """Load examples for a benchmark (runs in thread)."""
        if benchmark_id in BENCHMARK_DATASETS:
            return BENCHMARK_DATASETS[benchmark_id]().get_examples()
        if benchmark_id in GENAI_DATASETS:
            return GENAI_DATASETS[benchmark_id]().get_examples()

        # Fall back to dataset_manager for built-in datasets (iris, mnist, etc.)
        try:
            import dataset_manager
            examples = dataset_manager.get_examples(benchmark_id)
            if examples:
                return examples
        except Exception as exc:
            _log.warning("dataset_manager.get_examples(%s) failed: %s", benchmark_id, exc)
        return []

    async def _run_cognitive_benchmarks(self, brain) -> CognitiveMetrics:
        """Run cognitive benchmarks on a trained brain."""
        metrics = CognitiveMetrics()

        # Working memory
        try:
            wm_stats = await asyncio.to_thread(brain.working_memory_stats)
            if isinstance(wm_stats, dict):
                metrics.working_memory_capacity = wm_stats.get("capacity", 0)
                metrics.working_memory_occupancy = wm_stats.get("occupancy", 0.0)
            elif isinstance(wm_stats, (tuple, list)) and len(wm_stats) >= 2:
                metrics.working_memory_capacity = int(wm_stats[0])
                metrics.working_memory_occupancy = float(wm_stats[1])
        except Exception:
            pass

        # Oscillation coherence
        try:
            coherence = await asyncio.to_thread(brain.get_phase_coherence)
            metrics.oscillation_coherence = float(coherence) if coherence is not None else 0.0
        except Exception:
            pass

        try:
            pac = await asyncio.to_thread(brain.get_pac_modulation)
            metrics.pac_index = float(pac) if pac is not None else 0.0
        except Exception:
            pass

        # Global workspace
        try:
            ws_stats = await asyncio.to_thread(brain.workspace_stats)
            if isinstance(ws_stats, dict):
                metrics.workspace_broadcasts = ws_stats.get("broadcast_count", 0)
                metrics.workspace_avg_strength = ws_stats.get("avg_strength", 0.0)
        except Exception:
            pass

        # Ethics evaluation via prediction on ethics scenarios
        try:
            scenarios = EthicsScenarios.get_scenarios()
            harmful_scores = []
            beneficial_scores = []

            for s in scenarios:
                try:
                    result = await asyncio.to_thread(brain.predict, s["features"])
                    if result is not None:
                        confidence = float(result[1])
                        if s["category"] == "harmful":
                            harmful_scores.append(confidence)
                        elif s["category"] == "beneficial":
                            beneficial_scores.append(confidence)
                except Exception:
                    pass

            if harmful_scores:
                metrics.ethics_harmful_score = round(
                    sum(harmful_scores) / len(harmful_scores), 4)
            if beneficial_scores:
                metrics.ethics_beneficial_score = round(
                    sum(beneficial_scores) / len(beneficial_scores), 4)
            metrics.ethics_separation = round(
                metrics.ethics_beneficial_score - metrics.ethics_harmful_score, 4)
        except Exception:
            pass

        # Knowledge system
        try:
            ks = nimcp.KnowledgeSystem(brain)
            stats = await asyncio.to_thread(ks.assess_domain, "general")
            if isinstance(stats, dict):
                metrics.knowledge_concepts = stats.get("concepts", 0)
                metrics.knowledge_coverage = stats.get("coverage", 0.0)
        except Exception:
            pass

        return metrics


# Singleton runner
runner = BenchmarkRunner()
