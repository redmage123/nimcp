# NIMCP API Reference

**Version:** 2.6.3
**Last Updated:** 2026-01-16

This document provides comprehensive reference documentation for all public APIs in the NIMCP (Neuromorphic Infant Machine Cognitive Platform) library.

## Recent Updates (2026-01-16)

### Version 2.6.3 - Unified API Enhancements
- **Added:** Complete Training Pipeline API with loss functions, optimizers, LR schedulers
- **Added:** Training Callbacks API for event-driven training monitoring and control
- **Added:** Dynamic Brain Resizing API for automatic scaling based on utilization
- **Added:** Brain Snapshots API for named, timestamped backups and A/B testing
- **Added:** Copy-on-Write (COW) Cache API with 86% memory savings for clones
- **Added:** Working Memory API implementing Miller's 7±2 buffer
- **Added:** Global Workspace API for conscious access broadcasting (Baars' GWT)
- **Added:** Complex Oscillation API for phase coding and neural synchrony
- **Added:** Brain Immune System API (B cells, T cells, antibodies, cytokines)
- **Added:** Bio-Async API with neuromodulator channels and phase coupling
- **Added:** Swarm Brain API for distributed cognitive processing

### Security Hardening (v2.6.1)
- **Fixed:** Replaced all unsafe `strcpy()` calls with `strncpy()` + explicit null termination
- **Fixed:** Memory corruption bug in Knowledge module that caused test segfaults
- **Security:** Defense-in-depth buffer overflow protection across codebase

### Spectral Analysis & Brain Oscillations (v2.6.0)
- **Added:** FFT spectral analysis utilities with brain wave band extraction
- **Added:** Brain oscillation analysis module with cognitive state inference

---

## Table of Contents

1. [Core Neural Network APIs](#core-neural-network-apis)
2. [Brain & Cognitive Systems](#brain--cognitive-systems)
   - [Brain Oscillation Analysis](#brain-oscillation-analysis)
3. [Training Pipeline API](#training-pipeline-api)
   - [Loss Functions](#loss-functions)
   - [Optimizers](#optimizers)
   - [Learning Rate Schedulers](#learning-rate-schedulers)
4. [Training Callbacks API](#training-callbacks-api)
5. [Dynamic Brain Resizing](#dynamic-brain-resizing)
6. [Brain Snapshots](#brain-snapshots)
7. [Copy-on-Write (COW) Cache](#copy-on-write-cow-cache)
8. [Working Memory API](#working-memory-api)
9. [Global Workspace API](#global-workspace-api)
10. [Complex Oscillation API](#complex-oscillation-api)
11. [Brain Immune System](#brain-immune-system)
12. [Bio-Async System](#bio-async-system)
13. [Swarm Intelligence](#swarm-intelligence)
14. [Learning Systems](#learning-systems)
    - [Adaptive Learning](#adaptive-learning)
    - [Neuromodulator System](#neuromodulator-system)
    - [BCM Learning Rule](#bcm-learning-rule)
15. [Event Processing](#event-processing)
16. [P2P Networking](#p2p-networking)
17. [Data I/O & Streaming](#data-io--streaming)
18. [Attention & Salience](#attention--salience)
19. [Memory Consolidation](#memory-consolidation)
20. [Introspection & Monitoring](#introspection--monitoring)
21. [Higher-Level Cognitive APIs](#higher-level-cognitive-apis)
22. [Thread Safety & Synchronization](#thread-safety--synchronization)
23. [Utility APIs](#utility-apis)
24. [Language Bindings](#language-bindings)

---

## Core Neural Network APIs

### Neural Network Creation & Management
**Header:** `nimcp_neuralnet.h`

#### Types
```c
typedef struct nimcp_neuralnet_t nimcp_neuralnet_t;

typedef struct {
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t num_hidden;
    float learning_rate;
    float stdp_a_plus;
    float stdp_a_minus;
    float tau_plus;
    float tau_minus;
} nimcp_neuralnet_config_t;

typedef struct {
    uint32_t total_neurons;
    uint32_t total_synapses;
    float avg_activity;
    float network_stability;
} network_stats_t;
```

#### Functions

**`nimcp_neuralnet_t* nimcp_neuralnet_create(const nimcp_neuralnet_config_t* config)`**
- **Description:** Creates a new spiking neural network
- **Parameters:**
  - `config`: Network configuration (inputs, outputs, hidden neurons, learning parameters)
- **Returns:** Network handle or NULL on failure
- **Thread Safety:** Thread-safe

**`void nimcp_neuralnet_destroy(nimcp_neuralnet_t* net)`**
- **Description:** Destroys a neural network and frees resources
- **Thread Safety:** Must not be called concurrently with same network

**`void nimcp_neuralnet_forward(nimcp_neuralnet_t* net, const float* inputs, float* outputs)`**
- **Description:** Performs forward propagation through the network
- **Thread Safety:** Not thread-safe, external synchronization required

**`void nimcp_neuralnet_backward(nimcp_neuralnet_t* net, const float* target, float* error)`**
- **Description:** Performs backpropagation and learning

**`void nimcp_neuralnet_get_stats(nimcp_neuralnet_t* net, network_stats_t* stats)`**
- **Description:** Retrieves network statistics
- **Thread Safety:** Thread-safe (read-only operation)

**`int nimcp_neuralnet_save(nimcp_neuralnet_t* net, const char* filename)`**
- **Returns:** 0 on success, -1 on failure

**`nimcp_neuralnet_t* nimcp_neuralnet_load(const char* filename)`**
- **Returns:** Network handle or NULL on failure

---

## Brain & Cognitive Systems

### Unified Brain API
**Header:** `nimcp.h`

The unified API provides a single entry point for all language bindings with opaque handles for ABI stability.

#### Types
```c
typedef struct nimcp_brain_handle* nimcp_brain_t;
typedef struct nimcp_network_handle* nimcp_network_t;
typedef struct nimcp_ethics_handle* nimcp_ethics_t;
typedef struct nimcp_knowledge_handle* nimcp_knowledge_t;

typedef enum {
    NIMCP_BRAIN_TINY = 0,   // 100 neurons, <1MB, ~0.1ms inference
    NIMCP_BRAIN_SMALL = 1,  // 1K neurons, ~10MB, ~0.5ms inference
    NIMCP_BRAIN_MEDIUM = 2, // 10K neurons, ~50MB, ~5ms inference
    NIMCP_BRAIN_LARGE = 3   // 100K neurons, ~500MB, ~50ms inference
} nimcp_brain_size_t;

typedef enum {
    NIMCP_TASK_CLASSIFICATION = 0,
    NIMCP_TASK_REGRESSION = 1,
    NIMCP_TASK_PATTERN_MATCHING = 2,
    NIMCP_TASK_SEQUENCE = 3,
    NIMCP_TASK_ASSOCIATION = 4
} nimcp_brain_task_t;

typedef enum {
    NIMCP_OK = 0,
    NIMCP_ERROR = 1000,
    NIMCP_ERROR_NULL_ARG = 1003,
    NIMCP_ERROR_INVALID = 1004,
    NIMCP_ERROR_MEMORY = 2000,
    NIMCP_ERROR_IO = 4000
} nimcp_status_t;
```

#### Core Brain Functions

**`nimcp_brain_t nimcp_brain_create(const char* name, nimcp_brain_size_t size, nimcp_brain_task_t task, uint32_t num_inputs, uint32_t num_outputs)`**
- **Description:** Create a brain with preset configuration
- **Returns:** Brain handle or NULL on error

**`void nimcp_brain_destroy(nimcp_brain_t brain)`**
- **Description:** Destroy brain and free all resources

**`nimcp_status_t nimcp_brain_learn_example(nimcp_brain_t brain, const float* features, uint32_t num_features, const char* label, float confidence)`**
- **Description:** Learn from a single example
- **Returns:** NIMCP_OK on success

**`nimcp_status_t nimcp_brain_predict(nimcp_brain_t brain, const float* features, uint32_t num_features, char* out_label, float* out_confidence)`**
- **Description:** Make a decision/prediction
- **Returns:** NIMCP_OK on success

**`nimcp_status_t nimcp_brain_infer(nimcp_brain_t brain, const float* features, uint32_t num_features, float* outputs, uint32_t num_outputs)`**
- **Description:** Run inference and get raw output vector
- **Returns:** NIMCP_OK on success

**`nimcp_status_t nimcp_brain_save(nimcp_brain_t brain, const char* filepath)`**
**`nimcp_brain_t nimcp_brain_load(const char* filepath)`**
- **Description:** Persist and restore brain state

**`nimcp_brain_t nimcp_brain_create_from_config(const char* config_filepath)`**
- **Description:** Create brain from YAML or JSON configuration file

#### Brain Probe Statistics
```c
typedef struct {
    char task_name[64];
    nimcp_brain_size_t size;
    nimcp_brain_task_t task;
    uint32_t num_neurons;
    uint32_t num_synapses;
    uint32_t num_active_synapses;
    uint64_t total_inferences;
    uint64_t total_learning_steps;
    float avg_sparsity;
    float avg_inference_time_us;
    float current_learning_rate;
    float accuracy;
    size_t memory_bytes;
    uint32_t num_inputs;
    uint32_t num_outputs;
    bool is_cow_clone;
    uint32_t cow_ref_count;
    size_t cow_shared_bytes;
    size_t cow_private_bytes;
} nimcp_brain_probe_t;
```

**`nimcp_status_t nimcp_brain_probe(nimcp_brain_t brain, nimcp_brain_probe_t* probe)`**
- **Description:** Get comprehensive brain statistics

**`nimcp_status_t nimcp_brain_broadcast_probe(nimcp_brain_t brain)`**
- **Description:** Broadcast brain probe data via bio-async message system

---

## Training Pipeline API

**Header:** `nimcp.h`

Complete training system with loss functions, optimizers, and learning rate schedulers.

### Types

```c
typedef enum {
    NIMCP_API_LOSS_MSE = 0,           // Mean Squared Error (regression)
    NIMCP_API_LOSS_CROSS_ENTROPY = 1, // Cross-Entropy (classification)
    NIMCP_API_LOSS_BINARY_CE = 2,     // Binary Cross-Entropy
    NIMCP_API_LOSS_HUBER = 3,         // Huber Loss (robust regression)
    NIMCP_API_LOSS_MAE = 4,           // Mean Absolute Error
    NIMCP_API_LOSS_FOCAL = 5,         // Focal Loss (imbalanced classes)
    NIMCP_API_LOSS_KL_DIV = 6         // KL Divergence
} nimcp_api_loss_t;

typedef enum {
    NIMCP_API_OPT_SGD = 0,            // Stochastic Gradient Descent
    NIMCP_API_OPT_MOMENTUM = 1,       // SGD with Momentum
    NIMCP_API_OPT_ADAM = 2,           // Adam optimizer
    NIMCP_API_OPT_ADAMW = 3,          // AdamW (weight decay)
    NIMCP_API_OPT_RMSPROP = 4,        // RMSprop
    NIMCP_API_OPT_ADAGRAD = 5         // Adagrad
} nimcp_api_optimizer_t;

typedef enum {
    NIMCP_API_SCHED_CONSTANT = 0,        // Constant learning rate
    NIMCP_API_SCHED_STEP = 1,            // Step decay
    NIMCP_API_SCHED_EXPONENTIAL = 2,     // Exponential decay
    NIMCP_API_SCHED_COSINE = 3,          // Cosine annealing
    NIMCP_API_SCHED_WARMUP_COSINE = 4,   // Warmup + Cosine annealing
    NIMCP_API_SCHED_REDUCE_ON_PLATEAU = 5, // Reduce when metric plateaus
    NIMCP_API_SCHED_CYCLIC = 6           // Cyclic learning rate
} nimcp_api_scheduler_t;

typedef struct {
    nimcp_api_loss_t loss_type;
    nimcp_api_optimizer_t optimizer_type;
    nimcp_api_scheduler_t scheduler_type;
    float learning_rate;        // Default: 0.001
    float weight_decay;         // Default: 0.0
    float momentum;             // Default: 0.9
    float beta1;                // Adam beta1 (default: 0.9)
    float beta2;                // Adam beta2 (default: 0.999)
    float epsilon;              // Adam epsilon (default: 1e-8)
    uint32_t scheduler_step_size;
    float scheduler_gamma;      // Default: 0.1
    uint32_t warmup_steps;
    bool enable_gradient_clipping;
    float gradient_clip_value;  // Default: 1.0
    bool enable_biological_modulation;
    float biological_blend;     // Modulation strength (0-1)
} nimcp_training_config_t;

typedef struct {
    float loss;
    float learning_rate;
    uint32_t step;
    bool early_stopped;
    float gradient_norm;
} nimcp_training_result_t;
```

### Functions

**`nimcp_training_config_t nimcp_training_config_default(void)`**
- **Description:** Get default training configuration
- **Returns:** Sensible defaults (Cross-Entropy, Adam, Cosine annealing)

**`nimcp_status_t nimcp_brain_configure_training(nimcp_brain_t brain, const nimcp_training_config_t* config)`**
- **Description:** Configure brain's training pipeline
- **Example:**
```c
nimcp_training_config_t config = nimcp_training_config_default();
config.loss_type = NIMCP_API_LOSS_CROSS_ENTROPY;
config.optimizer_type = NIMCP_API_OPT_ADAM;
config.learning_rate = 0.001f;
nimcp_brain_configure_training(brain, &config);
```

**`nimcp_status_t nimcp_brain_train_step(nimcp_brain_t brain, const float* features, uint32_t num_features, const float* targets, uint32_t num_targets, nimcp_training_result_t* result)`**
- **Description:** Train brain using training pipeline (single step)
- **Process:** Forward → Loss → Gradients → Regularization → Biological modulation → Update → LR schedule

**`nimcp_status_t nimcp_brain_train_batch(nimcp_brain_t brain, const float* features, const float* targets, uint32_t batch_size, uint32_t num_features, uint32_t num_targets, nimcp_training_result_t* result)`**
- **Description:** Train on a batch of examples (mini-batch gradient descent)

**`nimcp_status_t nimcp_brain_get_training_stats(nimcp_brain_t brain, uint64_t* total_steps, float* total_loss, float* current_lr)`**
- **Description:** Get current training statistics

**`float nimcp_brain_step_scheduler(nimcp_brain_t brain, float validation_metric)`**
- **Description:** Step the learning rate scheduler (call at epoch end)
- **Returns:** New learning rate

---

## Training Callbacks API

**Header:** `nimcp.h`

Event-driven training monitoring and control system.

### Types

```c
typedef enum {
    NIMCP_CB_STEP_COMPLETE = 0,     // Training step finished
    NIMCP_CB_EPOCH_COMPLETE,        // Epoch finished
    NIMCP_CB_LOSS_COMPUTED,         // Loss calculated
    NIMCP_CB_WEIGHTS_UPDATED,       // Weights modified
    NIMCP_CB_LR_CHANGED,            // Learning rate changed
    NIMCP_CB_CONVERGENCE,           // Early stopping triggered
    NIMCP_CB_DIVERGENCE,            // Training instability
    NIMCP_CB_CHECKPOINT,            // Checkpoint saved
    NIMCP_CB_EVENT_COUNT
} nimcp_callback_event_t;

typedef enum {
    NIMCP_CB_ACTION_CONTINUE = 0,   // Continue training normally
    NIMCP_CB_ACTION_STOP,           // Stop training loop
    NIMCP_CB_ACTION_SKIP,           // Skip current step
    NIMCP_CB_ACTION_ROLLBACK,       // Rollback to checkpoint
    NIMCP_CB_ACTION_REDUCE_LR,      // Reduce learning rate
    NIMCP_CB_ACTION_INCREASE_LR     // Increase learning rate
} nimcp_callback_action_t;

typedef struct {
    uint64_t step;
    uint64_t epoch;
    float loss;
    float loss_ema;
    float learning_rate;
    float gradient_norm;
    uint64_t step_time_us;
    bool is_converging;
    bool is_diverging;
} nimcp_callback_metrics_t;

typedef nimcp_callback_action_t (*nimcp_training_callback_fn)(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* metrics,
    void* user_data
);

typedef struct {
    bool enable_auto_checkpoint;
    uint32_t checkpoint_interval;
    bool enable_early_stopping;
    uint32_t patience;
    float min_delta;
    float divergence_threshold;
    uint32_t log_interval;
} nimcp_callback_config_t;
```

### Functions

**`nimcp_callback_config_t nimcp_callback_config_default(void)`**
- **Description:** Get default callback configuration

**`nimcp_status_t nimcp_brain_enable_callbacks(nimcp_brain_t brain, const nimcp_callback_config_t* config)`**
- **Description:** Enable training callbacks

**`nimcp_status_t nimcp_brain_disable_callbacks(nimcp_brain_t brain)`**
- **Description:** Disable training callbacks

**`uint32_t nimcp_brain_register_callback(nimcp_brain_t brain, nimcp_callback_event_t event, nimcp_training_callback_fn callback, void* user_data, const char* name)`**
- **Description:** Register a callback for a specific event type
- **Returns:** Callback ID (>0) on success, 0 on error
- **Example:**
```c
nimcp_callback_action_t my_logger(
    nimcp_callback_event_t event,
    const nimcp_callback_metrics_t* m,
    void* user_data)
{
    printf("Step %lu: loss=%.4f lr=%.6f\n", m->step, m->loss, m->learning_rate);
    return NIMCP_CB_ACTION_CONTINUE;
}

uint32_t cb_id = nimcp_brain_register_callback(
    brain, NIMCP_CB_STEP_COMPLETE, my_logger, NULL, "logger");
```

**`nimcp_status_t nimcp_brain_unregister_callback(nimcp_brain_t brain, uint32_t callback_id)`**
- **Description:** Unregister a callback

**`nimcp_status_t nimcp_brain_get_callback_stats(nimcp_brain_t brain, uint64_t* total_fired, float* avg_time_us, uint32_t* early_stops)`**
- **Description:** Get callback statistics

---

## Dynamic Brain Resizing

**Header:** `nimcp.h`

Dynamic scaling based on hardware capabilities and utilization.

### Functions

**`bool nimcp_brain_resize(nimcp_brain_t brain, uint32_t new_neuron_count)`**
- **Description:** Manually resize brain to specific neuron count
- **Returns:** true on success

**`bool nimcp_brain_auto_resize(nimcp_brain_t brain)`**
- **Description:** Automatically resize based on hardware and utilization
- **Returns:** true if resized

**`uint32_t nimcp_brain_get_neuron_count(nimcp_brain_t brain)`**
- **Description:** Get current neuron count
- **Returns:** Neuron count or 0 on error

**`bool nimcp_brain_get_utilization_metrics(nimcp_brain_t brain, float* utilization, float* saturation)`**
- **Description:** Get brain utilization metrics
- **Parameters:**
  - `utilization`: Percentage of neurons being used (0.0-1.0)
  - `saturation`: Percentage of neurons at capacity (0.0-1.0)

---

## Brain Snapshots

**Header:** `nimcp.h`

Named, timestamped backups for versioning and A/B testing.

### Types

```c
typedef struct {
    char name[128];
    char description[512];
    uint64_t timestamp;
    uint32_t file_size;
    bool is_compressed;
    bool is_encrypted;
} nimcp_brain_snapshot_info_t;
```

### Functions

**`nimcp_status_t nimcp_brain_snapshot_save(nimcp_brain_t brain, const char* name, const char* description)`**
- **Description:** Save a named snapshot
- **Example:**
```c
nimcp_brain_snapshot_save(brain, "before_training", "Baseline state");
// Train...
nimcp_brain_snapshot_save(brain, "after_epoch_1", "After 1 epoch");
```

**`nimcp_brain_t nimcp_brain_snapshot_restore(nimcp_brain_t brain, const char* name)`**
- **Description:** Restore brain from snapshot
- **Returns:** Restored brain instance or NULL

**`nimcp_status_t nimcp_brain_snapshot_list(nimcp_brain_t brain, nimcp_brain_snapshot_info_t* infos, uint32_t max_count, uint32_t* out_count)`**
- **Description:** List all available snapshots

**`nimcp_status_t nimcp_brain_snapshot_delete(nimcp_brain_t brain, const char* name)`**
- **Description:** Delete a named snapshot

---

## Copy-on-Write (COW) Cache

**Header:** `nimcp.h`

Efficient memory sharing with 86% memory savings for brain clones.

### Types

```c
typedef struct nimcp_brain_snapshot_handle* nimcp_brain_snapshot_t;
```

### Functions

**`nimcp_brain_t nimcp_brain_clone_cow(nimcp_brain_t original)`**
- **Description:** Clone brain using copy-on-write semantics
- **Performance:**
  - Clone time: <10ms (vs ~1000ms for full copy)
  - Memory overhead: ~1MB metadata (vs ~50MB full copy)
  - Memory savings: 86% for replicas
- **Example:**
```c
nimcp_brain_t original = nimcp_brain_create(...);
nimcp_brain_t clone = nimcp_brain_clone_cow(original);
// clone shares memory with original (fast, low memory)
nimcp_brain_learn_example(clone, ...);  // Triggers copy on first write
```

**`nimcp_brain_snapshot_t nimcp_brain_snapshot_cow(nimcp_brain_t brain)`**
- **Description:** Create instant snapshot using COW (zero-copy)
- **Performance:** <1ms, ~48 bytes overhead, 99% memory savings
- **Example:**
```c
nimcp_brain_snapshot_t checkpoint = nimcp_brain_snapshot_cow(brain);
train_epochs(brain, 100);
if (performance < threshold) {
    nimcp_brain_restore_cow(brain, checkpoint);  // Instant rollback
}
nimcp_brain_snapshot_destroy(checkpoint);
```

**`nimcp_status_t nimcp_brain_restore_cow(nimcp_brain_t brain, nimcp_brain_snapshot_t snapshot)`**
- **Description:** Restore brain state from COW snapshot
- **Performance:** <1ms (pointer swapping)

**`void nimcp_brain_snapshot_destroy(nimcp_brain_snapshot_t snapshot)`**
- **Description:** Destroy snapshot and release references

---

## Working Memory API

**Header:** `nimcp.h`

Active representation buffer implementing Miller's 7±2 capacity limit.

### Functions

**`nimcp_status_t nimcp_brain_working_memory_add(nimcp_brain_t brain, const float* data, uint32_t size, float salience)`**
- **Description:** Add item to working memory
- **Parameters:**
  - `data`: Feature vector
  - `size`: Number of floats
  - `salience`: Initial importance (0.0-1.0)
- **Notes:**
  - Items stored with salience-based priority
  - Lowest-salience items evicted when at capacity
  - Items decay over time unless refreshed

**`const float* nimcp_brain_working_memory_get(nimcp_brain_t brain, uint32_t index, uint32_t* size_out)`**
- **Description:** Get item by index (0 = highest salience)
- **Returns:** Item data pointer or NULL

**`nimcp_status_t nimcp_brain_working_memory_stats(nimcp_brain_t brain, uint32_t* current_size_out, uint32_t* capacity_out)`**
- **Description:** Get working memory statistics

**`nimcp_status_t nimcp_brain_working_memory_refresh(nimcp_brain_t brain, uint32_t index)`**
- **Description:** Refresh item to prevent decay (simulates attention/rehearsal)

---

## Global Workspace API

**Header:** `nimcp.h`

Conscious access broadcasting based on Global Workspace Theory (Baars, 1988; Dehaene, 2011).

### Types

```c
typedef enum {
    NIMCP_MODULE_NONE = 0,
    NIMCP_MODULE_PERCEPTION,
    NIMCP_MODULE_WORKING_MEMORY,
    NIMCP_MODULE_EXECUTIVE,
    NIMCP_MODULE_THEORY_OF_MIND,
    NIMCP_MODULE_ETHICS,
    NIMCP_MODULE_ATTENTION,
    NIMCP_MODULE_EMOTION,
    NIMCP_MODULE_SALIENCE,
    NIMCP_MODULE_MOTOR,
    NIMCP_MODULE_LANGUAGE,
    NIMCP_MODULE_METACOGNITION,
    NIMCP_MODULE_CURIOSITY,
    NIMCP_MODULE_INTROSPECTION,
    NIMCP_MODULE_PREDICTIVE,
    NIMCP_MODULE_CONSOLIDATION,
    NIMCP_MODULE_EPISODIC_MEMORY,
    NIMCP_MODULE_SEMANTIC_MEMORY,
    NIMCP_MODULE_WELLBEING,
    NIMCP_MODULE_MENTAL_HEALTH,
    NIMCP_MODULE_GOAL_MOTIVATION,
    NIMCP_MODULE_COGNITIVE_CONTROL,
    NIMCP_MODULE_CUSTOM_START = 100
} nimcp_cognitive_module_t;
```

### Functions

**`nimcp_status_t nimcp_brain_workspace_compete(nimcp_brain_t brain, nimcp_cognitive_module_t module, const float* content, uint32_t content_dim, float strength)`**
- **Description:** Submit content for conscious broadcast
- **Parameters:**
  - `strength`: Competition strength (0.0-1.0, higher = more likely to win)
- **Returns:** NIMCP_OK if won and broadcast
- **Notes:**
  - Refractory period (default 50ms) prevents rapid successive broadcasts
  - Ignition threshold (default 0.6) gates conscious access

**`nimcp_status_t nimcp_brain_workspace_read(nimcp_brain_t brain, float* content, uint32_t max_dim, uint32_t* actual_dim, nimcp_cognitive_module_t* source_module)`**
- **Description:** Read current global workspace broadcast

**`nimcp_status_t nimcp_brain_workspace_subscribe(nimcp_brain_t brain, nimcp_cognitive_module_t module)`**
- **Description:** Subscribe module to workspace broadcasts

**`nimcp_status_t nimcp_brain_workspace_unsubscribe(nimcp_brain_t brain, nimcp_cognitive_module_t module)`**
- **Description:** Unsubscribe from broadcasts

**`nimcp_status_t nimcp_brain_workspace_has_broadcast(nimcp_brain_t brain, bool* has_broadcast)`**
- **Description:** Check if workspace has active broadcast

**`nimcp_status_t nimcp_brain_workspace_stats(nimcp_brain_t brain, uint32_t* total_broadcasts, uint32_t* total_competitions, float* avg_strength)`**
- **Description:** Get workspace statistics

---

## Complex Oscillation API

**Header:** `nimcp.h`

Phase coding and neural synchrony for theta-gamma coupling and phase-based memory.

### Types

```c
typedef struct {
    float amplitude;  // Oscillation amplitude (>= 0)
    float phase;      // Phase angle in radians (-pi to pi)
} nimcp_oscillation_phasor_t;
```

### Functions

**`bool nimcp_enable_complex_oscillations(nimcp_brain_t brain, bool enable)`**
- **Description:** Enable/disable complex oscillation features
- **Performance impact:**
  - Memory: +15% (phase data storage)
  - Compute: +10% (complex arithmetic)
- **Benefits:** Phase-based memory, theta-gamma coupling

**`bool nimcp_is_complex_oscillations_enabled(nimcp_brain_t brain)`**
- **Description:** Check if complex oscillations are enabled

**`nimcp_oscillation_phasor_t nimcp_get_oscillation_phasor(nimcp_brain_t brain, uint32_t neuron_id)`**
- **Description:** Get amplitude and phase for a specific neuron
- **Returns:** Phasor with amplitude and phase

**`float nimcp_get_phase_coherence(nimcp_brain_t brain, const uint32_t* neuron_ids, uint32_t count)`**
- **Description:** Compute phase coherence across multiple neurons
- **Returns:** Phase coherence [0, 1] where:
  - 0.0 = random phases (no synchronization)
  - 0.3 = weak synchronization
  - 0.6 = moderate synchronization
  - 1.0 = perfect phase locking
- **Example:**
```c
uint32_t neurons[] = {10, 20, 30, 40, 50};
float coherence = nimcp_get_phase_coherence(brain, neurons, 5);
if (coherence > 0.6f) {
    printf("High synchronization detected!\n");
}
```

**`float nimcp_get_pac_modulation(nimcp_brain_t brain, float theta_freq, float gamma_freq)`**
- **Description:** Compute phase-amplitude coupling (PAC) modulation index
- **Parameters:**
  - `theta_freq`: Theta frequency (typically 4-8 Hz)
  - `gamma_freq`: Gamma frequency (typically 30-100 Hz)
- **Returns:** PAC modulation index [0, 1] where:
  - 0.0 = no coupling
  - 0.2 = weak coupling
  - 0.4 = moderate coupling
  - 0.6+ = strong coupling (indicates active encoding/retrieval)

---

## Brain Immune System

**Header:** `cognitive/immune/nimcp_brain_immune.h`

Adaptive defense coordination implementing biological immune concepts.

### Biological Model

```
BIOLOGICAL CONCEPT              NIMCP IMPLEMENTATION
───────────────────────────────────────────────────────────────
Antigen presentation         → BBB threat detection → immune processing
B cells (antibody production)→ Swarm immune memory cells + response gen
Helper T cells (CD4+)        → Coordination signals via bio-async
Killer T cells (CD8+/CTL)    → BFT quarantine + DFT node isolation
Antibodies                   → Swarm immune response strategies
Memory cells                 → Swarm immune memory + BFT trust scores
Cytokines                    → Bio-async messages (NOREPINEPHRINE channel)
Inflammation                 → Hierarchical recovery escalation
Resolution                   → Recovery completion + trust restoration
```

### Types

```c
typedef struct brain_immune_system brain_immune_system_t;

typedef enum {
    B_CELL_NAIVE = 0,       // Unactivated, no bound antigen
    B_CELL_ACTIVATED,       // Antigen recognized, proliferating
    B_CELL_PLASMA,          // Antibody-producing state
    B_CELL_MEMORY           // Long-lived memory state
} b_cell_state_t;

typedef enum {
    T_CELL_HELPER = 0,      // CD4+ coordination
    T_CELL_KILLER           // CD8+/CTL cytotoxic
} t_cell_type_t;
```

### Key Functions

**`brain_immune_system_t* brain_immune_create(const brain_immune_config_t* config)`**
- **Description:** Create brain immune system
- **Returns:** System handle or NULL

**`void brain_immune_destroy(brain_immune_system_t* system)`**
- **Description:** Destroy immune system

**`int brain_immune_present_antigen(brain_immune_system_t* system, const antigen_t* antigen)`**
- **Description:** Present threat (antigen) to immune system
- **Returns:** 0 on success

**`int brain_immune_activate_b_cell(brain_immune_system_t* system, uint32_t cell_id, const antigen_t* antigen)`**
- **Description:** Activate B cell for specific antigen

**`int brain_immune_deploy_antibodies(brain_immune_system_t* system, uint32_t b_cell_id)`**
- **Description:** Deploy antibodies from plasma B cell

**`int brain_immune_signal_cytokine(brain_immune_system_t* system, cytokine_type_t type, float amount)`**
- **Description:** Signal via bio-async cytokine channels

**Important:** B cells must be in PLASMA state to produce antibodies. State progression: NAIVE → ACTIVATED → PLASMA.

---

## Bio-Async System

**Header:** `async/nimcp_bio_async.h`

Biologically-inspired asynchronous computation replacing traditional futures/promises.

### Neuromodulator Channels

| Channel | Purpose | Timing |
|---------|---------|--------|
| DOPAMINE | Goal completion, reward | Fast, medium decay |
| SEROTONIN | Mood, state coordination | Slow, long decay |
| NOREPINEPHRINE | Alertness, priority | Fast, medium decay |
| ACETYLCHOLINE | Attention, fast switching | Very fast, short decay |

### Types

```c
typedef struct nimcp_bio_promise_struct* nimcp_bio_promise_t;
typedef struct nimcp_bio_future_struct* nimcp_bio_future_t;
typedef struct nimcp_phase_sync_struct* nimcp_phase_sync_t;
typedef struct nimcp_glial_wave_struct* nimcp_glial_wave_t;

typedef enum {
    BIO_CHANNEL_DOPAMINE = 0,
    BIO_CHANNEL_SEROTONIN = 1,
    BIO_CHANNEL_NOREPINEPHRINE = 2,
    BIO_CHANNEL_ACETYLCHOLINE = 3,
    BIO_CHANNEL_COUNT = 4
} nimcp_bio_channel_type_t;

typedef enum {
    BIO_OSC_DELTA = 0,  // 0.5-4 Hz: Deep coordination
    BIO_OSC_THETA = 1,  // 4-8 Hz: Memory, sequential processing
    BIO_OSC_ALPHA = 2,  // 8-12 Hz: Attention, inhibitory gating
    BIO_OSC_BETA = 3,   // 12-30 Hz: Motor/working memory
    BIO_OSC_GAMMA = 4,  // 30-100 Hz: Fast binding, consciousness
    BIO_OSC_BAND_COUNT = 5
} nimcp_oscillation_band_t;
```

### Core Functions

**`nimcp_error_t nimcp_bio_async_init(const nimcp_bio_async_config_t* config)`**
- **Description:** Initialize bio-async system

**`nimcp_bio_promise_t nimcp_bio_promise_create(nimcp_bio_channel_type_t channel, size_t result_size)`**
- **Description:** Create neuromodulator-based promise

**`nimcp_bio_future_t nimcp_bio_promise_get_future(nimcp_bio_promise_t promise)`**
- **Description:** Get future from promise

**`nimcp_error_t nimcp_bio_promise_complete(nimcp_bio_promise_t promise, const void* result)`**
- **Description:** Complete promise with biological dynamics

**`nimcp_error_t nimcp_bio_future_wait(nimcp_bio_future_t future, void* result, uint32_t timeout_ms)`**
- **Description:** Wait with biological timeout (follows neuromodulator decay)

**`float nimcp_bio_future_get_confidence(nimcp_bio_future_t future)`**
- **Description:** Get confidence level (decays over time!)

### Phase Coupling

**`nimcp_phase_sync_t nimcp_phase_sync_create(nimcp_oscillation_band_t band)`**
- **Description:** Create phase synchronization coordinator

**`nimcp_error_t nimcp_phase_sync_add_future(nimcp_phase_sync_t sync, nimcp_bio_future_t future)`**
- **Description:** Add future to phase coupling group

**`nimcp_error_t nimcp_phase_sync_wait_coherent(nimcp_phase_sync_t sync, float coherence_threshold)`**
- **Description:** Wait for phase coherence (biological "all ready")

### Example Usage

```c
// Create neuromodulator-based promise (dopamine = reward/completion)
nimcp_bio_promise_t promise = nimcp_bio_promise_create(
    BIO_CHANNEL_DOPAMINE, sizeof(float));
nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);

// Complete with biological dynamics
float result = 42.0f;
nimcp_bio_promise_complete(promise, &result);

// Wait with biological timeout
nimcp_bio_future_wait(future, &result, 0);
float confidence = nimcp_bio_future_get_confidence(future);  // Decays!

// Phase-coupled synchronization
nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
nimcp_phase_sync_add_future(sync, future1);
nimcp_phase_sync_add_future(sync, future2);
nimcp_phase_sync_wait_coherent(sync, 0.8f);  // 80% coherence
```

---

## Swarm Intelligence

**Header:** `swarm/nimcp_swarm_brain.h`

Distributed cognitive processing for drone/robot collectives.

### Architecture

- **Local Brain:** Resource-constrained NIMCP processing
- **Collective Workspace:** Shared attention and goals
- **Consensus Engine:** Voting and decision-making
- **Signal Adapter:** Radio communication abstraction
- **Emergence Detection:** Tier tracking for swarm size

### Emergence Tiers

| Tier | Size | Behavior |
|------|------|----------|
| TIER_0_DISCONNECTED | 1 | Solo operation |
| TIER_1_PAIRED | 2-3 | Basic coordination |
| TIER_2_CLUSTER | 4-7 | Group behaviors |
| TIER_3_SWARM | 8+ | Emergent intelligence |
| TIER_4_SUPERORGANISM | 16+ | High coherence collective |

### Types

```c
typedef struct swarm_brain_config {
    uint32_t drone_id;
    char swarm_name[SWARM_MAX_NAME_LEN];
    uint32_t heartbeat_ms;
    float coherence_threshold;
    bool enable_bio_async;
} swarm_brain_config_t;
```

### Functions

**`swarm_brain_t* swarm_brain_create(const swarm_brain_config_t* config)`**
- **Description:** Create swarm brain coordinator

**`int swarm_brain_join(swarm_brain_t* swarm)`**
- **Description:** Join swarm network

**`int swarm_brain_leave(swarm_brain_t* swarm)`**
- **Description:** Leave swarm network

**`int swarm_brain_process(swarm_brain_t* swarm)`**
- **Description:** Process messages, votes, synchronization

**`int swarm_brain_broadcast_perception(swarm_brain_t* swarm, const perception_data_t* data)`**
- **Description:** Share sensor observations

**`swarm_emergence_tier_t swarm_brain_get_emergence_tier(swarm_brain_t* swarm)`**
- **Description:** Get current emergence level

**`void swarm_brain_destroy(swarm_brain_t* swarm)`**
- **Description:** Destroy swarm brain

### Example Usage

```c
swarm_brain_config_t config = {
    .drone_id = 1,
    .swarm_name = "alpha_squadron",
    .heartbeat_ms = 100,
    .coherence_threshold = 0.5,
    .enable_bio_async = true
};
swarm_brain_t* swarm = swarm_brain_create(&config);
swarm_brain_join(swarm);

while (running) {
    swarm_brain_process(swarm);

    perception_data_t data = {...};
    swarm_brain_broadcast_perception(swarm, &data);

    swarm_emergence_tier_t tier = swarm_brain_get_emergence_tier(swarm);
    sleep_ms(10);
}

swarm_brain_leave(swarm);
swarm_brain_destroy(swarm);
```

---

## Learning Systems

### Adaptive Threshold Spiking Network
**Header:** `nimcp_adaptive.h`

```c
typedef struct adaptive_network_t adaptive_network_t;

typedef struct {
    uint32_t num_inputs;
    uint32_t num_outputs;
    uint32_t num_hidden;
    float k_factor;           // Winner-take-all factor
    float min_threshold;
    float max_threshold;
    float target_sparsity;    // 0.0 - 1.0
} adaptive_network_config_t;
```

**`adaptive_network_t* adaptive_network_create(const adaptive_network_config_t* config)`**
**`void adaptive_network_forward(adaptive_network_t* net, const float* inputs, float* outputs)`**
**`float adaptive_network_get_sparsity(adaptive_network_t* net)`**
**`void adaptive_network_prune(adaptive_network_t* net, float threshold)`**

### Neuromodulator System
**Header:** `nimcp_neuromodulators.h`

Thread-safe neuromodulator system with reader-writer locks.

```c
typedef enum {
    NEUROMOD_DOPAMINE = 0,       // Reward, motivation
    NEUROMOD_SEROTONIN,          // Mood, patience
    NEUROMOD_ACETYLCHOLINE,      // Attention, arousal
    NEUROMOD_NOREPINEPHRINE,     // Alertness, stress
    NEUROMOD_COUNT
} neuromodulator_type_t;

typedef struct {
    float learning_rate_multiplier;
    float exploration_bias;
    float attention_focus;
    float memory_consolidation;
} modulation_effects_t;
```

**`neuromodulator_system_t neuromodulator_system_create(void)`**
**`void neuromodulator_release(neuromodulator_system_t system, neuromodulator_type_t type, float amount)`**
- Thread-safe with ~100ns write lock overhead

**`float neuromodulator_get_level(neuromodulator_system_t system, neuromodulator_type_t type)`**
- Thread-safe read (~50ns overhead)

**`modulation_effects_t neuromodulator_get_effects(neuromodulator_system_t system)`**
- Lock-free access via thread-local buffer

### BCM Learning Rule
**Header:** `nimcp_bcm.h`

Bienenstock-Cooper-Munro learning with spinlock protection.

```c
typedef struct {
    float weight;
    float threshold;
    float avg_post_activity;
    float eligibility;
    nimcp_spinlock_t lock;
} bcm_synapse_t;
```

**`bcm_synapse_t bcm_synapse_init(float initial_weight, float initial_threshold)`**
**`void bcm_apply_rule(bcm_synapse_t* synapse, float pre, float post, float dt, const bcm_params_t* params)`**
- Spinlock-protected (~10-20ns overhead)
- Algorithm: Δw = η × pre × post × (post - θ)

---

## Event Processing

### Event-Driven Processing
**Header:** `nimcp_events.h`

```c
typedef struct {
    uint64_t timestamp;
    uint32_t source_neuron;
    uint16_t feature_code;
    uint8_t  confidence;
    uint8_t  flags;
} event_packet_t;

typedef void (*event_callback_t)(const event_packet_t* event, void* user_data);
```

**`event_generator_t* event_generator_create(const event_generator_config_t* config, event_callback_t callback, void* user_data)`**
**`void event_generator_on_spike(event_generator_t* gen, uint32_t neuron_id)`**
**`event_receiver_t* event_receiver_create(const event_receiver_config_t* config, nimcp_neuralnet_t* target)`**
**`void event_receiver_process(event_receiver_t* recv, const event_packet_t* packet)`**

---

## P2P Networking

**Header:** `nimcp_p2pnode.h`

```c
typedef struct {
    char node_name[64];
    uint16_t listen_port;
    uint32_t max_peers;
    uint32_t connection_timeout_ms;
} p2p_node_config_t;
```

**`p2p_node_t* p2p_node_create(const p2p_node_config_t* config)`**
**`int p2p_node_start(p2p_node_t* node)`**
**`int p2p_node_connect(p2p_node_t* node, const char* peer_ip, uint16_t peer_port)`**
**`int p2p_node_broadcast(p2p_node_t* node, const uint8_t* data, size_t size)`**
**`int p2p_node_get_peers(p2p_node_t* node, peer_info_t* peers, uint32_t* count)`**
**`void p2p_node_stop(p2p_node_t* node)`**

---

## Data I/O & Streaming

### Data I/O
**Header:** `nimcp_dataio.h`

**`nimcp_dataset_t* nimcp_dataio_load_csv(const char* filepath, bool has_header)`**
**`int nimcp_dataio_read_batch(nimcp_dataset_t* dataset, nimcp_batch_t* batch, uint32_t batch_size)`**
**`void nimcp_dataio_reset(nimcp_dataset_t* dataset)`**
**`int nimcp_dataio_train(nimcp_brain_t* brain, nimcp_dataset_t* dataset, uint32_t epochs, float validation_split)`**

### Streaming API
**Header:** `nimcp_stream.h`

```c
typedef enum {
    STREAM_MODE_SYNCHRONOUS,
    STREAM_MODE_BACKGROUND,
    STREAM_MODE_BATCHED
} stream_mode_t;
```

**`nimcp_stream_t* nimcp_stream_create(nimcp_brain_t* brain, stream_mode_t mode, const stream_config_t* config)`**
**`int nimcp_stream_feed(nimcp_stream_t* stream, const float* features, uint32_t size)`**
**`int nimcp_stream_get_decision(nimcp_stream_t* stream, float* decision, float* confidence, uint32_t timeout_ms)`**

---

## Attention & Salience

**Header:** `nimcp_salience.h`

```c
typedef struct {
    float novelty;
    float surprise;
    float relevance;
    float urgency;
    float overall_salience;
} salience_score_t;
```

**`salience_evaluator_t* salience_evaluator_create(nimcp_brain_t* brain, salience_mode_t mode)`**
**`void salience_evaluate(salience_evaluator_t* eval, const float* features, uint32_t size, salience_score_t* score)`**
**`float salience_get_novelty(salience_evaluator_t* eval, const float* features, uint32_t size)`**

---

## Memory Consolidation

**Header:** `nimcp_consolidation.h`

```c
typedef struct {
    float* pattern;
    uint32_t size;
    float salience;
    uint64_t timestamp;
    uint32_t rehearsal_count;
} memory_trace_t;
```

**`consolidation_system_t* consolidation_create(nimcp_brain_t* brain, uint32_t max_traces)`**
**`void consolidation_add_trace(consolidation_system_t* sys, const float* pattern, uint32_t size, float salience)`**
**`void consolidation_consolidate(consolidation_system_t* sys)`**
**`int consolidation_recall(consolidation_system_t* sys, const float* cue, uint32_t size, float* output, float* confidence)`**

---

## Introspection & Monitoring

**Header:** `nimcp_introspection.h`

**`uint32_t nimcp_introspection_get_neuron_count(nimcp_neuralnet_t* net)`**
**`float nimcp_introspection_get_neuron_activation(nimcp_neuralnet_t* net, uint32_t neuron_id)`**
**`int nimcp_introspection_get_active_neurons(nimcp_neuralnet_t* net, uint32_t* neuron_ids, uint32_t max_count)`**
**`uint32_t nimcp_introspection_get_synapse_count(nimcp_neuralnet_t* net)`**
**`float nimcp_introspection_get_synapse_weight(nimcp_neuralnet_t* net, uint32_t from, uint32_t to)`**
**`void nimcp_introspection_dump_state(nimcp_neuralnet_t* net, FILE* output)`**

---

## Higher-Level Cognitive APIs

### Curiosity-Driven Learning
**Header:** `nimcp_curiosity.h`

**`curiosity_engine_t* curiosity_create(const curiosity_config_t* config)`**
**`float curiosity_get_intrinsic_reward(curiosity_engine_t* engine, const float* state, const float* next_state, float external_reward)`**

### Knowledge Acquisition
**Header:** `nimcp_knowledge.h`

**`knowledge_system_t knowledge_system_create(const char* learner_name)`**
**`uint32_t knowledge_learn_from_text(knowledge_system_t system, const char* text, knowledge_domain_t domain)`**
**`bool knowledge_retrieve(knowledge_system_t system, const char* concept, knowledge_item_t* item)`**
**`uint32_t knowledge_get_by_confidence_range(knowledge_system_t system, float min, float max, knowledge_item_t** results_out)`**
- B-tree indexed, O(log n + k) complexity

### Ethical Reasoning
**Header:** `nimcp_ethics.h`

**`ethics_engine_t* ethics_create(const char* rules_file)`**
**`void ethics_evaluate_action(ethics_engine_t* engine, const float* state, const float* action, ethical_evaluation_t* result)`**
**`bool ethics_is_allowed(ethics_engine_t* engine, const float* action)`**

---

## Thread Safety & Synchronization

**Header:** `utils/nimcp_thread.h`

### Reader-Writer Lock API

```c
nimcp_result_t nimcp_rwlock_init(nimcp_rwlock_t* rwlock);
nimcp_result_t nimcp_rwlock_rdlock(nimcp_rwlock_t* rwlock);   // ~50-100ns
nimcp_result_t nimcp_rwlock_wrlock(nimcp_rwlock_t* rwlock);   // ~50-100ns + wait
nimcp_result_t nimcp_rwlock_unlock(nimcp_rwlock_t* rwlock);
nimcp_result_t nimcp_rwlock_destroy(nimcp_rwlock_t* rwlock);
```

### Spinlock API (for <100 cycle critical sections)

```c
nimcp_result_t nimcp_spinlock_init(nimcp_spinlock_t* spinlock);
nimcp_result_t nimcp_spinlock_lock(nimcp_spinlock_t* spinlock);   // ~10-20ns
nimcp_result_t nimcp_spinlock_unlock(nimcp_spinlock_t* spinlock); // ~5-10ns
nimcp_result_t nimcp_spinlock_destroy(nimcp_spinlock_t* spinlock);
```

### Performance Comparison

| Operation | Latency | Parallel? | Best For |
|-----------|---------|-----------|----------|
| RWLock Read | ~50-100ns | Multiple readers | Read-heavy data |
| RWLock Write | ~50-100ns | Exclusive | Infrequent updates |
| Spinlock | ~10-20ns | Exclusive | <100 cycle sections |
| Mutex | ~50-100ns | Exclusive | General purpose |
| Atomic Increment | ~5ns | Lock-free | Counters, flags |
| Thread-Local | 0ns | No sync needed | Per-thread data |

---

## Utility APIs

### FFT Spectral Analysis
**Header:** `utils/spectral/nimcp_fft.h`

```c
fft_plan_t* fft_plan_create(uint32_t size, fft_type_t type);
bool fft_plan_set_window(fft_plan_t* plan, fft_window_type_t window);
bool fft_execute_real(fft_plan_t* plan, const float* input, fft_complex_t* output);
bool fft_power_spectrum(const fft_complex_t* spectrum, float* power, uint32_t size);
float fft_dominant_frequency(const float* power, uint32_t size, float sampling_rate);
float fft_brain_wave_power(const float* power, uint32_t size, float sampling_rate, brain_wave_band_t band);
```

### Memory Tracking
**Header:** `utils/nimcp_memory.h`

**`void* nimcp_malloc(size_t size, const char* file, int line)`**
**`void nimcp_free(void* ptr, const char* file, int line)`**
**`void nimcp_memory_report(FILE* output)`**
**`size_t nimcp_memory_get_allocated(void)`**

### Data Structures
**Header:** `utils/nimcp_hash_table.h`, `utils/nimcp_btree.h`, `utils/nimcp_graph.h`

Hash tables, B-trees, and graphs with neuromorphic optimizations.

---

## Utility Functions

**`const char* nimcp_version(void)`**
- Returns version string (e.g., "2.6.3")

**`int nimcp_version_int(void)`**
- Returns version integer (e.g., 20603)

**`const char* nimcp_get_error(void)`**
- Get error message for last error

**`nimcp_status_t nimcp_init(void)`**
- Initialize NIMCP library (call once at startup)

**`void nimcp_shutdown(void)`**
- Shutdown NIMCP library (call once at cleanup)

---

## Language Bindings

NIMCP provides bindings for 7 languages:

| Language | Directory | Build |
|----------|-----------|-------|
| Python | `src/python/` | `python setup.py install` |
| C++ | `src/bindings/cpp/` | CMake |
| Java | `src/bindings/java/` | `mvn package` |
| Rust | `src/bindings/rust/` | `cargo build` |
| Go | `src/bindings/go/` | `go build` |
| Perl | `src/bindings/perl/` | `perl Makefile.PL && make` |
| C# | `src/bindings/csharp/` | `dotnet build` |

### Python Example

```python
import nimcp

# Create brain with preset
config = nimcp.BrainConfig(
    num_inputs=10,
    num_outputs=5,
    hidden_layers=[20, 15],
    task_name="classification"
)
brain = nimcp.Brain(config)

# Process and learn
result = brain.process(inputs)
brain.release_dopamine(0.8)

# Training with callbacks
brain.configure_training(
    loss='cross_entropy',
    optimizer='adam',
    learning_rate=0.001
)
brain.train_step(features, targets)
```

### Language Binding Comparison

| Feature | Python | C++ | Java | Rust | Go | Perl | C# |
|---------|--------|-----|------|------|----|----|-----|
| Memory Mgmt | GC | RAII | GC | Ownership | GC | RC | GC |
| Thread Safety | GIL | Manual | JVM | Arc/Mutex | Goroutines | threads | Task |
| Error Handling | Exception | Exception | Exception | Result<T,E> | (val, err) | eval | Exception |
| Performance | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |

---

## API Conventions

### Return Value Conventions
- **FEP bridges:** Return `0` for success, `-1` for errors (not NIMCP_OK/NIMCP_ERROR_*)
- **Metabolic modulation:** `metabolic_compute_effects()` returns `0` for success, `-1` for errors
- **Standard NIMCP functions:** Return `nimcp_status_t` codes

### Thread Safety
- **Creation/Destruction:** Not thread-safe
- **Forward Pass:** Not thread-safe on same network
- **Statistics:** Thread-safe (read-only)
- **Queue Operations:** Thread-safe with internal locking

### Memory Management
- **Ownership:** Caller owns returned pointers from create functions
- **Cleanup:** Always call corresponding destroy function
- **Input Buffers:** Library does not take ownership

### Naming Conventions
- **Prefix:** All public APIs prefixed with `nimcp_`
- **Structs:** `*_t` suffix for types
- **Constants:** UPPERCASE_WITH_UNDERSCORES
- **Functions:** lowercase_with_underscores

---

## Version History

### 2.6.3 (Current)
- Training Pipeline API (loss functions, optimizers, LR schedulers)
- Training Callbacks API (event-driven monitoring)
- Dynamic Brain Resizing API
- Brain Snapshots API
- Copy-on-Write (COW) Cache API (86% memory savings)
- Working Memory API (Miller's 7±2)
- Global Workspace API (Baars' GWT)
- Complex Oscillation API (phase coding, PAC)
- Brain Immune System API
- Bio-Async System API
- Swarm Intelligence API

### 2.6.1
- Security hardening (strcpy → strncpy)
- Knowledge module memory corruption fix

### 2.6.0
- FFT spectral analysis utilities
- Brain oscillation analysis module
- Cognitive state inference

### 2.5.1
- Knowledge B-tree indexing
- Confidence-based queries

### 2.5.0
- Thread safety (rwlocks, spinlocks, atomics)
- Neuromodulator system
- BCM learning rule
- Language bindings (C++, Java, Rust, Go, Perl, C#)

---

## Additional Resources

- **README.md:** General project overview
- **SECURITY.md:** Security policy and reporting
- **EXTERNAL_API_GUIDE.md:** External API integration guide
- **Examples:** See `examples/` directory

---

**Copyright 2025-2026 NIMCP Project**
**License:** See LICENSE file
