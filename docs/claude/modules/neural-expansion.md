# Neural Expansion & Contraction — Runtime Brain Resizing

**Status**: Design Document
**Date**: 2026-03-20

## Overview

Runtime brain resizing: grow a trained 1M brain to 2M neurons (or shrink it back)
without destroying learned representations. Both directions must work.

**Key principle**: new neurons integrate gradually (biological neurogenesis model);
removed neurons transfer knowledge before dying (graceful degradation).

---

## Architecture

### The Resize Operation

```
nimcp_brain_resize(brain, new_neuron_count, resize_config)
```

Three modes:
- **EXPAND**: Add neurons to existing layers (proportional to diamond shape)
- **CONTRACT**: Remove lowest-activity neurons, consolidate knowledge
- **REBALANCE**: Redistribute neurons across layers without changing total

### Diamond Layer Redistribution

Current diamond architecture sizes layers proportionally. On resize, maintain
the same ratios:

```
7-layer diamond (100K+ hidden):
  Layer 1:  ~2%   entry projection
  Layer 2: ~12%   feature extraction
  Layer 3: ~36%   lower representation (peak)
  Layer 4: ~36%   upper representation (peak)
  Layer 5: ~12%   compression
  Layer 6:  ~2%   exit projection
  I/O layers: unchanged (num_inputs / num_outputs fixed)

Resize 1M → 2M:
  Each hidden layer doubles proportionally.
  Input/output layer sizes stay the same.
```

---

## Expansion (Growing the Brain)

### Phase 1: Allocate

1. **Grow neuron array**
   - `realloc(network->neurons, new_capacity * sizeof(neuron_t))`
   - Zero-initialize new slots
   - Update `network->capacity` and per-layer counts
   - All existing neuron pointers invalidated — must use indices only

2. **Grow bulk pools**
   - `spike_history_bulk`: realloc, reassign pointers for new neurons
   - `activity_history_bulk`: same
   - `bcm_pool`, `eligibility_pool`: grow if needed
   - `embedding_pool`: grow pinned CPU buffer

3. **Grow layer metadata**
   - Update `layer_sizes[]` array (realloc if num_layers unchanged)
   - Update `layer_offsets[]` (cumulative sums)
   - Grow `layer_norm_gamma/beta` arrays
   - Grow `residual_saved_states` arrays

4. **Grow GPU weight cache**
   - Rebuild `sparse_weights[]` CSR matrices for affected layers
   - Resize `biases[]` and `activations[]` tensors
   - Invalidate `connected_dst` (rebuilt on next forward)
   - Free and reallocate gradient accumulators

### Phase 2: Wire

New neurons start with connections sampled from the existing weight distribution:

```c
typedef struct {
    float initial_weight_scale;    // 0.01 — start weak
    uint32_t fan_in_target;        // 128 — match MIN_FAN_IN
    uint32_t fan_out_target;       // 128
    wiring_strategy_t strategy;    // RANDOM, ACTIVITY_BASED, NEIGHBOR
    float maturation_steps;        // 1000 — steps before full influence
} expansion_wiring_config_t;
```

**Wiring strategies**:
- `RANDOM`: Connect to random existing neurons in adjacent layers
- `ACTIVITY_BASED`: Preferentially connect to high-activity neurons
  (piggyback on learned representations)
- `NEIGHBOR`: Connect to neurons with similar layer position
  (locality-preserving expansion)

### Phase 3: Integrate (Maturation)

New neurons go through a maturation period (leveraging existing neurogenesis module):

```
PROGENITOR (steps 0-100)
  - Silent: output multiplied by 0.0
  - Receives input, accumulates statistics
  - STDP active but weight updates scaled by 0.1x

IMMATURE (steps 100-500)
  - Output multiplied by maturity factor (0.0 → 0.5)
  - Full STDP, learning rate 0.5x existing
  - Activity-dependent survival check at step 500

INTEGRATING (steps 500-1000)
  - Output multiplied by maturity factor (0.5 → 1.0)
  - Full learning rate
  - Synaptic competition with existing neurons

MATURE (steps 1000+)
  - Fully integrated, no special treatment
  - Subject to normal plasticity and pruning
```

**Existing neuron protection during expansion**:
- Learning rate for existing neurons reduced to 0.1x during maturation
- Existing weights frozen for first 100 steps (PROGENITOR phase)
- Gradually restore to full LR as new neurons mature

---

## Contraction (Shrinking the Brain)

### Phase 1: Select Neurons for Removal

Score every neuron by importance:

