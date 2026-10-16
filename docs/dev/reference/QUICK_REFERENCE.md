# NIMCP Core Architecture - Quick Reference Guide

## Overview
- **Total LOC**: 24,346 (core modules)
- **Version**: 2.7.0 / Phase 8.7
- **Language**: C with optional C++ bindings
- **Architecture**: Layered, modular, biologically-inspired

---

## Key Files & Line Counts

### Layer 1: Public API (1 file)
```
src/include/nimcp.h                           739 lines
```

### Layer 2: Brain (Facade)
```
src/core/brain/nimcp_brain.h                  1,382 lines
src/core/brain/nimcp_brain.c                  6,388 lines
src/core/brain/nimcp_distributed_cow.h       447 lines
src/core/brain/nimcp_distributed_cow.c       818 lines
src/core/brain/nimcp_pretrained.c            525 lines
src/core/brain/processing/                   281 lines (combined)
```
**Subtotal: 7,770 LOC** - High-level learning interface

### Layer 3: Neural Network (Core)
```
src/core/neuralnet/nimcp_neuralnet.h         524 lines
src/core/neuralnet/nimcp_neuralnet.c         2,672 lines
```
**Subtotal: 3,196 LOC** - Network computation engine

### Layer 4A: Synapse Computation (NIMCP 2.7)
```
src/core/synapse_compute/nimcp_synapse_compute.h  429 lines
src/core/synapse_compute/nimcp_synapse_compute.c  662 lines
```
**Subtotal: 1,091 LOC** - Programmable synapses

### Layer 4B: Synapse Types (Phase 8.7)
```
src/core/synapse_types/nimcp_synapse_types.h  682 lines
src/core/synapse_types/nimcp_synapse_types.c  680 lines
```
**Subtotal: 1,362 LOC** - Receptor diversity (AMPA, NMDA, GABA, etc)

### Layer 4C: Neuron Types (Phase 8.7)
```
src/core/neuron_types/nimcp_neuron_types.h    742 lines
src/core/neuron_types/nimcp_neuron_types.c    793 lines
src/core/neuron_types/nimcp_neural_logic.h    618 lines
src/core/neuron_types/nimcp_neural_logic.c    938 lines
```
**Subtotal: 3,091 LOC** - Specialized neurons (V1, place cells, etc)

### Layer 4D: Neuron Models
```
src/core/neuron_models/nimcp_neuron_model.h   285 lines
src/core/neuron_models/nimcp_neuron_model.c   232 lines
src/core/neuron_models/nimcp_izhikevich.h    199 lines
src/core/neuron_models/nimcp_izhikevich.c    425 lines
```
**Subtotal: 1,201 LOC** - LIF, Izhikevich, plugin architecture

### Layer 5: Topology
```
src/core/topology/nimcp_fractal_topology.h    425 lines
src/core/topology/nimcp_fractal_topology.c    628 lines
src/core/topology/nimcp_network_builder.h     154 lines
src/core/topology/nimcp_network_builder.c     256 lines
```
**Subtotal: 1,463 LOC** - Scale-free, fractal networks

### Layer 6A: Brain Oscillations
```
src/core/brain_oscillations/nimcp_brain_oscillations.h  394 lines
src/core/brain_oscillations/nimcp_brain_oscillations.c  542 lines
```
**Subtotal: 936 LOC** - Delta, theta, alpha, beta, gamma

### Layer 6B: Brain Regions
```
src/core/brain_regions/nimcp_brain_regions.h  476 lines
src/core/brain_regions/nimcp_brain_regions.c  717 lines
```
**Subtotal: 1,193 LOC** - Functional brain areas

### Layer 6C: Multimodal Integration
```
src/core/integration/nimcp_multimodal_integration.h  242 lines
src/core/integration/nimcp_multimodal_integration.c  404 lines
```
**Subtotal: 646 LOC** - Vision + Audio + Language pipeline

---

## Core Components at a Glance

### 1. Brain Module (7,770 LOC)
**Pattern**: Facade over complex neural operations

**Key Structures**:
- `brain_config_t` - 50+ configuration options
- `brain_decision_t` - Decision with confidence & explanation
- `brain_stats_t` - Network statistics

**Key APIs**:
- `brain_create()` / `brain_create_custom()` - Creation
- `brain_learn_example()` / `brain_learn_from_llm()` - Learning
- `brain_decide()` / `brain_process_multimodal()` - Inference
- `brain_clone_cow()` - Copy-on-Write cloning (86% savings)
- `brain_save_snapshot()` / `brain_restore_snapshot()` - Persistence

