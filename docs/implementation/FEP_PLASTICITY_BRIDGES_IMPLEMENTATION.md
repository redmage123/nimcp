# Free Energy Principle Plasticity Bridges Implementation

## Overview
This document describes the implementation of 6 FEP bridges for plasticity modules in NIMCP, following the established pattern from STDP, BCM, and Homeostatic FEP bridges.

## Implementation Status

### ✅ Completed: Adaptive Plasticity FEP Bridge
- **Header**: `/home/bbrelin/nimcp/include/plasticity/adaptive/nimcp_adaptive_fep_bridge.h` (566 lines)
- **Implementation**: `/home/bbrelin/nimcp/src/plasticity/adaptive/nimcp_adaptive_fep_bridge.c` (347 lines)
- **Biological Basis**: Precision controls sparsity target; PE scales threshold adaptation; sparsity informs precision
- **Key Features**:
  - FEP→Adaptive: PE scaling, precision-modulated sparsity, complexity regularization
  - Adaptive→FEP: Sparsity feedback, threshold change reporting, precision estimation

## Module Integration Patterns

### 1. Adaptive Plasticity (`nimcp_adaptive.h`)
**Biological Mapping**:
- **FEP → Adaptive**:
  - Precision → Sparsity target (high precision = higher sparsity)
  - Prediction error → Threshold adaptation rate
  - Complexity cost → Threshold scaling (higher thresholds for complexity minimization)
- **Adaptive → FEP**:
  - Activation sparsity → Precision estimates
  - Threshold adaptation → Generative model updates
  - Integer spike counts → Discrete belief representations

**Key Parameters**:
- `pe_sensitivity`: 1.0 (PE effect scaling)
- `precision_gain`: 1.0 (precision effect scaling)
- `complexity_gain`: 0.5 (complexity cost scaling)
- `sparsity_feedback_gain`: 1.0 (sparsity → precision scaling)

### 2. Attention Plasticity (`nimcp_attention.h`)
**Biological Mapping**:
- **FEP → Attention**:
  - Precision → Attention gate strength (high precision = focused attention)
  - Prediction error → Learning rate for attention weights
  - Salience → Attention allocation
- **Attention → FEP**:
  - Attention entropy → Precision estimates (low entropy = high precision)
  - Gate activation → Confidence in representations
  - Multi-head statistics → Hierarchical precision

**Key Mechanisms**:
- Precision-weighted attention gates
- PE-driven attention weight updates
- Entropy-based precision feedback
- RoPE/ALiBi position encoding integration

### 3. Neuromodulators (`nimcp_neuromodulators.h`)
**Biological Mapping**:
- **FEP → Neuromodulators**:
  - Prediction error → Dopamine release (reward prediction error)
  - Uncertainty → Acetylcholine release (salience signaling)
  - Threat/surprise → Norepinephrine release
  - Punishment → Serotonin modulation
  - Precision → Receptor density modulation
- **Neuromodulators → FEP**:
  - Dopamine level → Learning rate scaling
  - ACh level → Precision weighting
  - Neuromodulator levels → Prior beliefs about arousal/valence

**Key Mechanisms**:
- PE-driven dopamine bursts (reward prediction error)
- Precision-gated acetylcholine release
- Neuromodulator levels modulate FEP learning rates
- Volume transmission as precision broadcasting

### 4. Noise Plasticity (`nimcp_pink_noise.h`)
**Biological Mapping**:
- **FEP → Noise**:
  - Precision → Noise amplitude (low precision = higher noise for exploration)
  - Prediction error → Noise temporal correlation (high PE = faster noise dynamics)
  - Temperature parameter → Stochastic resonance optimization
- **Noise → FEP**:
  - Noise-driven exploration → Belief sampling
  - 1/f spectrum → Multi-timescale learning
  - Stochastic resonance → Optimal precision estimation

**Key Mechanisms**:
- Precision-modulated noise amplitude (exploration-exploitation)
- PE-scaled noise generation rate
- 1/f noise for biologically realistic fluctuations
- Stochastic resonance for optimal belief sampling

### 5. Predictive Plasticity (`nimcp_predictive_coding.h`)
**Biological Mapping**:
- **FEP → Predictive**:
  - FEP IS predictive coding (direct integration)
  - Free energy = Prediction error + Complexity
  - Precision = Confidence in predictions
- **Predictive → FEP**:
  - Hierarchical prediction errors → FEP updates
  - Precision learning → Precision estimates
  - Layer-wise free energy → Total free energy

**Key Mechanisms**:
- Direct FEP-predictive coding equivalence
- Hierarchical precision weighting
- Layer-wise free energy computation
- Variational message passing

### 6. Short-Term Plasticity (STP) (`nimcp_stp.h`)
**Biological Mapping**:
- **FEP → STP**:
  - Precision → Utilization probability (U parameter)
  - Prediction error → Recovery time constant modulation
  - Temporal prediction accuracy → Depression/facilitation balance
- **STP → FEP**:
  - Synaptic resource availability → Precision estimates
  - Depression state → Confidence in recent predictions
  - Facilitation state → Temporal correlation beliefs

