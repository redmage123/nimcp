"""Core test harness.

Wraps a brain client and provides unified probe / record / score
operations used by every test battery. Robust to missing RPCs:
callers check `.has_api(name)` and fall back to alternatives.
"""
from __future__ import annotations

import logging
import time
from typing import Any, Callable

from .trial import Trial
from .types import BatteryResult, StimulusItem, TestResult, TestScore

log = logging.getLogger("test_harness")


class TestHarness:
    """Adapter between a brain client and test batteries.

    The client must expose at minimum:
        - predict(features) → (label, confidence)   OR
        - learn_vector(features, target, ...)       OR
        - any callable exposed via _call(cmd, **kwargs)

    Optional APIs unlock deeper probes:
        - get_mental_health_report
        - get_emotion_state
        - get_internal_state
        - get_hypothesis_log
        - perturb_weights / inject_false_memory
        - predict_with_confidence / predict_with_deadline
        - enter_idle_with_telemetry
        - get_inner_speech_trace
        - snn_force_quench
    """

    OPTIONAL_APIS = [
        "get_mental_health_report",
        "get_mental_health_check",
        "get_emotion_state",
        "get_internal_state",
        "get_hypothesis_log",
        "get_inner_speech_trace",
        "get_inner_dialogue_history",
        "get_active_population",
        "predict_with_confidence",
        "predict_with_deadline",
        "perturb_weights",
        "revert_perturbation",
        "inject_false_memory",
        "enter_idle_with_telemetry",
        "cow_trial_snapshot",
        "cow_trial_restore",
        "get_dopamine_trajectory",
    ]

    def __init__(self, client, *, default_timeout_s: float = 20.0):
        self.client = client
        self.default_timeout_s = default_timeout_s
        self._api_cache: dict[str, bool] = {}

    # ---- Capability checks ----

    def has_api(self, name: str) -> bool:
        if name in self._api_cache:
            return self._api_cache[name]
        # Check if client has a matching method, else assume it passes through _call
        available = hasattr(self.client, name) or hasattr(self.client, f"_call")
        self._api_cache[name] = available
        return available

    def _safe_call(self, name: str, *args, default=None, **kwargs):
        """Call an RPC; return default on any error."""
        try:
            if hasattr(self.client, name):
                fn = getattr(self.client, name)
                return fn(*args, **kwargs)
            if hasattr(self.client, "_call"):
                return self.client._call(name, *args, **kwargs)
        except Exception as e:
            log.debug("RPC %s failed (%s); using default", name, e)
        return default

    # ---- Trial lifecycle ----

    def trial(self, *, isolate: bool = False, name: str = "") -> Trial:
        return Trial(self.client, isolate=isolate, name=name)

    # ---- Probes ----

    def probe_text(self, prompt: str, *,
                   want_confidence: bool = True,
                   deadline_ms: float | None = None,
                   capture_trace: bool = True) -> TestResult:
        """Present a text prompt and capture response + internal state."""
        t0 = time.time()

        # Encode prompt to features — fallback to hash-based deterministic embedding
        features = self._text_to_features(prompt)

        # Try confidence-aware predict; fall back to basic predict
        resp = None
        confidence = None
        if want_confidence and self.has_api("predict_with_confidence"):
            result = self._safe_call("predict_with_confidence", features)
            if result:
                if isinstance(result, dict):
                    resp = result.get("answer") or result.get("label")
                    confidence = result.get("confidence")
                elif isinstance(result, (list, tuple)) and len(result) >= 2:
                    resp, confidence = result[0], float(result[1])

        if resp is None and deadline_ms and self.has_api("predict_with_deadline"):
            r = self._safe_call("predict_with_deadline", features, int(deadline_ms))
            if r and isinstance(r, (list, tuple)):
                resp = r[0]
                confidence = r[1] if len(r) > 1 else None

        if resp is None:
            r = self._safe_call("predict", features)
            if r and isinstance(r, (list, tuple)):
                resp = r[0]
                confidence = r[1] if len(r) > 1 else None

        latency_ms = (time.time() - t0) * 1000.0

        internal_state = self._safe_call("get_internal_state", strategy=0, default={}) or {}
        emotion_state = self._safe_call("get_emotion_state", default={}) or {}
        trace = []
        if capture_trace:
            trace = self._safe_call("get_inner_speech_trace", n=10, default=[]) or []

        return TestResult(
            stimulus_id="",
            prompt=prompt,
            response=resp,
            internal_state=internal_state,
            reasoning_trace=trace,
            emotion_state=emotion_state,
            confidence=confidence,
            latency_ms=latency_ms,
        )

    def probe_stimulus(self, stim: StimulusItem, **kwargs) -> TestResult:
        result = self.probe_text(stim.prompt, **kwargs)
        result.stimulus_id = stim.id
        return result

    # ---- Text → feature encoding (fallback) ----

    def _text_to_features(self, text: str, dim: int = 1024) -> list[float]:
        """Encode text to a feature vector.

        Tries in order:
            1. Brain's native text-composer (if available via RPC)
            2. Semantic-hash encoding (word-level hashing with position)
            3. Character-level hash fallback

        The semantic-hash path is deterministic, reproducible, and better
        than pure MD5 — it preserves relative word similarity via shared
        prefix/stem hashing.
        """
        # Try the real composer path first
        try:
            composed = self._safe_call("compose_text", text=text, dim=dim, default=None)
            if composed and isinstance(composed, list) and len(composed) == dim:
                return composed
        except Exception:
            pass

        # Fallback: semantic-hash encoding
        return self._semantic_hash_encode(text or "", dim)

    def _semantic_hash_encode(self, text: str, dim: int = 1024) -> list[float]:
        """Semantic-hash text encoding — preserves word-level similarity.

        For each word:
            - Hash the 4-char prefix (stem) → shared bucket for similar words
            - Hash the word itself
            - Hash position-biased token

        Accumulates activation into the feature vector via trig mixing.
        """
        import hashlib
        import math

        out = [0.0] * dim
        tokens = text.lower().split()
        for pos, word in enumerate(tokens):
            # Three granularity levels — stem, full word, positioned word
            stem = word[:4] if len(word) >= 4 else word
            hashes = [
                int(hashlib.md5(stem.encode()).hexdigest()[:8], 16),
                int(hashlib.md5(word.encode()).hexdigest()[:8], 16),
                int(hashlib.md5(f"{pos}:{word}".encode()).hexdigest()[:8], 16),
            ]
            for h in hashes:
                for j in range(dim):
                    out[j] += math.sin((h + j * 31) * 0.01) * 0.33

        # Layer-norm style: center + scale
        if out:
            mean = sum(out) / len(out)
            out = [x - mean for x in out]
            norm = math.sqrt(sum(x * x for x in out)) or 1.0
            out = [x / norm for x in out]
        return out

    # ---- Convenience: run a battery's worth of stimuli ----

    def run_stimuli(self, stimuli, battery_name: str,
                    score_fn: Callable[[list[TestResult]], list[TestScore]] | None = None,
                    *, isolate: bool = False,
                    per_item_hook: Callable[[StimulusItem, TestResult], None] | None = None
                    ) -> BatteryResult:
        """Run through stimuli, collect responses, score at the end."""
        battery = BatteryResult(battery_name=battery_name)
        with self.trial(isolate=isolate, name=battery_name):
            for stim in stimuli:
                try:
                    res = self.probe_stimulus(stim)
                    if per_item_hook:
                        per_item_hook(stim, res)
                    battery.results.append(res)
                except Exception as e:
                    log.warning("Stimulus %s failed: %s", stim.id, e)
                    battery.flags.append(f"error:{stim.id}:{e}")

        if score_fn:
            try:
                battery.scores = score_fn(battery.results)
            except Exception as e:
                log.warning("Scoring failed for %s: %s", battery_name, e)
                battery.flags.append(f"scoring_error:{e}")
                battery.status = "error"

        return battery