**Features**:
- ✓ 4 size presets (TINY: 100 neurons → LARGE: 100K neurons)
- ✓ 5 task templates (classification, regression, etc)
- ✓ 50+ advanced features (working memory, emotions, sleep, ethics)
- ✓ Pre-trained model support
- ✓ Distributed P2P coordination
- ✓ Multi-modal processing (vision, audio, language)

---

### 2. Neural Network Core (3,196 LOC)
**Pattern**: Low-level computational engine

**Key Data Structures**:
```c
neuron_t {
    id, type (EXCITATORY/INHIBITORY/specialized)
    state, threshold, adaptation
    learning_rule (HEBBIAN/OJA/STDP/HYBRID)
    synapses[MAX=256], incoming_synapses[MAX=256]  // Bidirectional
    neuron_model (LIF/Izhikevich/custom)
    spike_history, activity_history
}

synapse_t {
    target_id, weight, plasticity
    stp (short-term plasticity)
    compute_function, learn_function, compute_state  // NIMCP 2.7
    type (AMPA/NMDA/GABA-A/GABA-B/etc)             // Phase 8.7
    type_state (type-specific kinetics)
}
```

**Key Functions**:
- `neural_network_create()` - Create network
- `neural_network_forward()` - Inference
- `neural_network_compute_step()` - Time stepping
- `neural_network_apply_stdp/oja/homeostasis()` - Learning
- `neural_network_add_neuron()` / `add_connection_typed()` - Building
- `neural_network_get_incoming_synapses()` - Optimization

**Constants**:
- MAX_NEURONS: 100,000
- MAX_SYNAPSES_PER_NEURON: 256
- SPIKE_HISTORY_LENGTH: 1,000

---

### 3. Synapse Computation (1,091 LOC) - NIMCP 2.7
**Pattern**: Strategy pattern - programmable synapses

**Revolutionary Feature**: Transform synapses from passive weights → active processors

**Built-in Functions**:
1. `synapse_compute_default()` - Baseline (O(1), ~10 cycles)
2. `synapse_compute_attention()` - Transformer-like (O(d), ~100-500 cycles)
3. `synapse_compute_semantic()` - Word embeddings (O(d), ~300 cycles)
4. `synapse_compute_gating()` - LSTM-like gates (O(1))
5. `synapse_compute_neuromodulated()` - Dopamine/serotonin modulation
6. `synapse_compute_dendritic()` - Dendritic computation (O(k))

**Built-in Learning Functions**:
1. `synapse_learn_three_factor()` - STDP + eligibility + reward
2. `synapse_learn_attention_modulated()` - Attention-modulated STDP
3. `synapse_learn_metaplastic()` - BCM-like meta-plasticity

**Context**:
```c
synapse_compute_context_t {
    float* global_state           // Attention outputs, etc
    float neuromodulation         // Dopamine/serotonin [0,1]
    uint64_t current_time
    void* custom_data
}
```

---

### 4. Synapse Types (1,362 LOC) - Phase 8.7
**Pattern**: Enum-based strategy dispatch

**Receptor Types** (biologically diverse):
- **AMPA**: Fast excitation (τ=2ms)
- **NMDA**: Slow excitation (τ=100ms), Ca²⁺ permeable for learning
- **GABA-A**: Fast inhibition (τ=10ms)
- **GABA-B**: Slow inhibition (τ=150ms)
- **DOPAMINE**: Reward modulation (D1/D2 receptors)
- **SEROTONIN**: Mood modulation (5-HT receptors)
- **ACETYLCHOLINE**: Attention modulation (nicotinic/muscarinic)
- **ELECTRICAL**: Gap junctions (instantaneous)

**Type-Specific State**:
```c
ampa_state_t {
    float conductance, g_max, tau_rise, tau_decay, reversal_potential
}

nmda_state_t {
    float conductance, g_max, tau_rise, tau_decay
    float mg_block              // Voltage-dependent Mg²⁺ block
    float calcium_influx        // Critical for LTP/LTD
}

// Similar for GABA-A, GABA-B, neuromodulators...
```

---

### 5. Neuron Types (3,091 LOC) - Phase 8.7
**Pattern**: Specialized neurons for different functions

**Canonical Types**:
- EXCITATORY (pyramidal cells)
- INHIBITORY (interneurons)

**Sensory Types**:
- **V1_SIMPLE_CELL**: Oriented edge detection (visual cortex)
- **V1_COMPLEX_CELL**: Scale/position invariant
- **A1_FREQ_TUNED**: Frequency selective (auditory cortex)

