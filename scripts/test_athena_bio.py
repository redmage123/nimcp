#!/usr/bin/env python3
"""
Athena Biological Metrics Suite

Metrics designed specifically for the NIMCP biological brain model.
Not standard ML metrics — these measure biological health, neural efficiency,
plasticity dynamics, energy conservation, and developmental progress.

Metrics:
  1. Neural Efficiency Index (NEI) — active neuron ratio, firing rate distribution
  2. Synaptic Plasticity Score (SPS) — LTP/LTD balance, STDP event rates
  3. Hamiltonian Conservation Index (HCI) — energy drift in HNN
  4. Temporal Coherence Score (TCS) — LNN state continuity across sequential inputs
  5. Cross-Modal Binding Strength (CMBS) — multimodal vs unimodal response difference
  6. Neuromodulatory Balance Index (NBI) — DA/ACh/5HT/NE ratios
  7. Spike Information Content (SIC) — entropy of SNN spike patterns
  8. Output Effective Dimensionality (OED) — eff_rank of response space
  9. Perceptual Cortex Integration (PCI) — cortex CNN training convergence
  10. Developmental Quotient (DQ) — stage-appropriate capability score

Usage:
  python3 scripts/test_athena_bio.py --daemon
"""

import sys
import os
import json
import time
import argparse
import numpy as np
from datetime import datetime

sys.path.insert(0, os.path.dirname(__file__))


def encode_text(text, encoder=None):
    if encoder:
        return encoder(text)
    np.random.seed(hash(text) & 0xFFFFFFFF)
    return np.random.randn(1024).astype(np.float32).tolist()


class BioMetric:
    def __init__(self, name, value, healthy_range, unit="", details=""):
        self.name = name
        self.value = value
        self.healthy_range = healthy_range  # (low, high) or None
        self.unit = unit
        self.details = details

    @property
    def healthy(self):
        if self.healthy_range is None:
            return True
        lo, hi = self.healthy_range
        return lo <= self.value <= hi

    def __str__(self):
        status = "OK" if self.healthy else "!!"
        range_str = ""
        if self.healthy_range:
            lo, hi = self.healthy_range
            range_str = f" [{lo}-{hi}]"
        return f"  [{status}] {self.name}: {self.value:.4f}{self.unit}{range_str} — {self.details}"


class BioMetricsSuite:
    def __init__(self, brain, encoder=None):
        self.brain = brain
        self.encoder = encoder
        self.metrics = []
        self.history_file = "checkpoints/athena/bio_metrics_history.jsonl"

    def add(self, metric):
        self.metrics.append(metric)
        print(metric)

    def summary(self):
        total = len(self.metrics)
        healthy = sum(1 for m in self.metrics if m.healthy)
        values = [m.value for m in self.metrics if m.healthy_range is not None]
        print(f"\n{'='*60}")
        print(f"  BIOLOGICAL HEALTH: {healthy}/{total} metrics in healthy range")
        if values:
            # Composite biological fitness score (geometric mean of normalized values)
            normalized = []
            for m in self.metrics:
                if m.healthy_range and m.healthy_range[1] > m.healthy_range[0]:
                    lo, hi = m.healthy_range
                    norm = (m.value - lo) / (hi - lo)
                    normalized.append(max(0.0, min(1.0, norm)))
            if normalized:
                geo_mean = np.exp(np.mean(np.log(np.array(normalized) + 1e-8)))
                print(f"  COMPOSITE FITNESS SCORE: {geo_mean:.4f}")
        print(f"{'='*60}")

    def save(self):
        os.makedirs(os.path.dirname(self.history_file), exist_ok=True)
        entry = {
            "timestamp": datetime.now().isoformat(),
            "metrics": [
                {"name": m.name, "value": float(m.value), "healthy": m.healthy,
                 "details": m.details}
                for m in self.metrics
            ],
        }
        with open(self.history_file, "a") as f:
            f.write(json.dumps(entry, default=str) + "\n")
        print(f"  Saved to {self.history_file}")


