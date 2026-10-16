# Predictive Coding Integration with Brain Regions and Hierarchical Processing

## Overview

This implementation integrates **Predictive Coding** (Free Energy Principle) with NIMCP's brain regions architecture, enabling hierarchical predictive processing across cortical areas. The system implements Friston's Free Energy Principle through a biologically realistic cortical hierarchy where:

- **Predictions flow top-down** (higher regions → lower regions)
- **Prediction errors flow bottom-up** (lower regions → higher regions)
- **Precision weights** (attention) modulate error gain
- **Iterative inference** minimizes variational free energy

## Architecture

### Key Components

1. **Hierarchical Predictive Coding Header**
   - Location: `/home/bbrelin/nimcp/include/core/brain_regions/nimcp_brain_region_predictive.h`
   - Extends `brain_region_t` with predictive processing capabilities
   - Provides complete API for hierarchical prediction

2. **Implementation**
   - Location: `/home/bbrelin/nimcp/src/core/brain/regions/nimcp_brain_region_predictive.c`
   - Full implementation of predictive processing API
   - Bio-async integration for message-based communication
   - Security monitoring integration with BBB

3. **Test Suite**
   - Unit tests: `test/unit/brain_regions/test_brain_region_predictive.cpp`
   - Integration tests: `test/integration/brain_regions/test_hierarchical_prediction_flow.cpp`
   - Regression tests: `test/regression/brain_regions/test_prediction_accuracy.cpp`

### Architectural Patterns

- **Chain of Responsibility**: Hierarchical error propagation
- **Strategy Pattern**: Different prediction strategies per region type
- **Observer Pattern**: Bio-async notifications for predictions/errors
- **Facade Pattern**: Simplified interface over complex predictive machinery

## Biological Basis

### Free Energy Principle (Friston 2010)

The brain minimizes **variational free energy** (F), which upper-bounds surprise:

```
F = <π × ε²> + complexity
  = Σ πᵢ × εᵢ² - ln(π)
```

Where:
- **ε** = prediction error (actual - predicted)
- **π** = precision (inverse variance, attention)
- **complexity** = model complexity term

### Cortical Hierarchy

```
Higher Region (V4)
  ↓ Top-Down Prediction: μ̂_V2 = f(μ_V4)
Middle Region (V2)
  ↓ Prediction Error: ε_V2 = x_V2 - μ̂_V2
  ↑ Error Propagation to V4
  ↓ Top-Down Prediction: μ̂_V1 = g(μ_V2)
Lower Region (V1)
  ↓ Prediction Error: ε_V1 = sensory_input - μ̂_V1
  ↑ Error Propagation to V2
```

### Cortical Layers

- **Superficial Layers (2/3)**: Encode prediction errors (ε)
- **Deep Layers (5/6)**: Encode predictions (μ̂), send to lower areas
- **Layer 4**: Receives sensory input (bottom level only)

## API Documentation

### Configuration

```c
// Create default configuration for hierarchy level N
brain_region_predictive_config_t config =
    brain_region_predictive_config_default(N);

// Sensory region configuration (level 0)
brain_region_predictive_config_t sensory_config =
    brain_region_predictive_config_sensory();

// Association region configuration (high level)
brain_region_predictive_config_t assoc_config =
    brain_region_predictive_config_association();
```

### Initialization

```c
// Enable predictive processing for region
nimcp_result_t result = brain_region_enable_predictive(region, &config);

// Disable predictive processing
brain_region_disable_predictive(region);
```

### Hierarchical Prediction

```c
// Generate top-down prediction for lower region
float prediction[lower_region_neurons];
brain_region_predict_lower(higher_region, lower_region_id,
                            prediction, size);

// Compute prediction error
float actual[neurons], predicted[neurons], error[neurons];
brain_region_compute_error(region, actual, predicted, error, size);

// Update from precision-weighted error
brain_region_update_from_error(region, error, precision, dt);
```

### Hierarchical Processing Flow

```c
// Single hierarchical step (prediction → error → update)
brain_region_hierarchical_step(region, sensory_input, input_size, dt);

// Run to convergence (iterative inference)
uint32_t iterations = brain_region_hierarchical_converge(
    region, sensory_input, input_size, max_iter, tolerance);
```

### Precision (Attention) Modulation

```c
// Set precision weights manually
float precisions[neurons];
brain_region_set_precision(region, precisions, size);

// Get current precisions
brain_region_get_precision(region, precisions, size);

// Learn precisions from error statistics
brain_region_learn_precisions(region, dt);
```

### Inter-Region Connections

```c
// Connect regions hierarchically (higher → lower)
brain_region_connect_predictive(higher_region, lower_region, strength);

// Disconnect hierarchical connection
brain_region_disconnect_predictive(higher_region, lower_region);
```

### Bio-Async Integration

```c
// Register for bio-async messaging
brain_region_register_predictive_bio_async(region, module_id);

// Broadcast prediction (top-down)
brain_region_broadcast_prediction(region, target_id, prediction, size);

// Broadcast error (bottom-up)
brain_region_broadcast_error(region, target_id, error, size);
```

