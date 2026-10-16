# NIMCP Core Architecture Summary

**Document Version:** 2.7.0  
**Last Updated:** November 11, 2025  
**Total Core Lines of Code:** 24,346 LOC

---

## Executive Summary

NIMCP is a sophisticated biologically-inspired neural network framework with a modular architecture consisting of:

- **Public API Layer**: Single unified header (`nimcp.h`, 739 lines)
- **Brain Abstraction Layer**: High-level application interface (1,382 lines header + 6,388 lines implementation)
- **Neural Network Core**: Low-level network implementation (524 lines header + 2,672 lines implementation)
- **Synapse Computation Engine**: Programmable synapse operations (429 lines header + 662 lines implementation)
- **Topology Generation**: Scale-free and fractal network builders (425 lines header + 628 lines implementation)
- **Neuron & Synapse Types**: Biologically realistic diversity system (742 + 682 lines headers + implementations)
- **Advanced Subsystems**: Oscillations, brain regions, integration layers (distributed across core/)

The architecture follows **clean separation of concerns** with well-defined interdependencies and biological realism at the lowest levels.

---

## Core Directory Structure

```
src/core/
├── brain/                          # High-level brain API (application layer)
│   ├── nimcp_brain.h              # 1,382 lines - Core brain API
│   ├── nimcp_brain.c              # 6,388 lines - Implementation
│   ├── nimcp_distributed_cow.h    # 447 lines   - Copy-on-Write cloning
│   ├── nimcp_distributed_cow.c    # 818 lines
│   ├── nimcp_pretrained.c         # 525 lines   - Pre-trained model loading
│   └── processing/                # Cognitive processing modules
│       ├── cognitive_processor.c  # 281 lines
│       ├── multimodal_integrator.c
│       └── sensory_extractor.c
│
├── neuralnet/                      # Low-level neural network (core compute)
│   ├── nimcp_neuralnet.h          # 524 lines   - Network data structures & API
│   └── nimcp_neuralnet.c          # 2,672 lines - Network operations
│
├── synapse_compute/                # Programmable synapse computation (NIMCP 2.7)
│   ├── nimcp_synapse_compute.h    # 429 lines   - Synapse strategy interface
│   └── nimcp_synapse_compute.c    # 662 lines   - Built-in compute functions
│
├── synapse_types/                  # Synapse type system (NIMCP 2.8.7 / Phase 8.7)
│   ├── nimcp_synapse_types.h      # 682 lines   - Receptor types & kinetics
│   └── nimcp_synapse_types.c      # 680 lines   - Type implementations
│
├── neuron_types/                   # Neuron type system (Phase 8.7)
│   ├── nimcp_neuron_types.h       # 742 lines   - Specialized neuron types
│   ├── nimcp_neuron_types.c       # 793 lines   - Type implementations
│   ├── nimcp_neural_logic.h       # 618 lines   - Neural logic gates (Phase 9.4)
│   ├── nimcp_neural_logic.c       # 938 lines
│   └── nimcp_neural_logic_kernels.cu # GPU kernels
│
├── neuron_models/                  # Neuron dynamics models
│   ├── nimcp_neuron_model.h       # 285 lines   - Model interface (LIF, Izhikevich)
│   ├── nimcp_neuron_model.c       # 232 lines
│   ├── nimcp_izhikevich.h         # 199 lines   - Izhikevich dynamics
│   ├── nimcp_izhikevich.c         # 425 lines
│   └── nimcp_neuron_model_internal.h # 60 lines
│
├── topology/                       # Network topology generation
│   ├── nimcp_fractal_topology.h   # 425 lines   - Scale-free & fractal networks
│   ├── nimcp_fractal_topology.c   # 628 lines
│   ├── nimcp_network_builder.h    # 154 lines
│   └── nimcp_network_builder.c    # 256 lines
│
├── brain_oscillations/             # Brain wave analysis (Phase 5.2)
│   ├── nimcp_brain_oscillations.h # 394 lines
│   └── nimcp_brain_oscillations.c # 542 lines
│
├── brain_regions/                  # Brain region modeling
│   ├── nimcp_brain_regions.h      # 476 lines
│   └── nimcp_brain_regions.c      # 717 lines
│
└── integration/                    # Multi-modal integration (Phase 8)
    ├── nimcp_multimodal_integration.h # 242 lines
    └── nimcp_multimodal_integration.c # 404 lines
```

**Total Core LOC: 24,346 lines**

---

## 1. Public API Surface (src/include/nimcp.h)

**File:** `/home/bbrelin/nimcp/src/include/nimcp.h` (739 lines)

The public API is the **single entry point** for all language bindings. It provides a clean C API with:

### Design Principles
- **Opaque handles**: No struct exposure, version-stable API
- **Single namespace**: All symbols use `nimcp_` prefix
- **C99 compatible**: Works with C, C++, Python bindings
- **Semantic versioning**: Version 2.6.1

### Main API Categories

#### 1. Brain API (High-level)
```c
// Creation & lifecycle
nimcp_brain_t nimcp_brain_create(
    const char* name,
    nimcp_brain_size_t size,      // TINY/SMALL/MEDIUM/LARGE
    nimcp_brain_task_t task,       // CLASSIFICATION/REGRESSION/etc
    uint32_t num_inputs,
    uint32_t num_outputs
);

// Learning & Inference
nimcp_status_t nimcp_brain_learn_example(...);
nimcp_status_t nimcp_brain_predict(...);

// Snapshots & Persistence
nimcp_status_t nimcp_brain_snapshot_save(...);
nimcp_brain_t nimcp_brain_snapshot_restore(...);

// Copy-on-Write (COW) - Memory efficient cloning
nimcp_brain_t nimcp_brain_clone_cow(nimcp_brain_t original);
nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(nimcp_brain_t brain);

// Working Memory (Phase 10.1)
nimcp_status_t nimcp_brain_working_memory_add(...);
const float* nimcp_brain_working_memory_get(...);
```

