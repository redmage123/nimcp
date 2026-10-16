# Axon Integration Architecture

## Overview

This document describes how the axon module integrates with neurons, synapses, and the brain network, with forward compatibility for future dendrite and myelin sheath modules.

## Integration Status: MINIMAL (Option A)

**Current Phase**: Data structure integration only
**Future Phase**: Full spike routing with dendrites and myelin sheaths

## Data Structure Changes

### 1. Neuron Structure (`neuron_t`)

**Location**: `src/core/neuralnet/nimcp_neuralnet.h`

**New Field**:
```c
typedef struct neuron_struct {
    // ... existing fields ...

    // Axon integration - Signal propagation with realistic conduction delays
    uint32_t axon_id;  /**< Axon ID for this neuron's output (0 = no axon, direct connection) */
} neuron_t;
```

**Semantics**:
- `axon_id = 0`: **Legacy mode** - Direct synaptic connection, zero delay (backward compatible)
- `axon_id != 0`: **Axon mode** - Spike propagates through axon with realistic delay

**Future Extension**:
```c
// Future: Dendrite integration
uint32_t* dendrite_ids;     // Array of dendrite IDs for input pathways
uint32_t num_dendrites;     // Number of dendrites
```

---

### 2. Synapse Structure (`synapse_t`)

**Location**: `src/core/neuralnet/nimcp_neuralnet.h`

**New Fields**:
```c
typedef struct synapse_t {
    uint32_t target_id;    /**< Target neuron ID (POST-synaptic) - EXISTING */

    // Axon integration - Track source neuron and axon for spike propagation
    uint32_t source_neuron_id;  /**< Pre-synaptic neuron ID (0 = unset/legacy) */
    uint32_t axon_id;           /**< Axon delivering spikes (0 = no axon, direct connection) */

    // ... rest of existing fields ...
} synapse_t;
```

**Semantics**:
- `source_neuron_id`: Pre-synaptic neuron that sends spikes
- `axon_id = 0`: Direct connection (legacy, backward compatible)
- `axon_id != 0`: Axon delivers spike with conduction delay

**Future Extension**:
```c
// Future: Dendrite and spine integration
uint32_t dendrite_id;           // Dendrite receiving the synapse
uint32_t dendritic_spine_id;    // Specific spine contact point (for spine morphology)
```

---

### 3. Neural Network Structure (`neural_network_t`)

**Location**: `src/core/neuralnet/nimcp_neuralnet.c`

**New Field**:
```c
struct neural_network_struct {
    // ... existing fields ...

    // Axon Integration - Signal propagation with realistic conduction delays
    void* axon_network;  /**< Axon network for spike propagation (axon_network_t*, NULL = no axons) */
};
```

**Semantics**:
- `axon_network = NULL`: **Default** - No axon delays, direct propagation (backward compatible)
- `axon_network != NULL`: Axon network manages spike queue and propagation delays

---

### 4. Brain Structure (`brain_t`)

**Location**: `src/core/brain/nimcp_brain_internal.h`

**New Field**:
```c
struct brain_struct {
    // ... existing fields ...

    // === AXON INTEGRATION ===

    // Axon network for realistic signal propagation with conduction delays
    void* axon_network;  /**< axon_network_t* (NULL = no axons, direct connections) */
};
```

---

## Signal Flow Architecture

### Current Signal Flow (Legacy, Backward Compatible)

```
Neuron (PRE-synaptic) fires
    ↓ [Zero delay, direct]
Synapse receives spike
    ↓
Synapse computes NT dynamics (AMPA/NMDA/GABA-A)
    ↓
Neuron (POST-synaptic) receives synaptic current
```

### Future Signal Flow (With Axons)

```
Neuron (PRE-synaptic) fires
    ↓
Axon receives spike (axon_initiate_spike)
    ↓ [Conduction delay: 1-100ms based on myelination]
Spike queued with arrival_time
    ↓
axon_network_step() processes queue
    ↓
Spike arrives at axon terminal
    ↓
Synapse receives spike
    ↓
Synapse computes NT dynamics
    ↓
Neuron (POST-synaptic) receives synaptic current
```

### Future Signal Flow (With Axons + Dendrites)

