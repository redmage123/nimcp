# NIMCP True Extrapolation - Complete Architecture

This directory contains the complete architectural and implementation specification for adding **true extrapolation capabilities** to NIMCP.

## Overview

Current NIMCP is a sophisticated pattern recognition system with symbolic logic capabilities, but is bounded by its training distribution. This architecture adds genuine extrapolation capabilities through:

1. **Causal Reasoning** - Distinguish causation from correlation
2. **Analogical Transfer** - Cross-domain knowledge transfer
3. **Compositional Generalization** - Infinite expressions from finite primitives
4. **Meta-Learning** - Learn to learn, few-shot adaptation
5. **World Models** - Mental simulation and planning
6. **Concept Formation** - Generate novel concepts
7. **Semantic Memory** - Large-scale knowledge
8. **Program Synthesis** - Learn programs from examples

## Documents

### Part 1: Core Architecture & Priority Modules
**File:** `EXTRAPOLATION_CAPABILITIES.md` (2,216 lines)

**Contents:**
1. Executive Summary - Goals, metrics, timeline
2. Current Limitations - What NIMCP can't do today
3. Architectural Vision - Target architecture diagrams
4. System Architecture - Module hierarchy, dependencies
5. Core Modules - 10 module overview
6. Integration Architecture - Brain structure enhancements
7. Implementation Phases - 24-month roadmap
8. Detailed Module Specifications:
   - 8.1 Causal Reasoning (complete API, 30+ functions)
   - 8.2 Compositional Generalization (complete API, 25+ functions)
   - 8.3 Analogical Reasoning (complete API, 20+ functions)

**Key Sections:**
- Complete header file specifications (`.h` files)
- Algorithm pseudocode (PC, do-calculus, SME)
- Implementation notes with complexity analysis

### Part 2: Advanced Modules & Implementation Details
**File:** `EXTRAPOLATION_CAPABILITIES_PART2.md` (1,800 lines)

**Contents:**
8. Detailed Module Specifications (continued):
   - 8.4 Meta-Learning (MAML, Prototypical Networks)
   - 8.5 World Model (Mental simulation, planning)
9. API Specifications - High-level usage API
10. Data Structures - Memory layouts, graph structures
11. Algorithms - PC, MAML, CEM with pseudocode
12. Testing Strategy - Unit, integration, benchmark tests
13. Performance Requirements - Latency, memory, throughput
14. Migration Path - 24-month phased rollout
15. References - 30+ foundational papers

**Key Sections:**
- Example usage scenarios (medicine, robotics, NLP)
- Performance benchmarks with target metrics
- Backward compatibility guarantees

## Quick Start

### For Developers

**1. Understand the vision:**
```bash
# Read sections 1-3 of Part 1
vim docs/architecture/EXTRAPOLATION_CAPABILITIES.md
# Jump to lines 1-350
```

**2. Review specific module:**
```bash
# Causal Reasoning: Part 1, Section 8.1 (lines 800-1200)
# Meta-Learning: Part 2, Section 8.4 (lines 1-300)
# World Model: Part 2, Section 8.5 (lines 300-700)
```

**3. Check implementation timeline:**
```bash
# Part 1, Section 7: Implementation Phases (lines 550-750)
```

**4. See API usage:**
```bash
# Part 2, Section 9: API Specifications (lines 800-950)
# Part 2, Appendix A: Example Usage (lines 1400-1650)
```

### For Project Managers

**Key Metrics (Part 1, Section 1.3):**
| Capability | Current | Target | Timeline |
|------------|---------|--------|----------|
| Domain Transfer | 15% | 70% | 12 months |
| Novel Concepts | 5% | 60% | 18 months |
| Causal Inference | 10% | 80% | 6 months |
| Few-Shot Learning | 30% | 85% | 12 months |

**Resource Requirements:**
- **LOC:** ~26,200 new + ~5,000 enhanced (includes Phase 0.5: ~1,200 LOC complex math)
- **Memory:** +220MB typical, +2.2GB maximum (complex infrastructure adds negligible overhead)
- **Timeline:** 25 months total (Phase 0.5 + 3 main phases)
- **Team:** 3-5 engineers + 1-2 researchers

### For Researchers

**Foundational Papers (Part 2, Section 15):**
- **Causal:** Pearl (2009) - Causality
- **Compositional:** Fodor & Pylyshyn (1988)
- **Analogical:** Gentner (1983) - Structure Mapping
- **Meta-Learning:** Finn et al. (2017) - MAML
- **World Models:** Ha & Schmidhuber (2018)

