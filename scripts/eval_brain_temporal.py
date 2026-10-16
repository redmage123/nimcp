#!/usr/bin/env python3
"""Temporal dynamics probe for NIMCP brain.

Complements eval_brain_biological.py (which measures static representation
quality) with metrics that capture how the brain behaves *over time*:

  1. SYNCHRONY       — from get_snn_stats, fraction of neurons firing
                       in the same step. Intrinsic.
  2. FIRING DRIFT    — mean firing rate over N repeated identical probes.
                       Measures whether the SNN's operating point wanders.
  3. TEMPORAL CONSISTENCY  — same stimulus probed with gaps between calls;
                             cosine between outputs. Measures reproducibility
                             across time and intervening activity.
  4. MEMORY PERSISTENCE   — present stimulus A, then probe with NEUTRAL input.
                            Measure how much of A's pattern bleeds into the
                            neutral response. Persistence > baseline means
                            the brain retains short-term context.
  5. SEQUENCE ORDER SENSITIVITY
                    — probe (A then B) vs (B then A) via two sequential
                      submit_multimodal + decide_full pairs. If brain is
                      order-sensitive, the two final states differ.

USAGE (on pod, training paused):
    python3 /workspace/nimcp/scripts/eval_brain_temporal.py

Runtime: ~45 min (23 decide_full calls at ~2 min each + a few get_snn_stats).
"""

import argparse
import os
import sys
import time

