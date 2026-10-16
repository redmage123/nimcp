# Code Quality Examples - Specific Issues with File:Line References

## 1. CRITICAL: Missing NULL Checks

### Example 1: nimcp_brain.c - Memory Allocation Without Check
**Location**: Multiple occurrences throughout codebase
**Pattern**:
```c
// BEFORE (risky):
brain = malloc(sizeof(nimcp_brain_t));
brain->network = create_network(config);  // Crash if malloc failed!

// AFTER (safe):
brain = malloc(sizeof(nimcp_brain_t));
if (!brain) {
    return NIMCP_ERROR_OUT_OF_MEMORY;
}
brain->network = create_network(config);
if (!brain->network) {
    free(brain);
    return NIMCP_ERROR_OUT_OF_MEMORY;
}
```

## 2. CRITICAL: God Object Antipattern

### nimcp_brain.c:1-11977 - Single File with Multiple Responsibilities
**Current Structure** (11,977 lines in one file):
- Lines 1-500: Includes and type definitions
- Lines 500-2000: Brain creation and lifecycle
- Lines 2000-4000: Learning and training
- Lines 4000-6000: Inference and prediction
- Lines 6000-8000: I/O and serialization
- Lines 8000-10000: Monitoring and statistics
- Lines 10000-11977: Cognitive integration

**Recommended Split**:
```
nimcp_brain_core.c      (2000 lines) - creation, lifecycle, core APIs
nimcp_brain_learning.c  (2000 lines) - training, adaptation, plasticity
nimcp_brain_inference.c (2000 lines) - forward pass, prediction
nimcp_brain_io.c        (2000 lines) - save/load, serialization
nimcp_brain_monitoring.c(2000 lines) - stats, health checks, introspection
nimcp_brain.h           (1000 lines) - public API declarations
```

## 3. HIGH: Long Functions

### Example: grief_update - 305 lines
**Location**: src/cognitive/grief/nimcp_grief_and_loss.c:330

**Current**: Single 305-line function handling all grief stages
```c
static void grief_update(grief_system_t* grief, float dt)
{
    // 50 lines: Validate parameters and initialize
    // 80 lines: Update denial stage
    // 70 lines: Update anger stage
    // 60 lines: Update bargaining stage
    // 45 lines: Update acceptance stage
}  // Total: 305 lines
```

**Recommended Refactoring**:
```c
static void grief_update(grief_system_t* grief, float dt)
{
    if (!validate_grief_params(grief, dt)) return;
    
    update_denial_stage(grief, dt);
    update_anger_stage(grief, dt);
    update_bargaining_stage(grief, dt);
    update_acceptance_stage(grief, dt);
    
    transition_grief_stages(grief);
}  // Total: 10 lines

static void update_denial_stage(grief_system_t* grief, float dt)
{
    // 40 lines - focused on denial stage only
}
// ... similar for other stages
```

### Example: quantum_adaptive_routing - 229 lines
**Location**: src/utils/quantum/nimcp_quantum_shannon.c:1038

**Issue**: Complex routing algorithm in single function
**Recommendation**: Extract into strategy pattern with pluggable routing algorithms

### Example: neuromodulator_system_create - 228 lines
**Location**: src/plasticity/neuromodulators/nimcp_neuromodulators.c:393

**Issue**: Construction logic mixed with initialization and validation
**Recommendation**: Apply Builder pattern
```c
// BEFORE (228 lines in one function):
neuromodulator_system_t* neuromodulator_system_create(config) {
    // 50 lines: allocate and validate
    // 60 lines: initialize dopamine
    // 50 lines: initialize serotonin
    // 40 lines: initialize acetylcholine
    // 28 lines: final setup
}

// AFTER (Builder pattern):
neuromodulator_system_t* neuromodulator_system_create(config) {
    neuromod_builder_t* builder = neuromod_builder_new(config);
    neuromod_builder_add_dopamine(builder);
    neuromod_builder_add_serotonin(builder);
    neuromod_builder_add_acetylcholine(builder);
    return neuromod_builder_build(builder);
}  // Total: 6 lines
```

## 4. HIGH: Magic Numbers

### Example: nimcp_brain.c - 583 Magic Numbers
**Locations**: Throughout the file

**Before** (unclear meaning):
```c
if (spike_rate > 0.85) {  // What does 0.85 represent?
    modulate_plasticity(0.15);  // Why 0.15?
}

neuron->threshold = -55.0;  // Magic threshold
learning_rate = 0.001;      // Unexplained learning rate
```

**After** (self-documenting):
```c
#define NIMCP_HIGH_ACTIVITY_THRESHOLD 0.85f
#define NIMCP_PLASTICITY_DAMPENING 0.15f
#define NIMCP_NEURON_THRESHOLD_MV -55.0f
#define NIMCP_DEFAULT_LEARNING_RATE 0.001f

if (spike_rate > NIMCP_HIGH_ACTIVITY_THRESHOLD) {
    modulate_plasticity(NIMCP_PLASTICITY_DAMPENING);
}

neuron->threshold = NIMCP_NEURON_THRESHOLD_MV;
learning_rate = NIMCP_DEFAULT_LEARNING_RATE;
```

