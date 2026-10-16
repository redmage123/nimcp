#!/usr/bin/env python3
"""
Automatic Learning Rate Tuner — Monitors training and adjusts LR dynamically.

Detects plateaus, loss spikes, and divergence, then adjusts LR via /tmp/athena_lr.

Strategies:
  - Plateau (loss flat for N steps): bump LR 3-5x to escape
  - Spike (loss jumps >5x): reduce LR 0.5x to stabilize
  - Convergence (loss steadily decreasing): maintain or slowly decay
  - Divergence (loss increasing for N steps): reduce LR 0.3x

Usage:
    python3 lr_auto_tuner.py                    # Monitor and auto-adjust
    python3 lr_auto_tuner.py --dry-run          # Monitor only, don't write
    python3 lr_auto_tuner.py --interval 60      # Check every 60 seconds
"""

import os
import re
import sys
import time
import argparse

LOG_FILE = "logs/training.log"
LR_FILE = "/tmp/athena_lr"
STATE_FILE = "/tmp/athena_lr_tuner_state"

# Thresholds
PLATEAU_STEPS = 300       # Steps of flat loss = plateau
PLATEAU_TOLERANCE = 0.05  # <5% change = flat
SPIKE_RATIO = 5.0         # Loss jump >5x = spike
DIVERGE_STEPS = 200       # Steps of increasing loss = divergence
MIN_LR = 1e-6
MAX_LR = 1e-2
BUMP_FACTOR = 3.0         # Multiply LR by this on plateau
REDUCE_FACTOR = 0.5       # Multiply LR by this on spike
DIVERGE_REDUCE = 0.3      # Multiply LR by this on divergence


def parse_losses(log_file, max_lines=500):
    """Extract (step, loss, lr) tuples from training log."""
    entries = []
    try:
        with open(log_file) as f:
            lines = f.readlines()
        for line in lines[-max_lines:]:
            # Match: [2700] loss=9765.4906 (50/50 non-zero) ...
            m = re.search(r'\[(\d+)\]\s+loss=([\d.]+)', line)
            if m:
                step = int(m.group(1))
                loss = float(m.group(2))
                entries.append((step, loss))
            # Match: lr=0.000028
            m2 = re.search(r'lr=([\d.]+)', line)
            if m2 and entries:
                entries[-1] = (entries[-1][0], entries[-1][1], float(m2.group(1)))
    except Exception:
        pass
    return entries


