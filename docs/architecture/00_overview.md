# Athena Architecture — Overview

**Version:** 0.9.0-beta (tag 2.7.0)
**Last Updated:** 2026-04-23

## Recent release — 2.7.0 (2026-04-23)

The F-campaign (F1–F9) and G-campaign (G1–G8) activated previously-dormant
biological subsystems at the hot-path level:

- **Glial cells** (astrocytes / oligodendrocytes / microglia) are now
  created during brain init and modulate synaptic transmission every
  forward pass. See [../claude/modules/glial.md](../claude/modules/glial.md).
- **Neural substrate + thalamic router** are created at init and attached
  to every SNN / LNN / cortex-CNN network. Phase 1-4 adapters that read
  substrate state are no longer dormant. See
  [../claude/modules/substrate.md](../claude/modules/substrate.md).
- **SNN cognitive bridges** (attention, mirror-neurons, emotion,
  working-memory) migrated off dense pops to lightweight CSR.
- **synapse_id** widened uint32 → uint64 (bijective at 2M neuron scale).
- **SIGPIPE SIG_IGN** installed in `nimcp_init_internal` — daemon no
  longer silent-dies when a socket peer closes.
- **Thalamic router queue** bumped 1000 → 16384 + new backpressure API
  (`thalamic_router_queue_usage` / `_is_under_pressure`).
- **RTX 5090 (Blackwell, compute_120)** enabled when CUDA ≥ 12.8.