# ============================================================================
# 1. Neural Efficiency Index (NEI)
# ============================================================================

def metric_neural_efficiency(suite):
    """Measures how efficiently the brain uses its neurons.

    Biological brains are sparse — only 1-10% of neurons fire at any time.
    Too many silent neurons = wasted capacity. Too many active = epileptic.
    """
    print("\n--- 1. Neural Efficiency Index ---")
    brain = suite.brain

    try:
        snn = brain.snn_get_stats()
        probe = brain.probe()
        total_neurons = probe.get("num_neurons", 1)
        utilization = probe.get("neuron_utilization", 0) if probe else 0

        if snn:
            sparsity = snn.get("sparsity", 0)
            silent = snn.get("silent_neurons", 0)
            hyper = snn.get("hyperactive_neurons", 0)
            total_snn = silent + (snn.get("total_spikes", 0) > 0 and 1 or 0)

            # NEI = ratio of "useful" neurons (firing 0.5-30Hz) to total
            active_healthy = max(0, total_snn - silent - hyper) if total_snn > 0 else 0
            nei = active_healthy / max(1, total_snn)
            suite.add(BioMetric("NEI/snn_active_ratio", nei, (0.01, 0.5),
                                details=f"active={active_healthy} silent={silent} hyper={hyper}"))

            # Sparsity should be biological (0.5-0.98)
            suite.add(BioMetric("NEI/sparsity", sparsity, (0.5, 0.98),
                                details=f"fraction of silent neurons"))

            # Firing rate distribution health
            rate = snn.get("mean_firing_rate", 0)
            suite.add(BioMetric("NEI/mean_firing_rate", rate, (0.5, 30.0), "Hz",
                                details=f"biological range is 1-20Hz"))
        else:
            suite.add(BioMetric("NEI/snn_active_ratio", 0.0, (0.01, 0.5),
                                details="SNN not initialized"))

        # Overall network utilization
        suite.add(BioMetric("NEI/neuron_utilization", utilization, (0.01, 0.5),
                            details=f"total neurons={total_neurons:,}"))
    except Exception as e:
        suite.add(BioMetric("NEI/error", 0.0, None, details=str(e)))


# ============================================================================
# 2. Synaptic Plasticity Score (SPS)
# ============================================================================

def metric_synaptic_plasticity(suite):
    """Measures plasticity health — LTP/LTD balance, STDP activity.

    Healthy plasticity: LTP slightly > LTD (net strengthening during learning),
    with active STDP and BCM updates. DA modulation should be non-zero.
    """
    print("\n--- 2. Synaptic Plasticity Score ---")
    brain = suite.brain

    try:
        stats = brain.get_plasticity_stats()
        if not stats:
            suite.add(BioMetric("SPS/active", 0.0, (0.5, 1.0),
                                details="no plasticity stats available"))
            return

        # LTP/LTD balance (should be slightly positive = net strengthening)
        ltp = stats.get("edp_ltp_events", 0)
        ltd = stats.get("edp_ltd_events", 0)
        total_events = ltp + ltd
        if total_events > 0:
            ltp_ratio = ltp / total_events
            suite.add(BioMetric("SPS/ltp_ltd_balance", ltp_ratio, (0.4, 0.7),
                                details=f"LTP={ltp} LTD={ltd} (0.5=balanced, >0.5=net potentiation)"))
        else:
            suite.add(BioMetric("SPS/ltp_ltd_balance", 0.5, (0.4, 0.7),
                                details=f"no LTP/LTD events yet"))

        # STDP update rate
        stdp = stats.get("tpb_stdp_updates", 0)
        bcm = stats.get("tpb_bcm_updates", 0)
        plasticity_updates = stats.get("tpb_plasticity_updates", 0)
        suite.add(BioMetric("SPS/stdp_updates", float(stdp), (0, 1e9),
                            details=f"STDP={stdp} BCM={bcm} total={plasticity_updates}"))

        # Dopamine level (should be > 0 during active learning)
        da = stats.get("dopamine", 0)
        suite.add(BioMetric("SPS/dopamine", da, (0.01, 1.0),
                            details=f"neuromodulatory drive"))

        # RPE (reward prediction error — should fluctuate, not be stuck at 0)
        rpe = stats.get("rpe", 0)
        suite.add(BioMetric("SPS/rpe", abs(rpe), (0.0, 2.0),
                            details=f"RPE={rpe:.4f} (non-zero = active reward learning)"))

        # Plasticity state
        state = stats.get("plasticity_state", "UNKNOWN")
        state_score = 1.0 if state in ("ACQUISITION", "CONSOLIDATION") else 0.0
        suite.add(BioMetric("SPS/state", state_score, (0.5, 1.0),
                            details=f"state={state}"))

    except Exception as e:
        suite.add(BioMetric("SPS/error", 0.0, None, details=str(e)))


