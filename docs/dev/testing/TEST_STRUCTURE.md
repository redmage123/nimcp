# NIMCP Test Directory Structure

## Overview
The NIMCP test suite has been reorganized into a modular structure where tests are organized by the module they test, making it easier to locate, maintain, and understand the test coverage for each component.

## Directory Structure

```
test/
├── unit/                    # Unit tests (215 tests)
│   ├── cognitive/          # Cognitive module tests
│   │   ├── bias/
│   │   ├── consolidation/
│   │   ├── curiosity/
│   │   ├── emotional_tagging/
│   │   ├── epistemic/
│   │   ├── ethics/
│   │   ├── executive/
│   │   ├── explanations/
│   │   ├── global_workspace/
│   │   ├── grief/
│   │   ├── joy/
│   │   ├── knowledge/
│   │   ├── logic/
│   │   ├── memory/
│   │   ├── mental_health/
│   │   ├── meta_learning/
│   │   ├── mirror_neurons/
│   │   ├── personality/
│   │   ├── predictive/
│   │   ├── salience/
│   │   ├── sleep_wake/
│   │   ├── theory_of_mind/
│   │   └── wellbeing/
│   ├── core/               # Core system tests
│   │   ├── brain/
│   │   ├── brain_regions/
│   │   ├── integration/
│   │   ├── neuralnet/
│   │   ├── neuron_models/
│   │   ├── synapse_types/
│   │   └── topology/
│   ├── glial/              # Glial cell tests
│   │   ├── astrocytes/
│   │   ├── microglia/
│   │   └── oligodendrocytes/
│   ├── gpu/                # GPU acceleration tests
│   ├── information/        # Information theory tests
│   ├── io/                 # I/O module tests
│   ├── networking/         # Distributed computing tests
│   │   ├── distributed/
│   │   ├── events/
│   │   ├── p2p/
│   │   └── replication/
│   ├── nlp/                # NLP and multimodal tests
│   ├── optimization/       # Optimization tests
│   │   └── quantum_annealing/
│   ├── plasticity/         # Synaptic plasticity tests
│   │   ├── adaptive/
│   │   ├── attention/
│   │   ├── bcm/
│   │   ├── eligibility/
│   │   ├── neuromodulators/
│   │   ├── noise/
│   │   ├── stdp/
│   │   └── stp/
│   ├── security/           # Security framework tests
│   └── utils/              # Utility tests
│       ├── cache/
│       ├── config/
│       ├── containers/
│       ├── geometry/
│       ├── memory/
│       ├── numerical/
│       ├── platform/
│       ├── quantum/
│       ├── queue_manager/
│       ├── tensor_networks/
│       └── thread/
│
├── integration/            # Integration tests (40 tests)
│   ├── cognitive/
│   │   ├── consolidation/
│   │   ├── global_workspace/
│   │   ├── grief/
│   │   ├── joy/
│   │   ├── logic/
│   │   └── memory/
│   ├── core/
│   │   ├── brain/
│   │   ├── brain_regions/
│   │   ├── integration/
│   │   └── topology/
│   ├── glial/
│   ├── gpu/
│   ├── information/
│   ├── io/
│   ├── nlp/
│   ├── optimization/
│   │   └── quantum_annealing/
│   ├── plasticity/
│   │   ├── adaptive/
│   │   ├── attention/
│   │   ├── bcm/
│   │   ├── eligibility/
│   │   ├── neuromodulators/
│   │   └── stp/
│   └── utils/
│       └── tensor_networks/
│
├── regression/             # Regression tests (32 tests)
│   ├── cognitive/
│   │   ├── consolidation/
│   │   ├── global_workspace/
│   │   ├── grief/
│   │   ├── joy/
│   │   ├── logic/
│   │   └── memory/
│   ├── core/
│   │   ├── brain_regions/
│   │   └── integration/
│   ├── gpu/
│   ├── information/
│   ├── io/
│   ├── optimization/
│   │   └── quantum_annealing/
│   ├── plasticity/
│   │   ├── adaptive/
│   │   ├── attention/
│   │   ├── bcm/
│   │   ├── eligibility/
│   │   ├── neuromodulators/
│   │   ├── noise/
│   │   └── stp/
│   └── utils/
│
├── e2e/                    # End-to-end tests (1 test)
├── fuzz/                   # Fuzz tests
├── fixtures/               # Test fixtures
├── mocks/                  # Mock objects
└── utils/                  # Test utilities
```