### Query and Statistics

```c
// Get current prediction
float prediction[neurons];
brain_region_get_prediction(region, prediction, size);

// Get prediction error
float error[neurons];
brain_region_get_prediction_error(region, error, size);

// Get free energy
float fe = brain_region_get_free_energy(region);

// Get statistics
brain_region_predictive_stats_t stats;
brain_region_get_predictive_stats(region, &stats);
```

## Usage Examples

### Example 1: Simple Hierarchical Network

```c
// Create regions
brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 128);
brain_region_t* v2 = brain_region_create(REGION_VISUAL_V2, 64);

// Enable predictive processing
brain_region_predictive_config_t v1_config =
    brain_region_predictive_config_sensory();
brain_region_predictive_config_t v2_config =
    brain_region_predictive_config_default(1);

brain_region_enable_predictive(v1, &v1_config);
brain_region_enable_predictive(v2, &v2_config);

// Connect hierarchically (V2 predicts V1)
brain_region_connect_predictive(v2, v1, 0.8f);

// Process sensory input
float sensory_input[128];
// ... fill with sensory data ...

// Run hierarchical processing
for (int i = 0; i < 10; i++) {
    brain_region_hierarchical_step(v1, sensory_input, 128, 1.0f);
    brain_region_hierarchical_step(v2, NULL, 0, 1.0f);
}

// Query free energy
float fe_v1 = brain_region_get_free_energy(v1);
printf("V1 Free Energy: %.4f\n", fe_v1);
```

### Example 2: Attention Modulation

```c
// Set high attention to specific region
float precisions[128];
for (int i = 0; i < 64; i++) {
    precisions[i] = 10.0f;  // High precision (attend)
}
for (int i = 64; i < 128; i++) {
    precisions[i] = 0.1f;   // Low precision (ignore)
}

brain_region_set_precision(v1, precisions, 128);

// Process with attention modulation
brain_region_hierarchical_step(v1, sensory_input, 128, 1.0f);

// Errors in attended region will be amplified
float errors[128];
brain_region_get_prediction_error(v1, errors, 128);
```

### Example 3: Convergence to Stable Representation

```c
// Configure for convergence
brain_region_predictive_config_t config =
    brain_region_predictive_config_sensory();
config.max_iterations = 50;
config.convergence_tolerance = 0.001f;

brain_region_enable_predictive(v1, &config);

// Run to convergence
uint32_t iterations = brain_region_hierarchical_converge(
    v1, sensory_input, 128, 0, 0.0f);  // Use config defaults

printf("Converged in %u iterations\n", iterations);

brain_region_predictive_stats_t stats;
brain_region_get_predictive_stats(v1, &stats);
printf("Final free energy: %.4f\n", stats.total_free_energy);
printf("Mean prediction error: %.4f\n", stats.mean_prediction_error);
```

## Bio-Async Message Flow

### Message Types

1. **BIO_MSG_PREDICTION_UPDATE** (Serotonin channel)
   - Top-down predictions from higher to lower regions
   - Slow, modulatory signals

2. **BIO_MSG_PREDICTION_ERROR** (Norepinephrine channel)
   - Bottom-up errors from lower to higher regions
   - Alerting, salient signals

3. **BIO_MSG_PREDICTIVE_CODING_UPDATE** (Dopamine channel)
   - General state updates
   - Learning signals

4. **BIO_MSG_ATTENTION_SHIFT** (Acetylcholine channel)
   - Precision modulation
   - Fast attention control

### Message Flow Example

```
Time T:
V4 → BIO_MSG_PREDICTION_UPDATE → V2
    (Top-down prediction)

V2 receives prediction
V2 computes error
V2 → BIO_MSG_PREDICTION_ERROR → V4
    (Bottom-up error)

V2 → BIO_MSG_PREDICTION_UPDATE → V1
    (Top-down prediction)

V1 receives prediction
V1 computes error against sensory input
V1 → BIO_MSG_PREDICTION_ERROR → V2
    (Bottom-up error)
```

## Security Integration

### Blood-Brain Barrier (BBB) Integration

```c
// Enable security monitoring
brain_region_predictive_enable_security(region, true);
```

**Monitored Events:**
- Large prediction errors (potential adversarial input)
- Sudden precision changes (attention hijacking)
- Free energy spikes (model confusion/attacks)

**Security Audit Trail:**
- All predictions logged for anomaly detection
- Error patterns analyzed for threats
- Precision changes tracked for manipulation

## Performance Characteristics

### Complexity Analysis

- **Prediction Generation**: O(N × M) - matrix multiplication
  - N = higher region neurons
  - M = lower region neurons

- **Error Computation**: O(N) - element-wise operations
  - N = region neurons

- **Hierarchical Step**: O(N × H × I)
  - N = neurons per region
  - H = hierarchy depth
  - I = convergence iterations

### Memory Usage

