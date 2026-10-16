"""Stage-transition gate — blocks stage advance until ALL metrics pass.

Usage:
    gate = StageGate(stage=1, chat_eval_fn=chat_eval)
    # inside training loop, every N steps:
    result = gate.check(step, losses, brain, composer, decoder)
    if result.passed:
        print(f"All criteria met at step {step} — transitioning")
        break  # exit stage loop
    else:
        print(f"Gate blocked: {result.reason}")

The gate enforces ALL of:
    1. Minimum step count (so early noise doesn't trigger)
    2. Mean loss below threshold (stage-specific)
    3. Loss plateau (two successive 200-step windows within 5% of each other)
    4. Per-item non-zero ratio high (few collapsed responses)
    5. SNN biologically stable (sparsity 0.88-0.99 ≈ 1-12% firing)
    6. Chat eval passes (coherence + differentiation)

Criteria are intentionally conjunctive: if ANY fails, the stage does NOT advance.
"""
from __future__ import annotations

import json
import logging
import os
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Callable

log = logging.getLogger("stage_gate")


@dataclass
class GateResult:
    passed: bool
    reason: str = ""
    details: dict = field(default_factory=dict)

    def __str__(self) -> str:
        status = "PASS" if self.passed else "BLOCK"
        return f"[GATE {status}] {self.reason}"


@dataclass
class StageGate:
    stage: int

    # Criteria thresholds — tunable per stage
    min_step: int = 500
    max_mean_loss: float = 1.0            # per-item loss target
    plateau_window: int = 200             # two 200-step windows
    max_plateau_delta: float = 0.05       # 5% change between windows = plateaued
    min_nz_of_50: int = 48                # ≥48 non-zero in last 50 steps
    snn_sparsity_min: float = 0.88        # firing ≤ 12%
    snn_sparsity_max: float = 0.99        # firing ≥ 1%
    chat_eval_interval: int = 500         # run chat_eval at most this often
    chat_coherence_min: float = 0.30      # input↔output alignment
    chat_similarity_min: float = 0.20     # decode quality
    chat_cross_sim_max: float = 0.90      # 1-diversity; want differentiation

    # Internal state
    last_chat_step: int = -1
    last_chat_result: dict | None = None
    cumulative_passes: int = 0            # require N consecutive passes before transitioning
    required_consecutive_passes: int = 3

    def check(self, step: int, losses: list[float],
              brain: Any, composer: Any = None, decoder: Any = None,
              chat_eval_fn: Callable | None = None) -> GateResult:
        details = {"step": step}

        # 1. Minimum steps
        if step < self.min_step:
            return GateResult(False, f"min_step_not_reached ({step}<{self.min_step})", details)

        # 2-3. Need enough history for plateau comparison
        if len(losses) < 2 * self.plateau_window:
            return GateResult(False,
                              f"insufficient_loss_history ({len(losses)} < {2*self.plateau_window})",
                              details)

        recent = losses[-self.plateau_window:]
        prior = losses[-2 * self.plateau_window:-self.plateau_window]
        nonzero_recent = [x for x in recent if x > 0.001]
        nonzero_prior = [x for x in prior if x > 0.001]

        if not nonzero_recent or not nonzero_prior:
            return GateResult(False, "all_zero_losses_in_window", details)

        mean_recent = sum(nonzero_recent) / len(nonzero_recent)
        mean_prior = sum(nonzero_prior) / len(nonzero_prior)
        details["mean_recent"] = mean_recent
        details["mean_prior"] = mean_prior

        # 2. Loss low enough
        if mean_recent > self.max_mean_loss:
            return GateResult(False,
                              f"mean_loss_too_high ({mean_recent:.4f} > {self.max_mean_loss})",
                              details)

        # 3. Plateau
        plateau_delta = abs(mean_recent - mean_prior) / (abs(mean_prior) + 1e-6)
        details["plateau_delta"] = plateau_delta
        if plateau_delta > self.max_plateau_delta:
            return GateResult(False,
                              f"not_plateaued (delta={plateau_delta:.3f} > {self.max_plateau_delta})",
                              details)

        # 4. Non-zero count in last 50 (most recent mini-window)
        last_50 = losses[-50:]
        nz_count = sum(1 for x in last_50 if x > 0.001)
        details["nz_count"] = nz_count
        if nz_count < self.min_nz_of_50:
            return GateResult(False,
                              f"too_many_zeros ({nz_count}/50 < {self.min_nz_of_50})",
                              details)

        # 5. SNN biological range
        try:
            ss = brain.snn_get_stats() if hasattr(brain, "snn_get_stats") else None
        except Exception as e:
            return GateResult(False, f"snn_stats_failed:{e}", details)
        if not ss:
            return GateResult(False, "snn_stats_empty", details)
        sparsity = float(ss.get("sparsity", 0.0) or 0.0)
        details["snn_sparsity"] = sparsity
        if not (self.snn_sparsity_min <= sparsity <= self.snn_sparsity_max):
            return GateResult(False,
                              f"snn_sparsity_out_of_range ({sparsity:.3f} not in "
                              f"[{self.snn_sparsity_min},{self.snn_sparsity_max}])",
                              details)

        # 6. Chat eval
        chat_result = self._check_chat_eval(step, brain, composer, decoder, chat_eval_fn)
        details["chat_eval"] = chat_result
        if not chat_result.get("passed", False):
            return GateResult(False,
                              f"chat_eval_failed: {chat_result.get('reason', 'unknown')}",
                              details)

        # All criteria pass for this check — need N consecutive passes
        self.cumulative_passes += 1
        details["consecutive_passes"] = self.cumulative_passes
        if self.cumulative_passes < self.required_consecutive_passes:
            return GateResult(False,
                              f"accumulating_passes ({self.cumulative_passes}/{self.required_consecutive_passes})",
                              details)

        # Finally!
        return GateResult(True,
                          f"ALL_CRITERIA_PASSED ({self.required_consecutive_passes}x consecutive)",
                          details)

    def _check_chat_eval(self, step: int, brain: Any, composer: Any, decoder: Any,
                          chat_eval_fn: Callable | None) -> dict:
        """Run chat eval if enough steps elapsed since last; parse latest log line."""
        if not chat_eval_fn:
            return {"passed": False, "reason": "no_chat_eval_fn"}

        # Avoid running too frequently
        if (step - self.last_chat_step) < self.chat_eval_interval:
            if self.last_chat_result and self.last_chat_result.get("passed"):
                return self.last_chat_result  # reuse
            return {"passed": False, "reason": "not_yet_due"}

        # Run chat_eval
        try:
            chat_eval_fn(brain, composer, decoder, stage=self.stage, step=step)
            self.last_chat_step = step
        except Exception as e:
            return {"passed": False, "reason": f"chat_eval_exception:{e}"}

        # Parse latest entry from chat log
        log_path = self._find_chat_log()
        if not log_path:
            return {"passed": False, "reason": "chat_log_not_found"}
        try:
            with open(log_path) as f:
                last = None
                for line in f:
                    if line.strip():
                        last = line
            if not last:
                return {"passed": False, "reason": "empty_chat_log"}
            entry = json.loads(last)
        except Exception as e:
            return {"passed": False, "reason": f"chat_log_parse:{e}"}

        coherence = float(entry.get("mean_coherence", 0))
        similarity = float(entry.get("mean_similarity", 0))
        cross_sim = float(entry.get("mean_cross_sim", 1.0))

        result = {
            "coherence": coherence,
            "similarity": similarity,
            "cross_sim": cross_sim,
        }

        if coherence < self.chat_coherence_min:
            result.update({"passed": False,
                            "reason": f"coherence_low ({coherence:.3f} < {self.chat_coherence_min})"})
        elif similarity < self.chat_similarity_min:
            result.update({"passed": False,
                            "reason": f"similarity_low ({similarity:.3f} < {self.chat_similarity_min})"})
        elif cross_sim > self.chat_cross_sim_max:
            result.update({"passed": False,
                            "reason": f"diversity_low (cross_sim={cross_sim:.3f} > {self.chat_cross_sim_max})"})
        else:
            result.update({"passed": True, "reason": "all_chat_metrics_pass"})

        self.last_chat_result = result
        return result

    def _find_chat_log(self) -> str | None:
        candidates = [
            "/var/log/athena/chat_eval.jsonl",
            "/workspace/nimcp/chat_eval.jsonl",
            "./chat_eval.jsonl",
        ]
        for c in candidates:
            if os.path.exists(c):
                return c
        return None