```c
float neuron_importance(neuron_t* n, neural_network_t net) {
    float activity = mean(n->activity_history);           // How active
    float connectivity = n->outgoing.count + n->incoming.count;
    float weight_magnitude = sum_abs(outgoing_weights);   // Signal strength
    float uniqueness = 1.0 - max_correlation_with_peers;  // Redundancy check

    return activity * 0.3 + connectivity * 0.2
         + weight_magnitude * 0.3 + uniqueness * 0.2;
}
```

Remove neurons with lowest importance scores, per-layer (maintain diamond ratios).
Never remove neurons in input or output layers.

### Phase 2: Knowledge Transfer (Graceful Degradation)

Before removing a neuron, transfer its knowledge:

1. **Weight absorption**: For each outgoing connection from dying neuron D
   to surviving neuron S, redistribute D's incoming signal:
   ```
   For each neuron P that connects to D:
     If P also connects to S:
       S.weight[P] += D.weight_out[S] * D.weight_in[P] / S.weight_out_sum
     Else:
       Create new connection P→S with weight = D.weight_out[S] * D.weight_in[P]
   ```

2. **Bias compensation**: Adjust bias of downstream neurons to compensate
   for lost input:
   ```
   For each downstream neuron S:
     S.bias += D.weight_out[S] * mean(D.activity_history)
   ```

3. **Gradual fade-out** (optional, for smooth transition):
   - Reduce dying neuron's output by 10% per step over 10 steps
   - Allows downstream neurons to adapt gradually

### Phase 3: Compact

1. **Remove neurons**: Zero out neuron slots, disconnect all synapses
2. **Compact arrays**: Slide neurons down to fill gaps, update all indices
   - OR: mark as dead and skip in forward/backward (lazy compaction)
3. **Shrink allocations**: realloc neuron array, bulk pools, GPU cache
4. **Update layer metadata**: Adjust layer_sizes, layer_offsets

### Lazy vs Eager Compaction

**Lazy** (recommended for frequent small contractions):
- Mark neurons as `DEAD` (new flag in neuron_t)
- Skip in forward/backward loops
- Compact only when dead fraction > 10%
- Avoids index remapping on every removal

**Eager** (for large one-time contractions):
- Compact immediately
- Remap all synapse target_neuron_id references
- Rebuild GPU weight cache
- Cleaner memory profile but O(N) remapping cost

---

## API Design

```c
/* ============================================================================
 * Core Resize API
 * ============================================================================ */

typedef enum {
    NIMCP_RESIZE_EXPAND,       // Add neurons
    NIMCP_RESIZE_CONTRACT,     // Remove neurons
    NIMCP_RESIZE_REBALANCE     // Redistribute without changing total
} nimcp_resize_mode_t;

typedef enum {
    NIMCP_WIRE_RANDOM,         // Random connections to adjacent layers
    NIMCP_WIRE_ACTIVITY,       // Prefer high-activity targets
    NIMCP_WIRE_NEIGHBOR        // Locality-preserving
} nimcp_wiring_strategy_t;

typedef enum {
    NIMCP_COMPACT_LAZY,        // Mark dead, compact later
    NIMCP_COMPACT_EAGER        // Compact immediately
} nimcp_compact_mode_t;

typedef struct {
    nimcp_resize_mode_t mode;
    uint32_t target_neuron_count;        // Desired total after resize

    /* Expansion options */
    float initial_weight_scale;          // Default: 0.01
    uint32_t maturation_steps;           // Default: 1000
    nimcp_wiring_strategy_t wiring;      // Default: ACTIVITY
    uint32_t fan_in_target;              // Default: 128 (MIN_FAN_IN)
    float existing_lr_scale;             // LR multiplier for existing neurons during maturation

    /* Contraction options */
    nimcp_compact_mode_t compaction;     // Default: LAZY
    bool enable_knowledge_transfer;      // Default: true
    uint32_t fadeout_steps;              // Default: 10 (0 = instant removal)
    float min_importance_threshold;      // Don't remove above this score

    /* Common */
    bool preserve_io_layers;             // Default: true (never resize I/O)
    bool rebuild_gpu_cache;              // Default: true
    float diamond_ratio_tolerance;       // Default: 0.05 (5% deviation OK)
} nimcp_resize_config_t;

nimcp_resize_config_t nimcp_resize_config_default(void);

/**
 * @brief Resize a brain's neural network at runtime.
 *
 * Expansion: allocates new neurons, wires them weakly, starts maturation.
 * Contraction: scores neurons by importance, transfers knowledge, removes.
 * Thread-safe: acquires network mutex for the duration.
 *
 * @param brain    Brain handle
 * @param config   Resize configuration
 * @return NIMCP_SUCCESS, NIMCP_ERROR_NO_MEMORY, NIMCP_ERROR_INVALID
 */
nimcp_status_t nimcp_brain_resize(nimcp_brain_t brain,
                                   const nimcp_resize_config_t* config);

/**
 * @brief Query resize feasibility without executing.
 * Reports estimated memory delta, affected layers, neuron counts.
 */
nimcp_status_t nimcp_brain_resize_check(nimcp_brain_t brain,
                                         const nimcp_resize_config_t* config,
                                         nimcp_resize_report_t* report);

/**
 * @brief Get maturation progress after expansion.
 * @return fraction of new neurons that have reached MATURE stage (0.0-1.0)
 */
float nimcp_brain_get_maturation_progress(nimcp_brain_t brain);

/* Python binding */
// brain.resize(target_neurons=2000000, mode="expand", wiring="activity")
// brain.resize(target_neurons=500000, mode="contract", transfer=True)
// brain.resize_check(target_neurons=2000000)  # dry run
// brain.maturation_progress()  # 0.0 - 1.0
```

