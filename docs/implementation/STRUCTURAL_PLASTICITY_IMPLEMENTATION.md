# Structural Plasticity Implementation

## Overview
Complete implementation of activity-dependent spine dynamics with sleep/immune integration for the NIMCP plasticity module.

## Files Created

### Core Implementation
1. **`include/plasticity/structural/nimcp_structural_plasticity.h`** (648 lines)
   - Spine morphology struct (volume, stability, actin dynamics, PSD size)
   - Synapse states: NASCENT, STABLE, POTENTIATED, PRUNING, ELIMINATED
   - Formation/elimination based on activity thresholds
   - Stabilization via consolidation signals
   - Configuration with biological defaults
   - Callback system for structural change events

2. **`src/plasticity/structural/nimcp_structural_plasticity.c`** (706 lines)
   - Full implementation with NIMCP utils
   - Thread-safe operations with mutex
   - Activity-dependent spine dynamics
   - State machine for spine lifecycle
   - Complement tagging for immune pruning
   - LTP/LTD accumulation and auto-potentiation

### Sleep Integration Bridge
3. **`include/plasticity/structural/nimcp_structural_sleep_bridge.h`** (159 lines)
   - Sleep state modulates formation/consolidation/pruning rates
   - AWAKE: Formation enabled (1.0x), minimal consolidation (0.2x)
   - LIGHT_NREM: Formation reduced (0.1x), active consolidation (1.0x)
   - DEEP_NREM: No formation (0.0x), strong consolidation (2.0x)
   - REM: No formation (0.0x), active pruning (1.5x)
   - Tagged spine consolidation during NREM
   - Weak spine pruning during REM

4. **`src/plasticity/structural/nimcp_structural_sleep_bridge.c`** (213 lines)
   - Full sleep state integration
   - Consolidation boost application
   - Formation rate modulation
   - Pruning rate adjustment

### Immune Integration Bridge
5. **`include/plasticity/structural/nimcp_structural_immune_bridge.h`** (475 lines)
   - Cytokine effects on formation/pruning
   - IL-1β/IL-6/TNF-α → impaired formation (-30% to -50%)
   - IL-10 → formation protection (+20%)
   - Microglia-mediated synaptic pruning
   - Complement tagging (C1q/C3) for weak synapses
   - Inflammation reduces formation (systemic: -70%, storm: -90%)
   - Bio-async integration (BIO_MODULE_IMMUNE_STRUCTURAL)

6. **`src/plasticity/structural/nimcp_structural_immune_bridge.c`** (364 lines)
   - Cytokine effect computation
   - Microglia pruning logic
   - Complement tagging of weak spines
   - Formation factor modulation
   - Bio-async router connection

### Tests
7. **`test/unit/plasticity/structural/test_structural_plasticity.cpp`** (700 lines, 38 tests)
   - System creation and initialization
   - Spine formation and morphology
   - Stabilization (nascent → stable)
   - Potentiation (stable → potentiated)
   - Activity tracking and LTP/LTD accumulation
   - Pruning and elimination
   - Complement tagging
   - Callback invocation
   - State queries and spine counting

8. **`test/integration/plasticity/structural/test_structural_integration.cpp`** (343 lines, 10 tests)
   - Sleep bridge integration
   - NREM consolidation of tagged spines
   - REM pruning of weak spines
   - Immune bridge integration
   - Microglia pruning of complement-tagged synapses
   - Weak spine complement tagging
   - Inflammation impairs formation
   - Combined sleep-immune cooperation

9. **`test/regression/plasticity/structural/test_structural_regression.cpp`** (337 lines, 19 tests)
   - Stability over extended simulation
   - Formation/elimination balance
   - Maturation progress monotonicity
   - Pruning urgency bounds
   - Morphology parameter realism
   - Formation performance (< 100 μs per spine)
   - Update performance (1000 spines in < 1 sec)
   - Query performance (< 10 μs per query)
   - Valid state transitions
   - Memory safety (NULL pointer handling, no leaks)

## Biological Basis

### Spine Morphology
- **Thin spines** (nascent): Learning phase, high motility, small PSD
- **Mushroom spines** (stable): Stable memory, large PSD, strong connections
- **Enlarged spines** (potentiated): Maximal strength after LTP
- Reference: Bhatt et al. (2009), Holtmaat & Svoboda (2009)