**Benchmarks (Part 2, Appendix B):**
- Causal discovery: Sachs, Alarm datasets (F1 > 0.8)
- Compositional: SCAN, COGS (Accuracy > 95% on comp split)
- Meta-learning: Omniglot, Mini-ImageNet (5-shot > 80%)
- World models: DMControl (10x sample efficiency)

## Implementation Roadmap

### Phase 0.5: Infrastructure (Month 0-1)
**Priority:** P0
**Modules:** Complex Number Support
**Deliverables:**
- Complex math utilities (`nimcp_complex_math.h/c`)
- Enhanced oscillation system with phasors
- Unit tests and benchmarks
- Foundation for hippocampal and imagination modules

### Phase 1: Foundation (Months 1-6)
**Priority:** P0
**Modules:** Causal Reasoning, Compositional Generalization, Enhanced Symbolic Logic
**Deliverables:**
- Unbounded knowledge base
- Causal graph construction and inference
- Compositional semantics engine
- Pass SCAN benchmark (>95%)

### Phase 2: Core Capabilities (Months 7-12)
**Priority:** P1
**Modules:** Analogical Reasoning, Meta-Learning, World Model
**Deliverables:**
- Cross-domain transfer (70% accuracy)
- Few-shot learning (<10 examples, <1s adaptation)
- Model-based planning (10x sample efficiency)

### Phase 3: Advanced Features (Months 13-24)
**Priority:** P2-P3
**Modules:** Concept Formation, Semantic Memory, Curiosity, Program Synthesis
**Deliverables:**
- Novel concept generation
- 1M+ fact knowledge base
- Intrinsic motivation
- Program induction from I/O examples

## Module Dependencies

```
                    Brain Instance
                          |
        ┌─────────────────┼─────────────────┐
        |                 |                 |
    Causal          Analogical        Meta-Learning
   Reasoning         Reasoning              |
        |                 |                 |
        └─────────┬───────┴─────────────────┘
                  |
            World Model
                  |
        ┌─────────┴──────────┐
        |                    |
  Compositional      Concept Formation
 Generalization              |
        |              Semantic Memory
        |
  Enhanced Symbolic Logic
        |
  Neural Network (Spiking)
```

## Infrastructure Enhancements

### Complex Number Integration (Phase 0.5 - Prerequisite)

**Priority:** P0 (Foundation for oscillation-based modules)
**Timeline:** Month 0-1 (Before Phase 1)
**LOC:** ~1,200 new

#### Neuroscience Justification

Neural computation fundamentally relies on **phase relationships**, not just firing rates:

1. **Hippocampal Place Cells** - Use theta phase precession to encode position
2. **Grid Cells** - Hexagonal spatial patterns emerge from phase interference
3. **Cross-Frequency Coupling** - Theta-gamma PAC requires phase alignment
4. **Sensory Processing** - V1 simple cells are complex Gabor filters
5. **Neural Binding** - Synchronization across regions via phase locking

**Key Insight:** Complex numbers naturally represent amplitude + phase: `z = A·e^(iφ)`

#### Computational Advantages

**1. Natural Phase Arithmetic**
```c
// Current approach (cumbersome)
float phase_diff = atan2(sin(phi1 - phi2), cos(phi1 - phi2));

// Complex approach (elegant)
complex float z_diff = z1 * conjf(z2);
float phase_diff = carg(z_diff);
```

**2. Simplified FFT Integration**
```c
// Preserves both amplitude AND phase
complex float* signal = neuron_complex_state;
fft_complex(signal, spectrum);
```

**3. Complex-Valued Neural Networks (CVNNs)**
- More parameter-efficient (single complex weight = amplitude + phase)
- Better for learning periodic/oscillatory patterns
- Natural for frequency domain processing

#### Integration Points

**Enhances Existing Features:**
- **Oscillation Tracking** - Natural phasor representation
- **Coherence Calculation** - Simplified to ~10 lines vs current ~50
- **PAC Detection** - Direct modulation index computation
- **Attention** - Phase reset and synchronization

**Enables New Capabilities:**
- **Hippocampal Models** - Place/grid cells with phase precession
- **Complex Synaptic Weights** - Phase delays in transmission
- **Sensory Cortex** - Gabor filters, frequency analysis
- **Working Memory** - Phase-coded sequential buffers

#### Implementation

