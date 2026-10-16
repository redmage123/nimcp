#!/usr/bin/env python3
"""Exercise the test harness end-to-end against a local brain.

Validates that the harness framework, stimulus loader, scoring primitives,
and report card all work together. Does NOT validate actual cognition.
"""
from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

# Make scripts importable
REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))


def test_harness_loads():
    from test_harness import TestHarness, ReportCard, ResultStore, load_stimuli
    print(f"  PASS: harness module imports cleanly")


def test_stimulus_banks_load():
    from test_harness import load_stimuli
    # Try a few banks
    attempts = [
        "cognitive/tier1_discrimination.json",
        "biases/anchoring.json",
        "personality/cluster_a_probes.json",
        "puzzles/logic.json",
    ]
    for bank_path in attempts:
        try:
            bank = load_stimuli(bank_path)
            assert len(bank) > 0, f"Empty bank: {bank_path}"
        except FileNotFoundError:
            print(f"  SKIP: {bank_path} not found (expected on local dev)")
            continue
    print(f"  PASS: stimulus banks load")


def test_battery_runs_end_to_end():
    """Run a single battery with a tiny local brain; expect completion not correctness."""
    import nimcp
    from test_harness import TestHarness
    from tests import BATTERIES

    b = nimcp.Brain("harness_test", 128, 10)
    h = TestHarness(b)

    # Pick a lightweight battery
    if "cognitive_discrimination" not in BATTERIES:
        print(f"  FAIL: cognitive_discrimination not in BATTERIES")
        raise AssertionError("missing battery")

    result = BATTERIES["cognitive_discrimination"](h)
    # We expect status OK or error (no stimuli locally is OK), but no crash
    print(f"  PASS: battery ran to completion (status={result.status}, "
          f"score={result.primary_score():.2f}, n_results={len(result.results)})")


def test_report_card_generation():
    from test_harness import ReportCard, BatteryResult, TestScore

    rc = ReportCard(run_id="test_run", checkpoint="none", notes="smoke")
    b = BatteryResult(battery_name="synthetic")
    b.scores = [TestScore(name="s1", value=0.75), TestScore(name="s2", value=0.5)]
    rc.add(b)

    text = rc.to_text()
    json_out = rc.to_json()
    html = rc.to_html()
    # Text output uppercases battery names; check both forms
    assert "synthetic" in text.lower(), f"'synthetic' missing in text: {text[:200]}"
    assert json_out["batteries"], "JSON output has no batteries"
    assert "<html" in html.lower() or "<!doctype" in html.lower(), "HTML missing root tag"
    print(f"  PASS: report card generates text/JSON/HTML")


def test_sqlite_store_cycle():
    from test_harness import ResultStore, BatteryResult, TestScore

    with tempfile.TemporaryDirectory() as tmp:
        db_path = os.path.join(tmp, "test.db")
        os.environ["ATHENA_TEST_DB"] = db_path
        store = ResultStore(db_path=db_path)
        run_id = store.start_run(checkpoint="smoke", notes="unit")
        b = BatteryResult(battery_name="smoke_bat")
        b.scores = [TestScore(name="x", value=0.5)]
        store.record_battery(run_id, b)
        store.finish_run(run_id, 0.5)

        hist = store.recent_metric("smoke_bat.x", n=5)
        assert len(hist) == 1
        assert abs(hist[0][1] - 0.5) < 1e-6
        store.close()
    os.environ.pop("ATHENA_TEST_DB", None)
    print(f"  PASS: SQLite store cycle (write + read)")


def main():
    failures = []
    for name, fn in [
        ("harness_loads", test_harness_loads),
        ("stimulus_banks_load", test_stimulus_banks_load),
        ("battery_runs_end_to_end", test_battery_runs_end_to_end),
        ("report_card_generation", test_report_card_generation),
        ("sqlite_store_cycle", test_sqlite_store_cycle),
    ]:
        print(f"[smoke/harness] {name}")
        try:
            fn()
        except Exception as e:
            failures.append((name, e))
            print(f"  FAIL: {type(e).__name__}: {e}")
    if failures:
        print(f"\n{len(failures)} failures")
        sys.exit(1)
    print("\nAll harness smoke tests passed.")


if __name__ == "__main__":
    main()