# ============================================================================
# 3. Hamiltonian Conservation Index (HCI)
# ============================================================================

def metric_hamiltonian_conservation(suite):
    """Measures energy conservation in the HNN.

    The Störmer-Verlet symplectic integrator should conserve H(q,p).
    Deviation < 5% = excellent, 5-20% = acceptable (learning causes drift),
    > 50% = integrator instability.
    """
    print("\n--- 3. Hamiltonian Conservation Index ---")
    brain = suite.brain

    try:
        nm = brain.get_network_metrics()
        if nm.get("hnn_active"):
            energy = nm.get("hnn_energy", 0)
            deviation = nm.get("hnn_energy_deviation", 1.0)
            initial = nm.get("hnn_initial_energy", 0)

            # HCI = 1 - deviation (1.0 = perfect conservation)
            hci = max(0.0, 1.0 - deviation)
            suite.add(BioMetric("HCI/conservation", hci, (0.5, 1.0),
                                details=f"E={energy:.4f} dev={deviation*100:.1f}% initial={initial:.4f}"))

            # Energy should be finite and non-zero
            suite.add(BioMetric("HCI/energy_magnitude", abs(energy), (0.01, 100.0),
                                details=f"H(q,p) = {energy:.4f}"))

            # Check for energy runaway (|E| growing without bound)
            if abs(initial) > 1e-6:
                energy_ratio = abs(energy / initial)
                suite.add(BioMetric("HCI/energy_stability", energy_ratio, (0.1, 10.0),
                                    details=f"|E/E0| = {energy_ratio:.4f} (1.0 = stable)"))
        else:
            suite.add(BioMetric("HCI/active", 0.0, (0.5, 1.0),
                                details="HNN not active"))
    except Exception as e:
        suite.add(BioMetric("HCI/error", 0.0, None, details=str(e)))


# ============================================================================
# 4. Temporal Coherence Score (TCS)
# ============================================================================

