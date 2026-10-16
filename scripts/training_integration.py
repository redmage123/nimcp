"""Integration layer for Phase A-E modules in immerse_athena.py.

Keeps the surface-level changes to immerse_athena.py minimal: the main
training script calls a few helper functions from here, and all the
module wiring lives in one place.

Usage pattern (to be added to immerse_athena.py):

    from training_integration import TrainingIntegration

    # At brain creation time:
    integration = TrainingIntegration(brain)
    integration.apply_innate_priors()

    # Wrap the stimulus source:
    source = integration.wrap_source(source)

    # At the top of each stage:
    integration.begin_stage(stage_number)

    # After each learn_vector:
    integration.after_learn_vector(label, description, modality, loss,
                                    baseline_loss=mean_recent_loss)

    # Periodically (every N steps):
    integration.periodic_consolidate()
"""
from __future__ import annotations

import logging
from typing import Any, Optional

log = logging.getLogger("training_integration")


class TrainingIntegration:
    """Bundle wiring for curiosity + curriculum + symbolic writer +
    innate priors + running implanter."""

    def __init__(self, brain, *,
                 enable_curiosity: bool = True,
                 enable_curriculum: bool = True,
                 enable_symbolic_writer: bool = True,
                 enable_running_implanter: bool = True,
                 enable_innate_priors: bool = True,
                 consolidation_interval_steps: int = 200,
                 consolidation_top_k: int = 20):
        self.brain = brain
        self.flags = {
            "curiosity": enable_curiosity,
            "curriculum": enable_curriculum,
            "symbolic_writer": enable_symbolic_writer,
            "running_implanter": enable_running_implanter,
            "innate_priors": enable_innate_priors,
        }
        self.consolidation_interval = consolidation_interval_steps
        self.consolidation_top_k = consolidation_top_k

        # Lazily initialized components
        self._symbolic_writer = None
        self._running_implanter = None
        self._curiosity_selector = None
        self._curriculum = None
        self._step_counter = 0
        self._current_stage = 0

    # ---- Init-time ----

    def apply_innate_priors(self) -> dict:
        """Apply Gabor filters, place cells, face template etc. at init time."""
        if not self.flags["innate_priors"]:
            return {"applied": [], "skipped_flag": True}
        try:
            from innate_priors import apply_all_priors
            summary = apply_all_priors(self.brain)
            log.info("Innate priors: generated=%s applied=%s",
                     summary["generated"], summary["applied"])
            return summary
        except Exception as e:
            log.warning("apply_innate_priors failed: %s", e)
            return {"applied": [], "error": str(e)}

    # ---- Source wrapping ----

    def wrap_source(self, source):
        """Wrap a stimulus source with curiosity + curriculum.

        Order: source → CuriositySelector → ProgressiveCurriculum.
        Each is opt-in per flag.
        """
        wrapped = source
        if self.flags["curriculum"]:
            try:
                from curriculum import ProgressiveCurriculum
                wrapped = ProgressiveCurriculum(wrapped)
                self._curriculum = wrapped
                log.info("Source wrapped with ProgressiveCurriculum")
            except Exception as e:
                log.warning("ProgressiveCurriculum wrap failed: %s", e)

        if self.flags["curiosity"]:
            try:
                from curiosity_selector import CuriositySelector
                # CuriositySelector wraps whatever's current (source or curriculum)
                self._curiosity_selector = CuriositySelector(
                    self.brain, wrapped, bias=0.35)
                wrapped = self._curiosity_selector
                log.info("Source wrapped with CuriositySelector (bias=0.35)")
            except Exception as e:
                log.warning("CuriositySelector wrap failed: %s", e)

        return wrapped

    # ---- Stage transitions ----

    def begin_stage(self, stage: int) -> None:
        """Called at the start of each training stage."""
        self._current_stage = stage
        self._step_counter = 0
        if self._running_implanter is not None:
            self._running_implanter.set_stage(stage)
        if self._curriculum is not None:
            self._curriculum.advance(step=0)
        # Propagate to the C brain's developmental gate (Slice E stage
        # table). The cascade's stage_motor truncates produced utterances
        # to `kStageTable[brain.current_stage].max_produce_words`, so
        # without this call the brain stays at stage 0 (1-word cap) for
        # the entire training run regardless of which run_stage_N the
        # trainer is in. Older nimcp.so builds may not export
        # set_active_stage; AttributeError is tolerated (logged once).
        try:
            self.brain.set_active_stage(stage)
        except AttributeError:
            if not getattr(self, "_warned_set_active_stage", False):
                log.warning("brain.set_active_stage not available — "
                            "developmental stage gate frozen at default. "
                            "Rebuild nimcp.so to enable.")
                self._warned_set_active_stage = True
        except Exception as e:
            log.warning("set_active_stage(%d) failed: %s", stage, e)
        log.info("begin_stage(%d)", stage)

    # ---- Per-step hooks ----

    def before_learn_vector(self) -> None:
        """Called before brain.learn_vector — attention boost, etc."""
        # Joint-attention boost (idempotent; safe to call redundantly)
        try:
            if hasattr(self.brain, "thalamus_set_attention"):
                self.brain.thalamus_set_attention("LGN", 1.3)
                self.brain.thalamus_set_attention("MGN", 1.15)
        except Exception:
            pass

    def after_learn_vector(self, *, label: str, description: str = "",
                            modality: str = "text",
                            loss: float = 0.0,
                            baseline_loss: Optional[float] = None,
                            reward: Optional[float] = None) -> None:
        """Called after brain.learn_vector with the training event data.

        Runs:
          - Attention reset
          - Symbolic writer (per-step KG/episode write)
          - Running implanter observe (buffered)
          - Curriculum step advance
        """
        self._step_counter += 1

        # Reset attention
        try:
            if hasattr(self.brain, "thalamus_set_attention"):
                self.brain.thalamus_set_attention("LGN", 1.0)
                self.brain.thalamus_set_attention("MGN", 1.0)
        except Exception:
            pass

        # Symbolic writer — immediate per-step symbolic write
        if self.flags["symbolic_writer"]:
            try:
                if self._symbolic_writer is None:
                    from symbolic_writer import SymbolicWriter
                    self._symbolic_writer = SymbolicWriter(self.brain)
                self._symbolic_writer.record(
                    label=label, modality=modality,
                    context={"description": description,
                             "loss": loss})
            except Exception as e:
                log.debug("SymbolicWriter.record failed: %s", e)

        # Running implanter — buffers for later consolidation
        if self.flags["running_implanter"]:
            try:
                if self._running_implanter is None:
                    from childhood_memories import RunningImplanter
                    self._running_implanter = RunningImplanter(self.brain)
                    self._running_implanter.set_stage(self._current_stage)
                self._running_implanter.observe(
                    label=label, description=description,
                    modality=modality, loss=loss,
                    baseline_loss=baseline_loss, reward=reward)
            except Exception as e:
                log.debug("RunningImplanter.observe failed: %s", e)

        # Curriculum step advance (so categories expand with training)
        if self._curriculum is not None:
            try:
                # Total step across stages
                self._curriculum.advance()
            except Exception:
                pass

        # Periodic consolidation
        if self.flags["running_implanter"] and self._step_counter % self.consolidation_interval == 0:
            self.periodic_consolidate()

    def periodic_consolidate(self) -> int:
        """Run memory consolidation. Safe to call any time."""
        if self._running_implanter is None:
            return 0
        try:
            n = self._running_implanter.consolidate_batch(
                top_k=self.consolidation_top_k)
            if n > 0:
                log.info("[consolidation] step %d: %d memories consolidated "
                         "(stats: %s)",
                         self._step_counter, n, self._running_implanter.stats())
            return n
        except Exception as e:
            log.debug("periodic_consolidate failed: %s", e)
            return 0

    # ---- Introspection ----

    def stats(self) -> dict:
        return {
            "flags": self.flags,
            "step_counter": self._step_counter,
            "current_stage": self._current_stage,
            "curiosity": (self._curiosity_selector.stats()
                           if self._curiosity_selector else None),
            "curriculum": (self._curriculum.stats()
                             if self._curriculum else None),
            "symbolic_writer": (self._symbolic_writer.stats()
                                  if self._symbolic_writer else None),
            "running_implanter": (self._running_implanter.stats()
                                    if self._running_implanter else None),
        }