**File Structure:**
```
nimcp/
├── include/
│   └── utils/
│       └── nimcp_complex_math.h              [NEW - 200 LOC]
├── src/
│   └── utils/
│       └── nimcp_complex_math.c              [NEW - 500 LOC]
├── include/
│   └── core/
│       ├── neuron/
│       │   └── nimcp_complex_neuron.h        [NEW - 150 LOC]
│       └── synapse/
│           └── nimcp_complex_synapse.h       [NEW - 100 LOC]
└── test/
    └── unit/utils/
        └── test_complex_math.cpp             [NEW - 250 LOC]
```

**Phase 0.5 Deliverables:**

**1. Complex Math Utilities** (`nimcp_complex_math.h/c`)
```c
#include <complex.h>

// Type definitions
typedef complex float neural_phasor_t;

// Core operations
neural_phasor_t phasor_from_polar(float amplitude, float phase);
float phasor_amplitude(neural_phasor_t z);
float phasor_phase(neural_phasor_t z);
float phasor_phase_difference(neural_phasor_t z1, neural_phasor_t z2);

// Array operations
void phasor_array_coherence(neural_phasor_t* signals, uint32_t n, float* coherence);
void phasor_array_synchrony(neural_phasor_t* signals, uint32_t n, float* synchrony);

// FFT integration
void phasor_fft(neural_phasor_t* input, neural_phasor_t* output, uint32_t n);
void phasor_ifft(neural_phasor_t* input, neural_phasor_t* output, uint32_t n);
```

**2. Enhanced Oscillation System**
```c
// Current: separate amplitude and phase
typedef struct {
    float frequency;
    float phase;
    float amplitude;
} oscillation_state_t;

// Complex version (more natural)
typedef struct {
    float frequency;
    neural_phasor_t phasor;  // Amplitude and phase unified
} complex_oscillation_t;
```

**3. Complex Neuron Models** (Optional extension)
```c
typedef struct {
    neural_phasor_t membrane_potential;  // Amplitude = voltage, Phase = oscillation
    neural_phasor_t* synaptic_weights;   // Amplitude = strength, Phase = delay
    uint32_t num_synapses;
} complex_neuron_t;

// Synaptic transmission with automatic phase delay
neural_phasor_t complex_neuron_compute(complex_neuron_t* neuron,
                                       neural_phasor_t* inputs);
```

**4. Hippocampal Module Integration**
```c
// Place cells with phase precession
typedef struct {
    neural_phasor_t theta_state;  // Encodes position via phase
    float place_field_center;
    float place_field_width;
} place_cell_t;

// Grid cells with phase interference
typedef struct {
    neural_phasor_t basis[3];     // Three directional basis functions
    float spacing;
    float orientation;
} grid_cell_t;
```

#### Use Cases

**1. Enhanced PAC Detection** (Already in nimcp)
```c
// Current implementation (from middleware/patterns/oscillation_detector.c)
// ~50 lines of phase extraction and modulation computation

// With complex numbers (simplified)
complex float coherence(neural_phasor_t* theta, neural_phasor_t* gamma, size_t n) {
    complex float cross = 0;
    for (size_t i = 0; i < n; i++) {
        cross += theta[i] * conjf(gamma[i]);
    }
    return cross / n;
}
float modulation_index = cabsf(coherence(theta, gamma, n));
```

**2. World Model Integration** (Phase 2)
```c
// Mental simulation of oscillatory dynamics
world_model_t* wm = brain->world_model;

// Predict future oscillatory state
neural_phasor_t current_state = neuron_get_phasor(neuron);
neural_phasor_t predicted_state = world_model_predict_phasor(wm, current_state, dt);

// Natural phase evolution: z(t+dt) = z(t) * e^(iω·dt)
```

**3. Imagination Module Integration** (Phase 2)
```c
// Visual cortex Gabor filters (from IMAGINATION_AND_GENERATION.md)
neural_phasor_t gabor_filter(float x, float y, float lambda, float theta, float phi) {
    float x_rot = x * cosf(theta) + y * sinf(theta);
    float envelope = expf(-x_rot*x_rot / (2.0f * sigma*sigma));
    return envelope * cexpf(I * (2.0f * M_PI * x_rot / lambda + phi));
}
```

**4. Working Memory** (Already in nimcp - can enhance)
```c
// Phase-coded sequential buffers
typedef struct {
    neural_phasor_t* items;  // Items encoded by theta phase
    uint32_t capacity;
} phase_coded_buffer_t;

// Natural ordering by phase (no explicit sorting needed)
void retrieve_sequence(phase_coded_buffer_t* buffer, neural_phasor_t** ordered) {
    qsort_by_phase(buffer->items, buffer->capacity);
}
```