def metric_temporal_coherence(suite):
    """Measures LNN state continuity — similar sequential inputs should
    produce smoothly varying states, not random jumps.
    """
    print("\n--- 4. Temporal Coherence Score ---")
    brain = suite.brain

    try:
        lnn = brain.lnn_get_stats()
        if not lnn:
            suite.add(BioMetric("TCS/active", 0.0, (0.5, 1.0), details="LNN not initialized"))
            return

        # LNN tau (time constant) — should be in biological range
        tau = lnn.get("avg_tau", 0)
        suite.add(BioMetric("TCS/tau", tau, (1.0, 20.0), "s",
                            details=f"biological time constants are 1-20s"))

        # State norm stability — should be bounded, not exploding
        state_norm = lnn.get("state_norm", 0)
        suite.add(BioMetric("TCS/state_norm", state_norm, (0.1, 100.0),
                            details=f"LNN hidden state magnitude"))

        # NaN/Inf check — any numerical instability is critical
        nans = lnn.get("nan_count", 0)
        infs = lnn.get("inf_count", 0)
        numerical_health = 1.0 if (nans == 0 and infs == 0) else 0.0
        suite.add(BioMetric("TCS/numerical_health", numerical_health, (0.5, 1.0),
                            details=f"NaN={nans} Inf={infs}"))

        # Sequential coherence — feed related inputs, check output similarity
        outputs = []
        sequence = ["sunrise", "morning light", "warm sun", "bright afternoon", "sunset"]
        for text in sequence:
            features = encode_text(text, suite.encoder)
            try:
                r = brain.decide_full(features)
                outputs.append(np.array(r.get("output_vector", [])))
            except Exception:
                outputs.append(np.zeros(10))

        # Adjacent outputs should be more similar than distant ones
        if len(outputs) >= 3:
            adjacent_sims = []
            distant_sims = []
            for i in range(len(outputs) - 1):
                ni = np.linalg.norm(outputs[i])
                nj = np.linalg.norm(outputs[i + 1])
                if ni > 1e-8 and nj > 1e-8:
                    adjacent_sims.append(np.dot(outputs[i], outputs[i + 1]) / (ni * nj))
            # Compare first and last
            n0 = np.linalg.norm(outputs[0])
            nL = np.linalg.norm(outputs[-1])
            if n0 > 1e-8 and nL > 1e-8:
                distant_sims.append(np.dot(outputs[0], outputs[-1]) / (n0 * nL))

            avg_adjacent = np.mean(adjacent_sims) if adjacent_sims else 0
            avg_distant = np.mean(distant_sims) if distant_sims else 0
            coherence = avg_adjacent - avg_distant
            suite.add(BioMetric("TCS/sequential_coherence", coherence, (-0.1, 1.0),
                                details=f"adjacent_sim={avg_adjacent:.4f} distant_sim={avg_distant:.4f}"))

    except Exception as e:
        suite.add(BioMetric("TCS/error", 0.0, None, details=str(e)))


# ============================================================================
# 5. Cross-Modal Binding Strength (CMBS)
# ============================================================================

def metric_crossmodal_binding(suite):
    """Measures how well different sensory cortices integrate.

    A multimodal stimulus (visual+audio+touch) should produce a stronger,
    more distinctive response than any single modality alone.
    """
    print("\n--- 5. Cross-Modal Binding Strength ---")
    brain = suite.brain

    try:
        ccm = brain.get_cortex_cnn_metrics()
        active = list(ccm.keys())
        n_active = len(active)

        suite.add(BioMetric("CMBS/active_cortices", float(n_active), (2.0, 4.0),
                            details=f"cortices: {active}"))

        # Training convergence across cortices
        if n_active > 0:
            losses = [ccm[k].get("ema_loss", 0) for k in active]
            avg_loss = np.mean(losses)
            suite.add(BioMetric("CMBS/cortex_avg_loss", avg_loss, (0.0, 5.0),
                                details="per-cortex: " + ", ".join(f"{k}={ccm[k].get('ema_loss',0):.3f}" for k in active)))

            # Forward/backward ratio — should be close to 1.0 (every forward gets trained)
            for k in active:
                fwd = ccm[k].get("forward_steps", 0)
                bwd = ccm[k].get("backward_steps", 0)
                ratio = bwd / max(1, fwd)
                suite.add(BioMetric(f"CMBS/{k}_train_ratio", ratio, (0.5, 1.0),
                                    details=f"fwd={fwd} bwd={bwd}"))

        # Embedding diversity — each cortex should produce distinct embeddings
        if n_active >= 2:
            norms = {k: ccm[k].get("embedding_norm", 0) for k in active}
            unique_norms = len(set(round(n, 1) for n in norms.values()))
            suite.add(BioMetric("CMBS/embedding_diversity", float(unique_norms) / n_active,
                                (0.3, 1.0),
                                details=f"norms: {', '.join(f'{k}={v:.2f}' for k,v in norms.items())}"))

    except Exception as e:
        suite.add(BioMetric("CMBS/error", 0.0, None, details=str(e)))


