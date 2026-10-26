"""V2BrainAdapter — Phase 7c shim mapping V1 harness brain.* calls to V2.

The V1 training harness (`scripts/immerse_athena.py`) calls 77 distinct
`brain.*` methods against the V1 `nimcp` Python module. V2's Rust
`nimcp_v2.Brain` exposes a narrower surface. This adapter gives the V1
harness a drop-in replacement (`import v2_brain_adapter as nimcp`) by
classifying every call into one of three buckets:

1. **Direct pass-through (~14 methods)** — map 1-to-1 onto `nimcp_v2.Brain`
   primitives (`learn`, `predict`, `save_ensemble`, `stats`, etc.). These
   carry the real training signal.

2. **Safe no-op stubs (~56 methods)** — cognitive instrumentation that V2
   deliberately dropped in §5 of V2_PLAN (bg_*, medulla_*, sleep_*,
   octopus_*, utm_*, enable_*, ti_*, etc.). Each returns a sensible
   default and emits exactly one WARNING the first time it is called,
   so surprises in the harness surface once without spamming logs.

3. **Semantic-gap adapters (~7 methods)** — behavior V2 has no equivalent
   for (train_cognitive, speak, ...). These either reduce to the closest
   V2 primitive (`train_cognitive` calls `learn` and packages the loss
   into the dict the harness expects) or return empty/zero placeholders.
   Since V2 L8/C4/C5, the language methods (`train_language`,
   `learn_language`, `generate_text`, `grounded_respond`, `comprehend`,
   `reason`, `working_memory_add`) are REAL when the brain was built with
   `language` in its `config_json`; on a language-less brain they fall
   back to the stubs.

Construction mirrors `nimcp_v2.Brain.__init__` with one extra knob:
passing `config_json=...` routes through `Brain.from_json` instead.
"""

from __future__ import annotations

import logging
import os
from typing import Any, Iterable

import nimcp_v2

_LOG = logging.getLogger("v2_brain_adapter")
_WARNED: set[str] = set()


def _warn_once(method_name: str) -> None:
    """Emit one WARNING the first time this method name is seen."""
    if method_name in _WARNED:
        return
    _WARNED.add(method_name)
    _LOG.warning(
        "V2BrainAdapter: '%s' is a no-op stub (V2 has no equivalent).",
        method_name,
    )


# Table of safe no-op stubs: (method_name, default_value).
# At class-build time each entry is installed as a bound method that
# warns once then returns its default. Lists/dicts are returned as a
# fresh copy each call so callers can safely mutate them.
_NOOP_DEFAULTS: dict[str, Any] = {
    # Basal ganglia scalars + reward nudger
    "bg_get_conflict": 0.0,
    "bg_get_dopamine": 0.5,
    "bg_get_mode": "cortical",
    "bg_get_rpe": 0.0,
    "bg_update_reward": None,
    # Cerebellum / curiosity / edge / EDP
    "cerebellum_process_error": None,
    "curiosity_detect_gaps": [],
    "edge_score_importance": 0.0,
    "edp_process_novelty": None,
    "edp_process_reward": None,
    # enable_* feature flags (V2 has them wired by config, not runtime)
    "enable_biological_plasticity": None,
    "enable_gradient_checkpointing": None,
    "enable_mixed_precision": None,
    "enable_multi_network": None,
    "enable_world_model": None,
    "enable_world_model_bridge": None,
    # Attention / cortex / LNN compat stubs
    "focus_attention": None,
    "init_cortex_cnns": None,
    "lnn_create": None,
    "lnn_forward_step": None,
    "lnn_get_state": None,
    # Medulla
    "medulla_boost_arousal": None,
    "medulla_get_arousal": 0.5,
    "medulla_get_circadian_efficiency": 1.0,
    "medulla_reduce_arousal": None,
    # Octopus peripheral explorers
    "octopus_explore_from_audio_cortex": [],
    "octopus_explore_from_occipital": [],
    "octopus_explore_from_somatosensory": [],
    # Parietal hypothesis simulator
    "parietal_simulate_hypothesis": {"confidence": 0.0},
    # Maintenance
    "prune_synapses": 0,
    "repair_nan_weights": 0,
    # Sensor / motor / embodiment
    "sensor_hub_create": None,
    "set_fast_training": None,
    "set_network_ablation": None,
    "set_plasticity_state": None,
    "set_task_type": None,
    # Sleep
    "sleep_get_pressure": 0.0,
    "sleep_get_state": "awake",
    "sleep_is_needed": False,
    "sleep_run_cycle": None,
    # Sensory submission
    "submit_sensory": None,
    "submit_sensory_batch": None,
    # Substrate
    "substrate_get_health": {"status": "ok"},
    "substrate_get_metabolic": 1.0,
    # Theory / inference rules
    "ti_add_fact": None,
    "ti_add_rule": None,
    "ti_forward_chain": [],
    "ti_init_reasoning": None,
    # Medulla update + UTM training-health gauges
    "update_medulla": None,
    "utm_get_training_health": {"status": "ok", "loss_ema": None},
    "utm_set_per_network_lr": None,
    "utm_swap_from_ema": None,
    "utm_swap_to_ema": None,
    # Visual cortex + watchdog
    "visual_cortex_process": None,
    "watchdog_create": None,
}


