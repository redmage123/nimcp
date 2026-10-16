# FEP Bridge Integration Tests Summary

## Overview
Comprehensive integration tests for Free Energy Principle (FEP) system with immune bridge modules, testing cross-bridge interactions and full pipeline integration.

## Test Files Created
- **test_fep_cognitive_bridges_integration.cpp** (552 lines, 19 tests)
- **test_fep_perception_bridges_integration.cpp** (569 lines, 21 tests)
- **test_fep_plasticity_bridges_integration.cpp** (632 lines, 23 tests)
- **test_fep_bridges_full_pipeline_integration.cpp** (703 lines, 21 tests)

**Total: 2,456 lines, 84 integration tests**

## Test Coverage by Category

### 1. Cognitive Bridges Integration (19 tests)
Tests FEP integration with cognitive immune bridges:
- **Attention-FEP** (3 tests)
  - Precision propagation
  - Prediction error narrowing attention
  - Bidirectional attention modulation
  
- **Memory-FEP** (3 tests)
  - Inflammation impairs both systems
  - Belief updates mirror memory formation
  - Memory recall affects FEP priors
  
- **Emotion-FEP** (3 tests)
  - Prediction error triggers emotional response
  - Emotional state modulates precision
  - IL-10 convergence reduces negative emotion
  
- **Executive-FEP** (3 tests)
  - Inflammation impairs executive and policies
  - Policy evaluation under inflammation
  - Executive control modulates action selection

- **Multi-Bridge Coordination** (3 tests)
  - All bridges synchronize with inflammation
  - Prediction error cascades through cognition
  - Recovery affects all cognitive systems

- **Belief-Cognition Integration** (2 tests)
  - Belief updates with cognitive constraints
  - Cognitive state informs FEP precision

- **Performance Under Inflammation** (2 tests)
  - Cognitive degradation progression
  - Cytokine storm delirium model

### 2. Perception Bridges Integration (21 tests)
Tests FEP integration with perception immune bridges:
- **Visual-FEP** (4 tests)
  - Visual PE triggers immune
  - Inflammation impairs visual processing
  - Visual attention modulates FEP priors
  - Visual contrast affected by inflammation

- **Audio-FEP** (3 tests)
  - Audio prediction error detection
  - Inflammation impairs auditory processing
  - Auditory stream segmentation with inflammation

- **Speech-FEP** (3 tests)
  - Speech prediction and parsing
  - Inflammation impairs speech comprehension
  - Phoneme recognition precision

- **Cross-Modal Integration** (3 tests)
  - Multimodal prediction error integration
  - Sensory precision weighting across modalities
  - Audio-visual binding with inflammation

- **Sensory Gain Control** (2 tests)
  - Inflammation reduces sensory gain
  - IL-10 boosts sensory recovery

- **Prediction-Perception Loop** (2 tests)
  - Top-down predictions modulate sensing
  - Sensory error propagation to beliefs

- **Sickness Behavior** (2 tests)
  - Sickness reduces sensory engagement
  - Inflammation progression in perception

- **Recovery and Adaptation** (2 tests)
  - Sensory recovery after inflammation
  - Adaptation to chronic inflammation

### 3. Plasticity Bridges Integration (23 tests)
Tests FEP integration with plasticity immune bridges:
- **STDP-FEP** (3 tests)
  - Prediction error scales STDP
  - Inflammation impairs STDP
  - FEP learning with STDP modulation

- **BCM-FEP** (3 tests)
  - BCM threshold affects FEP learning
  - Inflammation shifts BCM threshold
  - BCM metaplasticity in FEP

- **Homeostatic-FEP** (3 tests)
  - Homeostatic scaling maintains FEP stability
  - Inflammation disrupts homeostasis
  - FEP convergence with homeostasis

- **Synaptic Scaling-FEP** (3 tests)
  - Synaptic scaling normalizes FEP weights
  - Inflammation impairs scaling
  - Global scaling preserves FEP ratios