---

## Implementation Plan

### Phase 1: Core resize (expand only)
1. `nimcp_brain_resize()` with EXPAND mode
2. Neuron array realloc + bulk pool growth
3. Layer size redistribution (diamond ratios)
4. Random wiring for new neurons
5. GPU cache rebuild
6. Unit tests: resize 100→200, verify forward pass works

### Phase 2: Maturation system
1. Wire neurogenesis module to neural network
2. Maturation state machine (PROGENITOR→MATURE)
3. Output scaling by maturity factor
4. Existing neuron LR protection
5. Tests: verify new neurons gradually integrate

### Phase 3: Contraction
1. Neuron importance scoring
2. Knowledge transfer (weight absorption + bias compensation)
3. Lazy compaction with dead neuron skip
4. Eager compaction with index remapping
5. Tests: contract 200→100, verify accuracy retention

### Phase 4: GPU + persistence
1. GPU weight cache incremental rebuild (not full rebuild)
2. Checkpoint save/load with resize metadata
3. Mixed expansion+contraction (rebalance mode)
4. Python bindings

---

## Memory Budget

| Neurons | RAM (est.) | VRAM (est.) | Notes |
|---------|-----------|-------------|-------|
| 500K    | ~8 GB     | ~5 GB       | Comfortable on current HW |
| 1M      | ~16 GB    | ~10 GB      | Current config |
| 1.5M    | ~24 GB    | ~14 GB      | Tight on 20GB GPU |
| 2M      | ~32 GB    | ~16.5 GB    | OOM risk on RTX 4000 SFF |

**RTX 4000 SFF Ada**: 20 GB VRAM → practical max ~1.5M neurons with training
**System RAM**: 62 GB → practical max ~2M neurons (with swap pressure)

Contraction is the safer scaling direction on current hardware.

---

## Integration with Existing Systems

### Neurogenesis Module
- Bridge `nimcp_neurogenesis_create_neuron()` → `neural_network_add_neuron()`
- Use niche system for layer targeting (HIPPOCAMPAL → middle layers,
  CORTICAL → upper layers)
- Activity-dependent survival already implemented — wire to importance scoring

### Checkpoint Persistence
- Save resize history in checkpoint metadata
- On load, restore maturation state for neurons still integrating
- Contraction state (dead neurons awaiting compaction) must survive save/load

### Training Pipeline
- During maturation: skip new neurons in batch loss computation
  (they'd add noise to gradient signal)
- After maturation: include normally
- Contraction during training: pause training, contract, resume

### Synaptogenesis
- Existing synaptogenesis module handles new connection formation
- Wire expansion's fan_in/fan_out targets through synaptogenesis config
- Small-world and feedback connections added post-wiring

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Pointer invalidation after realloc | SIGSEGV | Use indices everywhere, never cache neuron_t* |
| GPU cache stale after resize | Wrong gradients | Force `weights_dirty_on_cpu = true`, rebuild |
| Knowledge loss on contraction | Accuracy drop | Knowledge transfer + gradual fadeout |
| OOM during expansion | Crash | `resize_check()` dry run before commit |
| Maturation disrupts training | Loss spike | LR protection for existing neurons |
| Index remapping bugs | Corruption | Eager compaction only with full validation pass |
| Checkpoint incompatibility | Can't resume | Version tag in checkpoint, migration path |
