#!/usr/bin/env python3
"""End-to-end memory implantation script.

Usage:
    # Generate memories to disk (offline):
    python3 scripts/childhood_memories/run_implantation.py generate \\
        --output-dir data/implanted_memories/v1 --seed 42

    # Implant into a running brain:
    python3 scripts/childhood_memories/run_implantation.py implant \\
        --memory-dir data/implanted_memories/v1 \\
        --socket /var/run/athena/brain.sock

    # Both in sequence (fresh brain):
    python3 scripts/childhood_memories/run_implantation.py full \\
        --output-dir data/implanted_memories/v1 --seed 42
"""
from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "scripts"))


def cmd_generate(args):
    from childhood_memories import MemoryGenerator
    gen = MemoryGenerator(output_dir=args.output_dir,
                          seed=args.seed,
                          vector_dim=args.vector_dim)
    summary = gen.generate_all()
    print(f"\nGeneration complete:")
    for layer, count in summary.items():
        print(f"  {layer}: {count}")
    return 0


def cmd_implant(args):
    from childhood_memories import MemoryImplanter, verify_retrievable

    # Try to connect to daemon first, else fresh local brain
    brain = None
    try:
        from brain_client import BrainProxy, is_daemon_running
        if is_daemon_running(args.socket):
            print(f"Connecting to daemon at {args.socket}")
            brain = BrainProxy(socket_path=args.socket)
    except Exception as e:
        print(f"Daemon unavailable ({e}), creating local brain")

    if brain is None:
        import nimcp
        brain = nimcp.Brain("implant_target", 128, 10)

    implanter = MemoryImplanter(brain, memory_dir=args.memory_dir,
                                  verbose=args.verbose)
    result = implanter.implant_all()
    print(f"\nImplantation: {result.summary()}")

    if args.verify:
        print("\nVerifying retrieval...")
        vr = verify_retrievable(brain, memory_dir=args.memory_dir,
                                 sample_n=args.verify_n)
        print(f"Verification: {vr.summary()}")
        if vr.retrieval_rate < 0.5:
            print("WARNING: retrieval rate below 50% — API availability issue")
            return 1
    return 0


def cmd_full(args):
    rc = cmd_generate(args)
    if rc != 0:
        return rc
    # Swap args for implant
    args.memory_dir = args.output_dir
    return cmd_implant(args)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)

    g = sub.add_parser("generate", help="Generate memory JSON")
    g.add_argument("--output-dir", required=True)
    g.add_argument("--seed", type=int, default=42)
    g.add_argument("--vector-dim", type=int, default=1024)
    g.set_defaults(func=cmd_generate)

    i = sub.add_parser("implant", help="Implant into a brain")
    i.add_argument("--memory-dir", required=True)
    i.add_argument("--socket", default="/var/run/athena/brain.sock")
    i.add_argument("--verify", action="store_true")
    i.add_argument("--verify-n", type=int, default=20)
    i.add_argument("-v", "--verbose", action="store_true")
    i.set_defaults(func=cmd_implant)

    f = sub.add_parser("full", help="Generate + implant")
    f.add_argument("--output-dir", required=True)
    f.add_argument("--seed", type=int, default=42)
    f.add_argument("--vector-dim", type=int, default=1024)
    f.add_argument("--socket", default="/var/run/athena/brain.sock")
    f.add_argument("--verify", action="store_true")
    f.add_argument("--verify-n", type=int, default=20)
    f.add_argument("-v", "--verbose", action="store_true")
    f.set_defaults(func=cmd_full)

    args = parser.parse_args()
    logging.basicConfig(level=logging.DEBUG if getattr(args, "verbose", False)
                         else logging.INFO,
                         format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
    sys.exit(args.func(args))


if __name__ == "__main__":
    main()
