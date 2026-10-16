# Eligibility Phase 4/5 Wiring Diagram

**Version**: 1.0.0
**Date**: 2026-01-12
**Status**: Integrated with KG Wiring System

## Overview

This document describes the wiring architecture for Phase 4 (Utils) and Phase 5 (Quantum) eligibility modules in the NIMCP plasticity subsystem.

## Module Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         ELIGIBILITY TRACE SYSTEM                             │
│                              (Phase 3-5)                                     │
└─────────────────────────────────────────────────────────────────────────────┘

                           ┌─────────────────────┐
                           │  ELIGIBILITY_TRACE  │
                           │     (0x040B)        │
                           │  ─────────────────  │
                           │  Core eligibility   │
                           │  Temporal credit    │
                           │  3-factor learning  │
                           └──────────┬──────────┘
                                      │
           ┌──────────────────────────┼──────────────────────────┐
           │                          │                          │
           ▼                          ▼                          ▼
┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
│  ELIGIBILITY_UTILS  │    │ ELIGIBILITY_QUANTUM │    │  INTEGRATION        │
│      (0x0440)       │    │      (0x0441)       │    │  BRIDGES            │
│  ─────────────────  │    │  ─────────────────  │    │  ─────────────────  │
│  PHASE 4            │    │  PHASE 5            │    │  PR     (0x0443)   │
│  • Memory pools     │    │  • QMC credit       │    │  FEP    (0x0444)   │
│  • Metrics          │    │  • Q-annealing      │    │  Pink   (0x0445)   │
│  • RK4 integration  │    │  • Q-walk           │    │  Sleep  (0x0446)   │
│  • Shannon analysis │    │  • Q-Shannon        │    │                     │
└──────────┬──────────┘    └──────────┬──────────┘    └─────────────────────┘
           │                          │
           │    ┌─────────────────────┘
           │    │
           ▼    ▼
    ┌─────────────────────────────────┐
    │   ELIGIBILITY_UTILS_QUANTUM     │
    │          (0x0442)               │
    │   ─────────────────────────     │
    │   PHASE 4-5 BIDIRECTIONAL       │
    │   • Forward triggers            │
    │   • Backward feedback           │
    │   • Coherence tracking          │
    │   • Stability monitoring        │
    └─────────────────────────────────┘
```

## Bidirectional Data Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    BIDIRECTIONAL COMMUNICATION FLOW                          │
└─────────────────────────────────────────────────────────────────────────────┘

  PHASE 4 (Utils)                    BRIDGE                    PHASE 5 (Quantum)
  ═══════════════                    ══════                    ═════════════════

┌─────────────────┐            ┌─────────────────┐            ┌─────────────────┐
│                 │  FORWARD   │                 │  FORWARD   │                 │
│  Metrics        │ ─────────► │  Trigger        │ ─────────► │  QMC Credit     │
│  Collection     │            │  Evaluation     │            │  Assignment     │
│                 │            │                 │            │                 │
│  Pool Stats     │ ─────────► │  LTP/LTD        │ ─────────► │  Q-Annealing    │
│                 │            │  Imbalance      │            │  Optimization   │
│                 │            │                 │            │                 │
│  Bottleneck     │ ─────────► │  Pool           │ ─────────► │  Q-Walk         │
│  Detection      │            │  Pressure       │            │  Diffusion      │
│                 │            │                 │            │                 │
│  RK4/Adaptive   │            │  Bottleneck     │ ─────────► │  Q-Shannon      │
│  Integration    │            │  Escalation     │            │  Analysis       │
└─────────────────┘            └─────────────────┘            └─────────────────┘
        ▲                              │                              │
        │                              │                              │
        │         BACKWARD             │          BACKWARD            │
        │        ◄─────────────────────┴──────────────────────────────┘
        │
        │   ┌─────────────────────────────────────────────────────────┐
        │   │  BACKWARD FEEDBACK TYPES:                               │
        │   │  • Credit fractions → Update metrics histograms         │
        │   │  • Optimized params → Adjust RK4 timestep/tolerance     │
        │   │  • Diffused priorities → Set pool allocation            │
        │   │  • Resolution results → Clear bottleneck state          │
        └───┴─────────────────────────────────────────────────────────┘
```