#### 2. Neural Network API (Low-level, advanced users)
```c
// Raw network control
nimcp_network_t nimcp_network_create(
    uint32_t num_inputs,
    uint32_t num_outputs,
    uint32_t num_hidden,
    float learning_rate
);

// Forward pass & training
nimcp_status_t nimcp_network_forward(...);
nimcp_status_t nimcp_network_train(...);
```

#### 3. Ethics Module API
```c
nimcp_ethics_t nimcp_ethics_create(void);
nimcp_status_t nimcp_ethics_check(
    nimcp_ethics_t ethics,
    const float* situation,
    uint32_t num_features,
    float* out_score  // -1.0 = harmful, 0.0 = neutral, 1.0 = beneficial
);
```

#### 4. Knowledge Graph API
```c
nimcp_knowledge_t nimcp_knowledge_create(void);
nimcp_status_t nimcp_knowledge_add_fact(...);
nimcp_status_t nimcp_knowledge_query(...);
```

### Brain Presets
```c
typedef enum {
    NIMCP_BRAIN_TINY,   // 100 neurons,  <1MB,   ~0.1ms inference
    NIMCP_BRAIN_SMALL,  // 1K neurons,   ~10MB,  ~0.5ms inference
    NIMCP_BRAIN_MEDIUM, // 10K neurons,  ~50MB,  ~5ms inference
    NIMCP_BRAIN_LARGE   // 100K neurons, ~500MB, ~50ms inference
} nimcp_brain_size_t;
```

---

## 2. Brain Module (src/core/brain/)

**Files:** 
- Header: `nimcp_brain.h` (1,382 lines)
- Implementation: `nimcp_brain.c` (6,388 lines)

### Design Pattern
**Facade Pattern** - Simplifies complex subsystem interactions

### Key Structures

#### brain_config_t (Comprehensive Configuration)
Enables selective feature activation across 50+ advanced options:

```c
typedef struct {
    // Core
    brain_size_t size;
    brain_task_t task;
    uint32_t num_inputs, num_outputs;
    float learning_rate;
    
    // Phase 5/6: Biological Realism
    bool enable_glial;              // Astrocytes, oligodendrocytes, microglia
    bool enable_oscillations;       // Brain waves (delta, theta, alpha, beta, gamma)
    
    // Phase 8: Sensory Processing
    bool enable_visual_cortex;      // V1 for images
    bool enable_audio_cortex;       // A1 for sound
    bool enable_speech_cortex;      // STG/Wernicke for language
    bool enable_multimodal_integration;
    
    // Phase 10: Advanced Cognition
    bool enable_working_memory;     // Miller's 7±2 buffer
    bool enable_emotional_tagging;  // Emotional context for memories
    bool enable_sleep_wake_cycle;   // Memory consolidation
    bool enable_theory_of_mind;     // Social cognition & empathy
    bool enable_mirror_neurons;     // Observational learning
    
    // Plasticity
    bool enable_eligibility_traces; // Temporal credit assignment
    bool enable_pink_noise;         // Exploration
    bool enable_spike_nlp;          // NLP via spike encoding
    bool enable_fractal_topology;   // Scale-free networks
    
    // Persistence
    const char* checkpoint_path;    // Auto-save/load location
    const char* snapshot_dir;       // Snapshot storage
    bool compress_snapshots;        // zlib compression
    bool encrypt_snapshots;         // AES-256 encryption
    
    // And many more...
} brain_config_t;
```

### API Functions

**Creation**
- `brain_t brain_create(...)` - Simple preset-based creation
- `brain_t brain_create_custom(config)` - Full configuration control
- `brain_t brain_create_distributed(...)` - P2P coordination
- `brain_t brain_create_pretrained(model_id)` - Load pre-trained models

**Learning**
- `float brain_learn_example(...)` - Single example learning
- `float brain_learn_batch(...)` - Batch learning
- `float brain_learn_from_llm(...)` - Learn from external AI (Claude, GPT, etc)

**Inference**
- `brain_decision_t* brain_decide(...)` - Single prediction
- `bool brain_decide_batch(...)` - Batch inference
- `bool brain_observe_action(...)` - Mirror neuron learning (Phase 10.11)

**Snapshots & Persistence**
- `bool brain_save_snapshot(...)` - Named, timestamped backups
- `brain_t brain_restore_snapshot(...)` - Rollback to previous state
- `bool brain_list_snapshots(...)` - Enumerate available snapshots
- `brain_t brain_clone_cow(...)` - Copy-on-Write cloning (86% memory savings)

**Analysis & Optimization**
- `bool brain_get_stats(...)` - Network statistics
- `bool brain_explain_decision(...)` - Interpretability
- `uint32_t brain_prune(...)` - Remove weak connections
- `bool brain_optimize_for_inference(...)` - Aggressive optimization

### Module Interdependencies

```
brain.h depends on:
├── plasticity/adaptive/nimcp_adaptive.h    (Network operations)
├── networking/distributed/nimcp_distributed_cognition.h (P2P)
└── common/nimcp_export.h                   (Build macros)

brain.c additionally uses:
├── core/neuralnet/nimcp_neuralnet.h        (Network access)
├── core/synapse_compute/...                (Synapse functions)
├── cognitive/...                           (Cognitive modules)
└── io/serialization/...                    (Save/load)
```

---

## 3. Neural Network Module (src/core/neuralnet/)

**Files:**
- Header: `nimcp_neuralnet.h` (524 lines)
- Implementation: `nimcp_neuralnet.c` (2,672 lines)

### Core Data Structures