- **Multi-Plasticity Coordination** (3 tests)
  - All plasticity mechanisms coordinate
  - Plasticity hierarchy with FEP
  - Inflammation affects all plasticity levels

- **Learning Under Inflammation** (3 tests)
  - Learning progression with inflammation
  - Learning rate modulation by inflammation
  - Plasticity recovery after inflammation

- **PE-Scaled Plasticity** (2 tests)
  - Prediction error scales all plasticity
  - Low PE suppresses plasticity

- **IL-10 Recovery** (1 test)
  - IL-10 boosts plasticity recovery

- **Metaplasticity** (2 tests)
  - FEP metaplasticity via immune
  - Long-term plasticity stability

### 4. Full Pipeline Integration (21 tests)
End-to-end tests with multiple bridges:
- **Complete Pipeline** (3 tests)
  - Observation through full pipeline
  - High prediction error cascade
  - Belief update with cross-system modulation

- **Active Inference Pipeline** (3 tests)
  - Action selection with bridge constraints
  - Policy evaluation under inflammation
  - Active inference with attention gating

- **Learning Pipeline** (3 tests)
  - Learning with multi-system modulation
  - Plasticity constrained by inflammation
  - Memory consolidation in FEP

- **Cross-Modal Integration** (2 tests)
  - Visual-attention-memory integration
  - Perception-plasticity loop

- **Inflammation Cascade** (3 tests)
  - System-wide inflammation cascade
  - Prediction error immune activation
  - Recovery propagation through system

- **Emergent Behavior** (3 tests)
  - Sickness behavior emergence
  - Attentional narrowing under threat
  - Learning adaptation to environment

- **Performance Under Stress** (2 tests)
  - Cytokine storm system breakdown
  - Graceful degradation under load

- **Long-Term Dynamics** (2 tests)
  - Extended processing session
  - Chronic inflammation adaptation

## Key Integration Scenarios Tested

### 1. Attention-Memory-FEP
Attention focus affects memory precision which feeds back to FEP:
- Attention narrowing reduces memory capacity
- Memory consolidation parallels belief updates
- Inflammation affects both systems similarly

### 2. Visual-FEP-Attention
Visual PE triggers attention shift via FEP:
- Unexpected visual input generates high PE
- PE activates immune response
- Inflammation narrows attention to threat

### 3. STDP-FEP-Learning
PE-scaled learning through STDP bridge:
- High PE enhances STDP plasticity
- Inflammation suppresses both FEP and STDP learning
- Learning rate tracks prediction error magnitude

### 4. Oscillations-FEP-Precision
(Note: Oscillations bridge not included in current tests)
Gamma/alpha ratio drives precision:
- High gamma → high precision weighting
- Low alpha → reduced inhibition

### 5. Full Pipeline
Observation → FEP → bridges → belief update → action selection:
- Complete sensory-cognitive-motor loop
- All bridges coordinate via shared immune substrate
- Emergent sickness behavior under inflammation

## Biological Realism Features

### Inflammation Effects
- **Local** (5% reduction): Mild impairment
- **Regional** (15-20% reduction): Moderate impairment
- **Systemic** (40-50% reduction): Severe impairment (fever model)
- **Storm** (70-90% reduction): Delirium model

### Cytokine Modulation
- **IL-6**: -30% precision, -25% learning
- **TNF-α**: -40% precision, -35% learning
- **IL-1β**: -20% precision, -15% learning
- **IL-10**: +20% precision, +15% learning (recovery)
- **IFN-γ**: -15% precision (quarantine signaling)

### Sickness Behavior
- Reduced exploration (low precision)
- Impaired learning (energy conservation)
- Attention narrowing (threat focus)
- Sensory withdrawal

### Recovery Mechanisms
- IL-10 release on FEP convergence
- Anti-inflammatory cascade
- Gradual restoration of function

## Test Patterns