**Learning & Memory**:
- **PLACE_CELL**: Location coding (hippocampus)
- **GRID_CELL**: Hexagonal spatial grid (entorhinal cortex)
- **HEAD_DIRECTION**: Directional tuning

**Executive & Emotional**:
- **PREFRONTAL_PFC**: Working memory & planning
- **MOTOR_PLANNING**: Movement preparation
- **FEAR_NEURON**: Threat detection (amygdala)
- **REWARD_NEURON**: Reward prediction (dopamine, VTA/SNc)

**Neural Logic Gates** (Phase 9.4):
- AND, OR, NOT, XOR, NAND, NOR
- THRESHOLD, COINCIDENCE_DETECTOR

---

### 6. Neuron Models (1,201 LOC)
**Pattern**: Plugin architecture

**Models**:
1. **LIF** (Leaky Integrate-and-Fire): Simple, fast (O(1))
2. **IZHIKEVICH**: Biologically realistic, diverse dynamics
   - Can exhibit: regular spiking, bursting, chattering, intrinsic bursting
   - Formula: `dv/dt = 0.04v² + 5v + 140 - u + I`
3. **CUSTOM**: User-defined

**Interface**:
```c
bool neural_network_set_neuron_model(
    network, neuron_id, 
    NEURON_MODEL_IZHIKEVICH, 
    izhikevich_params_t* params
);
```

---

### 7. Topology Generation (1,463 LOC)
**Pattern**: Builder/Factory

**Algorithms**:
1. **TOPOLOGY_SCALE_FREE** (Barabási-Albert)
   - Preferential attachment: P(connect) ∝ degree
   - Result: Power-law degree distribution P(k) ~ k^γ
   - 70-80% fewer synapses than random
   - Hub-based architecture

2. **TOPOLOGY_FRACTAL** (Hierarchical)
   - Self-similar recursive structure
   - Matches cortical columns & hierarchical processing

**Analysis**:
- `topology_compute_stats()` - Graph metrics
- `topology_is_small_world()` - Test σ coefficient
- `topology_fit_power_law()` - Estimate γ
- `topology_identify_hubs()` - Find hub neurons
- `topology_compute_betweenness()` - Centrality

**Metrics**:
- Degree distribution, clustering coefficient
- Characteristic path length, power-law fit
- Small-world coefficient σ = (C/Crand)/(L/Lrand)

---

### 8. Brain Oscillations (936 LOC)
**Pattern**: Observer - monitors network activity

**Types** (biological frequencies):
- **DELTA** (0.5-4 Hz): Deep sleep
- **THETA** (4-8 Hz): Learning, memory
- **ALPHA** (8-12 Hz): Relaxation, inhibition
- **BETA** (12-30 Hz): Motor, attention
- **GAMMA** (30-100 Hz): Attention, binding

**Functions**:
- `oscillation_detect_frequency()` - Analyze network activity
- `oscillation_get_power()` - Power at frequency
- `oscillation_get_coherence()` - Synchronization between neurons
- `oscillation_modulate_learning()` - Phase-dependent learning

---

### 9. Brain Regions (1,193 LOC)
**Pattern**: Functional specialization

**Regions** (computational functions):
- PRIMARY_VISUAL (V1)
- TEMPORAL_CORTEX (object recognition)
- PARIETAL_CORTEX (spatial processing)
- PREFRONTAL_CORTEX (executive)
- AMYGDALA (emotional processing)
- HIPPOCAMPUS (memory, navigation)
- STRIATUM (action selection, learning)
- CEREBELLUM (motor learning, timing)

**Functions**:
- `brain_region_create()` - Register region
- `brain_region_set_properties()` - Configure
- `brain_region_is_active()` - Query activity
- `brain_region_get_neuron_count()` - Region size

---

### 10. Multimodal Integration (646 LOC) - Phase 8
**Pattern**: Pipeline - sensory → cognitive processing

**Input Modalities**:
- Visual (RGB/grayscale images)
- Audio (PCM samples)
- Language (text, embeddings) - Phase 9.4
- Direct (feature vectors)

**7-Stage Processing Pipeline**:
1. **Sensory Extraction**: Visual CNN, audio FFT, language embeddings
2. **Modality Integration**: Attention-weighted fusion
3. **Neural Processing**: STDP, glial modulation, oscillations
4. **Introspection**: Uncertainty estimation
5. **Ethics Filtering**: Ethical approval
6. **Salience Detection**: Novelty and importance
7. **Consolidation**: Memory strengthening

