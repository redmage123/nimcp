#!/usr/bin/env python3
"""
Athena Hyperparameter Auto-Tuner — Dynamic Multi-Parameter Optimization

Monitors training metrics and dynamically adjusts hyperparameters
without restarting training. Communicates via /tmp/athena_* files
that the training script checks every 50 steps.

Parameters controlled:
  - Learning rate (LR): plateau escape, spike recovery, convergence
  - K-WTA sparsity: output activation density
  - Diversity loss weight: anti-mode-collapse
  - Gradient clip norm: gradient explosion/vanishing
  - Weight decay: regularization strength
  - Output LR boost: output layer learning rate multiplier
  - Dropout rate: regularization
  - Temperature: output distribution sharpness

Usage:
    python3 hyperparameter_auto_tuner.py
    python3 hyperparameter_auto_tuner.py --dry-run
    python3 hyperparameter_auto_tuner.py --interval 90 --aggressive
"""

import os
import re
import sys
import json
import time
import math
import argparse
from collections import deque

# === File-based communication with training script ===
PARAM_FILES = {
    'lr':              '/tmp/athena_lr',
    'sparsity':        '/tmp/athena_sparsity',
    'diversity_weight': '/tmp/athena_diversity_weight',
    'grad_clip':       '/tmp/athena_grad_clip',
    'weight_decay':    '/tmp/athena_weight_decay',
    'output_lr_boost': '/tmp/athena_output_lr_boost',
    'dropout':         '/tmp/athena_dropout',
    'temperature':     '/tmp/athena_temperature',
}

LOG_FILE = "logs/training.log"
MONITOR_LOG = "logs/collapse_monitor.log"
STATE_FILE = "/tmp/athena_tuner_state.json"

# === Default ranges ===
PARAM_RANGES = {
    'lr':              (1e-6, 1e-2),
    'sparsity':        (0.02, 0.30),    # 2% to 30% active neurons
    'diversity_weight': (0.0, 1.0),
    'grad_clip':       (0.1, 100.0),
    'weight_decay':    (0.0, 1e-3),
    'output_lr_boost': (1.0, 50.0),
    'dropout':         (0.0, 0.5),
    'temperature':     (0.1, 5.0),
}


class MetricsCollector:
    """Parse training logs for metrics."""

    def __init__(self, log_file=LOG_FILE, monitor_log=MONITOR_LOG):
        self.log_file = log_file
        self.monitor_log = monitor_log

    def get_loss_history(self, max_entries=50):
        """Extract (step, loss) from training log."""
        entries = []
        try:
            with open(self.log_file) as f:
                for line in f.readlines()[-500:]:
                    m = re.search(r'\[(\d+)\]\s+loss=([\d.]+)\s+\((\d+)/(\d+)\s+non-zero\)', line)
                    if m:
                        entries.append({
                            'step': int(m.group(1)),
                            'loss': float(m.group(2)),
                            'nonzero': int(m.group(3)),
                            'total': int(m.group(4)),
                        })
                    # Also capture LR
                    m2 = re.search(r'lr=([\d.]+)', line)
                    if m2 and entries:
                        entries[-1]['lr'] = float(m2.group(1))
                    # SNN stats
                    m3 = re.search(r'SNN:(\d+)spk/([\d.]+)Hz', line)
                    if m3 and entries:
                        entries[-1]['snn_spikes'] = int(m3.group(1))
                        entries[-1]['snn_hz'] = float(m3.group(2))
        except Exception:
            pass
        return entries[-max_entries:]

    def get_output_stats(self):
        """Get output sparsity and std from collapse monitor."""
        try:
            with open(self.monitor_log) as f:
                lines = f.readlines()
            for line in reversed(lines[-20:]):
                m = re.search(r'\[(\d+)stp\]\s+nonzero=(\d+)/(\d+).*std=([\d.]+).*ann=([\d.]+)', line)
                if m:
                    return {
                        'step': int(m.group(1)),
                        'nonzero': int(m.group(2)),
                        'total': int(m.group(3)),
                        'sparsity': int(m.group(2)) / max(int(m.group(3)), 1),
                        'std': float(m.group(4)),
                        'ann_loss': float(m.group(5)),
                    }
        except Exception:
            pass
        return None