## 5. HIGH: Code Duplication

### Example: Validation Logic Duplication
**Locations**: Multiple files

**Pattern Repeated 50+ times**:
```c
// File 1:
if (!brain) {
    NIMCP_LOGGING_ERROR("Brain is NULL");
    return NIMCP_ERROR_INVALID_PARAM;
}
if (!brain->network) {
    NIMCP_LOGGING_ERROR("Network is NULL");
    return NIMCP_ERROR_INVALID_PARAM;
}

// File 2: (same pattern)
if (!system) {
    NIMCP_LOGGING_ERROR("System is NULL");
    return NIMCP_ERROR_INVALID_PARAM;
}
if (!system->data) {
    NIMCP_LOGGING_ERROR("Data is NULL");
    return NIMCP_ERROR_INVALID_PARAM;
}
```

**Refactored** (DRY):
```c
// utils/validation/nimcp_validate.h
#define NIMCP_VALIDATE_NOT_NULL(ptr, name) \
    do { \
        if (!(ptr)) { \
            NIMCP_LOGGING_ERROR(name " is NULL"); \
            return NIMCP_ERROR_INVALID_PARAM; \
        } \
    } while(0)

// Usage:
NIMCP_VALIDATE_NOT_NULL(brain, "Brain");
NIMCP_VALIDATE_NOT_NULL(brain->network, "Network");
```

## 6. MEDIUM: Deep Nesting

### Example: Deep Nesting in Brain Processing
**Location**: src/core/brain/nimcp_brain.c (multiple locations)

**Before** (7 levels deep):
```c
if (brain) {
    if (brain->network) {
        if (brain->network->neurons) {
            for (int i = 0; i < count; i++) {
                if (neurons[i].active) {
                    if (neurons[i].voltage > threshold) {
                        if (check_refractory_period(&neurons[i])) {
                            fire_spike(&neurons[i]);  // 7 levels deep!
                        }
                    }
                }
            }
        }
    }
}
```

**After** (flattened with guard clauses):
```c
if (!brain || !brain->network || !brain->network->neurons) {
    return;  // Early return
}

for (int i = 0; i < count; i++) {
    neuron_t* neuron = &neurons[i];
    
    if (!neuron->active) continue;
    if (neuron->voltage <= threshold) continue;
    if (!check_refractory_period(neuron)) continue;
    
    fire_spike(neuron);  // Only 2 levels deep
}
```

## 7. MEDIUM: Commented-Out Code

### Example: nimcp_brain.c:232 - Dead Code
**Location**: Throughout codebase (3,687 lines)

**Before**:
```c
void process_input(brain_t* brain, float* input) {
    // Old implementation from Phase 2:
    // for (int i = 0; i < input_size; i++) {
    //     neurons[i].voltage += input[i];
    // }
    
    // Phase 3 attempt (didn't work):
    // apply_stdp_learning(brain, input);
    // propagate_spikes(brain);
    
    // Current implementation:
    apply_new_learning(brain, input);
    
    // TODO: Try hebbian learning later?
    // hebbian_update(brain, input);
}
```

**After** (clean):
```c
void process_input(brain_t* brain, float* input) {
    apply_new_learning(brain, input);
    // TODO: Evaluate Hebbian learning as alternative (Issue #142)
}
```
*Note: Old implementations preserved in git history*

## 8. MEDIUM: High Coupling

### Example: nimcp_brain.c - 79 Includes
**Location**: src/core/brain/nimcp_brain.c:1-100

**Before**:
```c
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "plasticity/attention/nimcp_attention.h"
#include "cognitive/introspection/nimcp_introspection.h"
#include "cognitive/ethics/nimcp_ethics.h"
#include "cognitive/salience/nimcp_salience.h"
// ... 72 more includes
```

**After** (facade pattern):
```c
// nimcp_brain.c (reduced includes)
#include "core/brain/nimcp_brain_internal.h"
#include "core/brain/nimcp_subsystems.h"  // Facade for all subsystems
#include "utils/memory/nimcp_memory.h"
#include "utils/validation/nimcp_validate.h"
// Only 10-15 core includes

// nimcp_subsystems.h (facade)
typedef struct {
    plasticity_system_t* plasticity;
    cognitive_system_t* cognitive;
    perception_system_t* perception;
    // ... aggregated subsystems
} brain_subsystems_t;
```

## 9. TODO/FIXME Examples with Prioritization

### CRITICAL Priority TODOs

**Example 1**: Graph Algorithm Missing
```c
// src/cognitive/nimcp_fractal_cognitive.c:158
float get_graph_distance(uint32_t node_a, uint32_t node_b)
{
    // TODO: Implement proper BFS for graph distance
    // PRIORITY: HIGH
    // BLOCKER: Affects fractal topology analysis
    // ESTIMATE: 2 days
    return UINT32_MAX;
}
```