### Formation
- High-frequency stimulation triggers formation (20-50 Hz threshold)
- Actin polymerization drives spine growth
- Nascent spines appear within minutes-hours
- Reference: Matsuzaki et al. (2004)

### Stabilization
- Repeated activation consolidates nascent spines
- PSD growth correlates with synaptic strength
- Maturation time: 1-7 days for full stabilization
- CaMKII translocation stabilizes structure
- Reference: Kasai et al. (2010)

### Pruning
- Low activity (<1 Hz) leads to spine retraction
- Microglia-mediated synaptic pruning during development/sleep
- Complement-tagged synapses (C1q/C3) targeted for elimination
- Reference: Hong et al. (2016), Schafer et al. (2012)

### Sleep-Dependent Consolidation
- NREM sleep strengthens tagged spines (synaptic upscaling)
- REM sleep prunes weak spines (synaptic homeostasis)
- Sleep deprivation impairs spine formation and stabilization
- Reference: Tononi & Cirelli (2014), Yang et al. (2014)

### Immune-Mediated Pruning
- Microglia engulf synaptic material via CR3 receptor
- Complement C1q/C3 tag weak/inactive synapses
- Inflammation triggers excessive pruning
- Critical for circuit refinement
- Reference: Stevens et al. (2007), Schafer et al. (2012)

## API Examples

### Formation
```c
structural_plasticity_system_t* system = structural_plasticity_create(&config);

uint32_t synapse_id;
float high_activity = 50.0f;  // Hz, above threshold

if (structural_plasticity_should_form(system, high_activity)) {
    structural_plasticity_form_synapse(
        system, pre_neuron_id, post_neuron_id, high_activity, &synapse_id);
}
```

### Stabilization
```c
// Tag for sleep consolidation
structural_plasticity_tag_for_consolidation(system, synapse_id);

// During NREM sleep, consolidate tagged spines
structural_plasticity_stabilize_synapse(system, synapse_id);
```

### Potentiation
```c
// Record LTP events
for (int i = 0; i < 6; i++) {
    structural_plasticity_record_ltp(system, synapse_id, 2.0f);
}

// Auto-potentiates when LTP accumulator exceeds threshold
synapse_structural_state_t state;
structural_plasticity_get_synapse_state(system, synapse_id, &state);
// state.state == SYNAPSE_STATE_POTENTIATED
```

### Pruning
```c
// Complement tagging for immune pruning
uint8_t tag[STRUCTURAL_EPITOPE_SIZE];
memset(tag, 0xC3, STRUCTURAL_EPITOPE_SIZE);
structural_plasticity_tag_complement(system, synapse_id, tag, STRUCTURAL_EPITOPE_SIZE);

// Check if should prune
if (structural_plasticity_should_prune(system, &synapse_state)) {
    structural_plasticity_eliminate_synapse(system, synapse_id);
}
```

### Sleep Integration
```c
structural_sleep_bridge_t bridge = structural_sleep_bridge_create(
    &config, sleep_system, structural_system);

// Set sleep state
sleep_set_state(sleep_system, SLEEP_STATE_DEEP_NREM);
structural_sleep_update(bridge);

// Consolidate tagged spines during NREM
structural_sleep_consolidate_tagged(bridge);

// Prune weak spines during REM
sleep_set_state(sleep_system, SLEEP_STATE_REM);
structural_sleep_prune_weak(bridge);
```

### Immune Integration
```c
structural_immune_bridge_t* bridge = structural_immune_bridge_create(
    &config, immune_system, structural_system);

// Tag weak spines with complement
uint32_t tagged = structural_immune_tag_weak_spines(bridge);

// Microglia prune complement-tagged synapses
structural_immune_microglia_prune(bridge);

// Get formation impairment from inflammation
float formation_factor = structural_immune_get_formation_factor(bridge);
```

## Test Statistics

### Unit Tests (38 tests)
- Creation and initialization: 3 tests
- Formation: 6 tests
- Stabilization: 4 tests
- Potentiation: 4 tests
- Activity tracking: 5 tests
- Pruning/elimination: 5 tests
- Immune integration: 5 tests
- Queries: 3 tests
- Callbacks: 3 tests