# ============================================================================
# 6. Neuromodulatory Balance Index (NBI)
# ============================================================================

def metric_neuromodulatory_balance(suite):
    """Measures the balance of the 4 major neuromodulators.

    Healthy brain: all 4 should be non-zero during active learning.
    DA=motivation, ACh=attention, 5HT=mood/inhibition, NE=arousal.
    """
    print("\n--- 6. Neuromodulatory Balance Index ---")
    brain = suite.brain

    try:
        stats = brain.get_plasticity_stats()
        modulators = {}
        if stats:
            modulators = {
                "dopamine": stats.get("dopamine", 0),
                "acetylcholine": stats.get("acetylcholine", 0),
                "serotonin": stats.get("serotonin", 0),
                "norepinephrine": stats.get("norepinephrine", 0),
            }

        for name, level in modulators.items():
            suite.add(BioMetric(f"NBI/{name}", level, (0.0, 1.0),
                                details=f"neuromodulator level"))

        # Balance: all should be active (non-zero)
        active_count = sum(1 for v in modulators.values() if v > 0.01)
        suite.add(BioMetric("NBI/active_count", float(active_count), (2.0, 4.0),
                            details=f"{active_count}/4 modulators active"))

        # Arousal from medulla
        try:
            arousal = brain.medulla_get_arousal()
            suite.add(BioMetric("NBI/arousal", arousal, (0.2, 0.8),
                                details=f"medulla arousal level"))
        except Exception:
            pass

        # Sleep pressure
        try:
            sleep_p = brain.sleep_get_pressure()
            suite.add(BioMetric("NBI/sleep_pressure", sleep_p, (0.0, 0.7),
                                details=f"adenosine accumulation"))
        except Exception:
            pass

    except Exception as e:
        suite.add(BioMetric("NBI/error", 0.0, None, details=str(e)))


# ============================================================================
# 7. Spike Information Content (SIC)
# ============================================================================

def metric_spike_information(suite):
    """Measures information content of SNN spike patterns.

    Run N different inputs, collect spike counts per input.
    High SIC = different inputs produce different spike patterns (information-bearing).
    Low SIC = all inputs produce similar spikes (no information coding).
    """
    print("\n--- 7. Spike Information Content ---")
    brain = suite.brain

    try:
        snn = brain.snn_get_stats()
        if not snn:
            suite.add(BioMetric("SIC/active", 0.0, (0.5, 1.0), details="SNN not initialized"))
            return

        # Collect spike counts across different stimuli
        spike_counts = []
        stimuli = [
            "bright red fire",
            "quiet dark night",
            "loud barking dog",
            "soft flowing water",
            "sharp cold ice",
            "warm fuzzy blanket",
            "fast running cheetah",
            "slow crawling snail",
        ]

        for text in stimuli:
            features = encode_text(text, suite.encoder)
            before = brain.snn_get_stats()
            before_spikes = before.get("total_spikes", 0) if before else 0
            try:
                brain.decide_full(features)
            except Exception:
                pass
            after = brain.snn_get_stats()
            after_spikes = after.get("total_spikes", 0) if after else 0
            delta = after_spikes - before_spikes
            spike_counts.append(max(0, delta))

        # Information content: variance of spike counts (more variety = more info)
        spike_arr = np.array(spike_counts, dtype=np.float32)
        if spike_arr.sum() > 0:
            # Normalized entropy of spike distribution
            probs = spike_arr / spike_arr.sum()
            probs = probs[probs > 0]  # Remove zeros for log
            entropy = -np.sum(probs * np.log2(probs))
            max_entropy = np.log2(len(stimuli))
            normalized_entropy = entropy / max_entropy if max_entropy > 0 else 0
            suite.add(BioMetric("SIC/spike_entropy", normalized_entropy, (0.3, 1.0),
                                details=f"H={entropy:.3f} bits (max={max_entropy:.1f})"))
        else:
            suite.add(BioMetric("SIC/spike_entropy", 0.0, (0.3, 1.0),
                                details="no spikes across stimuli"))

        # Spike variability (coefficient of variation)
        if spike_arr.mean() > 0:
            cv = spike_arr.std() / spike_arr.mean()
            suite.add(BioMetric("SIC/spike_variability", cv, (0.1, 2.0),
                                details=f"CV={cv:.3f} mean={spike_arr.mean():.1f} std={spike_arr.std():.1f}"))
        else:
            suite.add(BioMetric("SIC/spike_variability", 0.0, (0.1, 2.0),
                                details="mean spikes = 0"))

        # Synchrony (should be low — desynchronized is healthy)
        synchrony = snn.get("synchrony", 0)
        suite.add(BioMetric("SIC/desynchrony", 1.0 - synchrony, (0.5, 1.0),
                            details=f"synchrony={synchrony:.4f} (low = healthy)"))

    except Exception as e:
        suite.add(BioMetric("SIC/error", 0.0, None, details=str(e)))