class TrainingState:
    """Track training state for decision making."""

    def __init__(self):
        self.loss_history = deque(maxlen=20)
        self.sparsity_history = deque(maxlen=10)
        self.std_history = deque(maxlen=10)
        self.adjustments = []  # Log of all adjustments made
        self.last_adjustment_step = 0
        self.cooldown_steps = 300

    def update(self, entries, output_stats):
        for e in entries:
            self.loss_history.append(e)
        if output_stats:
            self.sparsity_history.append(output_stats['sparsity'])
            self.std_history.append(output_stats['std'])

    def current_step(self):
        return self.loss_history[-1]['step'] if self.loss_history else 0

    def in_cooldown(self):
        return (self.current_step() - self.last_adjustment_step) < self.cooldown_steps

    def record_adjustment(self, param, old_val, new_val, reason):
        self.adjustments.append({
            'step': self.current_step(),
            'param': param,
            'old': old_val,
            'new': new_val,
            'reason': reason,
            'time': time.strftime('%H:%M:%S'),
        })
        self.last_adjustment_step = self.current_step()

    def save(self):
        try:
            with open(STATE_FILE, 'w') as f:
                json.dump({
                    'last_step': self.current_step(),
                    'last_adjustment': self.last_adjustment_step,
                    'num_adjustments': len(self.adjustments),
                    'recent_adjustments': self.adjustments[-10:],
                }, f, indent=2)
        except Exception:
            pass


