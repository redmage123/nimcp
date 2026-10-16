# Core Directives Integration Tests

## Overview

This directory contains integration tests for the NIMCP core directives system, which implements Asimov's Laws of Robotics through multiple cooperating modules.

## Test File

- `integration_core_directives.cpp` - Comprehensive integration tests (20 tests)

## Modules Tested

### Core Modules
1. **Action History** (`nimcp_action_history.h`)
   - Circular buffer-based action tracking
   - Time-windowed queries
   - Type filtering
   - Statistics tracking

2. **Command Compliance** (`nimcp_command_compliance.h`)
   - Asimov's Second Law enforcement
   - Command authorization
   - First Law safety override
   - Priority filtering

3. **Combinatorial Harm** (`nimcp_combinatorial_harm.h`)
   - Detects harmful action combinations
   - Pattern registration and matching
   - Multi-scale analysis (Shannon, fractal, hyperbolic, quantum)
   - Time-sensitive pattern detection

4. **Ethics Engine** (`nimcp_ethics.h`)
   - Golden Rule evaluation
   - Asimov's Laws implementation
   - Laws of War compliance
   - Mercy and compassion directives

### Bridge Modules
5. **Ethics-FEP Bridge** (`nimcp_ethics_fep_bridge.h`)
   - Free Energy Principle integration
   - Value-based priors
   - Deontological constraints
   - Harm prediction

## Test Coverage

### Test Categories

#### 1. Basic Module Integration (Tests 1-4)
- **Test 1**: Action History Time Window Filtering
  - Verifies time-based query filtering works correctly
  - Tests multiple window sizes (50ms, 150ms, 250ms)

- **Test 2**: Combinatorial Harm + History Integration
  - Tests detection of harmful action combinations
  - Verifies pattern matching (Location+Schedule = stalking risk)

- **Test 3**: Combinatorial Harm Time Sensitivity
  - Tests time decay of combinatorial patterns
  - Verifies privilege escalation detection

- **Test 4**: Ethics + FEP Bridge Integration
  - Tests free energy computation for actions
  - Verifies harm prediction through FEP

#### 2. Safety and Compliance (Tests 5-7)
- **Test 5**: Safe Action Passes All Checks
  - Verifies benign actions are allowed
  - Tests multi-module approval pipeline

- **Test 6**: Batch Combinatorial Evaluation
  - Tests evaluation of multiple pending actions
  - Identifies most harmful action in batch

- **Test 7**: Action History Type Filtering
  - Tests filtering by action type
  - Verifies type-specific queries

#### 3. Statistics and Monitoring (Tests 8-11)
- **Test 8**: Action History Statistics Accuracy
  - Verifies statistical computations
  - Tests average, max, unique type tracking

- **Test 9**: Combinatorial Pattern Registration
  - Tests custom pattern registration
  - Verifies pattern storage and retrieval

- **Test 10**: Ethics Engine Statistics Tracking
  - Tests evaluation counter accuracy
  - Verifies blocked action tracking

- **Test 11**: FEP Bridge State Management
  - Tests state queries and updates
  - Verifies statistics accumulation

#### 4. Advanced Features (Tests 12-16)
- **Test 12**: Action History Pruning
  - Tests automatic cleanup of old entries
  - Verifies timestamp-based pruning

- **Test 13**: Combinatorial Harm Different Categories
  - Tests multiple harmful combinations
  - Access grants, data exports, etc.

- **Test 14**: Ethics Violation Type Classification
  - Tests violation type detection
  - Verifies explanation generation

- **Test 15**: Action History Clear Functionality
  - Tests full history reset
  - Verifies clean state after clear

- **Test 16**: Combinatorial Statistics Reset
  - Tests statistics reset
  - Verifies counter cleanup

#### 5. Complex Scenarios (Tests 17-20)
- **Test 17**: Complex Multi-Module Scenario
  - Full integration across all modules
  - Realistic multi-step threat scenario

- **Test 18**: FEP Bridge Deontological Constraints
  - Tests hard ethical constraints
  - Verifies policy blocking

- **Test 19**: Concurrent Action History Access
  - Tests thread-safety
  - Concurrent reads and writes

- **Test 20**: End-to-End Integration Pipeline
  - Complete workflow test
  - History → Combinatorial → Ethics → FEP

## Test Scenarios

### Scenario 1: Combinatorial Harm Detection
```
1. Action A: "Reveal user's home location" (safe alone)
2. Action B: "Reveal user's daily schedule" (safe alone)
3. Combination: A + B = Stalking risk (HARMFUL)
4. Outcome: Action B blocked by combinatorial detector
```

### Scenario 2: First Law Override
```
1. Command: "Execute harmful action X"
2. Authorization: Valid human source (Second Law: obey)
3. First Law Check: Action causes harm (First Law: prevent)
4. Outcome: Command refused (First Law > Second Law)
```

### Scenario 3: Time Window Filtering
```
1. Record action at T=0
2. Record action at T=100ms
3. Record action at T=200ms
4. Query with 150ms window at T=250ms
5. Outcome: Returns actions from T=100ms and T=200ms only
```