#### Performance Considerations

**Advantages:**
- Native C99 `<complex.h>` support on all platforms
- SIMD instructions for complex ops (AVX-512 has dedicated complex multiply)
- More compact than separate sin/cos tables
- Algorithmic simplification often compensates for 2x memory

**Costs:**
- 2x memory per value (real + imaginary)
- Slightly slower than real arithmetic (~1.5x for basic ops)

**Target Performance:**
- Phasor operations: <10ns per operation (similar to float ops)
- Array coherence: <1µs for 1000 elements
- FFT: <50µs for 1024 complex samples

#### Testing Strategy

**Unit Tests** (250 LOC)
```bash
# Test basic phasor operations
ctest -R "unit.*complex_math.*phasor" -V

# Test phase arithmetic
ctest -R "unit.*complex_math.*phase" -V

# Test array operations
ctest -R "unit.*complex_math.*array" -V
```

**Integration Tests** (with existing oscillation system)
```bash
# Verify complex oscillations match real-valued version
ctest -R "integration.*complex_oscillation" -V
```

**Benchmarks**
```bash
# Compare complex vs real-valued performance
./test/benchmarks/benchmark_complex_vs_real

# FFT performance
./test/benchmarks/benchmark_complex_fft
```

#### Migration Path

**Phase 0.5: Foundation** (Month 0-1)
- Implement `nimcp_complex_math.h/c`
- Add unit tests
- Benchmark performance
- Document API

**Phase 1: Integration** (Months 1-6 - during Causal Reasoning phase)
- Enhance oscillation detector to optionally use phasors
- Update PAC detector to use complex coherence
- Maintain backward compatibility (default: real-valued)

**Phase 2: Advanced Features** (Months 7-12 - during World Model phase)
- Implement hippocampal place/grid cells
- Add complex synaptic weights (optional feature)
- Integrate with imagination module (Gabor filters)

**Phase 3: Optimization** (Months 13-24)
- SIMD optimization for phasor arrays
- GPU support for complex convolutions
- Complex-valued meta-learning (if applicable)

#### Success Metrics

**Correctness:**
- 100% test pass rate
- Phase arithmetic accurate to <1e-6 radians
- Coherence calculations match reference implementations

**Performance:**
- Phasor operations within 2x of float operations
- Array coherence faster than current implementation
- FFT performance comparable to FFTW (within 20%)

**Integration:**
- Backward compatible with existing code
- Opt-in via configuration flags
- No performance regression for users who don't enable complex features

#### Dependencies

**Required Libraries:**
- C99 `<complex.h>` (standard library)
- Existing FFT implementation (can be enhanced)

**Optional:**
- FFTW3 (for complex FFT optimization)
- Intel MKL (for SIMD complex operations)

#### Impact on Extrapolation Modules

**Direct Benefits:**
- **World Model** - Natural oscillatory dynamics prediction
- **Hippocampal Models** - Enables place/grid cell implementation
- **Imagination** - Gabor filters for visual generation
- **Working Memory** - Phase-coded sequences

**Indirect Benefits:**
- **Meta-Learning** - Complex-valued networks for periodic tasks
- **Compositional** - Phase relationships in symbolic binding
- **Analogical** - Similarity via phase alignment

**No Impact:**
- **Causal Reasoning** - Uses discrete graphs
- **Semantic Memory** - Uses symbolic representations
- **Program Synthesis** - Uses abstract syntax trees

#### Recommendation

**Priority: P0 - Implement in Month 0-1**

Complex number support is foundational infrastructure that:
1. Simplifies existing oscillation code
2. Enables biologically realistic hippocampal models
3. Integrates naturally with imagination module
4. Has minimal performance cost with significant algorithmic benefits

This should be implemented **before** Phase 1 to avoid retrofitting later.

---

## File Structure After Implementation