## Message Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           MESSAGE TYPES (0x02A0-0x02AE)                      │
└─────────────────────────────────────────────────────────────────────────────┘

Phase 4 Utils Messages (0x02A0-0x02A3):
├── BIO_MSG_ELIG_UTILS_METRICS_UPDATE    (0x02A0) - Metrics snapshot
├── BIO_MSG_ELIG_UTILS_POOL_STATS        (0x02A1) - Pool utilization
├── BIO_MSG_ELIG_UTILS_BOTTLENECK_DETECTED (0x02A2) - Shannon bottleneck
└── BIO_MSG_ELIG_UTILS_RK4_STEP          (0x02A3) - Integration step

Phase 5 Quantum Messages (0x02A4-0x02A7):
├── BIO_MSG_ELIG_QUANTUM_CREDIT_ASSIGNED (0x02A4) - QMC results
├── BIO_MSG_ELIG_QUANTUM_ANNEAL_STATE    (0x02A5) - Annealing progress
├── BIO_MSG_ELIG_QUANTUM_WALK_DIFFUSION  (0x02A6) - Walk diffusion
└── BIO_MSG_ELIG_QUANTUM_BOTTLENECK_RESOLVED (0x02A7) - Q-Shannon fix

Phase 4-5 Bridge Messages (0x02A8-0x02AB):
├── BIO_MSG_ELIG_UQ_FORWARD_TRIGGER      (0x02A8) - Utils→Quantum trigger
├── BIO_MSG_ELIG_UQ_BACKWARD_FEEDBACK    (0x02A9) - Quantum→Utils feedback
├── BIO_MSG_ELIG_UQ_COHERENCE_UPDATE     (0x02AA) - Coherence metric
└── BIO_MSG_ELIG_UQ_STABILITY_UPDATE     (0x02AB) - Stability metric

Integration Messages (0x02AC-0x02AE):
├── BIO_MSG_ELIG_PR_CONSOLIDATION_GATE   (0x02AC) - PR memory gating
├── BIO_MSG_ELIG_FEP_PREDICTION_ERROR    (0x02AD) - FEP integration
└── BIO_MSG_ELIG_SLEEP_CONSOLIDATION     (0x02AE) - Sleep consolidation
```

## Module Dependencies

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        DEPENDENCY GRAPH (TOPOLOGICAL)                        │
└─────────────────────────────────────────────────────────────────────────────┘

Level 0 (Base):
    ┌──────────────────┐
    │ eligibility_trace │  ← Core eligibility trace module
    └────────┬─────────┘
             │
Level 1 (Phase 4/5):
             ├──────────────────┬──────────────────┬─────────────────┐
             ▼                  ▼                  ▼                 ▼
    ┌────────────────┐ ┌─────────────────┐ ┌────────────┐ ┌────────────┐
    │eligibility_utils│ │eligibility_quantum│ │eligibility_pr│ │eligibility_fep│
    └────────┬───────┘ └────────┬────────┘ └────────────┘ └────────────┘
             │                  │
Level 2 (Bridge):              │
             └──────────┬──────┘
                        ▼
            ┌───────────────────────┐
            │eligibility_utils_quantum│  ← Bidirectional bridge
            └───────────────────────┘
```

## Trigger Conditions

| Trigger Type | Condition | Quantum Action |
|--------------|-----------|----------------|
| LTP/LTD Imbalance | Ratio < 0.3 or > 3.0 | Quantum annealing to rebalance |
| Pool Pressure | Utilization > 90% | Quantum walk for priority allocation |
| Bottleneck | Info deficit > 60% | Quantum-Shannon deep analysis |
| History Ready | > 100 samples | Initialize quantum walk conditions |
| Latency Spike | > 1ms avg latency | Parameter optimization |

