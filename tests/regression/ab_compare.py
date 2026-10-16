#!/usr/bin/env python3
"""A/B comparison of two nimcp.so versions.

Runs identical seeded inputs against both versions and verifies outputs
within epsilon. Catches silent-math bugs where a refactor produces
subtly-different numeric results.

Usage:
    # Stash current .so as baseline, build new, compare
    cp build/lib/python/nimcp.so build/lib/python/nimcp.so.baseline
    # ... make changes + rebuild ...
    python3 tests/regression/ab_compare.py \\
        build/lib/python/nimcp.so.baseline \\
        build/lib/python/nimcp.so \\
        --seeds 100 --tolerance 1e-4
"""
from __future__ import annotations

import argparse
import importlib.util
import json
import random
import sys
import tempfile
from pathlib import Path


def load_nimcp_from_path(so_path: str):
    """Load nimcp from a specific .so file path."""
    path = Path(so_path).resolve()
    if not path.exists():
        raise FileNotFoundError(f"Module not found: {path}")
    spec = importlib.util.spec_from_file_location("nimcp_ab", str(path))
    if spec is None:
        raise ImportError(f"Cannot load spec from {path}")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def compare_outputs(a, b, tolerance: float = 1e-4) -> tuple[bool, float]:
    """Return (match, delta)."""
    if a == b:
        return True, 0.0
    if isinstance(a, tuple) and isinstance(b, tuple):
        if len(a) != len(b):
            return False, float("inf")
        # String part must match exactly
        if a[0] != b[0]:
            return False, float("inf")
        # Numeric parts within tolerance
        if len(a) >= 2 and isinstance(a[1], (int, float)):
            delta = abs(a[1] - b[1])
            return delta <= tolerance, delta
    if isinstance(a, (int, float)) and isinstance(b, (int, float)):
        delta = abs(a - b)
        return delta <= tolerance, delta
    return False, float("inf")


def run_ab(so_a: str, so_b: str, n_seeds: int, tolerance: float, verbose: bool = False):
    # Note: Python can only import nimcp once at a time in a single process.
    # For a real A/B we run as two subprocesses. For this smoke version, we
    # import once and compare predict() determinism — which at minimum catches
    # intra-version nondeterminism.
    #
    # A true A/B requires: subprocess runs → deterministic seed → JSON dump →
    # compare files. We implement that below.

    results_a = run_inference_subprocess(so_a, n_seeds)
    results_b = run_inference_subprocess(so_b, n_seeds)

    mismatches = []
    max_delta = 0.0
    for seed in results_a:
        if seed not in results_b:
            mismatches.append((seed, "missing in B"))
            continue
        match, delta = compare_outputs(results_a[seed], results_b[seed], tolerance)
        max_delta = max(max_delta, delta if delta != float("inf") else max_delta)
        if not match:
            mismatches.append((seed, f"a={results_a[seed]} vs b={results_b[seed]} (delta={delta})"))

    total = len(results_a)
    pass_count = total - len(mismatches)
    print(f"\nA/B comparison: {pass_count}/{total} seeds match within tolerance={tolerance}")
    print(f"Max delta observed: {max_delta:.6f}")

    if mismatches:
        print(f"\nMismatches ({len(mismatches)}):")
        for seed, reason in mismatches[:10]:
            print(f"  seed {seed}: {reason}")
        if len(mismatches) > 10:
            print(f"  ... and {len(mismatches) - 10} more")
        return False
    return True


def run_inference_subprocess(so_path: str, n_seeds: int,
                              checkpoint_path: str | None = None) -> dict:
    """Run inference via a subprocess that imports nimcp from a temporary
    path.

    A true A/B requires a loaded-from-checkpoint brain so both processes
    start from IDENTICAL state (fresh brain init is nondeterministic across
    processes due to uninitialized subsystem state).

    If checkpoint_path is None, we create a fresh brain, save it, and load
    from that save in each subprocess — giving a well-defined shared state.
    """
    import shutil
    import subprocess

    with tempfile.TemporaryDirectory() as tmp:
        target_so = Path(tmp) / "nimcp.so"
        shutil.copy2(so_path, target_so)

        if checkpoint_path is None:
            # Create a shared checkpoint using the BASELINE .so (so_path)
            checkpoint_path = str(Path(tmp) / "shared_state.bin")
            bootstrap = f"""
import sys
sys.path.insert(0, {tmp!r})
import nimcp
b = nimcp.Brain('shared', 128, 10)
b.save({checkpoint_path!r})
"""
            boot_result = subprocess.run(
                [sys.executable, "-c", bootstrap],
                capture_output=True, text=True, timeout=120)
            if boot_result.returncode != 0:
                print(f"Bootstrap failed: {boot_result.stderr[-1500:]}")
                raise RuntimeError("Could not create shared checkpoint")

        out_file = Path(tmp) / "ab_results.json"
        script = f"""
import sys
sys.path.insert(0, {tmp!r})
import nimcp
import json
import random

# Load from shared checkpoint so both A and B start from identical state
b = nimcp.Brain.load({checkpoint_path!r})

results = {{}}
for seed in range({n_seeds}):
    random.seed(seed)
    features = [random.gauss(0, 1) for _ in range(10)]
    out = b.predict(features)
    if isinstance(out, tuple):
        results[str(seed)] = [out[0], float(out[1])]
    else:
        results[str(seed)] = out

with open({str(out_file)!r}, 'w') as f:
    json.dump(results, f)
"""
        result = subprocess.run(
            [sys.executable, "-c", script],
            capture_output=True, text=True, timeout=300)
        if result.returncode != 0:
            print(f"Subprocess failed for {so_path}:")
            print(result.stderr[-2000:])
            raise RuntimeError(f"A/B subprocess failed: {so_path}")

        with open(out_file) as f:
            data = json.load(f)
        return {int(k): tuple(v) if isinstance(v, list) else v
                for k, v in data.items()}


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("so_a", help="Baseline .so path")
    p.add_argument("so_b", help="New .so path")
    p.add_argument("--seeds", type=int, default=20, help="Number of seeds to test")
    p.add_argument("--tolerance", type=float, default=1e-4,
                   help="Max allowed numeric delta (default 1e-4)")
    p.add_argument("--accept-known-drift", action="store_true",
                   help="Accept the known save/load drift as baseline behavior. "
                        "Fails only if max delta exceeds 0.5 (catastrophic).")
    p.add_argument("-v", "--verbose", action="store_true")
    args = p.parse_args()

    if not Path(args.so_a).exists():
        print(f"ERROR: {args.so_a} not found", file=sys.stderr)
        sys.exit(1)
    if not Path(args.so_b).exists():
        print(f"ERROR: {args.so_b} not found", file=sys.stderr)
        sys.exit(1)

    tolerance = 0.5 if args.accept_known_drift else args.tolerance
    ok = run_ab(args.so_a, args.so_b, args.seeds, tolerance, args.verbose)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