def detect_state(entries):
    """Analyze loss trajectory and return state + recommendation."""
    if len(entries) < 4:
        return "insufficient_data", 1.0, "Need more data points"

    recent = entries[-6:]  # Last 6 reports (300 steps at 50-step interval)
    losses = [e[1] for e in recent]
    steps = [e[0] for e in recent]

    # Current LR (from last entry with LR info)
    current_lr = None
    for e in reversed(entries):
        if len(e) > 2:
            current_lr = e[2]
            break

    # Mean and trend
    mean_loss = sum(losses) / len(losses)
    first_half = sum(losses[:len(losses)//2]) / max(len(losses)//2, 1)
    second_half = sum(losses[len(losses)//2:]) / max(len(losses) - len(losses)//2, 1)

    # Plateau: loss barely changing
    loss_range = max(losses) - min(losses)
    relative_range = loss_range / max(mean_loss, 1e-10)

    if relative_range < PLATEAU_TOLERANCE and len(recent) >= 4:
        step_span = steps[-1] - steps[0]
        if step_span >= PLATEAU_STEPS:
            return "plateau", BUMP_FACTOR, (
                f"Loss plateau at {mean_loss:.1f} for {step_span} steps "
                f"(range {loss_range:.1f}, {relative_range:.1%} variation)")

    # Spike: sudden large increase
    if len(losses) >= 2:
        prev = losses[-2]
        curr = losses[-1]
        if prev > 0 and curr / prev > SPIKE_RATIO:
            return "spike", REDUCE_FACTOR, (
                f"Loss spike: {prev:.1f} → {curr:.1f} ({curr/prev:.1f}x)")

    # Divergence: loss increasing over multiple reports
    if second_half > first_half * 1.3 and len(recent) >= 4:
        return "divergence", DIVERGE_REDUCE, (
            f"Loss increasing: {first_half:.1f} → {second_half:.1f} "
            f"(+{(second_half/first_half - 1)*100:.0f}%)")

    # Convergence: loss decreasing
    if second_half < first_half * 0.9:
        return "converging", 1.0, (
            f"Loss decreasing: {first_half:.1f} → {second_half:.1f} "
            f"({(1 - second_half/first_half)*100:.0f}% reduction)")

    # Normal: some fluctuation but not pathological
    return "normal", 1.0, (
        f"Loss at {mean_loss:.1f} (range {loss_range:.1f})")


def apply_lr_adjustment(factor, current_lr, dry_run=False):
    """Write new LR to override file."""
    if current_lr is None:
        current_lr = 0.00003  # Default assumption
    new_lr = current_lr * factor
    new_lr = max(MIN_LR, min(MAX_LR, new_lr))

    if abs(factor - 1.0) < 0.01:
        return current_lr  # No change needed

    if dry_run:
        print(f"  [DRY RUN] Would set LR: {current_lr:.6f} → {new_lr:.6f} ({factor:.1f}x)")
        return current_lr

    with open(LR_FILE, 'w') as f:
        f.write(f"{new_lr:.8f}\n")
    print(f"  LR adjusted: {current_lr:.6f} → {new_lr:.6f} ({factor:.1f}x)")
    return new_lr


def main():
    parser = argparse.ArgumentParser(description="Auto-tune learning rate")
    parser.add_argument("--interval", type=int, default=120,
                        help="Check interval in seconds (default: 120)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Monitor only, don't write LR overrides")
    parser.add_argument("--once", action="store_true",
                        help="Run once and exit")
    args = parser.parse_args()

    print("=" * 50)
    print("  Athena LR Auto-Tuner")
    print("=" * 50)
    print(f"  Log: {LOG_FILE}")
    print(f"  LR file: {LR_FILE}")
    print(f"  Interval: {args.interval}s")
    print(f"  Mode: {'DRY RUN' if args.dry_run else 'ACTIVE'}")
    print(f"  Plateau threshold: {PLATEAU_STEPS} steps, <{PLATEAU_TOLERANCE:.0%} variation")
    print(f"  Bump factor: {BUMP_FACTOR}x | Reduce factor: {REDUCE_FACTOR}x")
    print()

    last_state = None
    last_adjustment_step = 0
    cooldown_steps = 500  # Don't adjust more than once per 500 steps

    try:
        while True:
            entries = parse_losses(LOG_FILE)
            if not entries:
                print(f"[{time.strftime('%H:%M:%S')}] No loss data yet")
                if args.once:
                    break
                time.sleep(args.interval)
                continue

            current_step = entries[-1][0]
            current_loss = entries[-1][1]

            # Get current LR
            current_lr = None
            for e in reversed(entries):
                if len(e) > 2:
                    current_lr = e[2]
                    break
            # Also check the override file
            if os.path.exists(LR_FILE):
                try:
                    with open(LR_FILE) as f:
                        current_lr = float(f.read().strip())
                except Exception:
                    pass

            state, factor, reason = detect_state(entries)

            status = (f"[{time.strftime('%H:%M:%S')}] Step {current_step} | "
                     f"Loss {current_loss:.1f} | LR {current_lr or 0:.6f} | "
                     f"State: {state}")
            print(status)
            print(f"  {reason}")

            # Apply adjustment if state changed and not in cooldown
            steps_since_adjust = current_step - last_adjustment_step
            if (state != last_state and factor != 1.0 and
                    steps_since_adjust >= cooldown_steps):
                current_lr = apply_lr_adjustment(factor, current_lr, args.dry_run)
                last_adjustment_step = current_step
                last_state = state
            elif factor != 1.0 and steps_since_adjust < cooldown_steps:
                remaining = cooldown_steps - steps_since_adjust
                print(f"  (cooldown: {remaining} steps remaining before next adjustment)")
            else:
                last_state = state

            print()

            if args.once:
                break
            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nAuto-tuner stopped.")
        # Clean up override if we set one
        if os.path.exists(LR_FILE):
            print(f"Note: LR override still active at {LR_FILE}")
            print(f"Remove with: rm {LR_FILE}")


if __name__ == "__main__":
    main()
