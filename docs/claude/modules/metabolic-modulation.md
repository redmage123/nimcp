# Metabolic Modulation

**Last Updated**: 2026-03-11

## Overview

Shared utilities for ATP/fatigue-based modulation across substrate bridges, plus a tensor-based batch API for SIMD-accelerated multi-region processing.

**Header**: `include/cognitive/common/nimcp_metabolic_modulation.h`
**Source**: `src/cognitive/common/nimcp_metabolic_modulation.c`

## Neuromodulator System

6 neuromodulators defined in `include/plasticity/neuromodulators/nimcp_neuromodulators.h`:

| Neuromodulator | Enum | Biological Mapping |
|----------------|------|-------------------|
| Dopamine | `NEUROMOD_DOPAMINE` | VTA/SNc -> striatum, cortex (reward) |
| Serotonin | `NEUROMOD_SEROTONIN` | Raphe nuclei -> widespread (mood) |
| Acetylcholine | `NEUROMOD_ACETYLCHOLINE` | Basal forebrain -> cortex, hippocampus (attention) |
| Norepinephrine | `NEUROMOD_NOREPINEPHRINE` | Locus coeruleus -> cortex (arousal) |
| GABA | `NEUROMOD_GABA` | Fast inhibition |
| Glutamate | `NEUROMOD_GLUTAMATE` | Fast excitation |

Two-level architecture: global broadcast (volume transmission) + local synapse-specific receptor densities.

## Neuromodulator Integration

- 50 neuromodulation pools for main brain, 10 for secondary networks
- 500K synapses in main brain neuromod tensors
- Per-accessor functions: `neuromodulator_pool_get_dopamine/serotonin/acetylcholine/norepinephrine()`
- Neuromod bridges: sensory, emotion, WM, executive, attention, plasticity, gametheory, reasoning, memory (11+ inter-module bridges in `include/integration/inter/neuromod_*/`)
- Sleep/wake modulation of metabolic state
- Plasticity coordinator energy cost tracking per mechanism

## Scalar API

### Core Types

- `metabolic_input_t`: `atp_level` + `metabolic_capacity` (both [0,1])
- `metabolic_effect_multipliers_t`: 4 multipliers (atp_primary, atp_secondary, fatigue_primary, fatigue_secondary)
- `metabolic_modulation_config_t`: enable flags, sensitivity, min_capacity, multipliers
- `metabolic_effects_t`: 4 effect values + overall_capacity

### Key Functions

```c
metabolic_modulation_config_t metabolic_modulation_default_config(void);
int metabolic_compute_effects(input, config, effects);  // Returns 0/-1
float metabolic_compute_atp_effect(atp_level, sensitivity, multiplier, min_capacity);
float metabolic_compute_fatigue_effect(capacity, sensitivity, multiplier, min_capacity);
void metabolic_effects_init_full(effects);  // Sets all to 1.0
```

### Default Values

| Parameter | Default |
|-----------|---------|
| `atp_primary_mult` | 1.0 |
| `atp_secondary_mult` | 1.1 |
| `fatigue_primary_mult` | 1.0 |
| `fatigue_secondary_mult` | 0.9 |
| `min_capacity` | 0.2 |
| `atp/fatigue_sensitivity` | 1.0 |

## Tensor-Based Batch API

For SIMD-accelerated processing across multiple brain regions:

- `metabolic_effects_tensor_t`: Shape [batch, 5] or [5]
- `metabolic_input_tensor_t`: Separate atp_levels and metabolic_capacities tensors
- `metabolic_multipliers_tensor_t`: Per-region multipliers [batch, 4]
- `metabolic_compute_effects_tensor()`: Batch compute all regions

When `batch_size == 1`, tensors use rank-1 shape `[5]` instead of `[1, 5]`.

## Bridges Using This API

Grief, Joy (custom 0.95 fatigue multiplier), Knowledge, Personality, Salience (and others).

## GOTCHAs

- `metabolic_compute_effects()` returns `0` for success, `-1` for errors (NOT NIMCP error codes)
- Always call `metabolic_effects_init_full()` before `metabolic_compute_effects()`
- Effects are clamped to `[min_capacity, 1.0]`, not `[0, 1]`
- Pass `NULL` for multipliers to use defaults (not an empty struct)
- `metabolic_effects_tensor_get()` returns `NAN` on error
- Batch size must match between input and effects tensors
