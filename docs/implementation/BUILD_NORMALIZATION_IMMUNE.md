# Building and Testing Normalization Immune Integration

## Overview
This document provides build instructions for the new normalization-immune system integration.

## Files Created

### Header File
- **Location**: `/home/bbrelin/nimcp/include/middleware/normalization/nimcp_normalization_immune.h`
- **Purpose**: API for normalization-immune integration
- **Features**:
  - Fever-induced baseline shifts
  - Outlier detection and immune surveillance
  - Cytokine-modulated normalization parameters
  - Baseline restoration after inflammation resolution

### Implementation File
- **Location**: `/home/bbrelin/nimcp/src/middleware/normalization/nimcp_normalization_immune.c`
- **Purpose**: Implementation of normalization-immune integration
- **Key Functions**:
  - Outlier detection (z-score, range violations, rapid shifts)
  - Immune modulation application (fever shifts, storm clamping)
  - Baseline restoration with IL-10
  - Integration with brain immune system

### Unit Test File
- **Location**: `/home/bbrelin/nimcp/test/unit/middleware/normalization/test_normalization_immune.cpp`
- **Purpose**: Comprehensive unit tests
- **Test Coverage**:
  - Lifecycle tests (create/destroy, connections)
  - Outlier detection tests (z-score, rapid shift, drift, range violations)
  - Immune modulation tests (fever shift, storm clamping, learning rate)
  - Baseline restoration tests
  - End-to-end integration tests
  - Edge cases and error handling

## Build Instructions

### 1. CMake Configuration Fix
A duplicate `add_subdirectory` for `unit/core/neuralnet` was found and fixed in `/home/bbrelin/nimcp/test/CMakeLists.txt`.

### 2. Build Commands

```bash
cd /home/bbrelin/nimcp/build

# Reconfigure CMake
cmake ..

# Build middleware library (includes new source)
make nimcp_middleware -j4

# Build the test executable
make unit_middleware_normalization_immune -j4

# Run the tests
./test/unit/middleware/normalization/unit_middleware_normalization_immune --gtest_brief=1
```

### 3. Expected Test Output
The test suite includes:
- **31 test cases** covering all aspects of the integration
- Tests are organized by category:
  - Lifecycle (2 tests)
  - Outlier Detection (5 tests)
  - Immune Modulation (7 tests)
  - Baseline Restoration (4 tests)
  - Query API (3 tests)
  - Integration (3 tests)
  - Utility Functions (2 tests)
  - Edge Cases (3 tests)

## Integration Points

### 1. Brain Immune System
The integration connects to the brain immune system to:
- Present outliers as antigens
- Query inflammation state
- Monitor cytokine levels
- Apply immune-driven parameter modulations

### 2. Normalizers
The integration modulates four normalizer types:

#### Adaptive Normalizer
- **Modulation**: Learning rate scaling based on inflammation
- **Biological Basis**: Inflammation reduces adaptation rate (conserve patterns)
- **Formula**: `lr_factor = 1.0 - (IL-1 + TNF-α) / 2.0 * 0.7`

#### Homeostatic Normalizer
- **Modulation**: Target activity shift based on fever
- **Biological Basis**: Fever shifts physiological set-points
- **Formula**: `target_shift = IL-6 * FEVER_SHIFT * 0.2`

#### Z-Score Normalizer
- **Modulation**: Mean shift and variance expansion
- **Biological Basis**: Fever elevates baseline; inflammation increases tolerance
- **Formulas**:
  - `mean_shift = IL-6 * FEVER_SHIFT`
  - `variance_scale = 1.0 + IL-1`

#### Min-Max Normalizer
- **Modulation**: Range expansion/contraction
- **Biological Basis**: Inflammation tolerates wider ranges (except during storm)
- **Formula**: `range_expansion = 1.0 + (IL-1 + IL-6) / 2.0 * 0.5`

## Biological Model