#### neuron_t (Neuron)
```c
typedef struct neuron_struct {
    uint32_t id;                    // Unique ID
    neuron_type_t type;             // EXCITATORY/INHIBITORY/specialized
    float state;                    // Current activation
    float threshold;                // Firing threshold
    float adaptation;               // Adaptive threshold
    
    // Learning rules
    learning_rule_t learning_rule;  // HEBBIAN/OJA/STDP/HYBRID
    activation_type_t activation_type; // SIGMOID/TANH/RELU/LEAKY_RELU
    oja_params_t oja_params;        // Oja learning parameters
    stdp_params_t stdp_params;      // STDP parameters
    homeostatic_params_t homeostatic; // Homeostatic plasticity
    
    // Synaptic connections
    synapse_t synapses[MAX_SYNAPSES_PER_NEURON];  // Outgoing (256 max)
    uint32_t num_synapses;
    
    // Incoming synapses (OPTIMIZATION for O(S) input summation)
    synapse_t incoming_synapses[MAX_SYNAPSES_PER_NEURON];
    uint32_t num_incoming;
    
    // Activity tracking
    spike_record_t spike_history[SPIKE_HISTORY_LENGTH];
    float activity_history[HISTORY_WINDOW];
    
    // Neuron model (LIF, Izhikevich, etc)
    neuron_model_state_t model;
    neuron_model_type_t model_type;
    
    // Phase 8.7: Type-specific parameters
    void* type_params;              // neuron_type_params_t*
} neuron_t;
```

#### synapse_t (Synapse - NIMCP 2.7 with programmable computation)
```c
typedef struct synapse_t {
    uint32_t target_id;             // Target neuron
    float weight;                   // Synaptic weight
    float plasticity;               // Plasticity coefficient
    float strength;                 // Separate from weight
    
    // Short-term plasticity (NIMCP 2.6)
    stp_state_t stp;                // Depression/facilitation state
    bool enable_stp;
    
    // MAJOR FEATURE (NIMCP 2.7): Programmable computation
    synapse_compute_fn compute_function;  // Custom computation
    synapse_learn_fn learn_function;      // Custom learning
    struct synapse_compute_state_t* compute_state; // Function state
    
    // Type system (NIMCP 2.8.7 / Phase 8.7)
    synapse_type_t type;            // AMPA/NMDA/GABA-A/GABA-B/etc
    synapse_type_state_t type_state; // Type-specific kinetics
} synapse_t;
```

### Constants
```c
#define MAX_NEURONS                100000   // Support BRAIN_SIZE_LARGE
#define MAX_SYNAPSES_PER_NEURON    256      // Sparse connectivity
#define SPIKE_HISTORY_LENGTH       1000
#define HISTORY_WINDOW             100
#define NORMALIZATION_INTERVAL     1000     // ms
```

### Learning Rules Supported
- **LEARNING_HEBBIAN**: Basic Hebbian learning (fire together, wire together)
- **LEARNING_OJA**: Oja's learning rule (normalized Hebbian)
- **LEARNING_GENERALIZED_OJA**: Generalized Oja
- **LEARNING_STDP**: Spike-timing dependent plasticity (biologically accurate)
- **LEARNING_HYBRID**: Combined learning rules

### Activation Functions
- ACTIVATION_SIGMOID
- ACTIVATION_TANH
- ACTIVATION_RELU
- ACTIVATION_LEAKY_RELU
- ACTIVATION_ADAPTIVE (threshold-based)

### Key Functions

**Network Operations**
```c
neural_network_t neural_network_create(const network_config_t* config);
bool neural_network_forward(network, inputs, num_inputs, outputs, num_outputs);
uint32_t neural_network_compute_step(network, timestamp);

// Neuron access
uint32_t neural_network_get_num_neurons(network);
neuron_t* neural_network_get_neuron(network, neuron_id);
uint32_t neural_network_add_neuron(network, activation);
bool neural_network_add_connection(from_id, to_id, weight);
bool neural_network_add_connection_typed(from_id, to_id, weight, type); // Phase 8.7
```

**Plasticity Functions**
```c
uint32_t neural_network_apply_stdp(network, neuron_id, timestamp);
uint32_t neural_network_apply_oja(network, neuron_id, timestamp);
bool neural_network_apply_homeostasis(network, neuron_id, timestamp);
bool neural_network_normalize_weights(network, neuron_id);
```

**State & Analysis**
```c
bool neural_network_get_stats(network, stats);
bool neural_network_set_neuron_model(network, neuron_id, model_type, params);
uint32_t neural_network_prune_synapses(network, threshold);

// Bidirectional synapse access (OPTIMIZATION)
uint32_t neural_network_get_incoming_synapse_count(network, neuron_id);
uint32_t neural_network_get_incoming_synapses(network, neuron_id, &synapses);
```

**Integration with Other Modules**
```c
bool neural_network_set_global_state(network, global_state, size);
bool neural_network_set_neuromodulator_system(network, neuromod_system);
bool neural_network_set_glial_integration(network, glial_system);
float neural_network_get_neuromodulation(network);
```

---

## 4. Synapse Computation Module (src/core/synapse_compute/)

**Files:**
- Header: `nimcp_synapse_compute.h` (429 lines)
- Implementation: `nimcp_synapse_compute.c` (662 lines)

### Revolutionary Feature (NIMCP 2.7)
**Transforms synapses from passive weights → active processors**

Real biological synapses compute, not just multiply. This module enables per-synapse customization.

### Design Patterns
- **Strategy Pattern**: Function pointers for algorithmic variation
- **Memento Pattern**: State preservation without global variables
- **Template Method**: Consistent interface, variable implementation

### Context Structure

```c
typedef struct synapse_compute_context_t {
    float* global_state;            // Global network state (e.g., attention outputs)
    uint32_t global_state_size;
    
    float neuromodulation;          // Dopamine/serotonin/ACh levels [0,1]
    uint64_t current_time;          // Simulation timestamp (ms)
    
    void* custom_data;              // Task-specific context
    uint32_t custom_data_size;
} synapse_compute_context_t;
```

### State Structure

```c
typedef struct synapse_compute_state_t {
    float local_memory[16];         // Fast scratchpad (64B, L1-cached)
    float* extended_memory;         // Heap for large state (embeddings)
    uint32_t extended_size;
    
    void* function_data;            // Opaque function-specific data
    void (*cleanup_fn)(void*);      // Cleanup on destruction
} synapse_compute_state_t;
```

### Built-in Compute Functions (Strategy Library)

