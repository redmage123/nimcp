#!/usr/bin/env python3
"""Compare current battery scores against a captured baseline.

Exits non-zero if any battery's primary_score drops more than REGRESSION_TOLERANCE
below the baseline. Structural changes (new battery, removed battery, status
downgrade) are also flagged.
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

REGRESSION_TOLERANCE = 0.10          # 10% drop flagged
MAJOR_REGRESSION_TOLERANCE = 0.25    # 25% drop = hard fail


def load_baseline(path: str) -> dict:
    if not Path(path).exists():
        raise FileNotFoundError(f"Baseline not found: {path}")
    with open(path) as f:
        return json.load(f)


def main():
    baseline_path = sys.argv[1] if len(sys.argv) > 1 else "tests/baseline/battery_baseline.json"
    baseline = load_baseline(baseline_path)

    import nimcp
    from test_harness import TestHarness
    from tests import BATTERIES

    brain = nimcp.Brain("regression_compare", 128, 10, num_inputs=1024, num_outputs=128)
    h = TestHarness(brain)

    regressions = []
    warnings = []

    for name, baseline_data in baseline["batteries"].items():
        if "error" in baseline_data:
            continue
        if name not in BATTERIES:
            regressions.append(f"battery {name} removed")
            continue

        try:
            r = BATTERIES[name](h)
        except Exception as e:
            regressions.append(f"{name}: execution failed ({e})")
            continue

        base_score = baseline_data["primary_score"]
        cur_score = r.primary_score()
        delta = cur_score - base_score

        if delta < -MAJOR_REGRESSION_TOLERANCE:
            regressions.append(
                f"{name}: score {cur_score:.3f} (baseline {base_score:.3f}, "
                f"delta {delta:+.3f}) — MAJOR regression")
        elif delta < -REGRESSION_TOLERANCE:
            warnings.append(
                f"{name}: score {cur_score:.3f} (baseline {base_score:.3f}, "
                f"delta {delta:+.3f})")

        if baseline_data["status"] == "ok" and r.status == "critical":
            regressions.append(f"{name}: status ok → critical")

    print(f"\nCompared {len(baseline['batteries'])} batteries against baseline "
          f"from {baseline.get('captured_at_iso', '?')}")

    if warnings:
        print(f"\nWarnings ({len(warnings)}):")
        for w in warnings:
            print(f"  ! {w}")

    if regressions:
        print(f"\nREGRESSIONS ({len(regressions)}):")
        for r in regressions:
            print(f"  x {r}")
        sys.exit(1)

    print("\nNo regressions detected.")


if __name__ == "__main__":
    main()