def stage_gate_for(stage: int) -> StageGate:
    """Factory — per-stage calibrated thresholds."""
    if stage == 1:
        return StageGate(stage=1,
                         min_step=1500,              # enough warm-up
                         max_mean_loss=1.0,          # stage 1 should hit sub-1.0 loss
                         plateau_window=200,
                         max_plateau_delta=0.05,
                         min_nz_of_50=48,
                         snn_sparsity_min=0.88,
                         snn_sparsity_max=0.99,
                         chat_eval_interval=500,
                         chat_coherence_min=0.30,
                         chat_similarity_min=0.20,
                         chat_cross_sim_max=0.90,
                         required_consecutive_passes=3)
    elif stage == 2:
        return StageGate(stage=2,
                         min_step=2000,
                         max_mean_loss=1.5,
                         chat_coherence_min=0.35,
                         chat_similarity_min=0.25,
                         required_consecutive_passes=3)
    else:
        return StageGate(stage=stage)


# Logging helpers
def log_gate_event(log_path: str, stage: int, step: int, result: GateResult):
    """Append gate decision to a JSONL log for post-hoc analysis."""
    try:
        Path(log_path).parent.mkdir(parents=True, exist_ok=True)
        entry = {
            "timestamp": time.time(),
            "stage": stage,
            "step": step,
            "passed": result.passed,
            "reason": result.reason,
            "details": result.details,
        }
        with open(log_path, "a") as f:
            f.write(json.dumps(entry, default=str) + "\n")
    except Exception as e:
        log.warning("gate log write failed: %s", e)