### Standard Test Structure
```cpp
class FEP[Category]BridgesTest : public ::testing::Test {
protected:
    fep_system_t* fep;
    brain_immune_system_t* immune;
    fep_immune_bridge_t* fep_immune_bridge;
    [category]_immune_bridge_t* bridge;
    
    void SetUp() override { /* Create all systems */ }
    void TearDown() override { /* Destroy all systems */ }
};

TEST_F(FEP[Category]BridgesTest, TestName) {
    /* WHAT: What is being tested */
    /* WHY:  Biological/theoretical rationale */
    /* HOW:  Test implementation */
    
    // Test code...
}
```

### Helper Functions
- `updateAllBridges()`: Update all bridge systems
- `fullProcessingCycle()`: Complete observation → belief update
- `simulateSensoryPredictionError()`: Generate high PE
- `getInflammationLevel()`: Query current inflammation

## Build Integration

### CMakeLists.txt
```cmake
add_executable(integration_fep_enhanced
    # ... existing tests ...
    test_fep_cognitive_bridges_integration.cpp
    test_fep_perception_bridges_integration.cpp
    test_fep_plasticity_bridges_integration.cpp
    test_fep_bridges_full_pipeline_integration.cpp
)
```

### Build Command
```bash
cd /home/bbrelin/nimcp/build
cmake ..
make integration_fep_enhanced -j4
```

### Run Tests
```bash
./test/integration/cognitive/free_energy/integration_fep_enhanced --gtest_brief=1
```

## Expected Test Outcomes

### Success Criteria
- All 84 tests should pass
- No segmentation faults
- No memory leaks (valgrind clean)
- Consistent behavior across inflammation levels

### Test Categories
- **Connectivity**: Bridge connections work
- **Modulation**: Inflammation/cytokines affect systems
- **Cascade**: Effects propagate across bridges
- **Recovery**: IL-10 improves function
- **Stability**: Long-term operation stable
- **Emergent**: Sickness behavior emerges naturally

## Dependencies

### Core Headers
- `cognitive/free_energy/nimcp_free_energy.h`
- `cognitive/free_energy/nimcp_fep_immune_bridge.h`
- `cognitive/free_energy/nimcp_fep_learning.h`
- `cognitive/immune/nimcp_brain_immune.h`

### Bridge Headers
- Cognitive: attention, memory, emotion, executive
- Perception: visual, audio, speech
- Plasticity: STDP, BCM, homeostatic, synaptic_scaling

### Libraries
- nimcp (main library)
- GTest (testing framework)
- Threads (multi-threading)
- m (math library)

## Future Extensions

### Additional Tests
1. **Oscillations-FEP integration**: Gamma/alpha precision modulation
2. **Neuromodulator-FEP**: DA/NE/5HT effects on learning
3. **Sleep-FEP**: Offline consolidation and replay
4. **Thalamic routing**: Precision-gated information flow

### Performance Tests
1. Throughput benchmarks
2. Memory usage profiling
3. Latency measurements
4. Scalability tests

### Stress Tests
1. Extreme inflammation scenarios
2. Rapid state transitions
3. Concurrent updates
4. Resource exhaustion

## Summary Statistics

| Category | Tests | Lines | Key Focus |
|----------|-------|-------|-----------|
| Cognitive Bridges | 19 | 552 | Attention, memory, emotion, executive |
| Perception Bridges | 21 | 569 | Visual, audio, speech processing |
| Plasticity Bridges | 23 | 632 | STDP, BCM, homeostatic, scaling |
| Full Pipeline | 21 | 703 | End-to-end integration |
| **TOTAL** | **84** | **2,456** | **Comprehensive integration** |

## Conclusion

These integration tests provide comprehensive coverage of FEP bridge interactions, testing:
- ✓ Bidirectional information flow (FEP ↔ Bridges)
- ✓ Inflammation cascade effects
- ✓ Multi-system coordination
- ✓ Emergent sickness behavior
- ✓ Recovery mechanisms
- ✓ Long-term stability
- ✓ Biological realism

All tests follow NIMCP standards with WHAT-WHY-HOW documentation and guard clause patterns.