# ============================================================================
# 8. Output Effective Dimensionality (OED)
# ============================================================================

def metric_output_dimensionality(suite):
    """Measures the effective rank of the output space.

    Mode collapse → eff_rank=1. Healthy brain should use many dimensions.
    For 2048-dim output, eff_rank > 10 indicates diverse representations.
    """
    print("\n--- 8. Output Effective Dimensionality ---")
    brain = suite.brain

    try:
        # Collect outputs from diverse stimuli
        stimuli = [
            "red ball", "blue sky", "green tree", "loud thunder", "soft music",
            "cold ice", "hot fire", "sweet honey", "sour lemon", "running fast",
            "sleeping baby", "barking dog", "singing bird", "flowing river",
            "bright star", "dark cave",
        ]
        outputs = []
        for text in stimuli:
            features = encode_text(text, suite.encoder)
            try:
                r = brain.decide_full(features)
                ov = r.get("output_vector", [])
                if ov:
                    outputs.append(ov[:256])  # First 256 dims for speed
            except Exception:
                pass

        if len(outputs) >= 4:
            mat = np.array(outputs)
            # Center the outputs
            mat = mat - mat.mean(axis=0)
            # SVD for effective rank
            try:
                U, S, Vt = np.linalg.svd(mat, full_matrices=False)
                # Effective rank = exp(entropy of normalized singular values)
                S_pos = S[S > 1e-8]
                if len(S_pos) > 0:
                    probs = S_pos / S_pos.sum()
                    entropy = -np.sum(probs * np.log(probs))
                    eff_rank = np.exp(entropy)
                else:
                    eff_rank = 0.0

                suite.add(BioMetric("OED/effective_rank", eff_rank, (3.0, 100.0),
                                    details=f"from {len(outputs)} outputs, top SVs: {S[:5].tolist()}"))

                # Explained variance by top components
                total_var = (S ** 2).sum()
                top1_var = (S[0] ** 2) / total_var if total_var > 0 else 1.0
                suite.add(BioMetric("OED/top1_dominance", 1.0 - top1_var, (0.3, 1.0),
                                    details=f"top1 explains {top1_var*100:.1f}% (lower = more distributed)"))
            except Exception as e:
                suite.add(BioMetric("OED/svd_error", 0.0, None, details=str(e)))
        else:
            suite.add(BioMetric("OED/insufficient_data", 0.0, (3.0, 100.0),
                                details=f"only {len(outputs)} valid outputs"))

    except Exception as e:
        suite.add(BioMetric("OED/error", 0.0, None, details=str(e)))