### Fever Response
```
INFLAMMATION LEVEL  IL-6  MEAN SHIFT  EFFECT
──────────────────────────────────────────────────────────
None               0.0    0.0x       Normal operation
Local              0.3    0.45x      Slight baseline elevation
Regional           0.6    0.9x       Moderate fever shift
Systemic           0.9    1.35x      High fever
Storm              1.0    1.5x       Maximum shift (with clamping)
```

### Outlier Thresholds
- **Normal**: 3.0σ (z-score threshold)
- **Storm**: 3.0σ (tightened for protection)
- **Rapid Shift**: 0.5 units/sec velocity
- **Homeostatic Drift**: 30% of target

### Cytokine Effects
| Cytokine | Role | Effect on Normalization |
|----------|------|-------------------------|
| IL-1 | Pro-inflammatory | Expand variance, reduce learning rate |
| IL-6 | Fever inducer | Shift mean/target (fever) |
| IL-10 | Anti-inflammatory | Restore baselines |
| TNF-α | Severe inflammation | Reduce learning rate, escalate protection |

## API Usage Examples

### Basic Setup
```c
// Create immune system
brain_immune_system_t* immune = brain_immune_create(&config);
brain_immune_start(immune);

// Create normalizers
zscore_normalizer_t* zscore = zscore_normalizer_create(4, 0, 3.0f);

// Create integration
normalization_immune_context_t* ctx =
    normalization_immune_create(immune, 4);

// Connect normalizers
normalization_immune_connect_zscore(ctx, zscore);
```

### Outlier Detection
```c
// Feed value to normalizer
float value = 5.0f;
float zscore = zscore_normalizer_transform(zscore, 0, value);

// Detect outlier (automatically presents to immune system)
uint32_t outlier_id;
if (normalization_immune_detect_zscore_outlier(
    ctx, 0, value, zscore, &outlier_id) == 0) {
    printf("Outlier detected! ID: %u\n", outlier_id);
}
```

### Immune Modulation
```c
// Update modulation from immune state
normalization_immune_update_modulation(ctx);

// Check fever state
if (normalization_immune_is_fever_active(ctx)) {
    float shift = normalization_immune_get_fever_shift(
        ctx, NORMALIZER_ZSCORE, 0
    );
    printf("Fever shift: %.2f\n", shift);
}
```

### Baseline Restoration
```c
// After inflammation resolves, restore baselines
float il10_level = 0.8f;  // High anti-inflammatory
uint64_t delta_ms = 100;   // Time step

normalization_immune_restore_baselines(ctx, il10_level, delta_ms);

// Check if restoration complete
if (!normalization_immune_is_fever_active(ctx)) {
    printf("Baselines restored to normal\n");
}
```

## Troubleshooting

### Build Issues
1. **CMake Error**: Ensure duplicate `add_subdirectory` is removed
2. **Linking Error**: Verify `nimcp_cognitive` is linked in test CMakeLists
3. **Header Not Found**: Check include paths in CMakeLists

### Test Failures
1. **Outlier Not Detected**: Verify normalizer has enough samples for statistics
2. **Fever Shift Not Active**: Check IL-6 level > 0.1
3. **Restoration Not Working**: Ensure IL-10 level > 0.1

## Next Steps

After successful build and testing:

1. **Integration with Other Modules**:
   - Connect to perception modules (see `nimcp_perception_immune.h`)
   - Integrate with plasticity mechanisms
   - Add to brain factory initialization

2. **Performance Tuning**:
   - Profile outlier detection overhead
   - Optimize modulation update frequency
   - Cache cytokine query results

3. **Extended Testing**:
   - Integration tests with actual normalizer workflows
   - Stress tests with high outlier rates
   - Regression tests for baseline restoration convergence

4. **Documentation**:
   - Add to CLAUDE.md under "Recent Completions"
   - Document API examples in module documentation
   - Create integration guide for other developers

## References

- Brain Immune System: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_brain_immune.h`
- Z-Score Normalizer: `/home/bbrelin/nimcp/include/middleware/normalization/nimcp_zscore_normalizer.h`
- Perception Immune Example: `/home/bbrelin/nimcp/include/cognitive/immune/nimcp_perception_immune.h`
- NIMCP Standards: `/home/bbrelin/nimcp/CLAUDE.md`