#### 1. Default Computation
```c
output = weight × pre_activity × STP_modulation
Complexity: O(1), ~10 cycles
```

#### 2. Attention-Modulated (Transformer-like)
```c
attention = exp(query · key / √d)
output = weight × pre_activity × attention × STP
Complexity: O(d), ~100-500 cycles (d = embedding dim)
```

#### 3. Semantic Similarity (NLP)
```c
similarity = cosine(pre_embedding, post_embedding)
output = weight × pre_activity × similarity × STP
Complexity: O(d), ~300 cycles for typical embeddings
```

#### 4. Gating (LSTM-like)
```c
output = weight × pre_activity × gate_signal × STP
Use cases: Input/forget/output gates, attention gating
```

#### 5. Neuromodulation-Sensitive
```c
modulation = 1.0 + dopamine × sensitivity
output = weight × pre_activity × modulation × STP
```

#### 6. Dendritic Computation
```c
nonlinear = sigmoid(Σ nearby_synapses)
output = weight × pre_activity × nonlinear × STP
Complexity: O(k) where k = dendritic branch size (~10-50)
```

### Built-in Learning Functions

#### 1. Three-Factor Learning
```c
// Combines STDP, eligibility traces, and reward signals
Δw = η × eligibility_trace × reward_signal
Biological: Dopaminergic reward learning in basal ganglia
```

#### 2. Attention-Modulated Learning
```c
// Learning rate scales by attention weight
Δw = η × attention × STDP(Δt)
Effect: Focused learning on relevant inputs
```

#### 3. Meta-Plasticity (BCM-like)
```c
// Learning rate adapts based on recent activity
η_effective = η_base × f(activity - threshold)
Effect: Active synapses consolidate, learning slows
```

### Performance Characteristics
- Function call overhead: 1 indirect jump (~5 cycles)
- Memory overhead: 24 bytes per synapse (3 pointers on 64-bit)
- For 100K synapses: 2.4 MB overhead
- Cache efficient: Function pointers in L1, state in L1/L2
- Fully parallelizable across synapses
- GPU-friendly: Can use CUDA device functions

---

## 5. Synapse Type System (src/core/synapse_types/)

**Files:**
- Header: `nimcp_synapse_types.h` (682 lines)
- Implementation: `nimcp_synapse_types.c` (680 lines)

### NIMCP Phase 8.7 Feature
**Biologically realistic synapse diversity**

Real brains don't use homogeneous synapses - they use a diverse toolkit of specialized receptor types with different kinetics, signaling pathways, and plasticity rules.

### Synapse Types Supported

#### Excitatory Synapses
```c
SYNAPSE_AMPA        // Fast excitation (τ = 2ms)
                    // Glutamate ionotropic (Na+/K+ channels)
                    // Non-voltage dependent
                    // For rapid transmission

SYNAPSE_NMDA        // Slow excitation (τ = 100ms)
                    // Glutamate ionotropic + voltage-gated (Mg2+ block)
                    // Ca2+ permeable (critical for LTP/learning)
                    // For sustained excitation
```

#### Inhibitory Synapses
```c
SYNAPSE_GABA_A      // Fast inhibition (τ = 10ms)
                    // GABA ionotropic (Cl- channel)
                    // Hyperpolarizing
                    // For rapid suppression

SYNAPSE_GABA_B      // Slow inhibition (τ = 150ms)
                    // GABA metabotropic (K+ channel)
                    // G-protein coupled
                    // For sustained inhibition
```

#### Neuromodulatory Synapses
```c
SYNAPSE_DOPAMINE    // Reward/learning modulation
                    // D1/D2 receptors, G-protein coupled
                    // Amplifies relevant learning
                    
SYNAPSE_SEROTONIN   // Mood/stability modulation
                    // 5-HT receptors, G-protein coupled
                    // Influences decision-making
                    
SYNAPSE_ACETYLCHOLINE // Attention/arousal modulation
                    // Nicotinic/muscarinic receptors
                    // Enhances learning of attended inputs
```

#### Electrical Synapses
```c
SYNAPSE_ELECTRICAL  // Gap junction coupling
                    // Bidirectional, instantaneous
                    // For synchronization
```

### Type-Specific State Examples

#### AMPA State
```c
typedef struct ampa_state_t {
    float conductance;              // Current conductance (nS)
    float g_max;                    // Max conductance (typically 0.5-2.0 nS)
    float tau_rise;                 // Rise time (typically 0.5 ms)
    float tau_decay;                // Decay time (typically 2 ms)
    float reversal_potential;       // Reversal potential (typically 0 mV)
} ampa_state_t;
```

#### NMDA State
```c
typedef struct nmda_state_t {
    float conductance;
    float g_max;
    float tau_rise, tau_decay;
    float reversal_potential;
    
    // Voltage-dependent Mg2+ block
    float mg_block;                 // Mg2+ blockade [0,1]
    float voltage;                  // Postsynaptic voltage (mV)
    
    // Calcium influx (critical for learning)
    float calcium_influx;           // Ca2+ current (pA)
} nmda_state_t;
```

### Biological References
- Dayan & Abbott (2001): Theoretical Neuroscience
- Koch (1999): Biophysics of Computation
- Destexhe et al. (1994): AMPA/NMDA/GABA kinetics
- Jahr & Stevens (1990): NMDA voltage dependence
- Seamans & Yang (2004): Dopamine modulation
- Hasselmo (1999): Acetylcholine modulation

---

## 6. Topology Generation Module (src/core/topology/)

**Files:**
- Header: `nimcp_fractal_topology.h` (425 lines)
- Implementation: `nimcp_fractal_topology.c` (628 lines)

### Motivation
Real brains don't use random connectivity - they use scale-free networks with:
- 70-80% fewer connections than random
- Hub-based architecture (small % of neurons handle 80% of traffic)
- Power-law degree distribution
- Robust to failures
- Biologically accurate

### Topology Types