```
nimcp/
├── src/
│   ├── utils/
│   │   └── nimcp_complex_math.c                  [NEW - Phase 0.5 - 500 LOC]
│   ├── core/
│   │   ├── neuron/
│   │   │   └── nimcp_complex_neuron.c            [NEW - Phase 0.5 - 200 LOC]
│   │   ├── synapse/
│   │   │   └── nimcp_complex_synapse.c           [NEW - Phase 0.5 - 150 LOC]
│   │   ├── reasoning/
│   │   │   ├── nimcp_causal_reasoning.c          [NEW - Phase 1 - 2,500 LOC]
│   │   │   ├── nimcp_analogical_reasoning.c      [NEW - Phase 2 - 3,000 LOC]
│   │   │   └── nimcp_compositional.c             [NEW - Phase 1 - 2,000 LOC]
│   │   └── learning/
│   │       └── nimcp_meta_learning.c             [NEW - Phase 2 - 2,500 LOC]
│   ├── cognitive/
│   │   ├── nimcp_symbolic_logic.c                [ENHANCED - Phase 1 - +1,500 LOC]
│   │   ├── nimcp_world_model.c                   [NEW - Phase 2 - 3,500 LOC]
│   │   ├── nimcp_concept_formation.c             [NEW - Phase 3 - 2,500 LOC]
│   │   ├── nimcp_semantic_memory.c               [NEW - Phase 3 - 2,000 LOC]
│   │   ├── nimcp_curiosity.c                     [NEW - Phase 3 - 1,500 LOC]
│   │   └── nimcp_program_synthesis.c             [NEW - Phase 3 - 4,000 LOC]
│   └── include/
│       └── [corresponding .h files]
├── test/
│   ├── unit/utils/
│   │   └── test_complex_math.cpp                 [NEW - Phase 0.5 - 250 LOC]
│   ├── unit/core/reasoning/                      [NEW - Phase 1-2 - 3,000 LOC tests]
│   ├── unit/cognitive/concept_formation/         [NEW - Phase 3 - 2,000 LOC tests]
│   ├── integration/extrapolation/                [NEW - Phase 1-3 - 2,000 LOC tests]
│   └── regression/extrapolation/                 [NEW - Phase 1-3 - 1,500 LOC tests]
└── docs/
    └── architecture/
        ├── EXTRAPOLATION_CAPABILITIES.md         [THIS DOCUMENT - Part 1]
        ├── EXTRAPOLATION_CAPABILITIES_PART2.md   [THIS DOCUMENT - Part 2]
        ├── EXTRAPOLATION_README.md               [THIS DOCUMENT - Index]
        ├── IMAGINATION_AND_GENERATION.md         [CREATED - Imagination module]
        ├── CAUSAL_REASONING_SPEC.md              [TO BE CREATED]
        ├── ANALOGICAL_TRANSFER_SPEC.md           [TO BE CREATED]
        └── META_LEARNING_SPEC.md                 [TO BE CREATED]
```

## Key Features

### 1. Causal Reasoning
**What it enables:** Distinguish causation from correlation

**Example:**
```c
// Learn causal graph from data
causal_graph_learn_from_data(brain->causal_reasoning, ...);

// Query causation
bool is_cause = causal_is_cause(brain->causal_reasoning,
                                 "smoking", "cancer");

// Intervention: P(cancer | do(smoking=0))
causal_compute_intervention(...);

// Counterfactual: "What if I hadn't smoked?"
causal_compute_counterfactual(...);
```

**Complexity:** O(V^2 × 2^k × N) for learning, O(V × 2^k) for inference

### 2. Compositional Generalization
**What it enables:** Infinite expressions from finite primitives

**Example:**
```c
// Primitives: "jump", "run", "twice", "thrice"
// Training: "jump", "jump twice", "run", "run twice"
// Test: "jump thrice" → should output "JUMP JUMP JUMP"

compositional_learn_rule(cs, inputs, outputs, num_examples, &rule);
compositional_apply_rule(cs, rule, novel_input, &output);
```

**Benchmark:** SCAN compositional split >95% accuracy

### 3. Analogical Transfer
**What it enables:** Cross-domain knowledge transfer

**Example:**
```c
// Source: solar system (sun, earth, revolves)
// Target: atom (nucleus, electron, ?)
// Transfer: "revolves(electron, nucleus)"

analogy_t* analogy = analogical_find_mapping(engine, source, target);
analogical_transfer_knowledge(engine, analogy, fact, &inference);
```

**Accuracy:** 70% cross-domain transfer

### 4. Meta-Learning
**What it enables:** Learn from <10 examples in <1 second

**Example:**
```c
// Meta-train on 50 tasks
meta_learner_train(brain->meta_learner, tasks, 50, 1000);

// Adapt to new task with only 5 examples
meta_learner_adapt(brain->meta_learner, new_task_5_examples, &adapted);

// Evaluate immediately
meta_learner_evaluate(brain->meta_learner, adapted, test, &accuracy);
// Result: 80%+ accuracy after 5-shot, <1s adaptation
```