```
Neuron (PRE-synaptic) fires
    ↓
Axon receives spike
    ↓ [Axon conduction delay]
Spike arrives at synapse
    ↓
NT release at pre-synaptic terminal
    ↓ [Synaptic cleft diffusion: ~0.5ms]
NT binds to receptors on dendritic spine
    ↓
Dendritic spine receives input
    ↓ [Local Ca²⁺ dynamics, spine morphology]
Dendrite integrates inputs
    ↓ [Dendritic computation, backprop]
Neuron (POST-synaptic) soma receives integrated current
```

---

## Backward Compatibility

### Design Principle: Zero-Default Pattern

All new fields use **zero as the "disabled" value**:

- `neuron_t.axon_id = 0` → No axon (legacy behavior)
- `synapse_t.source_neuron_id = 0` → Not set (legacy)
- `synapse_t.axon_id = 0` → No axon (direct connection)
- `neural_network_t.axon_network = NULL` → No axon delays
- `brain_t.axon_network = NULL` → No axon delays

### Legacy Code Compatibility

**Existing code works without modification** because:

1. **Default initialization**: All new fields are zero-initialized by `nimcp_calloc()`
2. **Conditional logic**: Axon code checks `if (axon_id != 0)` before routing
3. **Direct propagation**: When `axon_id == 0`, synapses fire immediately (zero delay)

**Example**:
```c
// Legacy code (pre-axon integration)
neuron_t* n1 = neural_network_add_neuron(network);
neuron_t* n2 = neural_network_add_neuron(network);
neural_network_add_connection(network, n1->id, n2->id, 0.5f);

// Behavior: Direct connection, zero delay (unchanged)
// n1.axon_id = 0 (default)
// synapse.axon_id = 0 (default)
// → Works exactly as before
```

---

## Integration Points for Future Development

### Dendrite Module (Future)

**Expected Structure**:
```c
typedef struct dendrite_struct {
    uint32_t id;
    uint32_t target_neuron_id;     // Neuron that owns this dendrite
    uint32_t* synapse_ids;         // Synapses connecting to this dendrite
    uint32_t num_synapses;

    // Morphology
    float length;                  // Total dendritic length
    dendritic_branch_t* branches;  // Tree structure
    dendritic_spine_t* spines;     // Spine array (contact points)

    // Computation
    float local_potential;         // Local dendritic potential
    float calcium_level;           // Local Ca²⁺ concentration

    // Integration
    float integration_window_ms;   // Temporal integration window
} dendrite_t;
```

**Integration Points**:
- `neuron_t.dendrite_ids[]` - Array of dendrite IDs
- `synapse_t.dendrite_id` - Which dendrite receives this synapse
- `synapse_t.dendritic_spine_id` - Specific spine contact point

### Myelin Sheath Module (Future)

**Current State**: Myelination is currently a property of oligodendrocytes and axons:
- `oligodendrocyte_t.axons[].myelination_level` (0-1)
- `axon_t.myelination_level` (0-1)
- `axon_segment_t.myelination` (per-segment level)

**Future Dedicated Object**:
```c
typedef struct myelin_sheath_struct {
    uint32_t id;
    uint32_t oligodendrocyte_id;   // Oligodendrocyte that produced this sheath
    uint32_t axon_id;              // Axon being myelinated
    uint32_t segment_index;        // Which axon segment (internode)

    // Morphology
    float thickness;               // Myelin thickness (μm)
    float g_ratio;                 // Inner/outer diameter ratio (optimal = 0.77)
    float length;                  // Sheath length (internode length)

    // Biophysics
    float capacitance;             // Membrane capacitance (reduced by myelin)
    float resistance;              // Membrane resistance (increased by myelin)

    // Metabolic support
    float lactate_production;      // Lactate for axon metabolism
    float growth_factor_release;   // NRG1/BDNF signaling

    // Pathology
    float integrity;               // 0-1, degrades in demyelination diseases
} myelin_sheath_t;
```

**Integration Points**:
- `axon_segment_t.myelin_sheath_id` - Dedicated sheath object per segment
- `oligodendrocyte_t.myelin_sheath_ids[]` - Tracks all sheaths produced
- Bidirectional reference: sheath ↔ axon ↔ oligodendrocyte

---

## API Design (Future Implementation)

### Connection Creation with Axon