#### 1. Scale-Free (Barabási-Albert)
```c
typedef struct {
    float power_law_gamma;          // Exponent P(k)~k^γ, typical: -2.0 to -3.0
    float hub_ratio;                // Fraction of hubs, typical: 0.10-0.20
    uint32_t min_degree, max_degree; // Connection limits
    float spatial_constraint;       // 0=ignore distance, 1=strong spatial
    bool bidirectional;             // Reciprocal connections
} scale_free_config_t;
```

Algorithm:
1. Start with seed fully-connected network
2. Add neurons one at a time
3. Each new neuron connects with probability ∝ degree (preferential attachment)
4. Result: Power-law degree distribution (hub structure)

**Biological basis:** Matches cortical connectivity (Sporns et al., 2004)

#### 2. Fractal (Hierarchical)
```c
typedef struct {
    float fractal_dimension;        // Typical: 1.5-2.5 (cortex ~2.5)
    uint32_t hierarchy_levels;      // Typical: 3-5
    float branching_factor;         // Typical: 2-4
    float scale_factor;             // Size reduction per level: 0.5-0.8
    float clustering_coeff;         // Local clustering: 0.3-0.6
} fractal_config_t;
```

**Biological basis:** Cortical columns, hierarchical visual processing

### Key Analysis Functions

**Topology Statistics**
```c
typedef struct {
    uint32_t num_neurons, num_synapses;
    float avg_degree, degree_std;
    float clustering_coefficient;
    float characteristic_path;      // Average shortest path length
    float power_law_fit;            // R² for power-law fit (0-1)
    uint32_t num_hubs;
    float hub_connectivity;         // Fraction through hubs
    float small_world_sigma;        // C/Crand / L/Lrand
} topology_stats_t;
```

**Functions**
```c
bool topology_generate_scale_free(network, config, stats);
bool topology_generate_fractal(network, config, stats);
bool topology_generate(network, config, stats);      // Dispatcher

// Analysis
bool topology_compute_stats(network, stats);
bool topology_is_small_world(network, sigma);
bool topology_fit_power_law(network, gamma, r_squared);

// Hub neurons
bool topology_identify_hubs(network, percentile, hub_indices, count);
bool topology_compute_betweenness(network, centrality);
```

### Performance Characteristics
- Scale-free generation: O(N log N) time, O(N) space
- Analysis: O(N² log N) for full statistics
- 70-80% reduction in synapses vs random
- GPU-friendly: Connection matrix generation parallelizable

---

## 7. Neuron Types System (src/core/neuron_types/)

**Files:**
- Header: `nimcp_neuron_types.h` (742 lines)
- Implementation: `nimcp_neuron_types.c` (793 lines)
- Neural Logic: `nimcp_neural_logic.h` (618 lines) + `.c` (938 lines)

### Phase 8.7 Feature: Neuron Specialization

Standard neuron types have limitations. Real cortex uses ~20+ specialized neuron types, each optimized for different functions.

### Neuron Types Implemented

#### Canonical Types
```c
NEURON_EXCITATORY       // Standard pyramidal cell
NEURON_INHIBITORY       // Standard GABAergic interneuron
```

#### Specialized Types (Phase 8.7)

**Sensory Cortex Neurons**
```c
NEURON_V1_SIMPLE_CELL   // Primary visual cortex
                        // Detects oriented edges (Gabor filters)
                        // Thalamic input → Layer 4

NEURON_V1_COMPLEX_CELL  // Position/scale invariant
                        // Pools over simple cells
                        // Layer 2/3 → Higher visual areas

NEURON_A1_FREQ_TUNED    // Primary auditory cortex
                        // Frequency selective
                        // Cochlear nucleus → Layer 4
```

**Learning & Memory Neurons**
```c
NEURON_PLACE_CELL       // Spatial location coding
                        // Hippocampal formation
                        // Fire at specific locations

NEURON_GRID_CELL        // Hexagonal spatial grid
                        // Entorhinal cortex
                        // Regular spacing across environment

NEURON_HEAD_DIRECTION   // Directional tuning
                        // Postsubiculum, anterior thalamus
                        // Fire based on head direction
```

**Executive Function Neurons**
```c
NEURON_PREFRONTAL_PFC   // Working memory & planning
                        // Dorsolateral prefrontal cortex
                        // Sustained activity for rule holding

NEURON_MOTOR_PLANNING   // Movement planning
                        // Primary motor cortex
                        // Pre-plan actions
```

**Emotional & Motivational Neurons**
```c
NEURON_FEAR_NEURON      // Threat detection
                        // Amygdala (basal lateral nucleus)
                        // Conditional fear responses

NEURON_REWARD_NEURON    // Reward prediction
                        // Dopamine neurons (VTA/SNc)
                        // Fire on prediction errors
```

### Type-Specific Parameters

Each neuron type has customized parameters:
```c
typedef union {
    v1_simple_cell_params_t simple;
    v1_complex_cell_params_t complex;
    a1_freq_tuned_params_t audio;
    place_cell_params_t place;
    grid_cell_params_t grid;
    // ... more types
} neuron_type_params_t;
```

Example - V1 Simple Cell:
```c
typedef struct {
    float preferred_orientation;    // 0-180° (what edge it likes)
    float orientation_bandwidth;    // Selectivity (narrow vs broad)
    float spatial_frequency;        // Preferred grating frequency (cycles/degree)
    float temporal_frequency;       // Speed tuning (Hz)
    float receptive_field_size;     // RF diameter (degrees visual angle)
} v1_simple_cell_params_t;
```

### Neural Logic Gates (Phase 9.4)

Advanced logic operations enable reasoning and symbolic processing:
```c
GATE_AND, GATE_OR, GATE_NOT
GATE_XOR, GATE_NAND, GATE_NOR
GATE_THRESHOLD        // Custom threshold logic
GATE_COINCIDENCE_DETECTOR  // Temporal AND
```

---

## 8. Neuron Models Module (src/core/neuron_models/)

**Files:**
- Header: `nimcp_neuron_model.h` (285 lines)
- Implementation: `nimcp_neuron_model.c` (232 lines)
- Izhikevich: `nimcp_izhikevich.h` (199 lines) + `.c` (425 lines)