# ============================================================================
# 9. Perceptual Cortex Integration (PCI)
# ============================================================================

def metric_perceptual_integration(suite):
    """Measures whether all perceptual cortices are training and converging."""
    print("\n--- 9. Perceptual Cortex Integration ---")
    brain = suite.brain

    try:
        ccm = brain.get_cortex_cnn_metrics()
        nm = brain.get_network_metrics()

        expected = ["visual", "audio", "speech", "somato"]
        for cortex in expected:
            if cortex in ccm:
                m = ccm[cortex]
                fwd = m.get("forward_steps", 0)
                bwd = m.get("backward_steps", 0)
                loss = m.get("ema_loss", -1)
                dim = m.get("embedding_dim", 0)
                params = m.get("num_params", 0)

                suite.add(BioMetric(f"PCI/{cortex}_steps", float(fwd), (1.0, 1e9),
                                    details=f"fwd={fwd} bwd={bwd} dim={dim} params={params}"))
                if loss >= 0:
                    suite.add(BioMetric(f"PCI/{cortex}_loss", loss, (0.0, 5.0),
                                        details=f"EMA training loss"))
            else:
                suite.add(BioMetric(f"PCI/{cortex}_active", 0.0, (0.5, 1.0),
                                    details=f"{cortex} cortex not created"))

        # FNO audio (spectral processing)
        fno_steps = nm.get("fno_audio_steps", 0)
        suite.add(BioMetric("PCI/fno_audio", float(fno_steps), (0.0, 1e9),
                            details=f"FNO spectral conv steps={fno_steps}"))

        # Network training balance (all 4 networks should have similar step counts)
        steps = [nm.get(f"{n}_steps", 0) for n in ["ann", "cnn", "snn", "lnn"]]
        if max(steps) > 0:
            balance = min(steps) / max(steps)
            suite.add(BioMetric("PCI/network_balance", balance, (0.5, 1.0),
                                details=f"ANN={steps[0]} CNN={steps[1]} SNN={steps[2]} LNN={steps[3]}"))

    except Exception as e:
        suite.add(BioMetric("PCI/error", 0.0, None, details=str(e)))


# ============================================================================
# 10. Developmental Quotient (DQ)
# ============================================================================