Full release notes: [`../../CHANGELOG.md`](../../CHANGELOG.md#270---2026-04-23).

Athena is a biologically-plausible hybrid brain simulator. This document
summarizes the top-level architecture as it stands after the 2026-04
training-fix and test-infrastructure work.

## Substrate Layers (top to bottom)

```
┌─────────────────────────────────────────────────────────────────┐
│  Python API layer                                                │
│    • scripts/brain_daemon.py   — RPC server                      │
│    • scripts/brain_client.py   — BrainProxy + _call              │
│    • scripts/immerse_athena.py — developmental training driver   │
│    • scripts/run_full_battery.py — cognitive+safety battery      │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────────────────────────────────────────┐
│  Python bindings (src/bindings/python/nimcp_python.c)            │
│    • 200+ Brain_* methods                                         │
│    • Mental health, emotion, introspection, perturb, confidence  │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────────────────────────────────────────┐
│  Cognitive modules (60+)                                         │
│    • Introspection, curiosity, consolidation                     │
│    • Reasoning, abduction, inner speech, inner dialogue          │
│    • Mental health monitor, emotional system                     │
│    • Ethics, LGSS, safety audit log                              │
└─────────────────────────────────────────────────────────────────┘
                              ▲
┌─────────────────────────────────────────────────────────────────┐
│  Six neural networks (cooperating via bridges)                   │
│    • ANN   150,000 neurons      — teacher/backbone (adaptive_network_t)
│    • SNN   1,800,000 neurons    — primary learner (snn_network_s)
│    • LNN   512 neurons          — temporal/ODE, adjoint-scaled  │
│    • CNN   ~1.8M params (4 per-cortex) — CONV not neurons        │
│    • HNN   on LNN layer 0       — energy-conserving wrapper      │
│    • FNO   spectral wrapper      — Fourier operator on SNN pops  │
│                                                                  │
│    Total biological substrate: ~1,950,000 neurons               │
│    (SNN dominates; ANN is 150K teacher; LNN is 512 liquid cells)│
└─────────────────────────────────────────────────────────────────┘

### Network Neuron Apportionment (defaults as of 2026-04)

These are the defaults in `scripts/brain_daemon.py`:

| Network | Count | Purpose |
|---------|------:|---------|
| ANN (adaptive) | **150,000** | Teacher/backbone, gradient-descent learner |
| SNN (spiking)  | **1,800,000** | Primary learner; hierarchical populations |
| LNN (liquid)   | **512** | Temporal integration; O(n²) adjoint forces small size |
| CNN (visual)   | ~600K params | Per-cortex (4 cortices) — params not neurons |
| HNN            | — | Hamiltonian wrapper on LNN layer 0 |
| FNO            | — | Fourier operator per SNN population |

Rationale:
- **SNN primary (1.8M)**: biological plausibility; R-STDP, homeostasis
- **ANN teacher (150K)**: provides gradient signal for supervised learning
- **LNN tiny (512)**: adjoint ODE integration is O(n²) — sweet spot
- **CNN no-neuron-count**: convolutional; measured in params ~1.8M total
- **HNN, FNO**: structural wrappers, not independent neuron pools

Override via CLI: `--snn-neuron-count N --lnn-neuron-count N`. Brain
daemon defaults defined as `DEFAULT_ANN_NEURONS`, `DEFAULT_SNN_NEURONS`,
`DEFAULT_LNN_NEURONS` constants.
                              ▲
┌─────────────────────────────────────────────────────────────────┐
│  GPU substrate (CUDA)                                            │
│    • src/gpu/snn/nimcp_snn_kernels.cu                            │
│    • src/gpu/cnn/, src/gpu/tensor/, etc.                         │
│    • Persistent GPU-resident CSR storage (V2, 2026-04)           │
└─────────────────────────────────────────────────────────────────┘
```

## Key Invariants

1. **Sequential training (batch=1) is required.** Parallel/batched training
   triggers gradient explosion and SNN saturation in the biological stability
   package. See `docs/architecture/10_training_paradigm.md`.

2. **All safety systems are non-removable.** Ethics, LGSS, and tamper-resistant
   audit log cannot be disabled via configuration.

3. **SNN state includes per-neuron homeostatic fields** (threshold_offset,
   neuron_rate_ema, depression). These are persisted in `.snn` files as of
   schema v7. See `docs/architecture/20_snn.md`.

4. **GPU memory lifecycle**: SNN CSR storage can be GPU-resident (V2).
   See `docs/architecture/30_gpu_memory.md`.

## Where to Read Next

- [10_training_paradigm.md](10_training_paradigm.md) — training loop, gates, homeostasis
- [20_snn.md](20_snn.md) — SNN internals, populations, CSR storage
- [30_gpu_memory.md](30_gpu_memory.md) — GPU memory management
- [40_cognitive_layers.md](40_cognitive_layers.md) — cognitive module map
- [50_safety.md](50_safety.md) — safety stack and audit log
- [60_test_infrastructure.md](60_test_infrastructure.md) — tests, batteries, regression

## Recent Changes (2026-04)

| Change | Files | Docs |
|--------|-------|------|
| SNN schema v6 → v7 (homeostatic persistence) | `src/snn/nimcp_snn_network.c` | [20_snn.md](20_snn.md) |
| SNN GPU persistent CSR | `src/snn/nimcp_snn_synapse.{h,c}` | [30_gpu_memory.md](30_gpu_memory.md) |
| RPE → dopamine reward | `scripts/immerse_athena.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Joint-attention boost | `scripts/immerse_athena.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Aggressive sleep consolidation | `scripts/immerse_athena.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Stage gate (all-metrics-pass) | `scripts/stage_gate.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Cognitive & safety battery | `scripts/test_harness/`, `scripts/tests/`, `data/stimuli/` | [60_test_infrastructure.md](60_test_infrastructure.md) |
| Curiosity-driven selector | `scripts/curiosity_selector.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Progressive curriculum | `scripts/curriculum.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Symbolic writer | `scripts/symbolic_writer.py` | [40_cognitive_layers.md](40_cognitive_layers.md) |
| Synthesized sensory | `scripts/synthesized_sensory.py` | [10_training_paradigm.md](10_training_paradigm.md) |
| Gradient accumulator | `scripts/gradient_accumulator.py` | [10_training_paradigm.md](10_training_paradigm.md) |