### Plugin Architecture for Neuron Dynamics

Different neuron dynamics models have different properties:

#### Leaky Integrate-and-Fire (LIF) - Default
```c
dv/dt = (leak * (rest - v) + input) / tau
fires when v > threshold
v = rest_potential after firing
```

**Characteristics:**
- Simple, fast computation O(1)
- No neurophysiological detail
- Used in most spiking networks

#### Izhikevich Model
```c
dv/dt = 0.04v² + 5v + 140 - u + I
du/dt = a(bv - u)
if v >= 30: v = c, u = u + d
```

**Characteristics:**
- Biologically realistic spike shapes
- Can exhibit diverse dynamics (regular, bursting, chattering, etc)
- Moderate computational cost O(1) per step
- Single neuron can exhibit different firing patterns
- **Widely used in brain-inspired computing**

### Model Interface

```c
typedef enum {
    NEURON_MODEL_LIF,           // Leaky Integrate-and-Fire
    NEURON_MODEL_IZHIKEVICH,    // Izhikevich (biological)
    NEURON_MODEL_HODGKIN_HUXLEY, // Full biophysics (future)
    NEURON_MODEL_CUSTOM          // User-defined
} neuron_model_type_t;
```

**State Structure**
```c
typedef struct {
    float voltage;              // Current voltage/potential
    float recovery_variable;    // u in Izhikevich, adaptation in LIF
    float threshold;            // Dynamic threshold for spike
    // Model-specific state...
} neuron_model_state_t;
```

**Model-Specific Parameters**
```c
typedef struct {
    float a, b, c, d;  // Izhikevich parameters
    float rest_potential;
    float spike_magnitude;
} izhikevich_params_t;
```

### Configuration Functions
```c
bool neural_network_set_neuron_model(
    network,
    neuron_id,
    NEURON_MODEL_IZHIKEVICH,
    params  // NULL for defaults
);
```

---

## 9. Brain Oscillations Module (src/core/brain_oscillations/)

**Files:**
- Header: `nimcp_brain_oscillations.h` (394 lines)
- Implementation: `nimcp_brain_oscillations.c` (542 lines)

### Phase 5.2 Feature: Neural Oscillations

Real brains use synchronized rhythmic activity for:
- **Attention coordination** (gamma waves ~40 Hz)
- **Memory consolidation** (sleep spindles, slow waves)
- **Sensory binding** (oscillatory communication)

### Supported Oscillation Types
```c
typedef enum {
    OSCILLATION_DELTA,      // 0.5-4 Hz   - Deep sleep
    OSCILLATION_THETA,      // 4-8 Hz     - Learning, memory
    OSCILLATION_ALPHA,      // 8-12 Hz    - Relaxation, inhibition
    OSCILLATION_BETA,       // 12-30 Hz   - Motor, attention
    OSCILLATION_GAMMA       // 30-100 Hz  - Attention, binding
} oscillation_type_t;
```

### Key Functions
```c
// Analysis
bool oscillation_detect_frequency(network, type, confidence);
float oscillation_get_power(network, frequency);
float oscillation_get_coherence(network, neuron1, neuron2);

// Modulation
bool oscillation_modulate_learning(network, type, strength);
bool oscillation_sync_populations(network, type, neuron_ids, count);
```

---

## 10. Brain Regions Module (src/core/brain_regions/)

**Files:**
- Header: `nimcp_brain_regions.h` (476 lines)
- Implementation: `nimcp_brain_regions.c` (717 lines)

### Functional Brain Area Modeling

Maps computational functions to specialized brain regions:

```c
typedef enum {
    BRAIN_REGION_PRIMARY_VISUAL,    // V1 - Basic feature detection
    BRAIN_REGION_TEMPORAL_CORTEX,   // IT - Object recognition
    BRAIN_REGION_PARIETAL_CORTEX,   // Spatial processing
    BRAIN_REGION_PREFRONTAL_CORTEX, // Planning, working memory
    BRAIN_REGION_AMYGDALA,          // Emotional processing
    BRAIN_REGION_HIPPOCAMPUS,       // Declarative memory, navigation
    BRAIN_REGION_STRIATUM,          // Procedural learning, action selection
    BRAIN_REGION_CEREBELLUM,        // Motor learning, timing
} brain_region_type_t;
```

### Functions
```c
bool brain_region_create(network, type, neurons_start, neurons_end);
bool brain_region_set_properties(network, type, properties);
bool brain_region_is_active(network, type, &confidence);
uint32_t brain_region_get_neuron_count(network, type);
```

---

## 11. Multimodal Integration Module (src/core/integration/)

**Files:**
- Header: `nimcp_multimodal_integration.h` (242 lines)
- Implementation: `nimcp_multimodal_integration.c` (404 lines)

### Phase 8 Feature: Unified Multi-Modal Processing

Enables processing of vision, audio, language, and direct features in unified pipeline.

### Processing Pipeline

```
Input Stage:
├── Visual (RGB/Grayscale) → CNN features
├── Audio (PCM samples) → FFT/spectral features
├── Language (text) → Word embeddings (Phase 9.4)
└── Direct (feature vector)

Integration Stage:
└── Attention-weighted fusion of modalities

Neural Processing:
├── STDP learning
├── Glial modulation
├── Brain oscillations
└── Pink noise exploration

Cognitive Processing (7-stage pipeline):
1. Pre-processing wellbeing check
2. Introspection (confidence estimation)
3. Ethics filtering
4. Salience detection
5. Knowledge integration
6. Curiosity-driven exploration
7. Post-processing wellbeing check

Output Stage:
└── Decision + explanations + attention breakdown
```

### Data Structures