Per region with predictive processing enabled:
- Prediction buffer: N × sizeof(float)
- Error buffer: N × sizeof(float)
- Precision buffer: N × sizeof(float)
- Predictive hierarchy: O(L × N) where L = internal levels

Total: ~4N × sizeof(float) + hierarchy overhead

### Optimization Strategies

1. **Lazy Initialization**: Hierarchy created only when needed
2. **Buffer Caching**: Reuse prediction/error buffers
3. **Batch Messaging**: Group bio-async messages
4. **Lock-Free Reads**: Statistics queries without locking where safe

## Test Coverage

### Unit Tests (25 tests)

**Configuration:**
- Default, sensory, association config creation
- Parameter validation

**Initialization:**
- Enable/disable predictive processing
- NULL checks, idempotency
- Memory allocation

**Prediction:**
- Generation, error computation
- Precision weighting
- Guard clauses

**Hierarchical Processing:**
- Single step, convergence
- Multi-iteration inference

**Precision:**
- Setting, getting, learning
- Attention modulation

**Connections:**
- Hierarchical connections
- Disconnection

**Statistics:**
- Query capabilities

### Integration Tests (10 tests)

**Hierarchical Flow:**
- 3-level hierarchy processing
- Top-down/bottom-up message passing
- Error propagation, prediction flow

**Convergence:**
- Network convergence
- Complex input adaptation

**Attention:**
- Modulation across hierarchy
- Precision learning across levels

**Robustness:**
- Noisy input handling
- Dynamic reconfiguration

### Regression Tests (8 tests)

**Prediction Accuracy:**
- Constant input baseline
- Sinusoidal input baseline

**Convergence Speed:**
- Iteration count benchmarks

**Precision Learning:**
- Stability checks

**Free Energy:**
- Minimization trends

**Consistency:**
- Deterministic behavior

**Performance:**
- Memory usage stability

## Integration with Existing Systems

### Brain Regions

- Extends `brain_region_t` with `predictive_extension` field
- Non-invasive: Existing regions work unchanged
- Opt-in: Enable per-region as needed

### Predictive Coding Module

- Uses existing `pc_hierarchy_t` from predictive coding module
- Wraps low-level API with high-level region interface
- Leverages existing prediction/error computation

### Bio-Async System

- Integrates with message router
- Uses existing channel types
- Follows established messaging patterns

### Security System

- Registers with BBB for monitoring
- Uses existing audit trail infrastructure
- Follows security best practices

## Future Enhancements

### Planned Features

1. **Cortical Column Integration**
   - Map predictive hierarchy to cortical layers
   - Layer-specific prediction/error neurons
   - Minicolumn-level processing

2. **Temporal Dynamics**
   - Temporal prediction (predict t+1 from t)
   - Kalman filter-like state estimation
   - Dynamic time constants

3. **Active Inference**
   - Action selection to minimize future free energy
   - Motor predictions
   - Sensorimotor loops

4. **Hierarchical Learning**
   - Multi-timescale learning
   - Meta-learning of hyperparameters
   - Transfer learning across regions

5. **Advanced Attention**
   - Context-dependent precision
   - Salience-driven attention
   - Predictive attention shifts

## References

### Core Papers

1. **Friston, K. (2010)**. "The free-energy principle: a unified brain theory?"
   - *Nature Reviews Neuroscience*, 11(2), 127-138.

2. **Rao, R. P., & Ballard, D. H. (1999)**. "Predictive coding in the visual cortex: a functional interpretation of some extra-classical receptive-field effects."
   - *Nature Neuroscience*, 2(1), 79-87.

3. **Bastos, A. M., et al. (2012)**. "Canonical microcircuits for predictive coding."
   - *Neuron*, 76(4), 695-711.

4. **Friston, K., & Kiebel, S. (2009)**. "Predictive coding under the free-energy principle."
   - *Philosophical Transactions of the Royal Society B*, 364(1521), 1211-1221.

### Implementation Details

- All code follows NIMCP coding standards
- WHAT/WHY/HOW documentation
- Functions < 50 lines
- Guard clauses first
- No stubs - complete implementation

## Summary

This implementation provides a **complete, biologically-inspired hierarchical predictive processing system** integrated with NIMCP's brain regions architecture. Key features:

✅ **Hierarchical Prediction**: Multi-level predictive networks
✅ **Free Energy Minimization**: Implements Friston's Free Energy Principle
✅ **Attention Modulation**: Precision-weighted error processing
✅ **Bio-Async Integration**: Message-based hierarchical communication
✅ **Security Integration**: BBB monitoring and audit trails
✅ **Comprehensive Testing**: 43 tests covering unit, integration, and regression
✅ **Production Ready**: Complete implementation, no stubs

The system enables cortical regions to:
- Generate top-down predictions
- Compute bottom-up prediction errors
- Minimize surprise through iterative inference
- Learn attention (precision) weights automatically
- Communicate via biologically-inspired messaging

This forms the foundation for implementing predictive processing across NIMCP's cognitive architecture, enabling more sophisticated perception, action, and learning capabilities.