## Feedback Types

| Feedback | Source | Utils Update |
|----------|--------|--------------|
| Optimized params | Annealing | Adjust RK4 timestep and tolerance |
| Credit fractions | QMC | Update metrics histograms |
| Diffused priorities | Quantum walk | Set pool allocation priorities |
| Resolution results | Quantum-Shannon | Clear bottleneck state |

## Bio-Async Module IDs

| Module | ID | Description |
|--------|-----|-------------|
| BIO_MODULE_ELIGIBILITY_TRACE | 0x040B | Core eligibility trace |
| BIO_MODULE_ELIGIBILITY_UTILS | 0x0440 | Phase 4: Utils integration |
| BIO_MODULE_ELIGIBILITY_QUANTUM | 0x0441 | Phase 5: Quantum eligibility |
| BIO_MODULE_ELIGIBILITY_UTILS_QUANTUM | 0x0442 | Phase 4-5 bidirectional bridge |
| BIO_MODULE_ELIGIBILITY_PR | 0x0443 | Eligibility-PR memory bridge |
| BIO_MODULE_ELIGIBILITY_FEP | 0x0444 | Eligibility-FEP bridge |
| BIO_MODULE_ELIGIBILITY_PINK_NOISE | 0x0445 | Eligibility-pink noise bridge |
| BIO_MODULE_ELIGIBILITY_SLEEP | 0x0446 | Eligibility-sleep bridge |

## Integration with External Systems

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     EXTERNAL SYSTEM INTEGRATION                              │
└─────────────────────────────────────────────────────────────────────────────┘

                    ┌─────────────────┐
                    │  NEUROMODULATORS │
                    │    (0x0409)      │
                    └────────┬────────┘
                             │
              ┌──────────────┼──────────────┐
              │              │              │
              ▼              ▼              ▼
     ┌────────────┐  ┌─────────────┐  ┌────────────┐
     │ ELIG_UTILS │  │ELIG_QUANTUM │  │   STDP     │
     └─────┬──────┘  └──────┬──────┘  └────────────┘
           │                │
           └────────┬───────┘
                    ▼
          ┌─────────────────────┐
          │ ELIG_UTILS_QUANTUM  │
          └─────────┬───────────┘
                    │
    ┌───────────────┼───────────────┬───────────────┐
    ▼               ▼               ▼               ▼
┌────────┐   ┌───────────┐   ┌───────────┐   ┌───────────┐
│  PR    │   │    FEP    │   │   SLEEP   │   │   PINK    │
│ MEMORY │   │  LEARNING │   │  SYSTEM   │   │   NOISE   │
└────────┘   └───────────┘   └───────────┘   └───────────┘
```

## Platform Tier Requirements

| Module | Min Tier | Notes |
|--------|----------|-------|
| eligibility_trace | MINIMAL (0) | Core functionality |
| eligibility_utils | MINIMAL (0) | Basic utils always available |
| eligibility_quantum | MEDIUM (1) | Requires additional compute |
| eligibility_utils_quantum | MEDIUM (1) | Bridge requires quantum module |
| eligibility_pr | MINIMAL (0) | PR integration |
| eligibility_fep | MINIMAL (0) | FEP integration |
| eligibility_pink_noise | MINIMAL (0) | Pink noise integration |
| eligibility_sleep | MINIMAL (0) | Sleep integration |

## JSONL Wiring Location

```
.aim/wiring/subsystems/plasticity.jsonl
```

## Related Documentation

- [KG Wiring System](../claude/modules/kg-wiring.md)
- [Eligibility Trace API](../api/eligibility-trace.md)
- [Bio-Async Messaging](../claude/modules/bio-async.md)
