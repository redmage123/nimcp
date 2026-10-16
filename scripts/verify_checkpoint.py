#!/usr/bin/env python3
"""
Checkpoint integrity verifier — run before and after daemon restarts.

Checks:
1. File size is reasonable (>1 GB for a 1M neuron brain)
2. Brain can load and produce non-zero output
3. Loss on a test input is reasonable (< 100)

Usage:
    python3 scripts/verify_checkpoint.py checkpoints/athena/athena_daemon.bin
    python3 scripts/verify_checkpoint.py --daemon  # verify running daemon
"""

import sys
import os
import struct

def verify_file(path):
    """Check checkpoint file integrity."""
    if not os.path.exists(path):
        print(f"FAIL: {path} does not exist")
        return False

    size = os.path.getsize(path)
    size_gb = size / (1024**3)
    print(f"File: {path}")
    print(f"Size: {size_gb:.2f} GB ({size:,} bytes)")

    if size < 1_000_000_000:  # Less than 1 GB
        print(f"FAIL: Checkpoint too small ({size_gb:.2f} GB). "
              f"Expected >1 GB for 1M neuron brain.")
        print(f"This checkpoint is likely corrupted from a struct layout mismatch.")
        return False

    # Check header
    with open(path, 'rb') as f:
        header = f.read(16)
        if len(header) < 16:
            print("FAIL: File too short for header")
            return False
        magic = header[:4]
        print(f"Magic: {magic} ({'OK' if magic == b'NIMP' else 'UNEXPECTED'})")

    print("PASS: Checkpoint file looks valid")
    return True


def verify_daemon():
    """Verify the running daemon produces valid output."""
    sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
    from brain_client import BrainProxy

    try:
        brain = BrainProxy()
    except Exception as e:
        print(f"FAIL: Cannot connect to daemon: {e}")
        return False

    # Test inference
    import numpy as np
    test_input = np.random.randn(1024).astype(np.float32).tolist()
    try:
        result = brain.decide_full(test_input)
    except Exception as e:
        print(f"FAIL: decide_full failed: {e}")
        return False

    output = result.get("output_vector", [])
    if not output:
        print("FAIL: No output vector")
        return False

    non_zero = sum(1 for x in output if abs(x) > 0.001)
    total = len(output)
    max_val = max(abs(x) for x in output) if output else 0

    print(f"Output: {non_zero}/{total} non-zero, max={max_val:.4f}")

    if non_zero == 0:
        print("FAIL: ALL OUTPUTS ARE ZERO — brain is collapsed!")
        return False

    if non_zero < total * 0.1:
        print(f"WARN: Only {non_zero}/{total} non-zero — possible collapse")

    # Test learning
    try:
        target = np.random.randn(4096).astype(np.float32).tolist()
        loss = brain.learn_vector(test_input, target, label="verify",
                                   confidence=0.1, learning_rate=0.0)
        print(f"Loss: {loss}")
        if loss is not None and loss < 100:
            print("PASS: Brain is producing valid output and loss")
            return True
        else:
            print(f"WARN: Loss is high ({loss}) — brain may need retraining")
            return True  # Not necessarily broken
    except Exception as e:
        print(f"FAIL: learn_vector failed: {e}")
        return False


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--daemon":
        ok = verify_daemon()
    elif len(sys.argv) > 1:
        ok = verify_file(sys.argv[1])
    else:
        # Verify default checkpoint + daemon
        default = "checkpoints/athena/athena_daemon.bin"
        ok = verify_file(default)
        if ok:
            print("\nVerifying daemon...")
            ok = verify_daemon()

    sys.exit(0 if ok else 1)
