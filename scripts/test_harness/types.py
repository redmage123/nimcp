"""Shared dataclasses for the test harness."""
from __future__ import annotations

import json
from dataclasses import dataclass, field, asdict
from typing import Any


@dataclass
class StimulusItem:
    """A single presentation — prompt, expected response pattern, scoring recipe."""
    id: str
    prompt: str
    modality: str = "text"                    # text / image / audio / multimodal
    payload: Any = None                        # raw data for image/audio
    expected: Any = None                       # ground truth (if known)
    scoring: dict = field(default_factory=dict)
    variant_group: str | None = None           # groups paired stimuli (anchor pairs etc)
    metadata: dict = field(default_factory=dict)

    @classmethod
    def from_dict(cls, d: dict) -> StimulusItem:
        return cls(
            id=d["id"],
            prompt=d.get("prompt", ""),
            modality=d.get("modality", "text"),
            payload=d.get("payload"),
            expected=d.get("expected"),
            scoring=d.get("scoring", {}),
            variant_group=d.get("variant_group"),
            metadata=d.get("metadata", {}),
        )


@dataclass
class TestScore:
    """A single scored outcome for one test or subtest."""
    name: str
    value: float              # 0..1 normalized
    label: str = ""           # "A+", "PASS", "FLAG" etc
    components: dict = field(default_factory=dict)
    notes: str = ""


@dataclass
class TestResult:
    """Per-stimulus response and raw metrics."""
    stimulus_id: str
    prompt: str
    response: Any
    internal_state: dict = field(default_factory=dict)
    reasoning_trace: list = field(default_factory=list)
    emotion_state: dict = field(default_factory=dict)
    confidence: float | None = None
    latency_ms: float | None = None
    extra: dict = field(default_factory=dict)

    def to_json(self) -> str:
        return json.dumps(asdict(self), default=str)


@dataclass
class BatteryResult:
    """Outcome of an entire battery (many tests → scores)."""
    battery_name: str
    scores: list[TestScore] = field(default_factory=list)
    results: list[TestResult] = field(default_factory=list)
    summary: dict = field(default_factory=dict)
    flags: list[str] = field(default_factory=list)
    status: str = "ok"        # ok / flag / critical / error

    def primary_score(self) -> float:
        """Mean of top-level scores."""
        if not self.scores:
            return 0.0
        return sum(s.value for s in self.scores) / len(self.scores)


def grade(v: float) -> str:
    """Map 0..1 score to letter grade."""
    if v >= 0.97: return "A+"
    if v >= 0.93: return "A"
    if v >= 0.90: return "A-"
    if v >= 0.87: return "B+"
    if v >= 0.83: return "B"
    if v >= 0.80: return "B-"
    if v >= 0.77: return "C+"
    if v >= 0.73: return "C"
    if v >= 0.70: return "C-"
    if v >= 0.67: return "D+"
    if v >= 0.63: return "D"
    if v >= 0.60: return "D-"
    return "F"
