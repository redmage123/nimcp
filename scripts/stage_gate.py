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
    1. Minimum step count (the stage's full example budget — more is better)
    2. (only with use_loss_criteria) Mean loss below threshold
    3. (only with use_loss_criteria) Loss plateau
    4. Per-item non-zero ratio high (few collapsed responses)
    5. SNN biologically stable (sparsity 0.88-0.99 ≈ 1-12% firing)
    6. Chat eval passes (coherence + differentiation)

Criteria are intentionally conjunctive: if ANY fails, the stage does NOT advance.
Loss is deliberately NOT a stage-ending criterion by default (2026-10-27):
Athena should receive as many training examples as possible, as a human
would; the gate only verifies the brain is healthy before moving on.
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
    # Loss is NOT a stage-ending criterion (user directive 2026-10-27:
    # "athena should get as many training examples as possible just as a
    # human would"). A stage runs through at least min_step examples and
    # ends when the HEALTH criteria below pass; loss convergence/plateau is
    # only consulted when use_loss_criteria is set explicitly.
    use_loss_criteria: bool = False
    max_mean_loss: float = 1.0            # per-item loss target (only if use_loss_criteria)
    plateau_window: int = 200             # two 200-step windows
    max_plateau_delta: float = 0.05       # 5% change between windows = plateaued
    min_nz_of_50: int = 48                # ≥48 non-zero in last 50 steps
    snn_sparsity_min: float = 0.88        # legacy, unused (see criterion 5)
    snn_sparsity_max: float = 0.99        # legacy, unused
    snn_rate_min_hz: float = 0.5          # population mean firing rate band
    snn_rate_max_hz: float = 20.0
    snn_max_hyperactive: int = 1000       # neurons >200 Hz (pathological)
    chat_eval_interval: int = 500         # run chat_eval at most this often
    chat_coherence_min: float = 0.30      # input↔output alignment
    chat_similarity_min: float = 0.20     # decode quality
    chat_cross_sim_max: float = 0.90      # 1-diversity; want differentiation

    # Where chat_eval writes its JSONL summary. immerse_athena.py writes
    # checkpoints/athena/chat_eval_log.jsonl (relative to its cwd); the
    # fixed candidates in _find_chat_log never matched it, so the chat
    # criterion could not pass. Callers set this explicitly.
    chat_log_path: str | None = None

    # Internal state
    last_chat_step: int = -1
    last_chat_result: dict | None = None
    cumulative_passes: int = 0            # require N consecutive passes before transitioning
    required_consecutive_passes: int = 3

    def check(self, step: int, losses: list[float],
              brain: Any, composer: Any = None, decoder: Any = None,
              chat_eval_fn: Callable | None = None) -> GateResult:
        """Evaluate all criteria. Passes must be CONSECUTIVE: any failed
        criterion resets the pass counter (the original implementation
        returned early without resetting, so 3 passes spread over hours
        of failures would have counted)."""
        result = self._check_once(step, losses, brain, composer, decoder, chat_eval_fn)
        if not result.passed and not result.reason.startswith("accumulating_passes"):
            self.cumulative_passes = 0
        return result

    def _check_once(self, step: int, losses: list[float],
                    brain: Any, composer: Any = None, decoder: Any = None,
                    chat_eval_fn: Callable | None = None) -> GateResult:
        details = {"step": step}

        # 1. Minimum steps
        if step < self.min_step:
            return GateResult(False, f"min_step_not_reached ({step}<{self.min_step})", details)

        # 2-3. Loss level / plateau — informational unless use_loss_criteria.
        if len(losses) >= 2 * self.plateau_window:
            recent = losses[-self.plateau_window:]
            prior = losses[-2 * self.plateau_window:-self.plateau_window]
            nonzero_recent = [x for x in recent if x > 0.001]
            nonzero_prior = [x for x in prior if x > 0.001]
            if nonzero_recent and nonzero_prior:
                mean_recent = sum(nonzero_recent) / len(nonzero_recent)
                mean_prior = sum(nonzero_prior) / len(nonzero_prior)
                details["mean_recent"] = mean_recent
                details["mean_prior"] = mean_prior
                plateau_delta = abs(mean_recent - mean_prior) / (abs(mean_prior) + 1e-6)
                details["plateau_delta"] = plateau_delta
                if self.use_loss_criteria:
                    if mean_recent > self.max_mean_loss:
                        return GateResult(False,
                                          f"mean_loss_too_high ({mean_recent:.4f} > {self.max_mean_loss})",
                                          details)
                    if plateau_delta > self.max_plateau_delta:
                        return GateResult(False,
                                          f"not_plateaued (delta={plateau_delta:.3f} > {self.max_plateau_delta})",
                                          details)
            elif self.use_loss_criteria:
                return GateResult(False, "all_zero_losses_in_window", details)
        elif self.use_loss_criteria:
            return GateResult(False,
                              f"insufficient_loss_history ({len(losses)} < {2*self.plateau_window})",
                              details)

        # 4. Non-zero count in last 50 (most recent mini-window) — output
        # health (collapsed/empty responses), not a loss target.
        if len(losses) < 50:
            return GateResult(False, f"insufficient_history ({len(losses)} < 50)", details)
        last_50 = losses[-50:]
        nz_count = sum(1 for x in last_50 if x > 0.001)
        details["nz_count"] = nz_count
        if nz_count < self.min_nz_of_50:
            return GateResult(False,
                              f"too_many_zeros ({nz_count}/50 < {self.min_nz_of_50})",
                              details)

        # 5. SNN biological range. The network's `sparsity` statistic is the
        # fraction of neurons that fired ZERO times over the stats window
        # (~100 steps, nimcp_snn_network.c), not a per-step silent fraction:
        # at ~2 Hz mean rate most neurons fire at least once per window, so
        # it reads 0.0-0.4 on a healthy network and the old 0.88-0.99 band
        # could never pass. Gate on the population mean firing rate instead
        # (cortical neurons: ~0.5-20 Hz) and on hyperactive neurons.
        try:
            ss = brain.snn_get_stats() if hasattr(brain, "snn_get_stats") else None
        except Exception as e:
            return GateResult(False, f"snn_stats_failed:{e}", details)
        if not ss:
            return GateResult(False, "snn_stats_empty", details)
        rate_hz = float(ss.get("mean_firing_rate", ss.get("firing_rate", 0.0)) or 0.0)
        details["snn_mean_rate_hz"] = rate_hz
        details["snn_window_silent_fraction"] = float(ss.get("sparsity", 0.0) or 0.0)
        if not (self.snn_rate_min_hz <= rate_hz <= self.snn_rate_max_hz):
            return GateResult(False,
                              f"snn_rate_out_of_range ({rate_hz:.2f} Hz not in "
                              f"[{self.snn_rate_min_hz},{self.snn_rate_max_hz}])",
                              details)
        hyper = ss.get("hyperactive_neurons")
        if hyper is not None:
            details["snn_hyperactive"] = int(hyper)
            if int(hyper) > self.snn_max_hyperactive:
                return GateResult(False,
                                  f"snn_hyperactive ({int(hyper)} > {self.snn_max_hyperactive})",
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
        if self.chat_log_path and os.path.exists(self.chat_log_path):
            return self.chat_log_path
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
    if stage == 0:
        return StageGate(stage=0,
                         min_step=500,
                         max_mean_loss=1.0,          # informational only (use_loss_criteria=False)
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
    elif stage >= 4:
        # Stages 4-7 (docs/claude/curriculum-stages-4-7.md): health criteria
        # only, chat thresholds rising with the stage; stage-specific
        # capability probes are applied by stages_4_7._run_stage on top.
        coh = {4: 0.35, 5: 0.40, 6: 0.45}.get(stage, 0.50)
        sim = {4: 0.25, 5: 0.30, 6: 0.35}.get(stage, 0.40)
        xs = {4: 0.85, 5: 0.80, 6: 0.75}.get(stage, 0.70)
        return StageGate(stage=stage, min_step=1000,
                         chat_eval_interval=500,
                         chat_coherence_min=coh, chat_similarity_min=sim,
                         chat_cross_sim_max=xs,
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