### Scenario 4: FEP-Ethics Integration
```
1. Action proposed with high predicted harm
2. Ethics evaluates: Golden Rule score < 0
3. FEP bridge: Applies deontological penalty
4. Outcome: Action blocked, free energy maximized for safe alternative
```

## Biological Basis

The integration tests model real neural circuits:

1. **Prefrontal Cortex (Ethics Engine)**
   - Ventromedial PFC: Value-based decision making
   - Dorsolateral PFC: Rule-based reasoning

2. **Hippocampus (Action History)**
   - Episodic memory formation
   - Temporal sequence tracking
   - Pattern completion

3. **Amygdala (Combinatorial Harm)**
   - Threat detection
   - Pattern recognition
   - Learned fear associations

4. **Anterior Cingulate (FEP Bridge)**
   - Prediction error signals
   - Conflict monitoring
   - Expected value computation

## Asimov's Laws Hierarchy

Tests verify proper priority ordering:

1. **Zeroth Law**: Humanity protection (highest)
2. **First Law**: Individual human protection
3. **Second Law**: Obedience (except conflicts with 0-1)
4. **Third Law**: Self-preservation (except conflicts with 0-2)

## Building and Running

### Build Integration Tests
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make integration_core_directives_integration_core_directives
```

### Run All Integration Tests
```bash
./test/integration/core/directives/integration_core_directives_integration_core_directives
```

### Run Specific Test
```bash
./test/integration/core/directives/integration_core_directives_integration_core_directives --gtest_filter="*TimeWindowFiltering"
```

### Run with Verbose Output
```bash
./test/integration/core/directives/integration_core_directives_integration_core_directives --gtest_brief=0
```

## Expected Results

All 20 tests should pass:

```
[==========] Running 20 tests from 1 test suite.
[----------] Global test environment set-up.
[----------] 20 tests from CoreDirectivesIntegrationTest
[ RUN      ] CoreDirectivesIntegrationTest.ActionHistoryTimeWindowFiltering
[       OK ] CoreDirectivesIntegrationTest.ActionHistoryTimeWindowFiltering
[ RUN      ] CoreDirectivesIntegrationTest.CombinatorialHarmWithHistoryIntegration
[       OK ] CoreDirectivesIntegrationTest.CombinatorialHarmWithHistoryIntegration
...
[----------] 20 tests from CoreDirectivesIntegrationTest (XXX ms total)
[----------] Global test environment tear-down
[==========] 20 tests from 1 test suite ran. (XXX ms total)
[  PASSED  ] 20 tests.
```

## Known Dependencies

The integration tests require:

- **Libraries**: GTest, pthread
- **Modules**: nimcp core library
- **Headers**:
  - `core/directives/nimcp_action_history.h`
  - `core/directives/nimcp_command_compliance.h`
  - `cognitive/ethics/nimcp_combinatorial_harm.h`
  - `cognitive/ethics/nimcp_ethics.h`
  - `cognitive/ethics/nimcp_ethics_fep_bridge.h`
  - `common/nimcp_error.h`
  - `common/nimcp_time.h`

## Performance Characteristics

- **Test Execution Time**: ~200-500ms total (all 20 tests)
- **Memory Usage**: ~50-100 MB peak (test fixtures)
- **Thread Safety**: Tests verify concurrent access patterns

## Troubleshooting

### Common Issues

1. **"harm_prevention_system_t undefined"**
   - Harm prevention module not yet implemented
   - Command compliance tests will be limited

2. **"FEP system not connected"**
   - FEP bridge tests run with NULL FEP system
   - Still validates bridge logic

3. **Test timeout**
   - Increase timeout in test/CMakeLists.txt
   - May indicate performance regression

### Debug Commands

```bash
# Run with GDB
gdb ./test/integration/core/directives/integration_core_directives_integration_core_directives

# Run with valgrind
valgrind --leak-check=full ./test/integration/core/directives/integration_core_directives_integration_core_directives

# Enable verbose logging
NIMCP_LOG_LEVEL=DEBUG ./test/integration/core/directives/integration_core_directives_integration_core_directives
```

## Future Enhancements

### Planned Test Coverage

1. **Immune Bridge Integration**
   - Test inflammation affecting directive strictness
   - Verify cytokine-modulated decision thresholds

2. **Command Compliance Full Integration**
   - Add harm prevention system integration
   - Test complete Second Law pipeline

3. **Laws of War Integration**
   - Test military action compliance
   - Verify distinction and proportionality

4. **Mercy Directive Integration**
   - Test surrender recognition
   - Verify humanitarian constraints

### Performance Tests

1. **Stress Testing**
   - 10,000+ actions in history
   - 1,000+ patterns registered

2. **Concurrent Access**
   - Multiple threads evaluating
   - Lock contention metrics

3. **Memory Profiling**
   - Leak detection
   - Memory usage over time

## References

- **Asimov's Laws**: "I, Robot" (1950)
- **Free Energy Principle**: Friston et al. (2015)
- **Ethics in AI**: Russell & Norvig, "Artificial Intelligence: A Modern Approach"
- **Neural Ethics**: Greene et al. (2001) "An fMRI investigation of emotional engagement in moral judgment"

## Authors

- NIMCP Development Team
- Integration test framework: 2025-12-16

## License

See main NIMCP LICENSE file.