```c
typedef struct {
    // Visual (optional)
    const uint8_t* visual_data;
    uint32_t visual_width, visual_height, visual_channels;
    
    // Audio (optional)
    const float* audio_data;
    uint32_t audio_samples;
    uint8_t audio_channels;
    
    // Language (optional, Phase 9.4)
    const char* language_text;
    uint32_t language_length;
    const float* language_embeddings;
    uint32_t language_embed_dim;
    
    // Direct features (optional)
    const float* direct_data;
    uint32_t direct_dim;
    
    uint64_t timestamp_ms;
} brain_multimodal_input_t;

typedef struct {
    // Core output
    float* output_vector;
    uint32_t output_dim;
    char decision_label[64];
    float confidence;
    
    // Cognitive assessments
    float introspection_uncertainty;
    float salience_score;
    bool ethical_approved;
    float novelty_score;
    
    // Epistemic filtering (Phase 9.2)
    float epistemic_quality;
    float skepticism_score;
    float credibility_score;
    bool bias_detected;
    bool requires_verification;
    char epistemic_reasoning[256];
    
    // Attention breakdown
    float visual_attention;
    float audio_attention;
    float language_attention;
    float direct_attention;
    
    // Language response (Phase 9.4)
    char* language_response;
    uint32_t language_response_length;
    float language_confidence;
    
    // Logical reasoning (Phase 9.4)
    bool logical_consistency;
    float reasoning_confidence;
    char logical_reasoning[256];
    
    char explanation[256];
} brain_multimodal_output_t;
```

### Processing Function
```c
bool brain_process_multimodal(
    brain_t brain,
    const brain_multimodal_input_t* input,
    brain_multimodal_output_t* output
);
```

---

## 12. Distributed Copy-on-Write Module (src/core/brain/nimcp_distributed_cow.h)

**Files:**
- Header: `nimcp_distributed_cow.h` (447 lines)
- Implementation: `nimcp_distributed_cow.c` (818 lines)

### Phase 2.8 Feature: Efficient Distributed Deployment

Enables lightweight brain clones across network nodes without full data copies.

### Architecture

```
Master Node (192.168.1.100:5000)
├── Full Brain
├── Network weights
├── Knowledge base
└── P2P listener

Remote Nodes (192.168.1.101, .102, etc)
├── Lightweight COW clone
├── Cache (1-10MB)
└── Lazy loading on first use
```

### Performance
- Clone time: <10ms (vs ~1000ms full copy)
- Memory overhead: ~1MB metadata (vs ~50MB full copy)
- Memory savings: 86% for replicas, 99% for snapshots
- Bandwidth: Lazy loading - only fetches neurons needed

### Functions
```c
brain_t brain_clone_cow_distributed(
    brain_t original,
    const char* remote_host,
    uint16_t remote_port,
    const distributed_cow_config_t* config
);

bool brain_enable_distributed_cow_master(brain_t brain, p2p_node_t p2p_node);
bool brain_get_distributed_cow_stats(brain_t brain, distributed_cow_stats_t* stats);
bool brain_is_distributed_cow(brain_t brain);
```

---

## Architecture Patterns & Design Principles

### 1. Layered Architecture

```
┌─────────────────────────────────────────┐
│ Public API Layer (nimcp.h)              │ Single entry point
├─────────────────────────────────────────┤
│ Brain Layer (brain.h/c)                 │ Facade pattern
├─────────────────────────────────────────┤
│ Neural Network Layer (neuralnet.h/c)    │ Core computation
├─────────────────────────────────────────┤
│ Synapse/Neuron Specialization           │ Strategy pattern
│ ├─ Synapse Compute (synapse_compute)    │
│ ├─ Synapse Types (synapse_types)        │
│ ├─ Neuron Types (neuron_types)          │
│ └─ Neuron Models (neuron_models)        │
├─────────────────────────────────────────┤
│ Topology Generation (topology)          │ Builder pattern
├─────────────────────────────────────────┤
│ Specialized Subsystems                  │
│ ├─ Brain Oscillations                   │
│ ├─ Brain Regions                        │
│ ├─ Multimodal Integration               │
│ └─ Distributed COW                      │
├─────────────────────────────────────────┤
│ Support Layers                          │
│ ├─ Plasticity (adaptive, stp, etc)     │
│ ├─ Networking (distributed cognition)   │
│ ├─ I/O (serialization, encryption)      │
│ └─ Cognitive Modules (ethics, etc)      │
└─────────────────────────────────────────┘
```

### 2. Design Patterns Used

| Pattern | Module | Purpose |
|---------|--------|---------|
| **Facade** | brain.h | Simplifies complex neural network operations |
| **Strategy** | synapse_compute.h | Per-synapse algorithmic variation |
| **Template Method** | synapse_types.h | Consistent interface, type-specific implementations |
| **State** | synapse_compute_state_t | Preserve synapse-specific state |
| **Builder** | topology/ | Complex network construction |
| **Factory** | topology_config_t | Create various topologies |
| **Observer** | glial integration | Neural events notify glial system |
| **Dependency Injection** | neuralnet.h | Inject global state, neuromod system |
| **Opaque Pointers** | nimcp.h | Version stability, hiding internals |

### 3. Key Design Principles

**Biological Realism**
- Spike-timing dependent plasticity (STDP)
- Homeostatic plasticity
- Diverse neurotransmitter systems
- Hierarchical organization (cortical columns, brain regions)
- Oscillations for coordination

**Computational Efficiency**
- Scale-free networks: 70-80% fewer synapses
- Sparse connectivity: O(S) operations instead of O(N²)
- Copy-on-Write: 86% memory savings for clones
- Bidirectional synapse tracking: O(S) input summation

**Extensibility**
- Programmable synapses (NIMCP 2.7)
- Pluggable neuron models
- Diverse synapse types
- Custom compute/learning functions

**Version Stability**
- Public API uses opaque handles
- No internal struct exposure
- Semantic versioning
- Backward compatibility

---

## Module Interdependencies Graph

