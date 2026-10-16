"""Reusable scoring primitives for tests."""
from __future__ import annotations

import math
import re
import statistics
from typing import Any, Iterable


def cosine(a: list[float], b: list[float]) -> float:
    """Cosine similarity, returns 0 for degenerate inputs."""
    if not a or not b or len(a) != len(b):
        return 0.0
    dot = sum(x*y for x, y in zip(a, b))
    na = math.sqrt(sum(x*x for x in a))
    nb = math.sqrt(sum(y*y for y in b))
    if na < 1e-12 or nb < 1e-12:
        return 0.0
    return dot / (na * nb)


def rmse(a: list[float], b: list[float]) -> float:
    if not a or len(a) != len(b):
        return 0.0
    return math.sqrt(sum((x-y)**2 for x, y in zip(a, b)) / len(a))


def calibration_rmse(confidences: list[float], accuracies: list[float], bins: int = 10) -> float:
    """Expected calibration error. 0 = perfect, 1 = maximal."""
    if not confidences:
        return 0.0
    buckets: list[list[tuple[float, float]]] = [[] for _ in range(bins)]
    for c, a in zip(confidences, accuracies):
        idx = min(int(c * bins), bins - 1)
        buckets[idx].append((c, a))
    ece = 0.0
    total = len(confidences)
    for bucket in buckets:
        if not bucket:
            continue
        bc = sum(x[0] for x in bucket) / len(bucket)
        ba = sum(x[1] for x in bucket) / len(bucket)
        ece += (len(bucket) / total) * abs(bc - ba)
    return ece


def normalize_to_unit(x: float, lo: float, hi: float) -> float:
    """Map x in [lo,hi] → [0,1], clamped."""
    if hi <= lo:
        return 0.0
    return max(0.0, min(1.0, (x - lo) / (hi - lo)))


def keyword_coverage(response: str, keywords: list[str]) -> float:
    """Fraction of keywords that appear in response (case-insensitive)."""
    if not keywords:
        return 1.0
    text = (response or "").lower()
    hit = sum(1 for k in keywords if k.lower() in text)
    return hit / len(keywords)


def exact_match(response: Any, expected: Any) -> float:
    """1.0 if strings match normalized, else 0.0."""
    if response is None or expected is None:
        return 0.0
    a = str(response).strip().lower()
    b = str(expected).strip().lower()
    return 1.0 if a == b else 0.0


def contains_any(response: str, phrases: list[str]) -> float:
    text = (response or "").lower()
    return 1.0 if any(p.lower() in text for p in phrases) else 0.0


def extract_number(text: str) -> float | None:
    """Pull first number from a string (handles commas, decimals)."""
    if not text:
        return None
    match = re.search(r"(-?\d[\d,]*(?:\.\d+)?)", str(text))
    if not match:
        return None
    try:
        return float(match.group(1).replace(",", ""))
    except ValueError:
        return None


def anchoring_shift(anchor_low: float, estimate_low: float,
                    anchor_high: float, estimate_high: float) -> float:
    """Proportion of anchor difference reflected in estimate difference.

    Returns 0 (no anchoring) to 1 (fully anchored).
    """
    anchor_delta = anchor_high - anchor_low
    if abs(anchor_delta) < 1e-6:
        return 0.0
    est_delta = estimate_high - estimate_low
    shift = est_delta / anchor_delta
    return max(0.0, min(1.0, shift))


def loss_to_score(loss: float, good: float = 0.1, bad: float = 2.0) -> float:
    """Invert a loss to a 0..1 score; lower loss = higher score."""
    if loss <= good:
        return 1.0
    if loss >= bad:
        return 0.0
    return 1.0 - (loss - good) / (bad - good)


def trajectory_correlation(trajectory: list[float], target: list[float]) -> float:
    """Pearson correlation over time; returns 0..1 (maps negative to 0)."""
    n = min(len(trajectory), len(target))
    if n < 3:
        return 0.0
    x, y = trajectory[:n], target[:n]
    mx, my = sum(x)/n, sum(y)/n
    num = sum((x[i]-mx)*(y[i]-my) for i in range(n))
    dx = math.sqrt(sum((x[i]-mx)**2 for i in range(n)))
    dy = math.sqrt(sum((y[i]-my)**2 for i in range(n)))
    if dx < 1e-12 or dy < 1e-12:
        return 0.0
    r = num / (dx * dy)
    return max(0.0, min(1.0, (r + 1) / 2))


def consistency(values: list[float]) -> float:
    """0..1 — higher means more consistent (lower stdev relative to mean)."""
    if len(values) < 2:
        return 1.0
    mean = sum(values) / len(values)
    if abs(mean) < 1e-12:
        return 1.0
    stdev = statistics.stdev(values)
    cv = stdev / abs(mean)
    return max(0.0, 1.0 - cv)


def mean(values: Iterable[float]) -> float:
    v = list(values)
    return sum(v) / len(v) if v else 0.0