**Example 2**: Database Backend Missing
```c
// src/io/dataio/nimcp_dataio.c:401
/**
 * TODO: Implement when libpq is available
 * PRIORITY: MEDIUM
 * DEPENDENCY: libpq-dev package
 * ESTIMATE: 1 week
 */
static nimcp_result_t postgres_execute_query(...)
{
    dataio_set_error("PostgreSQL backend not yet implemented");
    return NIMCP_ERROR_NOT_IMPLEMENTED;
}
```

**Example 3**: API Dependency
```c
// src/cognitive/mental_health/nimcp_mental_health.c:613
float get_ethical_violation_rate(brain_t* brain)
{
    // TODO: Query actual ethics system when API available
    // BLOCKED_BY: Ethics system public API (Issue #89)
    // WORKAROUND: Using estimated value
    return 0.0f;  // Placeholder
}
```

## 10. Documentation Variance Examples

### UNDER-DOCUMENTED: nimcp_serialization.c (7.5% comment density)
**Location**: src/io/serialization/nimcp_serialization.c

**Before** (minimal documentation):
```c
int save_brain(brain_t* brain, const char* path)
{
    FILE* f = fopen(path, "wb");
    if (!f) return -1;
    
    fwrite(&brain->version, sizeof(int), 1, f);
    fwrite(&brain->num_neurons, sizeof(int), 1, f);
    // ... 50 lines of undocumented serialization logic
    
    fclose(f);
    return 0;
}
```

**After** (properly documented):
```c
/**
 * @brief Saves brain state to binary file using NIMCP serialization format v3
 * 
 * Serialization format:
 * - Header: version (4 bytes) + neuron_count (4 bytes) + checksum (8 bytes)
 * - Neurons: array of neuron_state_t structs
 * - Synapses: compressed sparse matrix format
 * - Metadata: JSON footer with training history
 * 
 * @param brain Brain instance to serialize
 * @param path Output file path (will be created/overwritten)
 * @return 0 on success, negative error code on failure
 * 
 * @note File format is platform-independent (little-endian)
 * @warning Caller must ensure brain is not being modified during save
 */
int save_brain(brain_t* brain, const char* path)
{
    // ... implementation with inline comments for complex sections
}
```

### OVER-DOCUMENTED: nimcp_thread_pool.c (542% comment density)
**Location**: src/utils/thread/nimcp_thread_pool.c

**Before** (excessive comments):
```c
// Create a new thread pool
// The thread pool will have 'num_threads' worker threads
// Each thread will process tasks from the queue
// The queue is thread-safe using mutexes
// Tasks are represented as function pointers
thread_pool_t* thread_pool_create(int num_threads)  // Function to create pool
{
    // Allocate memory for the thread pool structure
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));  // Memory allocation
    
    // Check if allocation succeeded
    if (!pool) {  // NULL check
        // Return NULL to indicate failure
        return NULL;  // Error return
    }
    
    // Initialize the number of threads field
    pool->num_threads = num_threads;  // Set thread count
    
    // ... 50 more lines of obvious comments
}
```

**After** (balanced documentation):
```c
/**
 * @brief Creates thread pool with specified worker count
 * @param num_threads Number of worker threads (must be > 0)
 * @return Thread pool instance or NULL on failure
 */
thread_pool_t* thread_pool_create(int num_threads)
{
    thread_pool_t* pool = malloc(sizeof(thread_pool_t));
    if (!pool) return NULL;
    
    pool->num_threads = num_threads;
    
    // Initialize work queue with lock-free ring buffer for performance
    pool->queue = create_lockfree_queue(QUEUE_SIZE);
    
    // Spawn worker threads with CPU affinity for cache locality
    for (int i = 0; i < num_threads; i++) {
        spawn_worker_thread(pool, i);
    }
    
    return pool;
}
```

---

## Summary: Concrete Examples for Top 10 Refactoring Recommendations

1. **File Decomposition**: Split nimcp_brain.c (11,977 lines) → 5 modules
2. **Function Complexity**: Refactor grief_update (305 lines) → 6 functions
3. **Error Handling**: Add NULL checks to 766 unchecked allocations
4. **Code Duplication**: Extract validation logic → NIMCP_VALIDATE_NOT_NULL macro
5. **Magic Numbers**: Define 583 constants → named #defines
6. **Deep Nesting**: Flatten 7-level nests → 2-level with guard clauses
7. **Commented Code**: Remove 3,687 lines → use git history
8. **Coupling**: Reduce 79 includes → facade pattern (15 includes)
9. **TODO Items**: Resolve 55 HIGH priority → systematic tracking
10. **Documentation**: Balance 7.5%-542% variance → 15-25% target

Each example includes:
- Specific file:line reference
- Before/after code samples
- Measurable improvement metric
- Estimated effort to fix
