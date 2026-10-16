# Brain Immune System

Biologically-inspired immune coordination layer integrating BBB, BFT, and swarm immune.

## Components
B Cells, T Helper (CD4+), T Killer (CD8+), Antibodies, Cytokines, Inflammation

## Key Features
- Auto-learning: Neutralization converts B cells to memory
- Fuzzy affinity: 3-component matching (exact 50%, bit 30%, length 20%)
- Cross-reactive immunity at 70% threshold
- Antigen detection for anomalies (NaN, crash patterns, instability)

## B Cell State Progression
**B cells must be in PLASMA state to produce antibodies.**
```
NAIVE -> ACTIVATED -> PLASMA
```
Use `brain_immune_t_help_b()` to advance state.

## Thread-Safe Antigen Access
- `brain_immune_get_antigen_copy()` -- thread-safe struct copy under mutex (PREFERRED)
- `brain_immune_get_antigen()` -- returns dangling pointer (DEPRECATED)
- 6 callers migrated away from dangling pointer API

## Health Monitoring
- Health agent macros: `NIMCP_DECLARE_HEALTH_AGENT_ATOMIC()` for per-module health monitoring
- STDP health metrics: weight divergence/saturation/oscillation/NaN/collapse anomaly detection

## Integration
- Training-immune bridge for optimizer integration
- Portia integration for resource-aware degradation
- Idle gating to skip immune processing when system is stable

## Test Coverage: 104 tests
