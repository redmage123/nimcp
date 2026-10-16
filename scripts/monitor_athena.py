#!/usr/bin/env python3
"""Monitor Athena training — probe brain state from a separate process.

Reads the latest checkpoint and prints brain health metrics.
Also tails the training log output.

Usage:
    python3 scripts/monitor_athena.py
    python3 scripts/monitor_athena.py --watch  # continuous monitoring
"""

import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import nimcp


CHECKPOINT_DIR = "checkpoints/athena"
STATE_FILE = os.path.join(CHECKPOINT_DIR, "immersive_state.json")
CHECKPOINT_FILE = os.path.join(CHECKPOINT_DIR, "athena_immersive.bin")


def load_state():
    """Load immersive training state."""
    if os.path.exists(STATE_FILE):
        with open(STATE_FILE) as f:
            return json.load(f)
    return None


def probe_checkpoint():
    """Load latest checkpoint and probe it."""
    if not os.path.exists(CHECKPOINT_FILE):
        print("No checkpoint found yet.")
        return None

    mtime = os.path.getmtime(CHECKPOINT_FILE)
    age = time.time() - mtime
    size_mb = os.path.getsize(CHECKPOINT_FILE) / 1e6

    print(f"Checkpoint: {CHECKPOINT_FILE}")
    print(f"  Size: {size_mb:.1f} MB")
    print(f"  Age: {age:.0f}s ({age/60:.1f} min)")

    try:
        brain = nimcp.Brain("monitor_probe",
                            num_inputs=1024, num_outputs=2048,
                            neuron_count=100, init_mode='fast')
        brain.load(CHECKPOINT_FILE)
        probe = brain.probe()

        print(f"\n  === Brain Probe ===")
        print(f"  Neurons:        {probe.get('num_neurons', '?'):>12,}")
        print(f"  Synapses:       {probe.get('num_synapses', '?'):>12,}")
        print(f"  Active synapses:{probe.get('num_active_synapses', '?'):>12,}")
        print(f"  Accuracy:       {probe.get('accuracy', 0):>12.4f}")
        print(f"  Last loss:      {probe.get('last_loss', 0):>12.6f}")
        print(f"  Gradient norm:  {probe.get('last_gradient_norm', 0):>12.6f}")
        print(f"  EMA loss:       {probe.get('ema_loss', 0):>12.6f}")
        print(f"  Learning rate:  {probe.get('current_learning_rate', 0):>12.6f}")
        print(f"  Learning vel:   {probe.get('learning_velocity', 0):>12.6f}")
        print(f"  Synapse growth: {probe.get('synapse_growth', 0):>12.6f}")
        print(f"  Memory RSS:     {probe.get('memory_rss_bytes', 0)/1e9:>12.2f} GB")
        print(f"  GPU VRAM:       {probe.get('gpu_vram_bytes', 0)/1e9:>12.2f} GB")
        print(f"  Neuron util:    {probe.get('neuron_utilization', 0):>12.4f}")
        print(f"  Immune excepts: {probe.get('immune_total_exceptions', 0):>12}")
        print(f"  Immune inflam:  {probe.get('immune_inflammation', 0):>12.4f}")
        print(f"  Inferences:     {probe.get('total_inferences', 0):>12,}")
        print(f"  Learn steps:    {probe.get('total_learning_steps', 0):>12,}")
        print(f"  Vocab size:     {probe.get('vocabulary_size', 0):>12}")
        print(f"  Response div:   {probe.get('response_diversity', 0):>12.4f}")
        print(f"  GPU available:  {probe.get('gpu_available', False)}")

        # Weight stats
        print(f"\n  === Weight Stats ===")
        print(f"  L2 norm:        {probe.get('weight_l2_norm', 0):>12.4f}")
        print(f"  Mean abs:       {probe.get('weight_mean_abs', 0):>12.6f}")
        print(f"  Max abs:        {probe.get('weight_max_abs', 0):>12.6f}")

        # Learning quality
        print(f"\n  === Learning Quality ===")
        print(f"  Mean label acc: {probe.get('mean_label_accuracy', 0):>12.4f}")
        print(f"  Worst label:    {probe.get('worst_label_accuracy', 0):>12.4f}")
        print(f"  Labels tracked: {probe.get('num_labels_tracked', 0):>12}")
        print(f"  Confidence cal: {probe.get('confidence_calibration', 0):>12.4f}")
        print(f"  Pred entropy:   {probe.get('prediction_entropy', 0):>12.4f}")

        # Layer gradient norms
        grad_norms = probe.get('layer_grad_norms', [])
        if grad_norms:
            print(f"\n  === Layer Gradient Norms ===")
            for i, gn in enumerate(grad_norms):
                bar = '#' * int(min(gn * 100, 50))
                print(f"  Layer {i}: {gn:.6f} {bar}")

        return probe
    except Exception as e:
        print(f"  Failed to probe: {e}")
        return None


def main():
    parser = argparse.ArgumentParser(description="Monitor Athena training")
    parser.add_argument("--watch", action="store_true",
                        help="Continuous monitoring (every 30s)")
    parser.add_argument("--interval", type=int, default=30,
                        help="Watch interval in seconds")
    args = parser.parse_args()

    while True:
        os.system('clear')
        print("=" * 60)
        print("  ATHENA TRAINING MONITOR")
        print(f"  {time.strftime('%Y-%m-%d %H:%M:%S')}")
        print("=" * 60)

        state = load_state()
        if state:
            print(f"\n  Training state:")
            print(f"    Stage: {state.get('stage', '?')}")
            print(f"    Step:  {state.get('step', '?')}")
            print(f"    Time:  {state.get('timestamp', '?')}")

        print()
        probe_checkpoint()

        if not args.watch:
            break

        print(f"\n  Next update in {args.interval}s... (Ctrl+C to exit)")
        try:
            time.sleep(args.interval)
        except KeyboardInterrupt:
            break


if __name__ == "__main__":
    main()