**Output**:
```c
brain_multimodal_output_t {
    float* output_vector
    char decision_label[64]
    float confidence
    float introspection_uncertainty
    float salience_score
    bool ethical_approved
    float novelty_score
    
    // Phase 9.2: Epistemic filtering
    float epistemic_quality
    bool bias_detected
    char epistemic_reasoning[256]
    
    // Phase 9.4: Language & logic
    char* language_response
    bool logical_consistency
    
    // Attention breakdown
    float visual_attention, audio_attention, language_attention
    
    char explanation[256]
}
```

---

### 11. Distributed Copy-on-Write (1,265 LOC) - Phase 2.8
**Pattern**: Lazy loading over P2P

**Architecture**:
- Master node: Full brain + network data
- Remote clones: Lightweight cache + lazy loading
- Benefit: 99% memory savings for snapshots, 86% for replicas

**Performance**:
- Clone time: <10ms (vs ~1000ms full copy)
- Memory overhead: 1MB metadata (vs 50MB full copy)
- Bandwidth: Lazy - only fetch neurons needed

**Functions**:
- `brain_clone_cow_distributed()` - Create remote clone
- `brain_enable_distributed_cow_master()` - Enable serving
- `brain_get_distributed_cow_stats()` - Monitor replication
- `brain_is_distributed_cow()` - Check status

---

## Design Patterns Used

| Pattern | Module | Purpose |
|---------|--------|---------|
| **Facade** | brain.h | Simplify complex neural network |
| **Strategy** | synapse_compute.h | Per-synapse algorithms |
| **State** | synapse_compute_state_t | Preserve synapse state |
| **Template Method** | synapse_types.h | Variable compute implementations |
| **Builder** | topology.h | Complex network construction |
| **Factory** | topology_config_t | Create various topologies |
| **Observer** | oscillations | Monitor network events |
| **Dependency Injection** | neuralnet.h | Inject global state, neuromod |
| **Opaque Pointers** | nimcp.h | Version stability |
| **Plugin** | neuron_models | Pluggable dynamics |

---

## Performance Characteristics

### Memory Usage
| Size | Neurons | Memory | Inference |
|------|---------|--------|-----------|
| TINY | 100 | <1 MB | ~0.1 ms |
| SMALL | 1K | ~10 MB | ~0.5 ms |
| MEDIUM | 10K | ~50 MB | ~5 ms |
| LARGE | 100K | ~500 MB | ~50 ms |

### COW Cloning
- Clone time: <10ms
- Memory overhead: ~1MB
- Memory savings: 86% (replicas), 99% (snapshots)

### Synapse Computation
- Default: ~10 cycles
- Attention: ~100-500 cycles (d = embedding dim)
- Semantic: ~300 cycles
- Dendritic: O(k) where k ∈ [10, 50]

### Topology
- Generation: O(N log N)
- Analysis: O(N² log N)
- Synapses saved: 70-80% vs random

---

## Key Innovation Points

### NIMCP 2.7: Programmable Synapses
- Transform synapses from passive weights to active processors
- Custom compute & learn functions
- Enables attention, semantic similarity, dendritic computation

### Phase 8.7: Synapse & Neuron Specialization
- **9 synapse types** with diverse kinetics
- **20+ neuron types** with specialized parameters
- Biologically realistic receptor diversity

### Phase 8: Unified Multi-Modal Processing
- Unified pipeline for vision, audio, language
- 7-stage cognitive processing
- Epistemic filtering (bias detection)

### Phase 9.4: Human Communication & Reasoning
- Language understanding & generation
- Logical reasoning with neural logic gates
- Theory of mind

