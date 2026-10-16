# NIMCP Software Specification and Architecture Document

**Version**: 2.6.3
**Date**: March 2026
**Author**: Braun Brelin, with Claude (Anthropic)

---

## Table of Contents

1. [System Overview](#1-system-overview)
2. [Design Philosophy](#2-design-philosophy)
3. [High-Level Architecture](#3-high-level-architecture)
4. [Core Neural Engine](#4-core-neural-engine)
5. [Neuron Models and Types](#5-neuron-models-and-types)
6. [Synapse System](#6-synapse-system)
7. [Multi-Layer Diamond Architecture](#7-multi-layer-diamond-architecture)
8. [Forward and Backward Pass](#8-forward-and-backward-pass)
9. [Learning and Plasticity](#9-learning-and-plasticity)
10. [Cognitive Modules](#10-cognitive-modules)
11. [Decision Pipeline](#11-decision-pipeline)
12. [GPU Acceleration](#12-gpu-acceleration)
13. [Memory Management](#13-memory-management)
14. [Threading and Asynchronous Communication](#14-threading-and-asynchronous-communication)
15. [Security and Safety Subsystems](#15-security-and-safety-subsystems)
16. [Glial Cell Simulation](#16-glial-cell-simulation)
17. [Configuration and Initialization](#17-configuration-and-initialization)
18. [Python Bindings](#18-python-bindings)
19. [Tensor Library](#19-tensor-library)
20. [Error Handling](#20-error-handling)
21. [File Organization](#21-file-organization)
22. [Build System](#22-build-system)
23. [Key Constants and Limits](#23-key-constants-and-limits)
24. [Design Patterns](#24-design-patterns)

---

## 1. System Overview

NIMCP (Neuromorphic Infant Machine Cognitive Platform) is a biologically-inspired neural computing platform implemented in C with CUDA acceleration. It combines spiking neural networks with 30+ cognitive modules to create a system that processes information through brain-like mechanisms.

### Scale

| Metric | Value |
|--------|-------|
| C source files | ~2,552 |
| Header files | ~2,456 |
| CUDA kernel files | 83 |
| GPU support files | 32 |
| Cognitive modules | 60+ |
| Brain regions | 33+ |
| Plasticity mechanisms | 26+ (STDP, BCM, eligibility, dendritic, homeostatic, etc.) |
| Plasticity header files | 164 (largest subsystem) |
| SNN cross-modal bridges | 42 |
| Synapse types | 9 |
| Neuron type categories | 7 (with 30+ subtypes) |
| Default neuron count | 2,000,000 |
| Brain struct fields | 800+ |
| Python API methods | 150+ |
| Neuromodulators | 6 (DA, 5-HT, ACh, NE, GABA, Glu) |
| Language bindings | 7 (Python, Go, Rust, Java, Node.js, Ruby, C#) |

### Hardware Target

- **Primary**: NVIDIA RTX 4000 SFF Ada Generation (20 GB VRAM, CUDA 12.8)
- **Policy**: GPU-first; all operations default to GPU with CPU fallbacks
- **CPU fallbacks**: `src/gpu/stubs/` provides non-CUDA implementations

---

## 2. Design Philosophy

### Biological Inspiration, Not Simulation

NIMCP draws architectural inspiration from neuroscience without claiming biological accuracy. The goal is to explore whether brain-like structural properties (self-regulation, modularity, developmental gating) provide useful engineering properties for AI systems.

### Key Principles

1. **Safety by architecture**: Immune systems, ethics modules, and developmental gating are structural, not bolted-on
2. **Modularity**: Each cognitive capability is an independent module with defined interfaces
3. **GPU-first**: Computation defaults to GPU; CPU is a fallback
4. **Opaque handles**: Public API exposes handles (`nimcp_brain_t`), not internal structures
5. **Thread safety**: All shared state is mutex-protected; bio-async messaging for inter-module communication
6. **Sparse representation**: Synapses use sparse storage (97% memory savings over dense arrays)

---

## 3. High-Level Architecture

```
+-------------------------------------------------------------------+
|                        Public API (nimcp.h)                        |
|  nimcp_brain_create() | nimcp_brain_predict() | nimcp_brain_learn()|
+-------------------------------------------------------------------+
         |                      |                      |
+-------------------------------------------------------------------+
|                      Brain Layer (brain_t)                         |
|  Configuration | Decision Cache | Subsystem Orchestration         |
+-------------------------------------------------------------------+
    |          |           |            |           |          |
+--------+ +--------+ +----------+ +--------+ +--------+ +-------+
| Neural | |Cognitive| |Plasticity| |  GPU   | |Security| | Glial |
| Engine | |Modules  | |  System  | |Backend | | Layer  | | Cells |
+--------+ +--------+ +----------+ +--------+ +--------+ +-------+
    |          |           |            |           |          |
+-------------------------------------------------------------------+
|                    Infrastructure Layer                            |
|  Memory Mgmt | Threading | Bio-Async | Tensor | Exception | Config|
+-------------------------------------------------------------------+
```

### Layer Responsibilities

| Layer | Responsibility |
|-------|---------------|
| **Public API** | Stable C interface; opaque handles; language-binding friendly |
| **Brain** | Orchestrates all subsystems; manages lifecycle; caches decisions |
| **Neural Engine** | Neurons, synapses, forward/backward pass, sparse storage |
| **Cognitive Modules** | Ethics, immune, introspection, ToM, working memory, etc. |
| **Plasticity** | STDP, homeostatic, eligibility traces, BCM, structural |
| **GPU Backend** | CUDA kernels for forward/backward, sparse matrix ops, training |
| **Security** | Immune system, BBB, alignment monitoring, anomaly detection |
| **Glial** | Astrocytes, microglia, oligodendrocytes, myelination |
| **Infrastructure** | Memory tracking, threading, async messaging, tensors, errors |

---

## 4. Core Neural Engine

### Neural Network Structure

The core neural network is defined in `include/core/neuralnet/nimcp_neuralnet.h` and implemented in `src/core/neuralnet/nimcp_neuralnet.c` (~4,000 lines).

**Key type**: `neural_network_t` (opaque handle to `struct neural_network_struct`)

```c
struct neural_network_struct {
    neuron_t* neurons;                    // Neuron array
    uint32_t num_neurons;                 // Total neuron count
    uint32_t num_inputs;                  // Input neuron count
    uint32_t num_outputs;                 // Output neuron count

    // Multi-layer topology
    uint32_t num_layers;                  // Layer count (3-7)
    uint32_t* layer_sizes;               // Size of each layer
    uint32_t* layer_offsets;             // Neuron ID offset per layer

    // Sparse synapse infrastructure
    sparse_synapse_handle_pool_t* handle_pool;
    sparse_synapse_meta_pool_t* meta_pool;

    // Configuration
    network_config_t config;
    integration_method_t integration_method;
};
```

### Neuron Structure

Each neuron (`neuron_t`) contains ~40 fields spanning:

```c
typedef struct neuron_struct {
    // Identity
    uint32_t id;
    neuron_type_t type;

    // Electrophysiology
    float state;                          // Current activation
    float rest_potential;                 // Resting potential
    float threshold;                      // Firing threshold
    float adaptation;                     // Adaptive threshold
    float refractory_period;             // Refractory period (ms)
    float bias;                          // Neuron bias
    float external_current;              // External input

    // Learning parameters
    learning_rule_t learning_rule;
    activation_type_t activation_type;
    oja_params_t oja_params;
    stdp_params_t stdp_params;
    homeostatic_params_t homeostatic;

    // Sparse connectivity (NIMCP 2.11)
    sparse_synapse_storage_t outgoing;   // Outgoing synapses
    sparse_synapse_storage_t incoming;   // Incoming synapses

    // Plasticity state
    float plasticity_rate;
    float homeostatic_factor;
    float calcium_concentration;
    float weight_norm;

    // Activity tracking
    spike_record_t* spike_history;       // Ring buffer
    float* activity_history;             // Activity EMA
    float avg_activity;
    uint64_t last_spike;
    uint64_t last_update;

    // Neuron model plugin
    neuron_model_state_t model;
    neuron_model_type_t model_type;

    // Morphology
    uint32_t axon_id;
    uint32_t* dendrite_ids;
    uint32_t num_dendrites;
} neuron_t;
```

### Sparse Synapse Storage

NIMCP 2.11 replaced fixed 256-synapse arrays with sparse storage, achieving 97% memory savings (106,496 bytes -> ~3,112 bytes per neuron).

```c
typedef struct {
    synapse_handle_t embedded[64];       // Inline for small neurons
    synapse_handle_t* overflow;          // Heap-allocated overflow
    uint32_t count;
    uint32_t overflow_capacity;
} sparse_synapse_storage_t;
```

**Access macros**:
- `NEURON_OUT_COUNT(n)` / `NEURON_IN_COUNT(n)` -- synapse counts
- `NEURON_OUT_HANDLE(n, i)` / `NEURON_IN_HANDLE(n, i)` -- synapse handles
- `NEURON_IN_META(n, i)` -- synapse metadata (weight, plasticity, traces)

**Metadata pool**: Chunked block allocator (64K entries/block). Pointers remain stable across pool growth.

---

## 5. Neuron Models and Types

### Neuron Models

| Model | Enum | Description |
|-------|------|-------------|
| Leaky Integrate-and-Fire | `NEURON_GENERIC_LIF` | Standard LIF with leak, threshold, reset |
| Izhikevich | `NEURON_GENERIC_IZHIKEVICH` | Two-variable model reproducing 20+ firing patterns |
| Excitatory | `NEURON_EXCITATORY` | Generic excitatory (backward compatibility) |
| Inhibitory | `NEURON_INHIBITORY` | Generic inhibitory (backward compatibility) |

### Specialized Neuron Types

**Visual cortex (100-199)**:
- `NEURON_V1_SIMPLE_CELL` (100) -- Gabor-filter edge detector
- `NEURON_V1_COMPLEX_CELL` (101) -- Phase-invariant edge detector
- `NEURON_VISUAL_ORIENTATION` (102) -- Orientation-selective
- `NEURON_VISUAL_DIRECTION` (103) -- Direction-selective (MT/V5)
- `NEURON_PYRAMIDAL_L23/L5/L6` (150-152) -- Cortical pyramidal neurons

**Auditory cortex (200-249)**:
- `NEURON_A1_FREQUENCY_TUNED` (200) -- Tonotopic frequency tuning
- `NEURON_A1_COINCIDENCE_DETECTOR` (201) -- Binaural temporal integration
- `NEURON_AUDITORY_ONSET` (202) -- Onset detection

**Motor (250-299)**:
- `NEURON_MOTOR_ALPHA` (250) -- Alpha motoneuron
- `NEURON_MOTOR_PATTERN_GEN` (251) -- Central pattern generator

**Cognitive (300-399)**:
- `NEURON_METACOGNITIVE` (300) -- Uncertainty estimation
- `NEURON_EXECUTIVE_CONTROL` (301) -- Goal-directed control with top-down modulation

**Neural logic gates (650-655)**:
- `NEURON_LOGIC_AND` (650) -- Fires if ALL inputs exceed threshold
- `NEURON_LOGIC_OR` (651) -- Fires if ANY input exceeds threshold
- `NEURON_LOGIC_NOT` (652) -- Baseline firing, suppressed on input
- `NEURON_LOGIC_XOR` (653) -- Fires if inputs differ
- `NEURON_LOGIC_VARIABLE` (654) -- Variable binding
- `NEURON_LOGIC_IMPLIES` (655) -- Logical implication

### Activation Functions

```c
typedef enum {
    ACTIVATION_SIGMOID,       // sigma(x) = 1/(1+e^-x), output [0,1]
    ACTIVATION_TANH,          // tanh(x), output [-1,1]
    ACTIVATION_RELU,          // max(0,x)
    ACTIVATION_LEAKY_RELU,    // max(0.01x, x)
    ACTIVATION_ADAPTIVE       // Adaptive threshold + tanh squashing
} activation_type_t;
```

---

## 6. Synapse System

### Synapse Structure

```c
typedef struct synapse_t {
    uint32_t target_id;
    float weight;
    float plasticity;
    float last_change;                    // Momentum
    uint64_t last_active;
    float strength;
    float meta_plasticity;
    float trace;                          // STDP trace

    // Source identification
    uint32_t source_neuron_id;
    uint32_t axon_id;

    // Short-term plasticity
    stp_state_t stp;
    bool enable_stp;

    // BCM homeostatic
    bcm_synapse_t* bcm;
    bool enable_bcm;

    // Eligibility traces (3-factor learning)
    eligibility_trace_t* eligibility;
    bool enable_eligibility;

    // Programmable computation (NIMCP 2.7)
    synapse_compute_fn compute_function;
    synapse_learn_fn learn_function;

    // Neurotransmitter type (NIMCP 2.8.7)
    synapse_type_t type;
    synapse_type_state_t type_state;

    // Semantic embeddings (NIMCP 2.9)
    float* semantic_embedding;
    uint16_t embedding_dim;
    float semantic_relevance;

    // Ternary weights (NIMCP 2.10)
    trit_t ternary_weight;
    bool use_ternary_weight;
    float ternary_scale;
} synapse_t;
```

### Synapse Types

| Type | Enum | Time Constant | Role |
|------|------|---------------|------|
| Generic | `SYNAPSE_GENERIC` (0) | -- | Baseline, no dynamics |
| AMPA | `SYNAPSE_AMPA` (1) | tau=2ms, rise=0.5ms | Fast excitatory |
| NMDA | `SYNAPSE_NMDA` (2) | tau=100ms, rise=10ms | Slow excitatory, Mg2+ voltage gate, Ca2+ for LTP/LTD |
| GABA-A | `SYNAPSE_GABA_A` (3) | tau=10ms, rise=1ms | Fast inhibitory (Cl-), drives oscillations |
| GABA-B | `SYNAPSE_GABA_B` (4) | tau=150ms | Slow inhibitory (K+), G-protein coupled |
| Dopamine | `SYNAPSE_DOPAMINE` (5) | Varies | Reward modulation, RPE-driven |
| Serotonin | `SYNAPSE_SEROTONIN` (6) | Varies | Mood/stability modulation |
| Acetylcholine | `SYNAPSE_ACETYLCHOLINE` (7) | Varies | Attention/arousal modulation |
| Electrical | `SYNAPSE_ELECTRICAL` (8) | Instantaneous | Bidirectional gap junction |

---

## 7. Multi-Layer Diamond Architecture

The network topology is automatically determined by neuron count, configured in `src/core/brain/factory/init/nimcp_brain_init_config.c`.

### Topology Selection

| Network Size | Neuron Count | Layers | Shape |
|-------------|-------------|--------|-------|
| Small | < 5,000 | 3 | Input -> Hidden -> Output |
| Medium | 5,000 - 100,000 | 5 (Diamond) | Input -> 17% -> 67% -> 17% -> Output |
| Large | > 100,000 | 7 (Deep Diamond) | Input -> 2% -> 12% -> 36% -> 36% -> 12% -> Output |

### Diamond Distribution (Medium, 5 layers)

```
Layer 0 (Input):  num_inputs neurons
Layer 1:          17% of hidden neurons
Layer 2:          67% of hidden neurons  (widest)
Layer 3:          17% of hidden neurons
Layer 4 (Output): num_outputs neurons
```

### Deep Diamond Distribution (Large, 7 layers)

```
Layer 0 (Input):  num_inputs neurons
Layer 1:          2% of hidden neurons
Layer 2:          12% of hidden neurons
Layer 3:          36% of hidden neurons  (widest)
Layer 4:          36% of hidden neurons
Layer 5:          12% of hidden neurons
Layer 6 (Output): num_outputs neurons + 2% remainder
```

### Layer-Aware Backbone Wiring

Connections are wired layer-by-layer (L0->L1->L2->...->Ln) with a budget of 200,000 connections per layer transition. For large hidden layers, proportional backbone reduction prevents over-connectivity.

---

## 8. Forward and Backward Pass

### Forward Pass

**Function**: `neural_network_forward()` in `src/core/neuralnet/nimcp_neuralnet.c`

```
Algorithm:
1. Set input layer neuron states from input vector
2. For each layer l = 1 to num_layers-1:
   a. For each neuron i in layer l:
      - activation = bias + SUM(weight * source_activity) for all incoming synapses
      - Apply activation function (sigmoid/tanh/ReLU/leaky_ReLU/adaptive)
      - Clamp unbounded activations
   b. Apply layer normalization (hidden layers only):
      - Compute mean and variance across layer
      - Normalize: (x - mean) / sqrt(variance + epsilon)
      - epsilon = 1e-5
3. Extract output layer activations to output vector
```

### Backward Pass (Backpropagation)

**File**: `src/core/neuralnet/nimcp_neuralnet_backprop.c`

**Context structure**:
```c
typedef struct {
    neural_network_t network;
    uint32_t num_layers;
    layer_activation_t* activations;      // [pre, post] per layer
    float* weight_gradients;              // dL/dW
    float* bias_gradients;               // dL/db
    size_t total_weights;
    size_t total_neurons;
    bool gradients_valid;
} backprop_ctx_t;
```

**Algorithm**:
```
Forward (recording):
  For each layer l = 1..num_layers:
    z = W * a_prev + bias                 // pre-activation
    a = f(z)                              // post-activation
    Store both z and a for backward pass

Backward:
  1. Output deltas: delta_L = dL/da_L * f'(z_L)
  2. Hidden deltas (l = L-1..1): delta_l = (W_{l+1}^T * delta_{l+1}) * f'(z_l)
  3. Weight gradients: dL/dW_l = delta_l * a_{l-1}^T
  4. Bias gradients: dL/db_l = delta_l
```

### Loss Functions

Implemented in `src/middleware/training/nimcp_loss_functions.c`:

| Loss | Formula | Use Case |
|------|---------|----------|
| MSE | `(1/n) * SUM((pred - target)^2)` | Regression |
| Cross-Entropy | `-(1/n) * SUM(target * log(pred))` | Classification |
| Binary CE | `-(1/n) * SUM(t*log(p) + (1-t)*log(1-p))` | Binary classification |
| Huber | Smooth MSE/MAE hybrid | Outlier-robust regression |
| Focal | `-alpha * (1-p)^gamma * log(p)` | Class imbalance |
| KL Divergence | `SUM(p * log(p/q))` | Distribution matching |
| Triplet | `max(0, d_pos - d_neg + margin)` | Metric learning |
| Contrastive | Pair-based similarity | Similarity learning |

Numerical stability: epsilon = 1e-7 for log operations; predictions clamped to [epsilon, 1-epsilon].

---

## 9. Learning and Plasticity

### Plasticity System Architecture

26 distinct plasticity mechanisms in `src/plasticity/`, coordinated by `nimcp_plasticity_coordinator.c` (40 KB).

### 9.1 Spike-Timing-Dependent Plasticity (STDP)

**Implementation**: Triplet STDP (Pfister & Gerstner, 2006)

```c
// Presynaptic traces
float r1_pre;       // Fast trace, tau_plus = 20ms
float r2_pre;       // Slow trace, tau_x = 113ms

// Postsynaptic traces
float o1_post;      // Fast trace, tau_minus = 34ms
float o2_post;      // Slow trace, tau_y = 160ms

// Learning amplitudes
float A2_plus;      // Pairwise LTP: 0.005
float A3_plus;      // Triplet LTP: 0.0001
float A2_minus;     // Pairwise LTD: 0.0055
float A3_minus;     // Triplet LTD: 0.0001
```

**Weight update on presynaptic spike (LTD)**:
```
dw = -A2_minus * o1_post - A3_minus * r1_pre * o2_post
```

**Weight update on postsynaptic spike (LTP)**:
```
dw = A2_plus * r1_pre + A3_plus * r2_pre * o1_post
```

**Trace decay**: Exponential with denormal flush at 1e-10.

### 9.2 Homeostatic Plasticity

Three mechanisms in `src/plasticity/homeostatic/`:

1. **Synaptic scaling**: Scale all input synapses to maintain target firing rate
   ```
   scale_factor = (target_rate / actual_rate)^gamma
   weight *= scale_factor
   ```
   Soft bounds prevent saturation.

2. **Intrinsic plasticity**: Adjust excitability via threshold modulation
   ```
   H = -rate * log(rate) - (1-rate) * log(1-rate)
   d_theta = alpha * (H_target - H_current) * dH/dtheta
   ```

3. **Metaplasticity**: BCM-like sliding threshold
   ```
   theta = beta * EMA(post_rate)
   LTP when post > theta; LTD when post < theta
   ```

### 9.3 Eligibility Traces

Three-factor learning with neuromodulatory gating:

```
e(t) = lambda^dt * e(t-1) + spike_contribution
dw = learning_rate * e * reward * dopamine
```

Config: `decay_lambda = 0.95`, `trace_threshold = 0.01`, optional burst-triggered consolidation (3x learning rate during dopamine bursts).

### 9.4 Training Pipeline (5-Layer)

| Layer | Purpose | Location |
|-------|---------|----------|
| 1. Convergent Decisions | Multi-module evidence for CONTINUE/PAUSE/ROLLBACK | `nimcp_training_convergent_decision.h` |
| 2. Causal DAG | 14-node causal model of training dynamics | `nimcp_training_causal_model.h` |
| 3. Abductive Diagnosis | Threshold-based failure detection -> hypothesis -> correction | `nimcp_training_diagnosis.h` |
| 4. Metacognitive Strategy | Per-domain EMA, stall/mastery detection, priority scheduling | `scripts/school.py` |
| 5. Unified Modulation | Continuous LR/batch/clip adjustment from biological signals | `nimcp_training_integration.h` |

**Unified learning rate composition**:
```
LR_effective = base_lr
    * arousal_gain
    * circadian_rhythm
    * (1 + reward_prediction_error)
    * stability_factor
    * (1 - inflammation_suppression)
    * portia_immune_modulation
    * (1 - 0.3 * stress)
    * (0.7 + 0.3 * cognitive_capacity)
```

### 9.5 Brain Learning Entry Point

**Function**: `brain_learn_example(brain, input, target, label, confidence)`

1. Forward pass: input -> hidden layers -> output
2. Loss computation (MSE or cross-entropy)
3. Backpropagation: chain rule through layers
4. Plasticity integration: STDP, homeostatic, eligibility
5. Weight update: SGD or Adam, gradient clipping (5.0 default)
6. Adaptive learning rate: 10-sample loss history, trend detection, [0.1x, 10x] bounds

Label management uses O(1) DJB2 hash table lookup.

### 9.6 Liquid Neural Network (LNN) Training

**Architecture**: NCP (Neural Circuit Policy) — 128 sensory -> 64 inter -> 32 command -> 64 motor (288 neurons).

**Gradient computation**: Adjoint method (backward-in-time ODE integration). Memory-efficient: does not store full trajectory.

**Training loop** (`nimcp_lnn_training.c`):
1. Forward pass (ODE integration)
2. Loss computation (MSE)
3. Adjoint backward pass (`lnn_gradient_compute_adjoint()`)
4. Gradient clipping to norm 1.0 (`lnn_gradient_clip()`) — clips per-layer tensors
5. Extract gradients (`lnn_network_get_gradients()`)
6. Adam optimizer step
7. Zero gradients for next iteration

**Gradient stability measures**:
- **Per-step clamping**: Each gradient element clamped to [-1e4, 1e4] during accumulation
- **Tau floor**: `fmaxf(tau, 0.01)` prevents 1/tau^2 explosion in tau gradient
- **NaN sanitization**: Loss gradient sanitized before adjoint seeding; NaN adjoint skips accumulation
- **Per-layer clipping**: `lnn_gradient_clip()` operates on actual per-layer grad tensors (grad_W_rec, grad_tau_base, grad_b_in), not legacy `ctx->grad_params`

**Key files**: `src/lnn/nimcp_lnn_gradient.c` (SRP split into 5 part files), `src/lnn/nimcp_lnn_training.c`, `src/lnn/nimcp_lnn_network.c`.

### 9.7 CPU-Staged Semantic Embeddings

**2048D embedding pool**: Network-level pinned CPU memory (`cudaHostAlloc`), indexed via `embedding_pool_index` (uint32_t) in each synapse.

**GPU batch relevance pipeline**: CUDA kernel (`kernel_batch_relevance`) computes cosine similarity with shared-memory context vector. Orchestrator gathers active indices -> GPU batch -> scatter results back to `semantic_relevance` float on each synapse.

**Key files**: `src/core/neuralnet/nimcp_synapse_embeddings.c` (pool + orchestrator), `src/gpu/embedding/nimcp_embedding_relevance_kernels.cu` (CUDA kernel), `src/gpu/stubs/nimcp_gpu_stubs_embedding.c` (CPU fallback).

### 9.8 SNN (Spiking Neural Network) Integration

**42 cross-modal SNN bridges** connect spiking networks to every cognitive subsystem (attention, emotion, language, hippocampus, theory of mind, etc.).

**SNN stats**: total_steps, total_spikes, mean_firing_rate, max_firing_rate, sparsity, synchrony, silent_neurons, hyperactive_neurons.

**SNN-Language bridge**: Sparse binding hash map (8192 buckets), STDP word-concept binding, population coding (8 neurons/pop), Broca cascade (production), Wernicke cascade (comprehension).

---

## 10. Cognitive Modules

### 10.1 Brain Immune System (`src/cognitive/immune/`)

Adaptive defense modeled on biological immune response.

**Components**:
- **B cells**: NAIVE -> ACTIVATED -> PLASMA -> MEMORY lifecycle. Only PLASMA cells produce antibodies.
- **T helper (CD4+)**: Coordination signals for B cell activation
- **T killer (CD8+)**: BFT quarantine and node isolation
- **Antibodies**: Response strategies (ISOLATION, EVASION, COUNTER_ATTACK, RECONFIGURATION)
- **Cytokines**: Bio-async signaling via NOREPINEPHRINE channel
- **Inflammation**: LOCAL -> REGIONAL -> SYSTEMIC -> STORM escalation

**Limits**: 256 antigens, 512 B cells, 512 T cells, 1024 antibodies.

**Integration effects**:
- Inflammation reduces working memory capacity (-1 to -4 items)
- Inflammation impairs Theory of Mind
- Affects IIT Phi consciousness metrics
- Modulates learning rate via `inflammation_suppression` factor

### 10.2 Ethics Module (`src/cognitive/ethics/`)

22 source files covering evaluation, policies, learning, and bridges.

**Violation types**: HARM, UNFAIRNESS, DECEPTION, PRIVACY, AUTONOMY, CONSENT, DIGNITY

**Integration**: `decide_full()` routes candidate actions through ethical evaluation. Ethics module can veto actions. Connected to immune system for threat response. Uses Theory of Mind for perspective-based harm assessment.

### 10.3 Introspection / IIT Phi (`src/cognitive/introspection/`)

**Capabilities**:
- `brain_get_active_population()` -- currently firing neurons
- `brain_get_internal_state()` -- compressed state vectors
- `brain_get_uncertainty()` -- epistemic vs. aleatoric uncertainty (ensemble methods)
- `brain_is_pattern_active()` -- O(1) hash-based pattern lookup
- `brain_get_activity_history()` -- state evolution tracking

**Extraction strategies**: FAST / BALANCED / DETAILED (0.1-2ms depending on strategy)

### 10.4 Theory of Mind (`src/cognitive/theory_of_mind/`)

Belief-Desire-Intention (BDI) model for social cognition.

**Key functions**:
- `tom_observe()` -- process behavioral observations
- `tom_infer_emotion()` -- behavior -> emotion mapping (12 categories)
- `tom_infer_goal()` -- deduce agent's desires
- `tom_predict_action()` -- forecast next action
- `tom_empathize()` -- generate emotional response
- `tom_detect_false_belief()` -- recognize belief-reality mismatches

**Biological basis**: Models TPJ (perspective-taking), mPFC (mental state modeling), mirror neuron system (empathy).

### 10.5 Working Memory (`src/cognitive/working_memory/`)

Miller's 7 +/- 2 capacity with salience-based eviction and temporal decay.

**Parameters**: Capacity default 7 (max 20), decay tau 1000ms (half-life ~700ms), min salience 0.01.

**Position encoding types**: Sinusoidal, Relative, Learned, RoPE, ALiBi.

**Integration**: Emotional tagging boosts salience. Inflammation reduces capacity. Sleep state modulation. Immune IL-6 release at >90% utilization.

### 10.6 Global Workspace (`src/cognitive/global_workspace/`)

Central broadcast architecture based on Baars' Global Workspace Theory.

**Mechanism**: Winner-take-all competition. Ignition threshold ~0.6. Max 32 competing modules, 32 subscribers. Workspace capacity 256 floats (1 KB). Refractory period 50ms (attentional blink).

**Phenomena modeled**: Attentional blink, change blindness, inattentional blindness, limited multitasking.

### 10.7 Epistemic Filtering (`src/cognitive/epistemic/`)

**Biases detected**: Confirmation, availability, anchoring, bandwagon, authority, ingroup, Dunning-Kruger, hindsight, motivated reasoning, conspiracy thinking, false balance, extraordinary claim.

**Evidence quality levels**: NONE / ANECDOTAL / WEAK / MODERATE / STRONG / SCIENTIFIC / CONSENSUS

### 10.8 Additional Modules

| Module | Purpose |
|--------|---------|
| Emotional System | Centralized emotion processing (valence/arousal) across tagging, recognition, shadow emotions |
| Mirror Neurons | Observational learning, dual observation/execution representation |
| Executive Functions | Goal management, conflict resolution, strategy selection |
| Autobiographical Memory | Personal episodic memories with time-indexed retrieval |
| Meta-Learning | Learning-to-learn, strategy optimization |
| Self-Awareness | Multi-layer self-model (social, embodied, extended) |
| Shadow Emotions | Maladaptive pattern detection, psychopathology screening |
| Mental Health Monitoring | Well-being tracking, depression/anxiety detection |
| Personality System | Trait tracking, behavioral patterns |
| Predictive Processing | Prediction error minimization, Bayesian inference |
| Sleep-Wake Cycle | Memory consolidation, circadian integration |
| Symbolic Logic | Propositional reasoning, knowledge graph inference |
| Recursive Cognition | Hierarchical self-delegation, sub-cognition workers |
| Imagination Engine | Mental simulation, counterfactual reasoning |
| JEPA | Joint-Embedding Predictive Architecture for latent representations |
| Collective Cognition | Swarm-like collaborative thinking, CRDT workspace |

---

## 11. Decision Pipeline

When NIMCP makes a decision, it follows this pipeline:

```
Input Features
    |
    v
Perception (visual/auditory/language cortex)
    |
    v
Working Memory (salience-based storage)
    |
    v
Global Workspace (competition + broadcast)
    |
    v
Decision Candidate Generation
    |
    v
Ethics Check -----> CAN VETO
    |
    v
Immune Monitoring --> CAN SUPPRESS
    |
    v
Introspection (state coherence check)
    |
    v
Action Execution
```

**Function**: `brain_decide(brain, features, num_features)` returns `brain_decision_t*`:

```c
typedef struct brain_decision {
    char label[64];
    float confidence;
    float* output_vector;
    uint32_t output_size;
    uint32_t num_active_neurons;
    uint32_t* active_neuron_ids;
    float sparsity;
    char explanation[256];
    uint64_t inference_time_us;
    uint32_t* _cow_refcount;
    bool _cow_is_shallow;
} brain_decision_t;
```

**Memory management**: `brain_free_decision()` to free, `copy_decision_deep()` for cache copies. Thread-safe via `cache_mutex`.

---

## 12. GPU Acceleration

### Architecture

50 subdirectories under `src/gpu/` covering all subsystems:

| Directory | Purpose |
|-----------|---------|
| `backend/` | GPU context management |
| `inference/` | Forward pass kernels |
| `training/` | Backward pass, gradient accumulation, optimizers |
| `tensor/` | Tensor operations |
| `sparse/` | Sparse matrix operations (SpMM, SpMV) |
| `neuron/`, `synapse/` | Per-neuron/synapse simulation |
| `plasticity/` | STDP, metaplasticity on GPU |
| `lnn/`, `snn/`, `cnn/` | Network-specific kernels |
| `stubs/` | CPU fallback implementations |

### GPU Training Bridge

**File**: `src/gpu/training/nimcp_training_bridge.c` (36 KB)

**Weight cache architecture**:
```c
// Host-side: COO/CSR sparse formats
host_coo_values[nnz], host_coo_row_idx[nnz], host_coo_col_idx[nnz]
host_csr_row_ptrs[rows]

// GPU-side: dense after extraction
device_weights_matrix     // [out_neurons x in_neurons]
device_activations        // [batch_size x layer_size]
device_loss               // scalar
```

**GPU forward**: Extract sparse weights -> dense matrix -> GEMV -> activation -> loss.
**GPU backward**: Loss gradient -> activation backprop -> weight gradient (outer product) -> bias gradient (sum).

### Key CUDA Kernel Files

- `nimcp_backprop_kernels.cu` (18 KB) -- backward pass
- `nimcp_gradient_kernels.cu` (25 KB) -- gradient accumulation
- `nimcp_optimizer_kernels.cu` (16 KB) -- SGD/Adam updates
- `nimcp_activation_checkpoint.cu` -- gradient checkpointing
- `nimcp_multigpu.c` (43 KB) -- multi-GPU coordination

### Requirements

All `.cu` files must use `nimcp_malloc/nimcp_calloc/nimcp_realloc/nimcp_free` (not raw malloc). Include `utils/memory/nimcp_memory.h`.

---

## 13. Memory Management

### Architecture

**Files**: `src/utils/memory/` (10 files, ~295K lines total)

**Design**: Decorator + Proxy pattern wrapping `malloc/free` with tracking.

**Memory block layout**:
```
[HEAD_CANARY 0xDEADBEEF (4 bytes)] [User Data (N bytes)] [TAIL_CANARY 0xDEADBEEF (4 bytes)]
```

**Features**:
- Leak detection at shutdown
- Double-free detection
- Buffer overflow/underflow detection via canary guards
- Allocation pattern analysis
- Thread-safe: global mutex + `_Atomic` counters
- ~2-5% overhead, ~40 bytes per allocation

**API**:
```c
void* nimcp_malloc(size_t size);
void* nimcp_calloc(size_t nmemb, size_t size);
void* nimcp_realloc(void* ptr, size_t size);
void  nimcp_free(void* ptr);
void  nimcp_memory_check_leaks(void);
```

**CRITICAL**: `nimcp_memory.c`, `nimcp_unified_memory.c`, and `nimcp_constant_time.c` must use raw `malloc/calloc/free/realloc` (bootstrap problem).

### Additional Memory Systems

- **Unified memory** (`nimcp_unified_memory.c`, 42 KB) -- GPU/CPU unified address space
- **Copy-on-Write** (`nimcp_page_cow.c`, 42 KB) -- efficient brain state snapshots
- **Pool allocators** (`nimcp_memory_pool.c`, 23 KB) -- fixed-size block pools
- **Layer pools** (`nimcp_layer_pools.c`, 41 KB) -- per-layer memory pools
- **Buffer pool** (`nimcp_buffer_pool.c`, 27 KB) -- reusable buffer management

---

## 14. Threading and Asynchronous Communication

### Threading Layer

**Files**: `src/utils/thread/` (10 files, ~242K lines)

**API**:
```c
nimcp_mutex_t* nimcp_mutex_create(mutex_attr_t* attr);  // Returns handle, NOT error code
void nimcp_mutex_free(nimcp_mutex_t* mutex);              // destroy + free (for heap mutexes)
```

Mutex types: `MUTEX_TYPE_NORMAL`, `MUTEX_TYPE_RECURSIVE`, `MUTEX_TYPE_ERRORCHECK`.

**Features**: Thread pools, deadlock detector (resource ordering), condition variables, atomic operations, semaphores, barriers.

**Deadlock prevention**: Never call public mutex-locking functions from within locked code. Create `*_unlocked()` internal helpers.

### Bio-Async Communication

**Files**: `src/async/` (20+ files, ~750K lines)

**Bio-Router** (`nimcp_bio_router.c`, split across 5 part files):
- Ring buffer message queues per module
- Handler registration per message type
- Module registry with inbox queues
- Predictive coding integration
- Phase synchronization

```c
struct bio_router_struct {
    bio_module_entry_t* modules;
    uint32_t module_count, module_capacity;
    bio_router_stats_t stats;
    predictive_protocol_t predictive_proto;
    void* brain_immune_system;
};
```

**Bio-Promise** (`nimcp_bio_async.c`, 88 KB):
```c
nimcp_bio_promise_complete(promise, result);  // 2 args, NOT 3
```

---

## 15. Security and Safety Subsystems

### Directory Structure

24 subdirectories under `src/security/`:

| Component | Purpose |
|-----------|---------|
| `immune/` | B cells, T cells, antigen responses |
| `blood_brain_barrier.c` | BBB enforcement and access control |
| `alignment_monitor.c` | Value alignment tracking |
| `anomaly_detector.c` | Anomaly detection |
| `corrigibility.c` | Corrigibility mechanisms |
| `constant_time.c` | Timing attack prevention |
| `bayesian_network.c` | Probabilistic inference |
| `game_theory/` | Game-theoretic reasoning |

### Blood-Brain Barrier (BBB)

Access control layer that gates which signals can reach core neural processing. Includes code signing and integrity verification.

### Alignment Monitoring

Continuous monitoring of system behavior against defined value constraints. Anomaly detection triggers immune response.

---

## 16. Glial Cell Simulation

**Files**: `src/glial/` (9 subdirectories)

| Cell Type | Directory | Function |
|-----------|-----------|----------|
| Astrocytes | `astrocytes/`, `astrocyte_types/` | Synaptic support, K+ buffering, metabolic supply |
| Microglia | `microglia/` | Immune cells, phagocytosis, synaptic pruning |
| Oligodendrocytes | `oligodendrocytes/` | Myelin production |
| Myelin Sheath | `myelin_sheath/` | Signal propagation speed |

Integration bridge: `nimcp_glial_substrate_bridge.c` (34 KB).

---

## 17. Configuration and Initialization

### Brain Configuration

```c
typedef struct brain_config {
    // Core
    brain_size_t size;                    // MICRO/TINY/SMALL/MEDIUM/LARGE/CUSTOM
    brain_task_t task;                    // CLASSIFICATION/REGRESSION/etc.
    uint32_t num_inputs, num_outputs;
    float learning_rate, sparsity_target;

    // Initialization
    brain_init_mode_t init_mode;          // FULL/FAST/MINIMAL
    bool parallel_init;
    uint32_t init_threads, wiring_threads;

    // Feature flags (30+)
    bool enable_working_memory;
    bool enable_global_workspace;
    bool enable_theory_of_mind;
    bool enable_visual_cortex;
    bool enable_audio_cortex;
    bool bbb_enabled;
    bool immune_enabled;
    // ... 90+ total fields
} brain_config_t;
```

### Initialization Modes

| Mode | Subsystems | Time (1.5M neurons) | Use Case |
|------|-----------|---------------------|----------|
| `BRAIN_INIT_FULL` | All 80+ | 150-250s | Research, full-featured |
| `BRAIN_INIT_FAST` | 6 of 27 waves (GPU, security, training, plasticity) | 14-30s | Training, iteration |
| `BRAIN_INIT_MINIMAL` | Neural network only | 10-20s | Benchmarking |

### Configuration Profiles

| Profile | Features | Init Time | Memory |
|---------|----------|-----------|--------|
| `BRAIN_CONFIG_MINIMAL` | Core network only | ~100ms | <10 MB |
| `BRAIN_CONFIG_STANDARD` | Core + WM + global workspace | ~500ms | 20-50 MB |
| `BRAIN_CONFIG_COGNITIVE` | Full cognitive systems | 1-3s | 50-200 MB |
| `BRAIN_CONFIG_RESEARCH` | All features | 5-30s | 500 MB - 5 GB |
| `BRAIN_CONFIG_EMBEDDED` | Constrained devices | <200ms | <20 MB |

### Configuration Files

- `config/nimcp_default.conf` -- runtime defaults (learning rate, batch size, dropout, STDP params)
- `config/core_modules.ini` -- core module configuration
- `config/glial_modules.ini` -- glial cell configuration

---

## 18. Python Bindings

### Two Source Locations

1. **`src/python/`** -- builds into `libnimcp.so` (15+ files, ~650K lines)
2. **`src/bindings/python/nimcp_python.c`** -- standalone extension (225 KB)

The versioned `.cpython-312-x86_64-linux-gnu.so` takes priority over `nimcp.so`.

### Python API

```python
import nimcp

# Create brain (with optional FAST init)
brain = nimcp.Brain("classifier", nimcp.BRAIN_SMALL,
                    nimcp.TASK_CLASSIFICATION, 10, 5,
                    init_mode='fast')

# Learn
brain.learn([0.1, 0.2, ...], "class_a", 0.9)

# Predict
label, confidence = brain.predict([0.15, 0.25, ...])

# Batch operations
labels, confs = brain.predict_batch([[...], [...]])

# Checkpointing
brain.enable_checkpointing("/path/to/checkpoints")
brain.checkpoint()
checkpoints = brain.list_checkpoints()

# Health monitoring
brain.health_agent_start()
```

### Build

```bash
cd build && cmake .. && make nimcp_python -j4
```

---

## 19. Tensor Library

**Files**: `src/utils/tensor/nimcp_tensor.c` (101 KB), `nimcp_tensor_simd.c` (33 KB)

### Structure

```c
struct nimcp_tensor_s {
    uint32_t magic;
    nimcp_tensor_shape_t shape;
    nimcp_dtype_t dtype;                  // float, double, int32, int64, complex
    void* data;
    bool owns_data, requires_grad;
    nimcp_tensor_t* grad;
    uint32_t refcount;
    nimcp_mutex_t lock;
};
```

### Features

- Arbitrary rank tensors with shape tracking
- Reference counting + thread safety
- Autodiff with tape-based backpropagation
- SIMD acceleration (AVX2)
- Broadcasting operations
- Gradient accumulation

### CRITICAL GOTCHA

`nimcp_tensor_sum()` returns `nimcp_tensor_t*` (a tensor), NOT a scalar float. The `op_div` operation uses epsilon clamping (1e-7) instead of returning 0.

---

## 20. Error Handling

### Exception System

**Files**: `src/utils/exception/` (6 files, ~185K lines)

**Error categories**:

| Range | Category |
|-------|----------|
| 1000-1099 | Generic |
| 1100-1199 | GPU |
| 2000-2099 | Memory |
| 3000-3099 | Brain |
| 6000-6099 | Threading |
| 7000-7099 | Signal/Fatal |
| 9000-9099 | Security |

**Severity levels**: DEBUG, WARNING, ERROR, SEVERE, CRITICAL, FATAL

**Rate limiting**: Max 5000 non-severe exceptions/second (1s window). Prevents performance degradation in hot paths.

**Immune integration**: `NIMCP_THROW_TO_IMMUNE` macro reports exceptions to the brain immune system. CRITICAL: Files in `src/utils/exception/`, `nimcp_memory.c`, `nimcp_unified_memory.c`, and `nimcp_constant_time.c` must NEVER use raw `NIMCP_THROW_TO_IMMUNE` (infinite recursion risk).

### Return Value Conventions

| Module Type | Success | Error |
|-------------|---------|-------|
| Standard NIMCP | `NIMCP_OK` | `NIMCP_ERROR_*` codes |
| FEP bridges | `0` | `-1` |
| Metabolic modulation | `0` | `-1` |
| `nimcp_mutex_create()` | Returns `nimcp_mutex_t*` | Returns `NULL` |

---

## 21. File Organization

```
nimcp/
├── src/
│   ├── api/                     # Public API facade (nimcp.c + parts)
│   ├── core/
│   │   ├── brain/               # Brain lifecycle, factory, learning
│   │   │   ├── factory/         # Initialization configs and profiles
│   │   │   └── learning/        # brain_learn_example implementation
│   │   ├── neuralnet/           # Neural network engine
│   │   ├── neuron_types/        # Specialized neuron implementations
│   │   ├── synapse_compute/     # Programmable synapse computation
│   │   ├── synapse_types/       # AMPA, NMDA, GABA, etc.
│   │   └── topology/            # Network topology generation
│   ├── cognitive/               # 30+ cognitive modules
│   │   ├── ethics/              # Ethical reasoning (22 files)
│   │   ├── immune/              # Brain immune system
│   │   ├── introspection/       # IIT Phi, self-monitoring
│   │   ├── theory_of_mind/      # BDI social cognition
│   │   ├── working_memory/      # Miller's 7+/-2
│   │   ├── global_workspace/    # Baars' GWT
│   │   ├── epistemic/           # Bias detection, evidence quality
│   │   └── ...                  # 20+ more modules
│   ├── plasticity/              # 26 plasticity mechanisms
│   │   ├── stdp/                # Triplet STDP
│   │   ├── homeostatic/         # Synaptic scaling, intrinsic, meta
│   │   ├── eligibility/         # 3-factor learning
│   │   ├── bcm/                 # BCM theory
│   │   ├── structural/          # Synaptogenesis/pruning
│   │   └── ...
│   ├── gpu/                     # 50 subdirectories, 83 .cu files
│   ├── glial/                   # Astrocytes, microglia, oligodendrocytes
│   ├── security/                # 24 subdirectories
│   ├── async/                   # Bio-router, promises, futures
│   ├── utils/                   # Memory, threading, tensor, exception
│   ├── python/                  # Python C extension module
│   └── bindings/                # Language bindings
├── include/                     # Public and internal headers
├── test/                        # Unit and integration tests
├── scripts/                     # Training, utility, benchmark scripts
├── examples/                    # Demo programs
├── config/                      # Runtime configuration files
├── docs/                        # Documentation
└── CMakeLists.txt               # Build configuration
```

---

## 22. Build System

### Requirements

- CMake 3.15+
- C11 compiler (GCC 7+ or Clang 6+)
- Python 3.7+ with development headers
- Optional: CUDA Toolkit 11.0+, libsodium, libcurl

### Commands

```bash
# Full library build
cd build && cmake .. && make nimcp -j4

# Python bindings
make nimcp_python -j4

# Static analysis
make lint          # clang-tidy
make cppcheck      # cppcheck
make check         # all analysis
```

### Compiler Flags

- C standard: C11
- C++ standard: C++20 (for C++ bindings)
- AVX2 SIMD: auto-enabled on x86_64
- Build type: RelWithDebInfo (debug symbols always present)

---

## 23. Key Constants and Limits

| Constant | Value | Purpose |
|----------|-------|---------|
| `MAX_NEURONS` | 2,000,000 | Maximum neuron count |
| `SPARSE_SYNAPSE_EMBEDDED_CAPACITY` | 320 | Inline synapses per neuron (~6.1 KB/neuron) |
| `MIN_FAN_IN` | 128 | Minimum incoming connections per neuron |
| `MAX_FAN_IN` | 384 | Maximum incoming connections per neuron |
| `OVERFLOW_POOL` | 200,000 | Overflow synapse pool slots |
| `CONNECTION_BUDGET` | 50,000,000 | Total connection budget |
| `SPIKE_HISTORY_DEFAULT_CAPACITY` | 128 | Spike ring buffer |
| `ACTIVITY_HISTORY_DEFAULT_CAPACITY` | 32 | Activity EMA buffer |
| `WEIGHT_UPDATE_THRESHOLD` | 1e-6 | Minimum weight change |
| `ACTIVITY_THRESHOLD` | 1e-5 | Minimum activity |
| `NORM_THRESHOLD` | 1e-7 | Normalization epsilon |
| `NORMALIZATION_INTERVAL` | 1000ms | Between normalizations |
| `GRADIENT_CLIP_VALUE` | 5.0 | Default main network gradient clip |
| `LNN_GRADIENT_CLIP_NORM` | 1.0 | LNN adjoint gradient clip |
| `LNN_PER_STEP_CLAMP` | 1e4 | LNN per-step gradient clamp |
| `LNN_TAU_FLOOR` | 0.01 | Minimum tau to prevent 1/tau^2 explosion |
| `LOSS_DEFAULT_EPSILON` | 1e-7 | Log/softmax/div stability |
| `NIMCP_DENORMAL_THRESHOLD` | 1e-10 | Denormal flush threshold |
| `LAYER_WIRING_BUDGET` | 200,000 | Connections per layer transition |
| `CORPUS_CALLOSUM_BANDWIDTH` | 500 msg/s | Inter-hemisphere message rate |
| `WORKING_MEMORY_CAPACITY` | 7 (max 20) | Miller's number |
| `GW_IGNITION_THRESHOLD` | 0.6 | Global workspace ignition |
| `GW_REFRACTORY_PERIOD` | 50ms | Attentional blink |
| `KWTA_SPARSITY` | 5% | K-Winner-Take-All active neuron fraction |
| `GPU_STREAM_POOL_SIZE` | 8 | Round-robin CUDA stream pool |

---

## 24. Design Patterns

| Pattern | Usage |
|---------|-------|
| **Decorator** | Memory tracking wraps malloc/free with canaries and counting |
| **Factory** | `nimcp_brain_create()` with profiles and initialization waves |
| **Strategy** | Pluggable plasticity mechanisms, extraction strategies |
| **Observer** | Pattern tracking, event subscriptions, stability monitoring |
| **Singleton** | Global workspace, exception system, tensor stats |
| **Ring Buffer** | Message queues in bio-router, spike history |
| **Proxy** | Bio-router dispatches to handlers |
| **Copy-on-Write** | Brain state snapshots, decision caching |
| **Bridge** | Cross-module integration (FEP, plasticity, substrate, thalamic, sleep) |
| **Opaque Handle** | Public API hides internal structs |
| **RAII** | Autodiff tape management, mutex lifecycle |
| **Thread-Local** | Error messages, exceptions, rate limiting |
| **Membrane** | Blood-Brain Barrier gates learning signals |
| **Guard Clause** | Both braces AND return required after `NIMCP_THROW_TO_IMMUNE` |

---

*This document describes NIMCP as of version 2.6.3 (March 2026). The system is under active development; interfaces may change.*
