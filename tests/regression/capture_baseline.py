#!/usr/bin/env python3
"""Capture a baseline JSON of scores for regression comparison.

Runs a lightweight subset of batteries on a fresh local brain and records
scores + structural metrics for later comparison.

Usage:
    python3 tests/regression/capture_baseline.py tests/baseline/battery_baseline.json
"""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))

# Subset that runs fast and deterministically on a minimal brain
BASELINE_BATTERIES = [
    "cognitive_discrimination",
    "cognitive_memory",
    "biases",
    "puzzles",
    "mirror_test",
]


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "tests/baseline/battery_baseline.json"

    import nimcp
    from test_harness import TestHarness
    from tests import BATTERIES

    # Brain needs num_inputs=1024 to match harness's text_to_features encoding
    brain = nimcp.Brain("baseline_capture", 128, 10, num_inputs=1024, num_outputs=128)
    h = TestHarness(brain)

    baseline = {
        "version": 1,
        "captured_at": time.time(),
        "captured_at_iso": time.strftime("%Y-%m-%d %H:%M:%S"),
        "batteries": {},
    }

    for name in BASELINE_BATTERIES:
        if name not in BATTERIES:
            print(f"  SKIP: battery {name} not found")
            continue
        print(f"  Capturing {name}...")
        try:
            r = BATTERIES[name](h)
            baseline["batteries"][name] = {
                "primary_score": r.primary_score(),
                "status": r.status,
                "n_results": len(r.results),
                "n_scores": len(r.scores),
                "scores": [
                    {"name": s.name, "value": s.value}
                    for s in r.scores
                ],
            }
        except Exception as e:
            print(f"  ERROR in {name}: {e}")
            baseline["batteries"][name] = {"error": str(e)}

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w") as f:
        json.dump(baseline, f, indent=2, default=str)

    print(f"\nBaseline written to {out_path}")
    print(f"  {len(baseline['batteries'])} batteries captured")
    for name, data in baseline["batteries"].items():
        if "error" in data:
            print(f"    {name}: ERROR — {data['error']}")
        else:
            print(f"    {name}: score={data['primary_score']:.3f} status={data['status']}")


if __name__ == "__main__":
    main()