### Integration Tests (10 tests)
- Sleep bridge: 4 tests
- Immune bridge: 5 tests
- Combined sleep-immune: 1 test

### Regression Tests (19 tests)
- Stability: 5 tests
- Performance: 3 tests
- State transitions: 2 tests
- Memory safety: 3 tests

**Total: 67 tests**

## CMakeLists.txt Updates

Added to `/home/bbrelin/nimcp/src/lib/CMakeLists.txt`:
- `nimcp_structural_plasticity.c` - Core implementation
- `nimcp_structural_immune_bridge.c` - Immune integration
- `nimcp_structural_sleep_bridge.c` - Sleep integration

Added to `/home/bbrelin/nimcp/test/unit/plasticity/CMakeLists.txt`:
- `unit_plasticity_structural` test target

## Integration Points

### Sleep System
- Formation rate modulation by wake/sleep state
- NREM consolidation of tagged spines
- REM pruning of weak spines
- Sleep deprivation impairs stabilization

### Immune System
- Cytokine modulation of formation/pruning
- Complement tagging (C1q/C3) of weak synapses
- Microglia-mediated engulfment and elimination
- Inflammation reduces formation rate

### Activity Tracking
- Exponential moving average of firing rate
- LTP/LTD accumulation for potentiation/pruning
- Activity-dependent formation and elimination decisions

### Callback System
- FORMATION, STABILIZATION, POTENTIATION events
- PRUNING_START, ELIMINATION events
- RECOVERY event for pruning→stable transition

## Performance Characteristics

- **Formation**: < 100 μs per spine (O(1))
- **Update**: 1000 spines in < 1 second (O(n))
- **Query**: < 10 μs per query (O(1))
- **Memory**: ~200 bytes per spine
- **Thread-safe**: All operations protected by mutex

## Compliance with NIMCP Standards

- ✓ All functions < 50 lines
- ✓ Guard clauses (early returns)
- ✓ WHAT-WHY-HOW documentation
- ✓ Thread-safe via nimcp_mutex
- ✓ nimcp_malloc/nimcp_free memory management
- ✓ NIMCP_LOGGING_* for all log messages
- ✓ Biological basis documented
- ✓ References cited

## Future Extensions

1. **Spine density homeostasis**: Regulate total spine count per neuron
2. **Spatial competition**: Nearby spines compete for resources
3. **Protein synthesis integration**: Frey & Morris synaptic tagging
4. **Calcium integration**: Ca²⁺-dependent spine enlargement
5. **Heterosynaptic plasticity**: Spine changes affect neighbors
6. **Developmental pruning**: Age-dependent pruning curves
7. **Disease models**: Alzheimer's (excessive pruning), autism (reduced pruning)

## References

1. Bhatt et al. (2009) "Dendritic spine dynamics" Annu Rev Physiol
2. Holtmaat & Svoboda (2009) "Experience-dependent structural synaptic plasticity in the mammalian brain" Nat Rev Neurosci
3. Matsuzaki et al. (2004) "Structural basis of long-term potentiation in single dendritic spines" Nature
4. Kasai et al. (2010) "Structural dynamics of dendritic spines in memory and cognition" Trends Neurosci
5. Hong et al. (2016) "Complement and microglia mediate early synapse loss in Alzheimer mouse models" Science
6. Tononi & Cirelli (2014) "Sleep and synaptic homeostasis: a hypothesis" Brain Res Bull
7. Yang et al. (2014) "Sleep promotes branch-specific formation of dendritic spines after learning" Science
8. Havekes et al. (2016) "Sleep deprivation causes memory deficits by negatively impacting neuronal connectivity in hippocampus" eLife
9. Xu et al. (2009) "Rapid formation and selective stabilization of synapses for enduring motor memories" Nature
10. Stevens et al. (2007) "The classical complement cascade mediates CNS synapse elimination" Cell
11. Schafer et al. (2012) "Microglia sculpt postnatal neural circuits in an activity and complement-dependent manner" Neuron
12. Lehrman et al. (2018) "CD47 protects synapses from excess microglia-mediated pruning during development" Neuron
