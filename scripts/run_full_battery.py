#!/usr/bin/env python3
"""Athena Full Cognitive & Safety Test Battery — orchestrator.

Runs all 21 test batteries against a live brain daemon, collects
results, stores them longitudinally, and emits a report card in
text/JSON/HTML formats.

Usage:
    python3 run_full_battery.py [--socket PATH] [--output DIR] [--only B1,B2]
                                [--notes "checkpoint description"]
                                [--skip-unstable] [--quick]
"""
from __future__ import annotations

import argparse
import logging
import os
import sys
import time
from pathlib import Path

# Make this script importable when run from anywhere
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from test_harness import TestHarness, ResultStore, ReportCard
from tests import BATTERIES, ALL_BATTERY_NAMES


def setup_logging(verbose: bool):
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%H:%M:%S")


def make_client(socket_path: str):
    """Connect to brain daemon via BrainProxy, or fall back to direct nimcp."""
    try:
        from brain_client import BrainProxy, is_daemon_running
        if is_daemon_running(socket_path):
            logging.info("Connecting to brain daemon at %s", socket_path)
            return BrainProxy(socket_path=socket_path)
    except Exception as e:
        logging.warning("BrainProxy unavailable: %s", e)

    # Direct fallback
    try:
        import nimcp
        logging.warning("No daemon — creating local brain (slow, test-only)")
        return nimcp.Brain("test_battery_brain", 128, 10)
    except Exception as e:
        logging.error("Cannot obtain a brain client: %s", e)
        raise


def snn_is_stable(client) -> bool:
    """Check that SNN firing rate is in a biological range."""
    try:
        stats = client._call("get_snn_stats") if hasattr(client, "_call") else None
        if not stats:
            return True  # can't check — assume ok
        rate_hz = stats.get("mean_firing_rate_hz", 0) or 0
        sparsity = stats.get("sparsity", 0) or 0
        # Biological range: sparsity 0.9-0.99 (firing 1-10%)
        if sparsity < 0.80:
            logging.warning("SNN appears SATURATED (sparsity=%.2f)", sparsity)
            return False
        if sparsity > 0.999:
            logging.warning("SNN appears SILENT (sparsity=%.3f)", sparsity)
            return False
        return True
    except Exception:
        return True


def run_battery(name: str, fn, harness: TestHarness, timeout_s: float):
    """Run one battery with error handling and timing."""
    t0 = time.time()
    logging.info("▶ %s", name)
    try:
        result = fn(harness)
        elapsed = time.time() - t0
        score = result.primary_score()
        logging.info("✓ %s: %.2f  (%.1fs)", name, score, elapsed)
        return result
    except Exception as e:
        elapsed = time.time() - t0
        logging.error("✗ %s failed after %.1fs: %s", name, elapsed, e)
        from test_harness import BatteryResult
        return BatteryResult(battery_name=name, status="error", flags=[str(e)])


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--socket", default=os.environ.get("ATHENA_SOCKET",
                        "/var/run/athena/brain.sock"))
    parser.add_argument("--output", default="/var/lib/athena/reports",
                        help="Output directory for report card files")
    parser.add_argument("--notes", default="", help="Run notes (e.g. checkpoint name)")
    parser.add_argument("--checkpoint", default="",
                        help="Checkpoint identifier for longitudinal tracking")
    parser.add_argument("--only", default="",
                        help="Comma-separated battery names (default: all)")
    parser.add_argument("--skip", default="",
                        help="Comma-separated battery names to skip")
    parser.add_argument("--quick", action="store_true",
                        help="Skip slow batteries (consolidation)")
    parser.add_argument("--skip-unstable", action="store_true",
                        help="Abort if SNN is saturated/silent")
    parser.add_argument("--timeout", type=float, default=600.0,
                        help="Per-battery timeout in seconds")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    setup_logging(args.verbose)

    # Connect to brain
    client = make_client(args.socket)

    # Pre-flight checks
    if args.skip_unstable and not snn_is_stable(client):
        logging.error("ABORT: SNN is not in biological range. Waiting for stability "
                      "before running tests. Use without --skip-unstable to force.")
        return 2

    # Determine which batteries to run
    if args.only:
        names = [n.strip() for n in args.only.split(",") if n.strip()]
    else:
        names = ALL_BATTERY_NAMES[:]

    if args.skip:
        skip = {n.strip() for n in args.skip.split(",") if n.strip()}
        names = [n for n in names if n not in skip]

    if args.quick:
        names = [n for n in names if n not in {"consolidation"}]

    unknown = [n for n in names if n not in BATTERIES]
    if unknown:
        logging.error("Unknown batteries: %s", unknown)
        logging.info("Available: %s", ", ".join(ALL_BATTERY_NAMES))
        return 1

    # Start the run
    store = ResultStore()
    run_id = store.start_run(checkpoint=args.checkpoint, notes=args.notes)
    logging.info("=== RUN %s — %d batteries ===", run_id, len(names))

    harness = TestHarness(client)
    report = ReportCard(run_id=run_id, checkpoint=args.checkpoint, notes=args.notes)

    # Execute
    for name in names:
        result = run_battery(name, BATTERIES[name], harness, args.timeout)
        report.add(result)
        try:
            store.record_battery(run_id, result)
        except Exception as e:
            logging.warning("Store failed for %s: %s", name, e)

    # Safety integration — emit audit events and check for drift
    try:
        from test_harness.safety import emit_battery_events, check_drift
        emit_battery_events(client, report.batteries)
        drift_flags = check_drift(store, report.batteries)
        if drift_flags:
            logging.warning("DRIFT DETECTED:")
            for f in drift_flags:
                logging.warning("  %s", f)
            # Tag these on the report's headline
            for b in report.batteries:
                if b.status == "ok":
                    continue
                b.flags.extend(f for f in drift_flags if b.battery_name in f)
    except Exception as e:
        logging.warning("Safety integration step failed: %s", e)

    # Finalize
    overall = report.overall_score()
    store.finish_run(run_id, overall)

    try:
        paths = report.write(args.output)
        logging.info("Reports written: %s", paths)
    except Exception as e:
        logging.warning("Could not write reports to %s: %s — trying home dir", args.output, e)
        paths = report.write(Path.home() / "athena_reports")
        logging.info("Reports written to: %s", paths)

    # Print summary to stdout
    print()
    print(report.to_text())
    print()
    print(f"Overall: {overall:.2f}")
    print(f"Reports: {paths}")

    store.close()
    return 0 if overall >= 0.5 else 3


if __name__ == "__main__":
    sys.exit(main())
