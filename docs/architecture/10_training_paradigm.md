# Training Paradigm

**Last Updated:** 2026-04-19

This document describes how Athena trains: the loop structure, biological
stability mechanisms, and recent additions.

## Sequential Training (batch=1) — Non-Negotiable

Every attempt to batch or parallelize training has produced:
- Cross-network gradient amplification → explosion
- SNN input current summation → saturation
- Homeostasis calibration mismatch → oscillation

**The biological stability package assumes per-sample temporal sequencing.**
Changing this requires a multi-week rewrite (see
[session_roadmap.md](../plans/session_roadmap.md) Phase 4.1).

Safer speedup paths that preserve sequentiality:
- GPU memory residency (done — Phase A.1)
- Shorter SNN simulation window
- Curiosity-driven sample selection (done — Phase A.3)
- Progressive curriculum (done — Phase A.4)
- Synthesized richer per-exposure data (done — Phase B.2)
- Gradient-accumulated LR smoothing (done — Phase B.3)

## The Training Loop

Entry point: `scripts/immerse_athena.py` → `run_stage_1()` → for-loop over
stimuli.

Per-step order:
```
1. Clock tick (may trigger sleep/consolidate)
2. Stimulus selection
   ├── Curiosity selector (if gaps reported)      # NEW, Phase A.3
   └── Curriculum-bounded sampler                 # NEW, Phase A.4
3. Synthesized sensory enrichment                 # NEW, Phase B.2
   → multi-channel tuple (visual, text, haptic, gaze, audio, emotion)
4. Joint-attention boost (LGN/MGN attention +50%) # earlier session
5. brain.learn_vector(features, target, label)
6. Symbolic layer update                          # NEW, Phase B.1
   ├── KG assertion
   ├── Hippocampal episode
   └── Semantic memory node
7. Restore attention                              # earlier session
8. RPE → dopamine reward (if loss dropped)        # earlier session
9. Gradient accumulator.record(loss, grad_norm)   # NEW, Phase B.3
10. Every 50 steps:
    ├── report training metrics
    ├── Stage gate check (if step ≥ 100)         # earlier session
    └── checkpoint save
11. Every 200 steps: aggressive sleep cycle      # earlier session
```

## Stage Gate — All-Metrics-Pass

`scripts/stage_gate.py` — prevents premature stage transition.

Six criteria that must all pass, three consecutive checks:
1. Minimum step count (≥1500 for Stage 1)
2. Mean loss < 1.0 over 200-step window
3. Loss plateau (Δ<5% across two successive 200-step windows)
4. ≥48/50 non-zero outputs in last 50 steps
5. SNN sparsity in 0.88-0.99 (firing 1-12%)
6. Chat eval: coherence >0.30, similarity >0.20, cross-sim <0.90

Rationale: transitioning to Stage 2 on a weak Stage 1 foundation compounds
errors. Better to block indefinitely than advance unready.

## Biological Stability Package (Schema v6-v7)

Five mechanisms running in parallel per sample, calibrated for sequential
training:

| Mechanism | Timescale | Purpose |
|-----------|-----------|---------|
| Synaptic scaling (Turrigiano) | minutes-hours | Pull firing rate toward 3% target via weight scaling |
| Intrinsic plasticity | seconds | Per-neuron threshold adaptation |
| Short-term depression | ms | Prevent bursts from saturating |
| Inhibitory plasticity | seconds-minutes | Separate E/I plasticity rules |
| Metabolic budget | per-update | Cap `sum|weights|` per neuron |

Emergency homeostatic range: ±10% weight scaling when firing >3× or <1/3
target. (Currently calibrated at ±10% — documented as possibly too aggressive
in [session_roadmap.md](../plans/session_roadmap.md).)

**Schema v7 (2026-04-19)**: per-neuron homeostatic state now persisted in
`.snn` files — no longer reset every brain restart.

## Additional Per-Step Fixes (2026-04)

### RPE → Dopamine Reward
When loss drops vs 50-step rolling mean by >20%, fire positive reward via
`brain.bg_update_reward(reward, expected)`. When loss spikes >50%, fire
negative. Closes the prediction-success → dopamine loop that was missing.

### Joint Attention During Naming
Before `parent.show_and_name()`, boost LGN (visual) to 1.5×, MGN (auditory)
to 1.2×. Restore to 1.0 after. Simulates caregiver pointing + naming.

### Aggressive Sleep Consolidation
Every 200 training steps: `brain.sleep_run_cycle()` +
`brain.world_model_dream(horizon=5)`. Pulls hippocampal→cortical transfer
into the default training cadence instead of leaving it to incidental idle.

## Throughput Profile

**Current apportionment (defaults):**
- ANN: 150K neurons (teacher)
- SNN: 1.8M neurons (primary learner)
- LNN: 512 (temporal)

Per-step breakdown (pre-Phase A.1):
```
learn_vector step N: total=3549ms (ann=630 plast=0 secondary=2916 cognitive=2)
```
- ANN (150K neurons): 630ms (18%)
- SNN secondary (isyn + LIF + readback over 1.8M): 2916ms (82%)

SNN `isyn` alone was 1.3-1.5s, dominated by PCIe transfer of ~12 GB static
CSR data every step. **Phase A.1 eliminates that** by making CSR arrays
GPU-resident — expected SNN step drop to ~100-200ms, overall step to
~1-1.5s.

The 1.8M SNN / 150K ANN ratio reflects the architectural decision that
**SNN is the primary learner** — ANN exists to provide gradient signal
during supervised training, not as the final representation substrate.

## Stage Advancement Criteria

Stage 0 → 1: after stage0_stimuli exposures (default 20,000)
Stage 1 → 2: **all stage gate criteria pass 3× consecutively** (new)
Stage 2 → 3: analogous gate (planned)

The older "fixed stimulus count" advancement is still the fallback ceiling
— 40,000 default per stage. The gate can release earlier once criteria met.

## Future Work

- Compressed-time sequential replay between wake steps (Phase D.1)
- Symbolic consultation during inference (Phase D.2)
- Reconstructive episodic recall (Phase D.3)
- Innate priors expansion (Phase E.1)
- Full batch-safe stability rewrite (Phase 4.1, deferred pending Phases A-E)