sys.path.insert(0, "/workspace/nimcp/scripts"
                if os.path.exists("/workspace/nimcp/scripts")
                else os.path.join(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

from brain_client import BrainProxy
from claude_teacher import encode_text
from talk_to_athena import extract_embedding_from_output


def cos(a, b):
    a = np.asarray(a, dtype=np.float32)
    b = np.asarray(b, dtype=np.float32)
    m = min(len(a), len(b))
    na, nb = np.linalg.norm(a[:m]), np.linalg.norm(b[:m])
    if na < 1e-8 or nb < 1e-8:
        return 0.0
    return float(np.dot(a[:m], b[:m]) / (na * nb))


def probe(b, desc):
    features = encode_text(desc).tolist()
    t0 = time.time()
    resp = b.decide_full(features)
    dt = time.time() - t0
    out = resp.get("output_vector")
    if out is None:
        return None, dt
    return extract_embedding_from_output(np.array(out, dtype=np.float32)), dt


def submit_sensory(b, desc):
    """Fire-and-forget sensory submission. For sequence probing."""
    try:
        b._send({"cmd": "submit_sensory", "modality": "text",
                 "data": encode_text(desc).tolist()})
    except Exception:
        pass


def report_snn_stats(b, label):
    try:
        s = b.get_snn_stats()
        if not s:
            print(f"  [{label}] snn_stats unavailable")
            return None
        print(f"  [{label}] rate={s.get('mean_firing_rate', 0):.2f}Hz  "
              f"max_rate={s.get('max_firing_rate', 0):.2f}Hz  "
              f"sparsity={s.get('sparsity', 0):.3f}  "
              f"synchrony={s.get('synchrony', 0):.3f}  "
              f"silent={s.get('silent_neurons', 0)}  "
              f"hyper={s.get('hyperactive_neurons', 0)}")
        return s
    except Exception as e:
        print(f"  [{label}] snn_stats error: {e}")
        return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--timeout", type=int, default=600)
    args = ap.parse_args()

    b = BrainProxy(timeout=args.timeout)

    # =================================================================
    # Metric 1: SYNCHRONY from get_snn_stats
    # =================================================================
    print("=" * 60)
    print("METRIC 1 — SYNCHRONY (from get_snn_stats)")
    print("=" * 60)
    baseline_stats = report_snn_stats(b, "baseline")

    # =================================================================
    # Metric 2: FIRING DRIFT — repeated identical probes
    # =================================================================
    print()
    print("=" * 60)
    print("METRIC 2 — FIRING DRIFT (stability of operating point)")
    print("=" * 60)
    rates = []
    syncs = []
    for trial in range(3):
        _, dt = probe(b, "a neutral observation of the surroundings")
        s = report_snn_stats(b, f"trial {trial+1} (+{dt:.0f}s)")
        if s:
            rates.append(s.get("mean_firing_rate", 0))
            syncs.append(s.get("synchrony", 0))
    if len(rates) >= 2:
        rate_cv = np.std(rates) / (np.mean(rates) + 1e-8)
        sync_cv = np.std(syncs) / (np.mean(syncs) + 1e-8)
        print(f"\n  Firing-rate CV:  {rate_cv:.4f}  (lower = more stable)")
        print(f"  Synchrony CV:    {sync_cv:.4f}")
        if rate_cv < 0.1:
            print("  ✓ STABLE operating point")
        elif rate_cv < 0.3:
            print("  ~ MODERATE drift")
        else:
            print("  ✗ UNSTABLE — homeostasis not holding")

    # =================================================================
    # Metric 3: TEMPORAL CONSISTENCY — same input across 3 time points
    # =================================================================
    print()
    print("=" * 60)
    print("METRIC 3 — TEMPORAL CONSISTENCY (same input, intervening gap)")
    print("=" * 60)
    consistency_probes = [
        "a soft dog with wagging tail",
        "bright yellow sunlight",
        "a gentle rain on the window",
    ]
    consistency_scores = []
    for desc in consistency_probes:
        # First reading
        out1, dt1 = probe(b, desc)
        # Let the brain run some cortex stimuli in between (simulated via other probe)
        _, _ = probe(b, "a different neutral topic for intervening activity")
        # Second reading on same stimulus
        out2, dt2 = probe(b, desc)
        if out1 is not None and out2 is not None:
            c = cos(out1, out2)
            consistency_scores.append(c)
            print(f"  {desc[:35]:35s} : cos={c:+.4f}  ({dt1+dt2:.0f}s)")
    if consistency_scores:
        mean_cons = np.mean(consistency_scores)
        print(f"\n  Mean temporal consistency: {mean_cons:+.4f}")
        if mean_cons > 0.85:
            print("  ✓ HIGH reliability across time")
        elif mean_cons > 0.6:
            print("  ~ MODERATE reliability")
        else:
            print("  ✗ LOW reliability — outputs drift with time")

    # =================================================================
    # Metric 4: MEMORY PERSISTENCE — stimulus → neutral probe bleed
    # =================================================================
    print()
    print("=" * 60)
    print("METRIC 4 — MEMORY PERSISTENCE (stimulus→neutral bleed)")
    print("=" * 60)
    # Baseline: neutral probe alone, its own cosine to a stimulus
    neutral_desc = "a vast empty white space with nothing happening"
    stim_descs = [("dog_stimulus", "a large friendly dog barking loudly in a field"),
                  ("rain_stimulus", "heavy rain pouring down on a metal roof")]

    # First, what does the neutral probe look like when nothing preceded it?
    neutral_baseline, _ = probe(b, neutral_desc)
    neutral_baseline_stim = probe(b, "a different unrelated stimulus")[0]
    # Baseline drift — two "empty" probes should be similar
    drift = cos(neutral_baseline, neutral_baseline_stim) if neutral_baseline is not None else 0
    print(f"  Neutral baseline drift: cos={drift:+.4f}")

    for name, stim in stim_descs:
        stim_out, _ = probe(b, stim)
        neutral_after, _ = probe(b, neutral_desc)
        if stim_out is None or neutral_after is None:
            continue
        bleed = cos(stim_out, neutral_after)
        bleed_minus_drift = bleed - drift
        print(f"  {name:15s}: stim→neutral cos={bleed:+.4f}  "
              f"(above drift by {bleed_minus_drift:+.4f})")

    print("\n  Interpretation: if stim→neutral cosine is notably ABOVE the")
    print("  baseline drift, the stimulus left a persistent trace in recurrent")
    print("  activity. Higher persistence = stronger working-memory dynamics.")

    # =================================================================
    # Metric 5: SEQUENCE ORDER SENSITIVITY
    # =================================================================
    print()
    print("=" * 60)
    print("METRIC 5 — SEQUENCE ORDER SENSITIVITY")
    print("=" * 60)
    # Probe A-then-B vs B-then-A via sequential submit + decide
    pairs = [
        ("red_ball", "a red ball", "blue_sky", "a blue sky"),
        ("loud_bark", "a loud bark", "soft_purr", "a soft purr"),
    ]
    order_cosines = []
    for name1, d1, name2, d2 in pairs:
        # A then B
        submit_sensory(b, d1)
        ab_out, _ = probe(b, d2)
        # B then A
        submit_sensory(b, d2)
        ba_out, _ = probe(b, d1)
        if ab_out is not None and ba_out is not None:
            c = cos(ab_out, ba_out)
            order_cosines.append(c)
            print(f"  {name1}→{name2}  vs  {name2}→{name1}:  cos={c:+.4f}")
    if order_cosines:
        mean_order = np.mean(order_cosines)
        print(f"\n  Mean (A→B) vs (B→A) cosine: {mean_order:+.4f}")
        if mean_order < 0.8:
            print("  ✓ ORDER-SENSITIVE — different orderings produce distinct states")
        elif mean_order < 0.95:
            print("  ~ WEAKLY order-sensitive")
        else:
            print("  ✗ ORDER-BLIND — sequence information lost")

    # =================================================================
    # Metric 6: FFT, AUTOCORRELATION, CROSS-CORRELATION
    # (uses get_population_history — cheap, no extra decide_full calls)
    # =================================================================
    print()
    print("=" * 60)
    print("METRIC 6 — SPECTRAL / CORRELATION ANALYSIS")
    print("=" * 60)
    # Sample a few populations to analyze. IDs are offsets into the
    # flat population array; 3=first tier pop, and the tiered layout
    # places L1, L3, L5 roughly at these indices for the 1.8M brain.
    target_pops = [
        (3,  "input_0"),
        (10, "L2_pattern_0"),
        (26, "L4_integr_0"),
        (34, "L5_exec_0"),
    ]
    pop_traces = {}
    for pid, name in target_pops:
        try:
            h = b.get_population_history(pid)
            counts = h.get("counts", [])
            if len(counts) < 32:
                print(f"  pop {pid} '{name}': too few samples ({len(counts)}) — skipping")
                continue
            pop_traces[name] = np.asarray(counts, dtype=np.float32)
        except Exception as e:
            print(f"  pop {pid} '{name}': error {e}")
    if not pop_traces:
        print("  No population traces available — skipping spectral analysis")
    else:
        # Per-population autocorrelation and dominant frequency
        print("\n  Per-population FFT + autocorrelation:")
        for name, trace in pop_traces.items():
            n = len(trace)
            # Detrend (subtract mean) so DC doesn't dominate
            trace_c = trace - trace.mean()
            # Autocorrelation: normalized, lags 0..n/4
            ac = np.correlate(trace_c, trace_c, mode='full')[n-1:]
            ac = ac / (ac[0] if ac[0] > 1e-8 else 1.0)
            max_lag = n // 4
            # First zero-crossing and first peak after lag 0
            first_zero = None
            for k in range(1, max_lag):
                if ac[k] <= 0:
                    first_zero = k
                    break
            # FFT → dominant frequency bin
            if trace_c.std() > 1e-8:
                fft = np.abs(np.fft.rfft(trace_c))
                # Skip DC (index 0)
                dom_bin = int(np.argmax(fft[1:]) + 1) if len(fft) > 1 else 0
                # Cycles per SNN step (trace length = n steps)
                dom_freq = dom_bin / n if n > 0 else 0
            else:
                fft = None
                dom_bin = 0
                dom_freq = 0
            mean_spikes = trace.mean()
            std_spikes = trace.std()
            cv = std_spikes / mean_spikes if mean_spikes > 1e-6 else float('inf')
            print(f"    {name:16s}: "
                  f"mean={mean_spikes:7.1f}  std={std_spikes:6.1f}  "
                  f"CV={cv:.2f}  "
                  f"dom_freq={dom_freq:.3f}cyc/step  "
                  f"first_zero_cross_lag={first_zero}")

        # Cross-correlation between adjacent tiers (feedforward lag)
        print("\n  Cross-correlation (population A → B):")
        names = list(pop_traces.keys())
        for i in range(len(names) - 1):
            a = pop_traces[names[i]] - pop_traces[names[i]].mean()
            b_ = pop_traces[names[i+1]] - pop_traces[names[i+1]].mean()
            if a.std() < 1e-6 or b_.std() < 1e-6:
                continue
            a_n = a / (a.std() * len(a))
            b_n = b_ / b_.std()
            xc = np.correlate(a_n, b_n, mode='full')
            lag_axis = np.arange(-len(a_n)+1, len(b_n))
            # Peak lag within ±16 steps
            center = len(a_n) - 1
            window = 16
            sub = xc[center - window:center + window + 1]
            peak_off = int(np.argmax(sub)) - window
            peak_val = float(sub[peak_off + window])
            print(f"    {names[i]:16s} → {names[i+1]:16s}:  "
                  f"peak_lag={peak_off:+d} steps  corr={peak_val:+.3f}")

    # =================================================================
    # Final SNN state
    # =================================================================
    print()
    print("=" * 60)
    print("FINAL STATE")
    print("=" * 60)
    report_snn_stats(b, "final")


if __name__ == "__main__":
    main()