```
nimcp.h (Public API)
└── brain.h
    ├── adaptive_network_t (plasticity/adaptive)
    ├── distributed_cognition (networking)
    ├── neuralnet.h
    │   ├── neuron_model.h
    │   │   └── izhikevich.h
    │   ├── neuron_types.h (Phase 8.7)
    │   │   └── neural_logic.h
    │   ├── synapse_types.h (Phase 8.7)
    │   ├── synapse_compute.h (NIMCP 2.7)
    │   └── stp.h (plasticity)
    ├── distributed_cow.h
    ├── brain_oscillations.h
    ├── brain_regions.h
    ├── multimodal_integration.h
    │   ├── sensory extraction (visual, audio, language)
    │   └── cognitive_processor.h
    ├── working_memory_t
    ├── emotional_system_t
    ├── theory_of_mind_t
    ├── meta_learner_t
    └── ... (50+ subsystems)

topology.h
└── neuralnet.h

synapse_compute.h
└── neuralnet.h

No circular dependencies (DAG structure)
```

---

## Total Lines of Code Breakdown

| Module | Header | Implementation | Total |
|--------|--------|-----------------|-------|
| Brain | 1,382 | 6,388 | **7,770** |
| Neural Network | 524 | 2,672 | **3,196** |
| Synapse Compute | 429 | 662 | **1,091** |
| Synapse Types | 682 | 680 | **1,362** |
| Neuron Types | 742 | 793 | **1,535** |
| Neural Logic | 618 | 938 | **1,556** |
| Topology | 579 | 884 | **1,463** |
| Brain Oscillations | 394 | 542 | **936** |
| Brain Regions | 476 | 717 | **1,193** |
| Multimodal Integration | 242 | 404 | **646** |
| Neuron Models | 544 | 657 | **1,201** |
| Distributed COW | 447 | 818 | **1,265** |
| Processing | - | 281 | **281** |
| **TOTAL CORE** | | | **24,346** |
| **Public API (nimcp.h)** | | | **739** |
| **TOTAL WITH API** | | | **25,085** |

---

## Key Features by NIMCP Version

### NIMCP 2.0-2.5
- Basic neural network
- STDP learning
- Synaptic plasticity

### NIMCP 2.6
- Short-term plasticity (STP)
- Izhikevich neuron model
- Neuron model plugin architecture

### NIMCP 2.7 (Synapse Computation Revolution)
- **Programmable synapses** (function pointers)
- Custom compute functions (attention, semantic, dendritic)
- Custom learning functions (three-factor, meta-plastic)
- Synapse compute context & state
- Strategy pattern for synapse operations

### NIMCP 2.8.7 / Phase 8.7 (Synapse & Neuron Specialization)
- **Synapse type system** (AMPA, NMDA, GABA-A, GABA-B, etc)
- Type-specific kinetics & dynamics
- **Neuron type system** (V1 simple/complex, place cells, etc)
- Type-specific parameters
- Biologically realistic receptor diversity

### Phase 8 (Unified Multi-Modal Processing)
- Visual cortex (CNN features)
- Audio cortex (FFT features)
- Language processing (embeddings, Phase 9.4)
- Multimodal integration & attention
- 7-stage cognitive pipeline

### Phase 9.4 (Human Communication & Reasoning)
- Language understanding & generation
- Logical reasoning (neural logic gates)
- Epistemic filtering (bias detection)
- Theory of mind
- Natural language explanations

### Phase 10 (Advanced Cognition)
- Working memory (Miller's 7±2)
- Emotional tagging
- Sleep-wake cycles
- Mental health monitoring
- Theory of mind & empathy
- Mirror neurons (observational learning)
- Predictive processing

---

## Performance Characteristics

### Memory Usage
- **Tiny brain**: 100 neurons, <1MB
- **Small brain**: 1K neurons, ~10MB
- **Medium brain**: 10K neurons, ~50MB
- **Large brain**: 100K neurons, ~500MB

### Inference Speed
- **Tiny**: ~0.1ms
- **Small**: ~0.5ms
- **Medium**: ~5ms
- **Large**: ~50ms

### Copy-on-Write Efficiency
- Clone time: <10ms
- Memory overhead: ~1MB metadata
- Memory savings: 86% for replicas, 99% for snapshots

### Synapse Computation Overhead
- Default: ~10 cycles per synapse
- Attention: ~100-500 cycles (d = embedding dimension)
- Semantic: ~300 cycles (typical embeddings)
- Dendritic: O(k) where k = dendritic branch size

---

## File Locations Summary

### Core Modules
```
/home/bbrelin/nimcp/src/core/
├── brain/                    (7,770 LOC)
├── neuralnet/               (3,196 LOC)
├── synapse_compute/         (1,091 LOC)
├── synapse_types/           (1,362 LOC)
├── neuron_types/            (1,535 LOC)
├── neuron_models/           (1,201 LOC)
├── topology/                (1,463 LOC)
├── brain_oscillations/        (936 LOC)
├── brain_regions/           (1,193 LOC)
├── integration/               (646 LOC)
└── brain/ (distributed)     (1,265 LOC)
```

### Public API
```
/home/bbrelin/nimcp/src/include/nimcp.h (739 LOC)
```

---

## Architectural Strengths

1. **Biological Realism**: STDP, diverse synapse/neuron types, oscillations
2. **Computational Efficiency**: Scale-free networks, sparse connectivity, COW memory sharing
3. **Extensibility**: Programmable synapses, pluggable models, strategy pattern
4. **Modularity**: Clean layered architecture, no circular dependencies
5. **Performance**: <1-50ms inference depending on size
6. **Version Stability**: Opaque API, semantic versioning
7. **Cognitive Capabilities**: Ethics, working memory, theory of mind, mirror neurons
8. **Distributed Processing**: P2P coordination, efficient cloning, lazy loading

---

## Conclusion

NIMCP is a sophisticated, well-architected neural framework that balances:
- **Biological realism** (STDP, neurotransmitter diversity, oscillations)
- **Computational efficiency** (sparse networks, COW caching)
- **Extensibility** (programmable synapses, pluggable models)
- **Usability** (clean API, high/low-level options)

The core 24,346 LOC represents a mature, production-ready system supporting everything from basic classification to advanced cognitive tasks with distributed deployment.