```c
/**
 * WHAT: Create synaptic connection with optional axon propagation
 * WHY:  Enable realistic conduction delays based on distance and myelination
 * HOW:  Links pre-neuron → axon → synapse → post-neuron
 *
 * @param brain Brain instance
 * @param from_neuron_id Pre-synaptic neuron ID
 * @param to_neuron_id Post-synaptic neuron ID
 * @param weight Synaptic weight
 * @param type Synapse type (AMPA, NMDA, GABA-A, etc.)
 * @param axon_config Optional axon configuration (NULL = no axon, direct connection)
 * @return true if connection created successfully
 *
 * BACKWARD COMPATIBLE: If axon_config == NULL, behaves like legacy add_connection()
 */
bool brain_add_connection_with_axon(
    brain_t* brain,
    uint32_t from_neuron_id,
    uint32_t to_neuron_id,
    float weight,
    synapse_type_t type,
    axon_config_t* axon_config  // NULL = no axon
);
```

**Example Usage**:
```c
// Legacy mode (no axon, backward compatible)
brain_add_connection_with_axon(brain, 1, 2, 0.5f, SYNAPSE_AMPA, NULL);

// With axon (future)
axon_config_t axon_config = {
    .type = AXON_TYPE_MYELINATED,
    .length = 1000.0f,  // 1mm
    .diameter = 2.0f,   // 2μm
    .myelination_level = 0.8f
};
brain_add_connection_with_axon(brain, 1, 2, 0.5f, SYNAPSE_AMPA, &axon_config);
```

---

## Testing Strategy

### Unit Tests

1. **Backward Compatibility Tests**:
   - Verify `axon_id = 0` works (legacy mode)
   - Verify existing code runs unchanged
   - Verify zero-delay propagation when no axon

2. **Field Initialization Tests**:
   - Verify new fields are zero-initialized
   - Verify `nimcp_calloc()` sets axon_id = 0

3. **Future Integration Tests** (when dendrites/myelin added):
   - Verify axon → synapse → dendrite routing
   - Verify myelin sheath affects conduction velocity
   - Verify Ca²⁺ dynamics in dendritic spines

### Integration Tests

1. **Simple Network Test**:
   - Create 2 neurons
   - Connect without axon (`axon_id = 0`)
   - Verify direct propagation (zero delay)

2. **Future Axon Network Test**:
   - Create axon network
   - Add axons with varying lengths
   - Verify spike delays match distance/myelination

3. **Future Dendrite Integration Test**:
   - Create neurons with dendrites
   - Verify synapses connect to correct dendrites
   - Verify dendritic integration works

---

## Memory Overhead

### Current Overhead

**Per Neuron**: +4 bytes
```c
uint32_t axon_id;  // 4 bytes
```

**Per Synapse**: +8 bytes
```c
uint32_t source_neuron_id;  // 4 bytes
uint32_t axon_id;           // 4 bytes
```

**Per Network**: +8 bytes (64-bit pointer)
```c
void* axon_network;  // 8 bytes (NULL by default)
```

**Total for 10K neurons, 100K synapses**:
- Neurons: 10,000 × 4 bytes = 40 KB
- Synapses: 100,000 × 8 bytes = 800 KB
- Network: 8 bytes
- **Total: ~840 KB overhead** (negligible)

### Future Overhead (With Axons Active)

When `axon_network != NULL`:
- Axon network structure: ~100 bytes
- Axon objects: ~200 bytes each
- Spike queue: ~40 bytes per queued spike

For 10K axons with 100 queued spikes:
- 10,000 × 200 bytes = 2 MB (axons)
- 100 × 40 bytes = 4 KB (queue)
- **Total: ~2 MB** (acceptable for realistic propagation)

---

## References

- **Axon Module**: `src/core/axon/nimcp_axon.h`
- **Neuron Structure**: `src/core/neuralnet/nimcp_neuralnet.h`
- **Brain Structure**: `src/core/brain/nimcp_brain_internal.h`
- **Oligodendrocyte Integration**: `src/glial/oligodendrocytes/nimcp_oligodendrocytes.h`

---

## Changelog

- **2024-11**: Initial axon module implementation
- **2024-11**: Minimal integration (Option A) - Data structures only
- **Future**: Full spike routing with dendrites and myelin sheaths (Option B)