## Test Categories

### Unit Tests (215 tests)
Located in `test/unit/<module>/`

Unit tests verify individual components in isolation. Each module's unit tests are in their respective subdirectory.

**Example:**
- `test/unit/security/test_security.cpp` - Security module unit tests
- `test/unit/plasticity/bcm/test_bcm.cpp` - BCM plasticity unit tests
- `test/unit/cognitive/ethics/test_ethics.cpp` - Ethics module unit tests

### Integration Tests (40 tests)
Located in `test/integration/<module>/`

Integration tests verify interactions between multiple components.

**Example:**
- `test/integration/plasticity/attention/test_attention_integration.cpp`
- `test/integration/core/brain/test_brain_integration.cpp`

### Regression Tests (32 tests)
Located in `test/regression/<module>/`

Regression tests ensure backward compatibility and prevent bugs from reappearing.

**Example:**
- `test/regression/plasticity/bcm/test_bcm_backward_compat.cpp`
- `test/regression/security/test_security_regression.cpp`

## Building and Running Tests

### Build All Tests
```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

### Build Specific Module Tests
```bash
# Build all security unit tests
cmake --build build --target unit_security_test_security

# Build all BCM plasticity tests
cmake --build build --target unit_plasticity_bcm_test_bcm
```

### Run Tests with CTest

```bash
# Run all tests
cd build
ctest -j$(nproc)

# Run only unit tests
ctest -L unit -j$(nproc)

# Run only integration tests
ctest -L integration -j$(nproc)

# Run only regression tests
ctest -L regression -j$(nproc)

# Run tests for a specific module (example: security)
ctest -R unit_security

# Run with verbose output
ctest -V
```

### Run Individual Test Executables

```bash
# From build directory
./test/unit_security_test_security
./test/unit_plasticity_bcm_test_bcm
./test/integration_core_brain_test_brain_integration
```

## Test Naming Convention

Test executables follow this naming pattern:
```
<category>_<module_path>_<test_name>
```

**Examples:**
- `unit_security_test_security` - Unit test for security module
- `unit_plasticity_bcm_test_bcm` - Unit test for BCM plasticity
- `integration_core_brain_test_brain_integration` - Integration test for core brain
- `regression_plasticity_adaptive_test_adaptive_backward_compat` - Regression test for adaptive plasticity

The module path uses underscores to represent directory separators.

## Benefits of Modular Structure

1. **Easy Navigation**: Find tests for a specific module quickly
2. **Clear Organization**: Tests are grouped with related functionality
3. **Scalability**: Easy to add new tests without cluttering directories
4. **Parallel Execution**: CTest can run module tests in parallel
5. **Selective Testing**: Run tests for only the modules you're working on
6. **Code Reviews**: Easier to review test changes alongside source changes
7. **Coverage Analysis**: Analyze coverage per module

## Migration Notes

- All existing tests have been reorganized from flat directories into module-specific subdirectories
- Test file names and content remain unchanged
- Build system automatically discovers tests recursively in subdirectories
- Backup of original structure: `test_backup_YYYYMMDD_HHMMSS.tar.gz`

## CMake Configuration

The test build system uses recursive test discovery:

```cmake
# Recursively find all test files in category subdirectories
file(GLOB_RECURSE TEST_FILES "${CATEGORY_DIR}/*.cpp" "${CATEGORY_DIR}/*.c")

# Create unique target names based on module path
# Example: unit/security/test_security.cpp -> unit_security_test_security
```

This allows adding new tests simply by placing them in the appropriate module directory - no CMake configuration changes needed.