### Phase 10: Advanced Cognition
- Working memory (Miller's 7±2)
- Emotional tagging
- Sleep-wake cycles
- Mirror neurons

---

## Brain Size Presets

### Tiny Brain (100 neurons)
```c
nimcp_brain_t brain = nimcp_brain_create(
    "tiny_model", NIMCP_BRAIN_TINY,
    NIMCP_TASK_CLASSIFICATION, 784, 10
);
// Memory: <1MB, Inference: ~0.1ms
// Use: Edge devices, embedded systems
```

### Small Brain (1K neurons)
```c
// Memory: ~10MB, Inference: ~0.5ms
// Use: Mobile, real-time applications
```

### Medium Brain (10K neurons)
```c
// Memory: ~50MB, Inference: ~5ms
// Use: Server, research, general purpose
// RECOMMENDED for most applications
```

### Large Brain (100K neurons)
```c
// Memory: ~500MB, Inference: ~50ms
// Use: High-capacity learning, complex tasks
```

---

## Common Usage Patterns

### Simple Classification
```c
// Create brain
brain_t brain = nimcp_brain_create(
    "classifier", NIMCP_BRAIN_SMALL,
    NIMCP_TASK_CLASSIFICATION, 784, 10
);

// Learn from examples
nimcp_brain_learn_example(brain, features, 784, "class_a", 0.95);

// Predict
char label[64];
float confidence;
nimcp_brain_predict(brain, features, 784, label, &confidence);

// Save
nimcp_brain_save(brain, "classifier.brain");
nimcp_brain_destroy(brain);
```

### Multi-Modal Processing
```c
// Create with all modalities
brain_config_t config = {...};
config.enable_visual_cortex = true;
config.enable_audio_cortex = true;
config.enable_language_processing = true;  // Phase 9.4
config.enable_multimodal_integration = true;

brain_t brain = brain_create_custom(&config);

// Process all modalities together
brain_multimodal_input_t input = {
    .visual_data = camera_frame,
    .visual_width = 640, .visual_height = 480, .visual_channels = 3,
    .audio_data = microphone_samples,
    .audio_samples = 1024,
    .language_text = "What's happening?",
    .language_length = strlen(...),
};

brain_multimodal_output_t output = {0};
brain_process_multimodal(brain, &input, &output);
```

### Copy-on-Write Cloning
```c
// Create original brain
brain_t original = brain_create(...);

// Instant clone (86% memory savings!)
brain_t clone = nimcp_brain_clone_cow(original);

// Can modify independently
brain_learn_example(clone, new_features, ...);

// Memory still shared until first write
brain_cow_stats_t stats;
brain_get_cow_stats(clone, &stats);  // Check COW status
```

### Snapshots for A/B Testing
```c
// Take snapshot before training
brain_save_snapshot(brain, "before_training", "Baseline");

// Train variant 1
brain_learn_batch(brain, data1, count1);
brain_save_snapshot(brain, "variant1", "After variant 1 training");

// Rollback and try variant 2
brain_restore_snapshot(brain, "before_training");
brain_learn_batch(brain, data2, count2);
brain_save_snapshot(brain, "variant2", "After variant 2 training");

// Compare performance
brain_stats_t stats1, stats2;
brain_restore_snapshot(brain, "variant1");
brain_get_stats(brain, &stats1);
brain_restore_snapshot(brain, "variant2");
brain_get_stats(brain, &stats2);
```

---

## File Location Reference

| Component | Header | Implementation | LOC |
|-----------|--------|-----------------|-----|
| Public API | src/include/nimcp.h | - | 739 |
| Brain | src/core/brain/nimcp_brain.h | .c | 7,770 |
| Neural Network | src/core/neuralnet/nimcp_neuralnet.h | .c | 3,196 |
| Synapse Compute | src/core/synapse_compute/nimcp_synapse_compute.h | .c | 1,091 |
| Synapse Types | src/core/synapse_types/nimcp_synapse_types.h | .c | 1,362 |
| Neuron Types | src/core/neuron_types/nimcp_neuron_types.h | .c | 3,091 |
| Neuron Models | src/core/neuron_models/nimcp_neuron_model.h | .c | 1,201 |
| Topology | src/core/topology/nimcp_fractal_topology.h | .c | 1,463 |
| Oscillations | src/core/brain_oscillations/nimcp_brain_oscillations.h | .c | 936 |
| Brain Regions | src/core/brain_regions/nimcp_brain_regions.h | .c | 1,193 |
| Integration | src/core/integration/nimcp_multimodal_integration.h | .c | 646 |
| Distributed COW | src/core/brain/nimcp_distributed_cow.h | .c | 1,265 |
| **TOTAL** | | | **24,346** |

---

## Resources

- **Main Documentation**: `/home/bbrelin/nimcp/ARCHITECTURE_SUMMARY.md` (comprehensive)
- **Visual Diagram**: `/home/bbrelin/nimcp/ARCHITECTURE_DIAGRAM.txt` (layered overview)
- **This Guide**: `/home/bbrelin/nimcp/QUICK_REFERENCE.md` (quick lookup)

---

**Last Updated**: November 11, 2025  
**NIMCP Version**: 2.7.0 / Phase 8.7  
**Total Core Lines**: 24,346