**Key Mechanisms**:
- Precision-modulated release probability
- PE-scaled recovery dynamics
- STP state as temporal precision indicator
- Depression/facilitation as temporal prediction accuracy

## File Structure Template

Each bridge follows this structure:

### Header File (`nimcp_<module>_fep_bridge.h`)
1. Biological basis documentation (FEP ↔ Module pathways)
2. Constants and thresholds
3. Configuration struct (`<module>_fep_config_t`)
4. Effects structs (FEP→Module, Module→FEP)
5. State and statistics structs
6. Bridge struct with mutex and bio-async support
7. Full API:
   - Lifecycle: default_config, create, destroy
   - Connection: connect_fep, connect_<module>, disconnect
   - FEP→Module direction functions
   - Module→FEP direction functions
   - Update cycle
   - State/stats getters
   - Bio-async: connect, disconnect, is_connected

### Implementation File (`nimcp_<module>_fep_bridge.c`)
1. Includes: module header, utils (memory, thread, logging), math, string
2. Lifecycle implementations (guard clauses, nimcp_malloc/free, mutex)
3. Connection implementations (mutex-protected pointer assignments)
4. Modulation functions (clamping, scaling, biological mappings)
5. Update cycle (compute all effects, update statistics)
6. Bio-async integration (register with BIO_MODULE_FEP_<MODULE>_BRIDGE)

### Unit Test File (`test_<module>_fep_bridge.cpp`)
1. Lifecycle tests (create, destroy, config)
2. Connection tests (FEP, module, disconnect)
3. FEP→Module tests (all modulation functions)
4. Module→FEP tests (all feedback functions)
5. Update cycle tests
6. State/stats tests
7. Bio-async tests
8. Integration tests

## Bio-Async Module IDs

Add to `/home/bbrelin/nimcp/include/async/nimcp_bio_messages.h`:

```c
/* FEP plasticity bridge modules (0x0E00 - 0x0EFF) */
BIO_MODULE_FEP_ADAPTIVE_BRIDGE = 0x0E00,
BIO_MODULE_FEP_ATTENTION_PLASTICITY_BRIDGE = 0x0E01,
BIO_MODULE_FEP_NEUROMODULATORS_BRIDGE = 0x0E02,
BIO_MODULE_FEP_NOISE_BRIDGE = 0x0E03,
BIO_MODULE_FEP_PREDICTIVE_BRIDGE = 0x0E04,
BIO_MODULE_FEP_STP_BRIDGE = 0x0E05,
```

## CMakeLists.txt Updates

### For each module directory:

```cmake
# Add FEP bridge source
set(MODULE_SOURCES
    ${MODULE_SOURCES}
    nimcp_<module>_fep_bridge.c
)

# Add FEP bridge tests
add_executable(test_<module>_fep_bridge
    test_<module>_fep_bridge.cpp
)
target_link_libraries(test_<module>_fep_bridge
    nimcp
    gtest
    gtest_main
)
```

## Testing Strategy

### Unit Tests (30+ tests per module)
1. Lifecycle: create, destroy, default_config
2. Connections: connect_fep, connect_module, disconnect
3. FEP→Module: Each modulation function with edge cases
4. Module→FEP: Each feedback function
5. Update cycle: Effects computation, statistics
6. State/Stats: Getters, accumulation
7. Bio-async: Connect, disconnect, is_connected

### Integration Tests (10+ tests per module)
1. Full update cycles with both systems connected
2. Bidirectional communication
3. Effect propagation
4. Statistics accumulation over time
5. Bio-async message passing

## Implementation Guidelines

### NIMCP Coding Standards
- ✅ Functions < 50 lines
- ✅ Guard clauses (early returns)
- ✅ WHAT/WHY/HOW documentation on all functions
- ✅ Thread-safe via mutex
- ✅ nimcp_malloc/nimcp_free for memory
- ✅ NIMCP_LOGGING_INFO/ERROR/DEBUG for logging
- ✅ Return 0 for success, -1 for errors

### Biological Realism
- Document biological basis for each pathway
- Reference neuroscience papers where applicable
- Map FEP concepts to neural mechanisms
- Explain bidirectional interactions

### Performance
- O(1) modulation functions
- Minimal mutex lock duration
- Lock-free bio-async registration
- Efficient statistics accumulation

## Next Steps

1. ✅ Complete adaptive FEP bridge (DONE)
2. Create remaining 5 bridge headers
3. Create remaining 5 bridge implementations
4. Write unit tests for all 6 bridges
5. Write integration tests
6. Update nimcp_bio_messages.h
7. Update all CMakeLists.txt files
8. Build and test all modules
9. Document results

## Expected Test Coverage

- **Total Files**: 18 (6 headers + 6 implementations + 6 test files)
- **Total Lines**: ~6000 (similar to immune bridges)
- **Total Tests**: 180+ (30+ per module)
- **Test Categories**: Lifecycle, Connection, Modulation, Feedback, Update, Stats, Bio-async

## References

- Friston (2010). "The free-energy principle: a unified brain theory"
- Feldman & Friston (2010). "Attention, uncertainty, and free-energy"
- Rao & Ballard (1999). "Predictive coding in the visual cortex"
- Tsodyks & Markram (1997). "The neural code between neocortical pyramidal neurons"