class HyperparameterTuner:
    """Multi-parameter auto-tuner."""

    def __init__(self, aggressive=False, dry_run=False):
        self.aggressive = aggressive
        self.dry_run = dry_run
        self.collector = MetricsCollector()
        self.state = TrainingState()
        if aggressive:
            self.state.cooldown_steps = 150

    def analyze_and_adjust(self):
        """Main analysis loop — check all parameters."""
        entries = self.collector.get_loss_history()
        output_stats = self.collector.get_output_stats()
        self.state.update(entries, output_stats)

        if len(self.state.loss_history) < 4:
            return "insufficient_data", []

        adjustments = []

        # 1. Learning Rate
        lr_adj = self._check_lr()
        if lr_adj:
            adjustments.append(lr_adj)

        # 2. K-WTA Sparsity
        sparsity_adj = self._check_sparsity(output_stats)
        if sparsity_adj:
            adjustments.append(sparsity_adj)

        # 3. Diversity Loss Weight
        div_adj = self._check_diversity(output_stats)
        if div_adj:
            adjustments.append(div_adj)

        # 4. Gradient Clip Norm
        clip_adj = self._check_grad_clip()
        if clip_adj:
            adjustments.append(clip_adj)

        # 5. Output LR Boost
        boost_adj = self._check_output_boost(output_stats)
        if boost_adj:
            adjustments.append(boost_adj)

        # Apply (respect cooldown — only apply most important)
        if adjustments and not self.state.in_cooldown():
            # Sort by priority (first adjustment is highest priority)
            adj = adjustments[0]
            self._apply(adj['param'], adj['value'], adj['reason'])
            return adj['state'], adjustments
        elif adjustments:
            return "cooldown", adjustments

        return "healthy", []

    def _check_lr(self):
        """Check if LR needs adjustment."""
        losses = [e['loss'] for e in self.state.loss_history]
        if len(losses) < 6:
            return None

        recent = losses[-6:]
        mean_loss = sum(recent) / len(recent)
        loss_range = max(recent) - min(recent)
        relative_range = loss_range / max(mean_loss, 1e-10)

        first_half = sum(recent[:3]) / 3
        second_half = sum(recent[3:]) / 3

        # Plateau: <5% variation over 6 reports
        if relative_range < 0.05:
            current_lr = self._read_param('lr') or 0.00003
            factor = 3.0 if self.aggressive else 2.0
            new_lr = min(current_lr * factor, PARAM_RANGES['lr'][1])
            return {
                'param': 'lr', 'value': new_lr,
                'state': 'plateau',
                'reason': f'Loss plateau at {mean_loss:.0f} ({relative_range:.1%} variation) '
                          f'→ LR {current_lr:.6f} → {new_lr:.6f} ({factor}x)'
            }

        # Spike: last loss >5x previous
        if len(losses) >= 2 and losses[-1] / max(losses[-2], 1e-10) > 5.0:
            current_lr = self._read_param('lr') or 0.00003
            new_lr = max(current_lr * 0.5, PARAM_RANGES['lr'][0])
            return {
                'param': 'lr', 'value': new_lr,
                'state': 'spike',
                'reason': f'Loss spike {losses[-2]:.0f} → {losses[-1]:.0f} '
                          f'→ LR {current_lr:.6f} → {new_lr:.6f} (0.5x)'
            }

        # Divergence: increasing over 6 reports
        if second_half > first_half * 1.3:
            current_lr = self._read_param('lr') or 0.00003
            new_lr = max(current_lr * 0.3, PARAM_RANGES['lr'][0])
            return {
                'param': 'lr', 'value': new_lr,
                'state': 'divergence',
                'reason': f'Loss diverging {first_half:.0f} → {second_half:.0f} '
                          f'→ LR {current_lr:.6f} → {new_lr:.6f} (0.3x)'
            }

        return None

    def _check_sparsity(self, output_stats):
        """Check if K-WTA sparsity needs adjustment."""
        if not output_stats:
            return None

        # output_stats['sparsity'] is actually density (nonzero/total)
        # 1.0 = fully dense (all neurons active), 0.05 = 5% active
        density = output_stats['sparsity']

        # For regression tasks with dense output (>90% active), sparsity
        # adjustment is not applicable — the output SHOULD be dense.
        if density > 0.90:
            return None  # Dense output is correct for regression

        # Too sparse (<8% active): network can't differentiate — increase
        if density < 0.08:
            current = self._read_param('sparsity') or 0.05
            new_val = min(current * 1.5, 0.20)
            if new_val > current * 1.1:
                return {
                    'param': 'sparsity', 'value': new_val,
                    'state': 'too_sparse',
                    'reason': f'Output density {density:.1%} too low → '
                              f'target {current:.2f} → {new_val:.2f}'
                }

        # Moderately sparse (8-25%): acceptable range for classification
        # Too dense (>25%) for classification — decrease
        if density > 0.25 and density < 0.90:
            current = self._read_param('sparsity') or 0.05
            new_val = max(current * 0.7, 0.05)
            return {
                'param': 'sparsity', 'value': new_val,
                'state': 'too_dense',
                'reason': f'Output density {density:.1%} too high for classification → '
                          f'target {current:.2f} → {new_val:.2f}'
            }

        return None

    def _check_diversity(self, output_stats):
        """Check if diversity loss weight needs adjustment."""
        if not output_stats:
            return None

        std = output_stats['std']

        # Low std = outputs too similar = need diversity pressure
        if std < 5.0 and len(self.state.std_history) >= 3:
            recent_std = list(self.state.std_history)[-3:]
            if all(s < 5.0 for s in recent_std):
                current = self._read_param('diversity_weight') or 0.1
                new_val = min(current + 0.1, 0.5)
                return {
                    'param': 'diversity_weight', 'value': new_val,
                    'state': 'low_diversity',
                    'reason': f'Output std={std:.1f} (low diversity) → '
                              f'diversity_weight {current:.2f} → {new_val:.2f}'
                }

        # High std = good diversity, can reduce pressure
        if std > 30.0:
            current = self._read_param('diversity_weight') or 0.1
            if current > 0.05:
                new_val = max(current - 0.05, 0.0)
                return {
                    'param': 'diversity_weight', 'value': new_val,
                    'state': 'good_diversity',
                    'reason': f'Output std={std:.1f} (good diversity) → '
                              f'diversity_weight {current:.2f} → {new_val:.2f}'
                }

        return None

    def _check_grad_clip(self):
        """Check gradient clip based on loss dynamics."""
        losses = [e['loss'] for e in self.state.loss_history]
        if len(losses) < 4:
            return None

        # Very high loss variance = possible gradient explosion
        mean_loss = sum(losses[-4:]) / 4
        max_loss = max(losses[-4:])
        if max_loss > mean_loss * 3 and mean_loss > 1000:
            current = self._read_param('grad_clip') or 1.0
            new_val = max(current * 0.5, 0.1)
            if new_val < current * 0.9:
                return {
                    'param': 'grad_clip', 'value': new_val,
                    'state': 'gradient_explosion',
                    'reason': f'High loss variance (max={max_loss:.0f}, mean={mean_loss:.0f}) → '
                              f'grad_clip {current:.1f} → {new_val:.1f}'
                }

        return None

    def _check_output_boost(self, output_stats):
        """Check if output layer needs more/less LR boost."""
        if not output_stats:
            return None

        # If sparsity is stuck at minimum AND loss is plateaued,
        # the output layer needs more boost
        losses = [e['loss'] for e in self.state.loss_history]
        if len(losses) < 6:
            return None

        recent = losses[-6:]
        relative_range = (max(recent) - min(recent)) / max(sum(recent) / len(recent), 1e-10)
        sparsity = output_stats['sparsity']

        density = output_stats['sparsity']  # Actually density (nonzero/total)
        if relative_range < 0.05 and density < 0.08:
            current = self._read_param('output_lr_boost') or 10.0
            new_val = min(current * 1.5, 50.0)
            if new_val > current * 1.1:
                return {
                    'param': 'output_lr_boost', 'value': new_val,
                    'state': 'output_stuck',
                    'reason': f'Output layer stuck (sparsity={sparsity:.1%}, plateau) → '
                              f'output_lr_boost {current:.0f} → {new_val:.0f}'
                }

        return None

    def _read_param(self, name):
        """Read current param value from override file."""
        path = PARAM_FILES.get(name)
        if path and os.path.exists(path):
            try:
                with open(path) as f:
                    return float(f.read().strip())
            except Exception:
                pass
        return None

    def _apply(self, param, value, reason):
        """Write parameter override."""
        path = PARAM_FILES.get(param)
        if not path:
            return

        old_val = self._read_param(param)

        if self.dry_run:
            print(f"  [DRY RUN] {param}: {old_val} → {value}")
            print(f"  Reason: {reason}")
        else:
            with open(path, 'w') as f:
                f.write(f"{value:.8f}\n")
            print(f"  ADJUSTED: {param} → {value:.6f}")
            print(f"  Reason: {reason}")

        self.state.record_adjustment(param, old_val, value, reason)
        self.state.save()