def metric_developmental_quotient(suite):
    """Stage-appropriate capability score.

    Stage 0 (newborn): should discriminate basic stimuli, show attention to faces/voices.
    Measured by response strength to innate-bias stimuli vs neutral stimuli.
    """
    print("\n--- 10. Developmental Quotient ---")
    brain = suite.brain

    try:
        probe = brain.probe()
        total_steps = probe.get("total_learning_steps", 0)
        accuracy = probe.get("accuracy", 0)
        loss = probe.get("ema_loss", 1.0)
        sparsity = probe.get("avg_sparsity", 0)

        # Learning progress (loss should decrease over time)
        # Use 1/(1+loss) as a normalized progress metric
        progress = 1.0 / (1.0 + loss)
        suite.add(BioMetric("DQ/learning_progress", progress, (0.1, 1.0),
                            details=f"loss={loss:.4f} steps={total_steps}"))

        # Innate bias test: face/voice stimuli should produce stronger responses
        # than abstract stimuli (newborn preference)
        innate_stimuli = ["mama's face", "baby voice cooing", "human eyes looking"]
        neutral_stimuli = ["abstract geometry", "random noise pattern", "blank white wall"]

        innate_norms = []
        neutral_norms = []
        for text in innate_stimuli:
            features = encode_text(text, suite.encoder)
            try:
                r = brain.decide_full(features)
                innate_norms.append(np.linalg.norm(r.get("output_vector", [])))
            except Exception:
                innate_norms.append(0)

        for text in neutral_stimuli:
            features = encode_text(text, suite.encoder)
            try:
                r = brain.decide_full(features)
                neutral_norms.append(np.linalg.norm(r.get("output_vector", [])))
            except Exception:
                neutral_norms.append(0)

        avg_innate = np.mean(innate_norms)
        avg_neutral = np.mean(neutral_norms)
        bias_ratio = avg_innate / (avg_neutral + 1e-8)
        suite.add(BioMetric("DQ/innate_bias", bias_ratio, (0.8, 3.0),
                            details=f"innate={avg_innate:.4f} neutral={avg_neutral:.4f} ratio={bias_ratio:.2f}"))

        # Response differentiation — different stimuli should produce different outputs
        all_outputs = []
        test_stimuli = ["red ball", "dog barking", "cold water", "warm blanket", "bright light"]
        for text in test_stimuli:
            features = encode_text(text, suite.encoder)
            try:
                r = brain.decide_full(features)
                all_outputs.append(np.array(r.get("output_vector", [])))
            except Exception:
                all_outputs.append(np.zeros(10))

        cos_sims = []
        for i in range(len(all_outputs)):
            for j in range(i + 1, len(all_outputs)):
                ni = np.linalg.norm(all_outputs[i])
                nj = np.linalg.norm(all_outputs[j])
                if ni > 1e-8 and nj > 1e-8:
                    cos_sims.append(np.dot(all_outputs[i], all_outputs[j]) / (ni * nj))
        avg_cos = np.mean(cos_sims) if cos_sims else 1.0
        differentiation = 1.0 - avg_cos  # Higher = more differentiated
        suite.add(BioMetric("DQ/response_differentiation", differentiation, (0.05, 1.0),
                            details=f"avg_cos={avg_cos:.4f} (lower = more distinct responses)"))

        # Network sparsity health
        suite.add(BioMetric("DQ/network_sparsity", sparsity, (0.3, 0.95),
                            details=f"avg activation sparsity"))

    except Exception as e:
        suite.add(BioMetric("DQ/error", 0.0, None, details=str(e)))


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="Athena Biological Metrics")
    parser.add_argument("--daemon", action="store_true", help="Connect to daemon")
    parser.add_argument("--checkpoint", type=str, default=None)
    args = parser.parse_args()

    if args.daemon:
        from brain_client import BrainProxy
        brain = BrainProxy()
        print(f"Connected to daemon (neurons={brain.get_neuron_count():,})")
    elif args.checkpoint:
        import nimcp
        brain = nimcp.Brain.load(args.checkpoint)
    else:
        try:
            from brain_client import BrainProxy
            brain = BrainProxy()
            print(f"Connected to daemon (neurons={brain.get_neuron_count():,})")
        except Exception:
            import nimcp
            brain = nimcp.Brain("test", neuron_count=1000, num_inputs=64,
                               num_outputs=64, init_mode="full")
            brain.enable_multi_network()

    encoder = None
    try:
        from sentence_transformers import SentenceTransformer
        model = SentenceTransformer("BAAI/bge-large-en-v1.5")
        encoder = lambda text: model.encode(text, normalize_embeddings=True).tolist()
        print("Using BERT encoder")
    except Exception:
        print("Using hash-based encoder")

    suite = BioMetricsSuite(brain, encoder)
    print(f"\n{'='*60}")
    print(f"  ATHENA BIOLOGICAL METRICS — {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"{'='*60}")

    metric_neural_efficiency(suite)
    metric_synaptic_plasticity(suite)
    metric_hamiltonian_conservation(suite)
    metric_temporal_coherence(suite)
    metric_crossmodal_binding(suite)
    metric_neuromodulatory_balance(suite)
    metric_spike_information(suite)
    metric_output_dimensionality(suite)
    metric_perceptual_integration(suite)
    metric_developmental_quotient(suite)

    suite.summary()
    suite.save()


if __name__ == "__main__":
    sys.exit(main())