**Performance:** 5-shot →85% accuracy, <1s adaptation

### 5. World Model
**What it enables:** Mental simulation and planning

**Example:**
```c
// Train world model on experience
world_model_train(wm, transitions, 100000, epochs=10);

// Plan using imagination (no real interaction needed)
world_model_plan(wm, current_state, horizon=10, samples=1000, &plan);

// Execute first action, replan at next step (MPC)
```

**Efficiency:** 10-20x sample efficiency vs model-free RL

## Testing

### Unit Tests (Target: 90%+ coverage)
```bash
# Run all extrapolation unit tests
ctest -R "unit.*extrapolation" -j$(nproc)

# Test specific module
ctest -R "unit.*causal_reasoning" -V
```

### Integration Tests
```bash
# Test cross-module integration
ctest -R "integration.*extrapolation" -j$(nproc)
```

### Benchmarks
```bash
# Run standard benchmarks
./test/benchmarks/run_causal_benchmarks.sh      # Sachs, Alarm
./test/benchmarks/run_compositional_benchmarks.sh  # SCAN, COGS
./test/benchmarks/run_meta_learning_benchmarks.sh  # Omniglot, Mini-ImageNet
```

## Performance Targets

| Metric | Target | Measurement |
|--------|--------|-------------|
| Causal query latency | <100ms | 1000-node graph |
| Compositional parse | <50ms | 50-token expression |
| Analogical mapping | <500ms | 100-entity domains |
| Meta-learning adapt | <1s | 5-shot learning |
| World model predict | <10ms | Single step |
| MPC planning | <200ms | Horizon=10, 1000 samples |

## Memory Footprint

| Component | Typical | Maximum |
|-----------|---------|---------|
| Causal graph | 5MB | 50MB |
| Compositional cache | 5MB | 50MB |
| Analogical mappings | 10MB | 100MB |
| Meta-learner | 50MB | 500MB |
| World model (5-ensemble) | 125MB | 1.25GB |
| **Total extrapolation** | **~220MB** | **~2.2GB** |

## API Compatibility

**Backward Compatible:** All existing NIMCP code continues to work unchanged.

```c
// Old code (no changes needed)
brain_config_t config = brain_config_default();
brain_t brain = brain_create_custom(&config);
// All extrapolation features disabled by default

// New code (opt-in)
config.enable_causal_reasoning = true;
config.enable_meta_learning = true;
brain_t brain2 = brain_create_custom(&config);
// Extrapolation features enabled
```

## Getting Started

1. **Read Part 1, Section 1-3:** Understand vision and architecture
2. **Review Phase 1 modules:** Causal, Compositional (Part 1, Sections 8.1-8.2)
3. **Check implementation timeline:** Part 1, Section 7
4. **See example usage:** Part 2, Appendix A
5. **Start with causal reasoning:** Highest impact, P0 priority

## Questions?

- **Architecture questions:** See Part 1, Sections 1-4
- **API questions:** See Part 2, Section 9
- **Performance questions:** See Part 2, Section 13
- **Timeline questions:** See Part 1, Section 7
- **Algorithm questions:** See Part 2, Section 11

## Status

**Version:** 1.1
**Date:** 2025-11-22
**Status:** Design Complete - Ready for Implementation
**Next Steps:**
1. Review with team
2. **Phase 0.5:** Implement complex number infrastructure (Month 0-1)
3. Begin causal reasoning implementation (Week 5-12)
4. Integrate imagination module with complex phasors (Month 7-12)

**Recent Additions (v1.1):**
- ✅ Complex number integration plan (Phase 0.5 - foundational infrastructure)
- ✅ Imagination and generation module (merged with extrapolation architecture)
- ✅ Hippocampal models specification (place/grid cells with phase precession)

---

**Total Documentation:** ~4,500 lines across 3 files
**Files:**
- `EXTRAPOLATION_README.md` - Overview and roadmap
- `EXTRAPOLATION_CAPABILITIES.md` - Core modules (Part 1)
- `EXTRAPOLATION_CAPABILITIES_PART2.md` - Advanced modules (Part 2)
- `IMAGINATION_AND_GENERATION.md` - Generative capabilities and integration

**Estimated Implementation:** ~26,200 LOC new code, ~5,000 LOC enhancements
**Timeline:** 25 months (Phase 0.5 + 3 main phases)
**Impact:** Transform NIMCP from pattern recognizer to true extrapolator with generative imagination