def main():
    parser = argparse.ArgumentParser(description="Multi-parameter auto-tuner")
    parser.add_argument("--interval", type=int, default=120, help="Check interval (seconds)")
    parser.add_argument("--dry-run", action="store_true", help="Monitor only")
    parser.add_argument("--aggressive", action="store_true", help="Shorter cooldown, bigger adjustments")
    parser.add_argument("--once", action="store_true", help="Run once and exit")
    args = parser.parse_args()

    tuner = HyperparameterTuner(aggressive=args.aggressive, dry_run=args.dry_run)

    print("=" * 60)
    print("  Athena Hyperparameter Auto-Tuner")
    print("=" * 60)
    print(f"  Mode: {'DRY RUN' if args.dry_run else 'ACTIVE'}")
    print(f"  Aggressive: {args.aggressive}")
    print(f"  Interval: {args.interval}s")
    print(f"  Cooldown: {tuner.state.cooldown_steps} steps")
    print(f"  Parameters monitored:")
    for param, (lo, hi) in PARAM_RANGES.items():
        current = tuner._read_param(param)
        status = f"{current:.6f}" if current else "default"
        print(f"    {param:20s} [{lo:.6f} - {hi:.6f}] current: {status}")
    print()

    try:
        while True:
            state, adjustments = tuner.analyze_and_adjust()

            step = tuner.state.current_step()
            losses = [e['loss'] for e in tuner.state.loss_history]
            current_loss = losses[-1] if losses else 0

            print(f"[{time.strftime('%H:%M:%S')}] Step {step} | Loss {current_loss:.0f} | State: {state}")

            if adjustments:
                for adj in adjustments:
                    marker = ">>>" if adj == adjustments[0] and state not in ('cooldown', 'healthy') else "   "
                    print(f"  {marker} {adj['param']}: {adj['reason']}")
            else:
                print(f"  All parameters healthy")

            output_stats = tuner.collector.get_output_stats()
            if output_stats:
                print(f"  Output: {output_stats['sparsity']:.1%} sparse, "
                      f"std={output_stats['std']:.1f}")

            print()

            if args.once:
                break
            time.sleep(args.interval)

    except KeyboardInterrupt:
        print("\nAuto-tuner stopped.")
        active = [p for p, path in PARAM_FILES.items() if os.path.exists(path)]
        if active:
            print(f"Active overrides: {', '.join(active)}")
            print(f"Remove all: rm /tmp/athena_*")


if __name__ == "__main__":
    main()