def _make_noop(name: str, default: Any):
    """Build a bound-method-shaped stub that warns once and returns default.

    Lists and dicts are copied per-call so the caller can safely mutate
    without poisoning the shared default.
    """
    if isinstance(default, (list, dict)):
        def _stub(self, *args: Any, _n: str = name, _d: Any = default, **kwargs: Any) -> Any:
            _warn_once(_n)
            return type(_d)(_d)
        return _stub

    def _stub(self, *args: Any, _n: str = name, _d: Any = default, **kwargs: Any) -> Any:
        _warn_once(_n)
        return _d
    return _stub


class V2BrainAdapter:
    """77-method compatibility shim over `nimcp_v2.Brain`.

    See module docstring for the direct / no-op / semantic-gap split.
    """

    # -----------------------------------------------------------------
    # Construction
    # -----------------------------------------------------------------
    def __init__(
        self,
        rng_seed: int = 0x5EED,
        deterministic: bool = False,
        layers: list[int] | None = None,
        activation: str = "relu",
        config_json: str | None = None,
    ) -> None:
        if config_json is not None:
            self._brain = nimcp_v2.Brain.from_json(config_json)
        else:
            self._brain = nimcp_v2.Brain(
                rng_seed=rng_seed,
                deterministic=deterministic,
                layers=layers,
                activation=activation,
            )

    # -----------------------------------------------------------------
    # (1) Direct pass-through
    # -----------------------------------------------------------------
    def learn_vector(
        self,
        features: list[float],
        target: list[float],
        lr: float = 0.01,
        label: Any = None,
        confidence: float = 1.0,
        task: Any = None,
    ) -> float:
        return self._brain.learn(list(features), list(target), lr)

    def learn_vector_batch(
        self,
        pairs: Iterable[tuple[list[float], list[float]]],
        lr: float = 0.01,
    ) -> float:
        last = 0.0
        for pair in pairs:
            features, target = pair[0], pair[1]
            last = self._brain.learn(list(features), list(target), lr)
        return last

    def predict(self, features: list[float]) -> list[float]:
        return self._brain.predict(list(features))

    def probe(self, features: list[float]) -> list[float]:
        return self._brain.predict(list(features))

    def decide_full(self, features: list[float]) -> dict[str, Any]:
        out = self._brain.predict(list(features))
        return {"output": out, "ethics": None, "bg": None, "lnn_readout": None}

    def save(self, path: str) -> None:
        # V1's save() took a single path. V2 splits per-subsystem checkpoints
        # into a directory (save_ensemble). Prefer save_ensemble when the
        # caller gave us a directory-like path; fall back to rkyv-only save.
        p = os.fspath(path)
        looks_like_dir = not os.path.splitext(p)[1] or os.path.isdir(p)
        if looks_like_dir:
            try:
                self._brain.save_ensemble(p)
                return
            except Exception as e:  # pragma: no cover — fallback path
                _LOG.warning("save_ensemble(%s) failed, falling back: %s", p, e)
        self._brain.save(p)

    def get_neuron_count(self) -> int:
        adaptive = (self._brain.stats() or {}).get("adaptive") or {}
        return int(sum(adaptive.get("layer_widths") or []))

    def get_stats(self) -> dict[str, Any]:
        return self._brain.stats()

    def snn_get_stats(self) -> Any:
        return (self._brain.stats() or {}).get("snn")

    def lnn_get_stats(self) -> Any:
        return (self._brain.stats() or {}).get("lnn")

    def cnn_get_stats(self) -> Any:
        _warn_once("cnn_get_stats")
        return {}

    def get_network_metrics(self) -> dict[str, Any]:
        s = self._brain.stats() or {}
        return {k: s.get(k) for k in ("adaptive", "snn", "lnn", "memory", "loss")}

    def get_cortex_cnn_metrics(self) -> dict[str, Any]:
        _warn_once("get_cortex_cnn_metrics")
        return {}

    def get_plasticity_stats(self) -> dict[str, Any]:
        return {"stdp_updates": 0, "bcm_updates": 0, "mean_weight_change": 0.0}

    def get_transcript(self) -> str:
        return ""  # V2 has no language/TTS layer.

    def consolidate(self, dt_seconds: float = 1.0) -> Any:
        try:
            return self._brain.memory_consolidate(float(dt_seconds))
        except Exception:
            _warn_once("consolidate")
            return None

    # -----------------------------------------------------------------
    # (3) Semantic-gap adapters
    # -----------------------------------------------------------------
    def train_cognitive(
        self,
        features: list[float],
        target: list[float],
        *args: Any,
        **kwargs: Any,
    ) -> dict[str, Any]:
        lr = float(kwargs.get("lr", 0.01))
        loss = self._brain.learn(list(features), list(target), lr)
        return {
            "adaptive_loss": loss,
            "snn_loss": None,
            "lnn_loss": None,
            "cognitive_loss": loss,
        }

    # Language (V2 L8 + C4/C5). Real when the brain was built with
    # `language` in its config JSON; otherwise the pre-L8 no-op stubs so a
    # language-less brain keeps the harness alive.
    def _has_language(self) -> bool:
        return bool(getattr(self._brain, "has_language", False))

    def train_language(self, text: str, target_text: str | None = None) -> dict[str, float]:
        if not self._has_language():
            _warn_once("train_language")
            return {"loss": 0.0}
        self._brain.language_learn(str(text))
        if target_text:
            self._brain.language_learn(str(target_text))
        # V2 has no supervised language loss; report comprehension
        # uncertainty (1 - confidence) of the text just learned as the
        # closest monotone proxy so harness EMAs still move.
        _, conf = self._brain.language_comprehend(str(text))
        return {"loss": float(max(0.0, 1.0 - conf))}

    def train_batch_text(self, *args: Any, **kwargs: Any) -> dict[str, float]:
        _warn_once("train_batch_text")
        return {"loss": 0.0}

    def learn_language(self, text: str) -> None:
        if not self._has_language():
            _warn_once("learn_language")
            return None
        self._brain.language_learn(str(text))
        return None

    def speak(self, text: str = "") -> str:
        _warn_once("speak")
        return ""

    def generate_text(self, prompt: str = "", max_tokens: int = 0) -> str:
        if not self._has_language():
            _warn_once("generate_text")
            return ""
        text = self._brain.language_respond(str(prompt))["text"]
        if max_tokens and max_tokens > 0:
            text = " ".join(text.split()[:max_tokens])
        return text

    def grounded_respond(self, query: str) -> dict[str, Any]:
        if not self._has_language():
            _warn_once("grounded_respond")
            return {"response": "", "confidence": 0.0}
        r = self._brain.language_respond(str(query))
        return {
            "response": r["text"],
            "confidence": float(r["confidence"]),
            "blocked_for_toxicity": bool(r["blocked_for_toxicity"]),
            "toxicity_category": r["toxicity_category"],
        }

    def comprehend(self, text: str) -> dict[str, Any]:
        if not self._has_language():
            _warn_once("comprehend")
            return {"vector": [], "confidence": 0.0}
        vec, conf = self._brain.language_comprehend(str(text))
        return {"vector": list(vec), "confidence": float(conf)}

    def reason(self, query: str) -> dict[str, Any]:
        if not self._has_language():
            _warn_once("reason")
            return {}
        return self._brain.reason(str(query))

    def set_reason_in_content(self, on: bool) -> None:
        if not self._has_language():
            _warn_once("set_reason_in_content")
            return None
        self._brain.set_reason_in_content(bool(on))
        return None

    def working_memory_add(self, data: list[float], salience: float = 0.5) -> None:
        if not self._has_language():
            _warn_once("working_memory_add")
            return None
        self._brain.working_memory_add([float(x) for x in data], float(salience))
        return None

    # -----------------------------------------------------------------
    # Forgiving fallback — unknown methods should not crash the harness.
    # -----------------------------------------------------------------
    def __getattr__(self, name: str) -> Any:
        if name.startswith("_"):
            raise AttributeError(name)

        def _stub(*args: Any, **kwargs: Any) -> None:
            _warn_once(name)
            return None

        return _stub


# Install the 56 safe no-op stubs onto the class body.
for _name, _default in _NOOP_DEFAULTS.items():
    setattr(V2BrainAdapter, _name, _make_noop(_name, _default))


# Module-level alias so `import v2_brain_adapter as nimcp` + `nimcp.Brain(...)`
# mirrors V1's module shape.
Brain = V2BrainAdapter
