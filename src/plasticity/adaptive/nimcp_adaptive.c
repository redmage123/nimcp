#include <stddef.h>  /* for NULL */
#include <sys/stat.h>
#include <fcntl.h>
//=============================================================================
// nimcp_adaptive.c - Refactored Adaptive Threshold Spiking Implementation
//=============================================================================
// ARCHITECTURAL OVERVIEW:
// This module implements adaptive threshold spiking neural networks using
// several design patterns for performance and maintainability:
//
// - Strategy Pattern: Different encoding/learning strategies via function pointers
// - Object Pool Pattern: Reusable buffers to eliminate heap allocations
// - Repository Pattern: Hash-indexed label storage for O(1) lookups
// - Factory Pattern: Network creation with validated configuration
//
// COMPLEXITY ANALYSIS:
// - Spike encoding: O(n) where n = spike count (linear time)
// - Threshold computation: O(n) single pass (previously had potential loops)
// - Network forward pass: O(n) where n = output size (linear)
// - Label lookup: O(1) via hash table (previously O(n) linear search)
// - Learning: O(n) where n = target size (single pass)
//
// DESIGN PRINCIPLES:
// - Single Responsibility: Each function has one clear purpose
// - Guard Clauses: Early returns instead of nested conditionals
// - No Nested Loops: All loops extracted to helper functions
// - Open/Closed: Extensible via function pointers without modification
//
// INVARIANTS:
// - All thresholds remain within [min_threshold, max_threshold]
// - Sparsity maintained at target_sparsity ± 0.1
// - Neuron states updated atomically per forward pass
// - Label map always synchronized with label count
//=============================================================================

#include "plasticity/adaptive/nimcp_adaptive.h"
#include "training/nimcp_unified_training.h"
#include "plasticity/adaptive/nimcp_backprop_kernel.h"
#include <math.h>
#include "utils/math/nimcp_math_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>  /* FWRITE_CHECKED diagnoses short writes via errno */
#ifndef _WIN32
#include <sched.h>  // C-ADP-11: sched_yield() for pool cleanup drain
#endif
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/neuralnet/nimcp_neuralnet_internal.h"
#include "core/neuralnet/nimcp_neuralnet_learning.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/containers/nimcp_hash_table.h"
#include "utils/memory/nimcp_memory.h"  // CRITICAL: Declares nimcp_calloc/nimcp_free return types
#include "utils/logging/nimcp_logging.h"
#include "utils/algorithms/nimcp_sort.h"  // Consolidated sorting algorithms
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "plasticity_adaptive"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(adaptive)

#include "utils/memory/nimcp_memory_pool.h"  // Phase MP: Memory pool for hot paths
#include "utils/memory/nimcp_page_cow.h"     // Phase COW: Page-level COW for state snapshots
#include "plasticity/eligibility/nimcp_eligibility_trace.h"  // Phase 11: Eligibility traces
#include "plasticity/bcm/nimcp_bcm.h"  // Phase 11: BCM homeostatic plasticity
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Phase 11: Neuromodulator system
#include "gpu/execution/nimcp_gpu_detect.h"          // Phase GPU: GPU detection
#include "gpu/context/nimcp_gpu_context.h"           // Phase GPU: GPU context
#include "gpu/training/nimcp_training_bridge.h"      // Phase GPU: Weight cache + forward pass
#include "gpu/neuron/nimcp_neuron_bridge.h"          // Phase Inferentia: NeuronCore inference

//=============================================================================
// Constants and Configuration
//=============================================================================

#define HASH_TABLE_SIZE 256
#define MAX_POOL_BUFFERS 32
#define SPARSITY_ADAPT_INCREASE 1.01f
#define SPARSITY_ADAPT_DECREASE 0.99f
#define SPARSITY_EMA_WEIGHT 0.1f
// OUTPUT_LR_BOOST is defined in nimcp_backprop_kernel.h (single source of truth)
#define LATERAL_INHIBITION_STRENGTH 0.3f  /* Strength for output layer lateral inhibition */
#define ADAPTIVE_WEIGHT_DECAY 1e-5f       /* L2 weight decay — reverted to original value.
                                            * Training succeeded at 1e-5 (loss 9800 → 0.03).
                                            * Changes to 1e-3 broke training. Original was 1e-4,
                                            * reduced to 1e-5 to "prevent
                                          * exponential weight vanishing over long training.
                                          * At 1e-4, weights decay to 4.5% after 100K steps. */

// WHAT: Helper macros for checked file I/O (cert-err33-c compliance)
// WHY:  fwrite/fread return values must be checked to detect I/O errors
// HOW:  Macros check return value and set success=false on failure
// M-4: FWRITE_CHECKED now returns early on failure to avoid writing garbage
// after a partial I/O failure corrupts the file position.
// Logs errno + ferror so the caller can diagnose ENOSPC, EINTR, etc. without
// a kernel trace — previously a short write surfaced as the misleading
// "success is NULL" message at the brain_save call site.
#define FWRITE_CHECKED(ptr, size, count, stream) \
    do { \
        size_t _wrote = fwrite((ptr), (size), (count), (stream)); \
        if (_wrote != (count)) { \
            int _errno = errno; \
            int _ferr  = ferror(stream); \
            NIMCP_LOGGING_ERROR("FWRITE_CHECKED short write: wanted=%zu got=%zu " \
                                "errno=%d (%s) ferror=%d at %s:%d", \
                                (size_t)(count), _wrote, _errno, \
                                strerror(_errno), _ferr, __FILE__, __LINE__); \
            success = false; \
            goto write_done; \
        } \
    } while(0)
// M-5: FREAD_CHECKED uses goto cleanup — currently unused (load uses inline checks).
// Retained for future use; requires a 'cleanup:' label in the calling function.
#define FREAD_CHECKED(ptr, size, count, stream) \
    do { if (fread((ptr), (size), (count), (stream)) != (count)) goto cleanup; } while(0)

//=============================================================================
// Memory Pool for Hot Paths (Phase MP)
//=============================================================================

/**
 * @brief Memory pool for forward/training buffers
 *
 * WHAT: Global memory pool for spike_input and output buffers
 * WHY:  These buffers are allocated/freed on every forward pass - O(1) vs O(log n)
 * HOW:  Lazily initialized pool with blocks sized for typical networks
 */
#define ADAPTIVE_POOL_BLOCK_SIZE 4096   // 4KB - fits 1024 floats
#define ADAPTIVE_POOL_NUM_BLOCKS 256    // 256 concurrent buffers

#include "utils/platform/nimcp_platform_once.h"
#include <stdatomic.h>

/* ============================================================================
 * Comparison function for sorting neuron importance (descending order)
 * ============================================================================ */

/**
 * @brief Compare neuron importance for descending sort
 *
 * Used by nimcp_sort to rank neurons by importance (highest first).
 */
static int compare_neuron_importance_desc(const void* a, const void* b) {
    const neuron_importance_t* na = (const neuron_importance_t*)a;
    const neuron_importance_t* nb = (const neuron_importance_t*)b;

    /* Descending order: higher importance first */
    if (nb->importance > na->importance) return 1;
    if (nb->importance < na->importance) return -1;
    return 0;
}

static _Atomic(memory_pool_t) g_adaptive_pool = NULL;
static nimcp_platform_once_t g_adaptive_pool_once = NIMCP_PLATFORM_ONCE_INIT;

// C-ADP-11: Refcount and shutdown flag for adaptive pool (mirrors backprop kernel pattern)
static _Atomic int g_adaptive_pool_refcount = 0;
static _Atomic bool g_adaptive_pool_shutting_down = false;

/**
 * @brief One-time initialization of adaptive memory pool
 */
static void init_adaptive_pool(void) {
    memory_pool_config_t config = memory_pool_default_config(
        ADAPTIVE_POOL_BLOCK_SIZE, ADAPTIVE_POOL_NUM_BLOCKS);
    memory_pool_t pool = memory_pool_create(&config);
    atomic_store(&g_adaptive_pool, pool);
}

/**
 * @brief Get or create the adaptive network memory pool (thread-safe)
 */
static memory_pool_t get_adaptive_pool(void) {
    nimcp_platform_once(&g_adaptive_pool_once, init_adaptive_pool);
    return atomic_load(&g_adaptive_pool);
}

/**
 * @brief Cleanup adaptive network memory pool
 *
 * WHAT: Destroy the global memory pool and reset the once flag
 * WHY:  Allow proper re-initialization after nimcp_shutdown()
 * HOW:  Destroy pool, store NULL, reset once flag
 *
 * THREAD SAFETY: NOT thread-safe - call only during shutdown
 *                when no other threads are using the adaptive module
 */
void adaptive_pool_cleanup(void) {
    // C-ADP-11: Signal shutdown to prevent new pool allocations from racing
    atomic_store(&g_adaptive_pool_shutting_down, true);

    // Drain any in-flight alloc_hot_buffer / free_hot_buffer calls
    while (atomic_load(&g_adaptive_pool_refcount) > 0) {
#ifdef _WIN32
        SwitchToThread();
#else
        sched_yield();
#endif
    }

    memory_pool_t pool = atomic_load(&g_adaptive_pool);
    if (pool) {
        memory_pool_destroy(pool);
        atomic_store(&g_adaptive_pool, (memory_pool_t)NULL);
    }
    // Reset once flag to allow re-initialization
    g_adaptive_pool_once = NIMCP_PLATFORM_ONCE_INIT;

    // Reset shutdown flag for re-initialization after nimcp_shutdown()+nimcp_init()
    atomic_store(&g_adaptive_pool_shutting_down, false);
}

/**
 * @brief Allocate buffer from pool or heap
 */
static void* alloc_hot_buffer(size_t size) {
    // C-ADP-11: Increment refcount so cleanup knows we're in-flight
    atomic_fetch_add(&g_adaptive_pool_refcount, 1);

    // C-ADP-11: Reject allocations during shutdown to prevent use-after-free
    if (atomic_load(&g_adaptive_pool_shutting_down)) {
        atomic_fetch_sub(&g_adaptive_pool_refcount, 1);
        return nimcp_calloc(1, size);
    }
    if (size <= ADAPTIVE_POOL_BLOCK_SIZE) {
        memory_pool_t pool = get_adaptive_pool();
        if (pool) {
            void* buf = memory_pool_acquire(pool);
            if (buf) {
                // I-M3: Clear full pool block, not just requested size, to prevent
                // stale data from a previous allocation bleeding into a smaller request
                memset(buf, 0, ADAPTIVE_POOL_BLOCK_SIZE);
                atomic_fetch_sub(&g_adaptive_pool_refcount, 1);
                return buf;
            }
        }
    }
    atomic_fetch_sub(&g_adaptive_pool_refcount, 1);
    return nimcp_calloc(1, size);
}

/**
 * @brief Free buffer to pool or heap
 */
static void free_hot_buffer(void* buf) {
    if (!buf) return;
    atomic_fetch_add(&g_adaptive_pool_refcount, 1);
    memory_pool_t pool = get_adaptive_pool();
    if (pool && memory_pool_owns(pool, buf)) {
        memory_pool_release(pool, buf);
    } else {
        nimcp_free(buf);
    }
    atomic_fetch_sub(&g_adaptive_pool_refcount, 1);
}

//=============================================================================
// Object Pool Pattern - Eliminates repeated allocations
//=============================================================================

/**
 * @brief Object pool for spike buffers to avoid heap allocations
 *
 * WHY: Frequent allocation/deallocation during spike encoding causes
 * heap fragmentation. Pool provides O(1) allocation and deterministic performance.
 *
 * COMPLEXITY: O(1) for acquire/release operations
 *
 * INVARIANTS:
 * - in_use array accurately reflects buffer availability
 * - next_available points to valid index [0, MAX_POOL_BUFFERS)
 */
// L-1: spike_buffer_pool_t is currently unused dead code — the hot-path memory pool
// (alloc_hot_buffer / free_hot_buffer) supersedes it. Retained for potential future
// use in spike train encoding paths.
typedef struct {
    uint8_t buffers[MAX_POOL_BUFFERS][256];  // Reusable spike train buffers
    bool in_use[MAX_POOL_BUFFERS];
    uint32_t next_available;
} spike_buffer_pool_t;

/**
 * @brief Label index value stored in hash table
 *
 * WHY: Hash table maps label strings to neuron indices for O(1) lookup.
 * No longer needs 'next' pointer as chaining is handled by nimcp_hash_table.
 *
 * INVARIANTS:
 * - index matches position in label_map array
 */
typedef struct {
    uint32_t index;
} label_index_t;

/**
 * @brief Strategy Pattern - Spike encoding function pointer
 *
 * WHY: Eliminates switch statement in hot path. Each encoding has its own
 * optimized function. New encodings can be added without modifying existing code.
 *
 * @param spike_count - Integer spike count to encode
 * @param spike_train - Output buffer
 * @param max_length - Buffer capacity
 * @return Actual spike train length
 */
typedef uint32_t (*spike_encoder_fn)(int32_t spike_count, uint8_t* spike_train,
                                     uint32_t max_length);

/**
 * @brief Strategy Pattern - Spike decoding function pointer
 *
 * @param spike_train - Input spike train
 * @param length - Train length
 * @return Decoded spike count
 */
typedef int32_t (*spike_decoder_fn)(const uint8_t* spike_train, uint32_t length);

/**
 * @brief Encoding/decoding strategy table
 *
 * WHY: O(1) dispatch to encoding function vs O(n) switch statement
 */
// W6-10 FIX: Use SPIKE_ENCODING_PASSTHROUGH as the array bound sentinel instead of
// magic number 4. PASSTHROUGH is the last encoding before the count sentinel in the
// enum, so the table has exactly the right number of slots for encodable types.
typedef struct {
    spike_encoder_fn encoders[SPIKE_ENCODING_PASSTHROUGH];
    spike_decoder_fn decoders[SPIKE_ENCODING_PASSTHROUGH];
} spike_strategy_table_t;

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal adaptive network structure
 *
 * INVARIANTS:
 * - base_network always valid after creation
 * - num_neurons == config.base_config.num_neurons
 * - neuron_states array has exactly num_neurons elements
 * - running_sparsity in range [0.0, 1.0]
 * - num_labels == label_table.num_entries
 */
struct adaptive_network_struct {
    // Base neural network
    neural_network_t base_network;

    // Configuration
    adaptive_network_config_t config;

    // Adaptive neuron states
    adaptive_neuron_state_t* neuron_states;
    uint32_t num_neurons;

    // Design pattern components
    spike_buffer_pool_t buffer_pool;
    hash_table_t* label_table;  // Using nimcp_hash_table utility
    spike_strategy_table_t strategy_table;

    // Statistics
    uint64_t total_inferences;
    uint64_t total_learning_steps;
    float running_sparsity;
    float running_inference_time_us;
    float running_learning_time_us;
    uint32_t num_pruned_synapses;

    // Threshold adaptation
    uint32_t adaptation_counter;
    float* input_statistics;  // Running statistics for adaptation

    // Label mapping for classification
    char** label_map;
    uint32_t num_labels;
    uint32_t label_map_capacity;  // M-7: Geometric growth capacity to avoid O(n^2) realloc

    // Copy-on-Write support for state snapshots (Phase COW)
    // WHY: Enable O(1) network state snapshots for training rollback
    bool uses_cow_states;                    // True if neuron states use COW
    page_cow_region_t cow_states_region;     // COW region for neuron states
    page_cow_view_t cow_states_view;         // View into COW states region

    // GPU acceleration (Phase GPU)
    // WHY: Forward pass + loss on GPU when available, CPU bio-plasticity unchanged
    struct nimcp_gpu_context_s* gpu_ctx;
    struct nimcp_gpu_weight_cache_s* gpu_weight_cache;
    bool gpu_enabled;

    // Neuron inference acceleration (Phase Inferentia)
    // WHY: NeuronCore-accelerated forward pass on AWS Inferentia hardware
    struct nimcp_neuron_inference_cache* neuron_cache;
    bool neuron_enabled;

    // Frozen network (Phase GPU-INF)
    // WHY: Disable learning and lock weights for inference-only mode
    bool frozen;

    // Gradient norm tracking
    // C-ADP-2: Use _Atomic float for proper cross-thread visibility.
    // These fields are written by learn() on training threads and read by Python
    // threads via getters. Relaxed ordering suffices for approximate monitoring values.
    // Requires -latomic on GCC/Linux (already linked).
    _Atomic float last_grad_norm;  /**< L2 norm of gradients from most recent learn step */

    // Per-layer gradient norm tracking
    // WHY: Enables layer-wise learning rate diagnostics (e.g., detecting vanishing/exploding
    // gradients in specific layers). Written by learn() alongside last_grad_norm.
    float last_layer_grad_norms[BP_MAX_GRAD_LAYERS]; /**< Per-layer L2 gradient norms */
    uint32_t num_grad_layers;                         /**< Number of valid entries */

    // EMA tracking for training stability detection
    _Atomic float ema_grad_norm;   /**< Exponential moving average of gradient norms (decay=0.99), -1 = uninitialized */
    _Atomic float ema_loss;        /**< Exponential moving average of training loss (decay=0.99), -1 = uninitialized */

    // === Probe tracking state ===
    // Per-label accuracy tracking (circular buffer of recent predictions)
    // Index by label_index from label_table. Max 2048 labels (matches ATHENA_NUM_OUTPUTS).
    struct {
        _Atomic uint32_t correct;
        _Atomic uint32_t total;
    } label_accuracy[2048];
    _Atomic uint32_t label_accuracy_size;  /**< Number of distinct labels tracked */

    // Confidence calibration (10 bins: [0-0.1), [0.1-0.2), ..., [0.9-1.0])
    struct {
        _Atomic uint32_t correct;
        _Atomic uint32_t total;
    } confidence_bins[10];

    // Learning velocity (rolling window of accuracy snapshots)
    float accuracy_history[64];
    _Atomic uint32_t accuracy_history_idx;
    _Atomic uint32_t accuracy_history_count;

    // Prediction diversity (count of each label predicted recently)
    _Atomic uint32_t recent_predictions[2048];
    _Atomic uint32_t recent_prediction_total;

    // Running accuracy EMA (fed by GPU probe tracking, read by brain_learn)
    _Atomic float running_accuracy_ema;

    // Synapse count for growth tracking
    _Atomic uint64_t prev_synapse_count;
};

//=============================================================================
// Object Pool Implementation
//=============================================================================

/**
 * @brief Initializes the spike buffer pool
 *
 * WHY: Pre-allocates all buffers at startup to avoid runtime allocations.
 * Provides deterministic performance and eliminates allocation failures.
 *
 * COMPLEXITY: O(1) - Fixed size initialization
 */
static void init_spike_buffer_pool(spike_buffer_pool_t* pool)
{
    // Guard clause: Validate input
    if (!pool)
        return;

    memset(pool->in_use, 0, sizeof(pool->in_use));
    pool->next_available = 0;
}

/**
 * @brief Acquires a spike buffer from the pool
 *
 * WHY: O(1) buffer acquisition vs O(log n) malloc. Eliminates heap
 * fragmentation and allocation failures during encoding.
 *
 * COMPLEXITY: O(1) average case
 *
 * @return Pointer to buffer or NULL if pool exhausted
 */
static uint8_t* acquire_spike_buffer(spike_buffer_pool_t* pool)
{
    // Guard clause: Validate input
    if (!pool) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pool is NULL");

        return NULL;

    }

    // Find available buffer starting from next_available
    for (uint32_t i = 0; i < MAX_POOL_BUFFERS; i++) {
        uint32_t idx = (pool->next_available + i) % MAX_POOL_BUFFERS;
        if (!pool->in_use[idx]) {
            pool->in_use[idx] = true;
            pool->next_available = (idx + 1) % MAX_POOL_BUFFERS;
            return pool->buffers[idx];
        }
    }

    return NULL;  // Pool exhausted - all buffers in use
}

/**
 * @brief Releases a spike buffer back to the pool
 *
 * WHY: Enables buffer reuse. O(1) operation maintains performance.
 *
 * COMPLEXITY: O(1)
 */
static void release_spike_buffer(spike_buffer_pool_t* pool, uint8_t* buffer)
{
    // Guard clause: Validate inputs
    if (!pool || !buffer)
        return;

    // Find and release the buffer
    for (uint32_t i = 0; i < MAX_POOL_BUFFERS; i++) {
        if (pool->buffers[i] == buffer) {
            pool->in_use[i] = false;
            return;
        }
    }
}

//=============================================================================
// Hash Table Implementation (Repository Pattern)
//=============================================================================

/**
 * @brief Initializes hash table for label storage using nimcp_hash_table utility
 *
 * WHY: Provides O(1) label lookup. Uses nimcp_hash_table with djb2 algorithm.
 * Essential for real-time performance with many labels.
 *
 * COMPLEXITY: O(1)
 */
static hash_table_t* create_label_hash_table(void)
{
    hash_table_config_t config = {.initial_buckets = HASH_TABLE_SIZE,
                                  .key_type = HASH_KEY_STRING,
                                  .hash_algorithm = HASH_ALG_DJB2,
                                  .case_insensitive = false,
                                  .value_destructor = NULL,  // label_index_t doesn't need cleanup
                                  .thread_safe = false};
    return hash_table_create(&config);
}

/**
 * @brief Looks up label in hash table using nimcp_hash_table utility
 *
 * WHY: O(1) average case lookup is critical for real-time classification.
 * Delegates to nimcp_hash_table for consistent behavior.
 *
 * COMPLEXITY: O(1) average, O(k) worst case where k = collision chain length
 *
 * @return Label index or UINT32_MAX if not found
 */
static uint32_t hash_table_lookup_label(hash_table_t* table, const char* label)
{
    if (!table || !label)
        return UINT32_MAX;

    label_index_t* entry = (label_index_t*) hash_table_lookup_string(table, label);
    return entry ? entry->index : UINT32_MAX;
}

/**
 * @brief Inserts label into hash table using nimcp_hash_table utility
 *
 * WHY: O(1) insertion enables fast label registration.
 * Delegates to nimcp_hash_table for consistent behavior.
 *
 * COMPLEXITY: O(1) average case
 *
 * @return true if inserted, false on failure
 */
static bool hash_table_insert_label(hash_table_t* table, const char* label, uint32_t index)
{
    if (!table || !label) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hash_table_insert_label: required parameter is NULL (table, label)");
        return false;
    }

    label_index_t value = {.index = index};
    return hash_table_insert_string(table, label, &value, sizeof(label_index_t));
}


//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Computes mean absolute value of input vector
 *
 * WHY: Used for adaptive threshold calculation. Single pass algorithm
 * is optimal for this calculation.
 *
 * COMPLEXITY: O(n) where n = size - single pass, cannot be improved
 *
 * @return Mean absolute value or 0.0f if invalid input
 */
static float compute_mean_abs(const float* input, uint32_t size)
{
    // Guard clause: Validate inputs
    if (!input || size == 0)
        return 0.0F;

    // C-ADP-14: Use double accumulator to prevent float precision loss
    // when summing many small values (large input vectors)
    double sum = 0.0;
    for (uint32_t i = 0; i < size; i++) {
        sum += (double)fabsf(input[i]);
    }
    return (float)(sum / size);
}

/**
 * @brief Updates running statistics using Welford's online algorithm
 *
 * WHY: Welford's algorithm is numerically stable and computes mean and
 * variance in a single pass with O(1) space complexity.
 *
 * COMPLEXITY: O(1) - Constant time update
 *
 * @param state - Neuron state to update
 * @param activation - New activation value
 */
static void update_statistics(adaptive_neuron_state_t* state, float activation)
{
    // Guard clause: Validate input
    if (!state)
        return;

    // Welford's online algorithm for mean and variance
    // Guard against uint32_t overflow: if sample_count would wrap to 0,
    // division by zero occurs. Cap at UINT32_MAX - 1.
    if (state->sample_count >= UINT32_MAX - 1) {
        return;  // Saturate — statistics are well-converged at this point
    }
    state->sample_count++;
    float delta = activation - state->activation_mean;
    state->activation_mean += delta / state->sample_count;
    float delta2 = activation - state->activation_mean;
    state->activation_variance += delta * delta2;

    // C-ADP-12: Cap Welford's running sum-of-squares to prevent float overflow
    // after billions of updates. The value is still unnormalized (divide by N to
    // get variance), but capping prevents Inf propagation.
    if (state->activation_variance > 1e30f) {
        state->activation_variance = 1e30f;
    }
}

/**
 * @brief Finds or adds label to label map with hash table acceleration
 *
 * WHY: Uses hash table for O(1) lookup instead of O(n) linear search.
 * Critical for performance with many labels.
 *
 * COMPLEXITY: O(1) average case for lookup, O(1) for insertion
 *
 * @return Label index in label_map array
 */
static uint32_t get_label_index(adaptive_network_t network, const char* label)
{
    // Guard clause: Validate inputs
    // BUG-12 fix: Return UINT32_MAX on failure instead of 0 (valid index)
    if (!network || !label)
        return UINT32_MAX;

    // O(1) hash table lookup
    uint32_t index = hash_table_lookup_label(network->label_table, label);

    // Guard clause: Return if found
    if (index != UINT32_MAX)
        return index;

    // Add new label — M-7: geometric growth to avoid O(n^2) realloc
    if (network->num_labels >= network->label_map_capacity) {
        uint32_t new_cap = network->label_map_capacity == 0
            ? 16
            : network->label_map_capacity * 2;
        // M2-FIX: Guard against uint32_t overflow in geometric doubling
        if (new_cap < network->label_map_capacity) {
            // Overflow — capacity already at max; try adding just 1
            new_cap = network->label_map_capacity + 1;
            if (new_cap == 0) return UINT32_MAX;  // truly maxed out
        }
        char** new_map = nimcp_realloc(network->label_map, new_cap * sizeof(char*));
        // Guard clause: Check allocation
        // BUG-12 fix: Return UINT32_MAX on allocation failure
        if (!new_map)
            return UINT32_MAX;
        network->label_map = new_map;
        network->label_map_capacity = new_cap;
    }
    // Use nimcp_malloc instead of strdup to match nimcp_free in free_label_map
    size_t label_len = strlen(label);
    network->label_map[network->num_labels] = nimcp_malloc(label_len + 1);
    if (!network->label_map[network->num_labels])
        return UINT32_MAX;
    strncpy(network->label_map[network->num_labels], label, label_len + 1);
    network->label_map[network->num_labels][label_len] = '\0';

    // H-2: Insert into hash table for future O(1) lookups; on failure, undo
    // the label_map entry to keep label_map and label_table in sync
    if (!hash_table_insert_label(network->label_table, network->label_map[network->num_labels],
                                 network->num_labels)) {
        nimcp_free(network->label_map[network->num_labels]);
        network->label_map[network->num_labels] = NULL;
        return UINT32_MAX;
    }

    return network->num_labels++;
}

//=============================================================================
// Strategy Pattern - Spike Encoding Implementations
//=============================================================================

/**
 * @brief Encodes spike count as direct integer (fastest encoding)
 *
 * WHY: Fastest encoding/decoding with zero compression. Used for
 * performance-critical paths where bandwidth is not a concern.
 *
 * COMPLEXITY: O(1) - Constant time copy
 */
static uint32_t encode_integer(int32_t spike_count, uint8_t* spike_train, uint32_t max_length)
{
    // Guard clause: Check buffer size
    if (max_length < sizeof(int32_t))
        return 0;

    memcpy(spike_train, &spike_count, sizeof(int32_t));
    return sizeof(int32_t);
}

/**
 * @brief Encodes spike count as binary expansion (sparse representation)
 *
 * WHY: Provides sparse representation for small spike counts.
 * Efficient for counts < 10.
 *
 * COMPLEXITY: O(n) where n = |spike_count|
 */
static uint32_t encode_binary(int32_t spike_count, uint8_t* spike_train, uint32_t max_length)
{
    uint32_t abs_count = (spike_count >= 0) ? (uint32_t)spike_count : (uint32_t)(-(spike_count + 1)) + 1u;
    uint32_t length = (abs_count < max_length) ? abs_count : max_length;
    memset(spike_train, 1, length);
    return length;
}

/**
 * @brief Encodes spike count as ternary (-1, 0, +1)
 *
 * WHY: Most compressed encoding for simple sign information.
 * Used when only direction matters, not magnitude.
 *
 * COMPLEXITY: O(1) - Single byte encoding
 */
static uint32_t encode_ternary(int32_t spike_count, uint8_t* spike_train, uint32_t max_length)
{
    // Guard clause: Check buffer
    if (max_length < 1)
        return 0;

    spike_train[0] = (spike_count > 0) ? 1 : ((spike_count < 0) ? 255 : 0);
    return 1;
}

/**
 * @brief Encodes spike count as bitwise representation
 *
 * WHY: Balanced compression for medium spike counts.
 * Good for counts in range [10, 1000].
 *
 * COMPLEXITY: O(log n) where n = spike_count
 */
static uint32_t encode_bitwise(int32_t spike_count, uint8_t* spike_train, uint32_t max_length)
{
    uint32_t abs_count = (spike_count >= 0) ? (uint32_t)spike_count : (uint32_t)(-(spike_count + 1)) + 1u;
    uint32_t bytes_needed = (abs_count + 7) / 8;

    // Guard clause: Check capacity
    if (bytes_needed > max_length)
        bytes_needed = max_length;

    // Additional guard: Prevent shift overflow (max 4 bytes for uint32_t)
    if (bytes_needed > 4)
        bytes_needed = 4;

    for (uint32_t i = 0; i < bytes_needed; i++) {
        spike_train[i] = (abs_count >> (i * 8)) & 0xFF;
    }
    return bytes_needed;
}

/**
 * @brief Decodes integer-encoded spike train
 *
 * COMPLEXITY: O(1)
 */
static int32_t decode_integer(const uint8_t* spike_train, uint32_t length)
{
    // Guard clause: Check minimum length
    if (length < sizeof(int32_t))
        return 0;

    int32_t spike_count = 0;
    memcpy(&spike_count, spike_train, sizeof(int32_t));
    return spike_count;
}

/**
 * @brief Decodes binary-encoded spike train
 *
 * COMPLEXITY: O(n) where n = length
 */
static int32_t decode_binary(const uint8_t* spike_train, uint32_t length)
{
    int32_t spike_count = 0;
    for (uint32_t i = 0; i < length; i++) {
        spike_count += spike_train[i];
    }
    return spike_count;
}

/**
 * @brief Decodes ternary-encoded spike train
 *
 * COMPLEXITY: O(1)
 */
static int32_t decode_ternary(const uint8_t* spike_train, uint32_t length)
{
    // Guard clause: Check length
    if (length < 1)
        return 0;

    return (spike_train[0] == 1) ? 1 : ((spike_train[0] == 255) ? -1 : 0);
}

/**
 * @brief Decodes bitwise-encoded spike train
 *
 * COMPLEXITY: O(log n) where n = spike_count
 */
static int32_t decode_bitwise(const uint8_t* spike_train, uint32_t length)
{
    // M-2: Clamp to 4 bytes to prevent undefined behavior from shifting
    // beyond the width of int32_t (32 bits = 4 bytes max)
    uint32_t limit = (length < 4) ? length : 4;
    int32_t spike_count = 0;
    for (uint32_t i = 0; i < limit; i++) {
        spike_count |= ((uint32_t)spike_train[i] << (i * 8));
    }
    return spike_count;
}

/**
 * @brief Initializes spike encoding/decoding strategy table
 *
 * WHY: Replaces switch statement with O(1) function pointer dispatch.
 * Follows Open/Closed Principle - new encodings can be added without
 * modifying existing code.
 *
 * COMPLEXITY: O(1)
 */
static void init_spike_strategy_table(spike_strategy_table_t* table)
{
    // Guard clause: Validate input
    if (!table)
        return;

    // Register encoding strategies
    table->encoders[SPIKE_ENCODING_INTEGER] = encode_integer;
    table->encoders[SPIKE_ENCODING_BINARY] = encode_binary;
    table->encoders[SPIKE_ENCODING_TERNARY] = encode_ternary;
    table->encoders[SPIKE_ENCODING_BITWISE] = encode_bitwise;

    // Register decoding strategies
    table->decoders[SPIKE_ENCODING_INTEGER] = decode_integer;
    table->decoders[SPIKE_ENCODING_BINARY] = decode_binary;
    table->decoders[SPIKE_ENCODING_TERNARY] = decode_ternary;
    table->decoders[SPIKE_ENCODING_BITWISE] = decode_bitwise;
}

//=============================================================================
// Adaptive Threshold Functions
//=============================================================================

/**
 * @brief Computes adaptive threshold for input vector
 *
 * WHY: Adaptive thresholds enable sparse activation by adjusting to input
 * statistics. Formula: V_th(x) = (1/k) × mean(|x|)
 *
 * COMPLEXITY: O(n) where n = size - Single pass through input
 *
 * @param input - Input vector
 * @param size - Vector size
 * @param k_factor - Firing rate control (higher = more sparsity)
 * @return Adaptive threshold value
 */
float adaptive_compute_threshold(const float* input, uint32_t size, float k_factor)
{
    // Guard clause: Validate inputs
    if (!input || size == 0)
        return 1.0F;

    // Guard clause: Validate k_factor
    if (k_factor <= 0.0F)
        return 1.0F;

    float mean_abs = compute_mean_abs(input, size);
    return mean_abs / k_factor;
}

/**
 * @brief Converts continuous value to integer spike count
 *
 * WHY: Quantizes continuous values to integer spikes for efficient
 * computation. Formula: s_INT = round(x / V_th(x))
 *
 * COMPLEXITY: O(1) - Constant time operation
 *
 * @param value - Continuous input value
 * @param threshold - Adaptive threshold
 * @return Integer spike count
 */
int32_t adaptive_value_to_spikes(float value, float threshold)
{
    // Guard clause: Ensure valid threshold
    if (threshold <= 0.0F)
        threshold = 1.0F;

    return (int32_t) roundf(value / threshold);
}

/**
 * @brief Encodes spike count using strategy pattern (no switch statement)
 *
 * WHY: Uses function pointer dispatch for O(1) encoding selection.
 * Eliminates switch statement and makes code extensible.
 *
 * COMPLEXITY: O(1) dispatch + encoding complexity (varies by type)
 *
 * @return Actual spike train length or 0 on error
 */
uint32_t adaptive_encode_spikes(int32_t spike_count, spike_encoding_t encoding,
                                uint8_t* spike_train, uint32_t max_length)
{
    // Guard clause: Validate inputs
    if (!spike_train || max_length == 0)
        return 0;

    // Guard clause: Validate encoding type
    if (encoding >= SPIKE_ENCODING_PASSTHROUGH)
        return 0;

    // BUG-13: Known performance issue — strategy table is rebuilt per encode call.
    // TODO: Cache the strategy table (e.g., static or pass from caller) to avoid
    // redundant initialization on every invocation.
    spike_strategy_table_t table;
    init_spike_strategy_table(&table);

    // O(1) function pointer dispatch
    spike_encoder_fn encoder = table.encoders[encoding];

    // Guard clause: Check encoder exists
    if (!encoder)
        return 0;

    return encoder(spike_count, spike_train, max_length);
}

/**
 * @brief Decodes spike train back to continuous value using strategy pattern
 *
 * WHY: Uses function pointer dispatch instead of switch statement.
 * Maintains symmetry with encoding for clean design.
 *
 * COMPLEXITY: O(1) dispatch + decoding complexity (varies by type)
 *
 * @return Decoded value or 0.0f on error
 */
float adaptive_decode_spikes(const uint8_t* spike_train, uint32_t length, spike_encoding_t encoding,
                             float threshold)
{
    // Guard clause: Validate inputs
    if (!spike_train || length == 0)
        return 0.0F;

    // Guard clause: Validate encoding type
    if (encoding >= SPIKE_ENCODING_PASSTHROUGH)
        return 0.0F;

    // Create temporary strategy table
    spike_strategy_table_t table;
    init_spike_strategy_table(&table);

    // O(1) function pointer dispatch
    spike_decoder_fn decoder = table.decoders[encoding];

    // Guard clause: Check decoder exists
    if (!decoder)
        return 0.0F;

    int32_t spike_count = decoder(spike_train, length);
    return spike_count * threshold;
}

//=============================================================================
// Factory Pattern - Network Creation Helper Functions
//=============================================================================

/**
 * @brief Validates network configuration
 *
 * WHY: Extracts validation logic to reduce creation function complexity.
 * Single responsibility - only validates configuration parameters.
 *
 * COMPLEXITY: O(1)
 *
 * @return true if valid, false otherwise
 */
static bool validate_network_config(const adaptive_network_config_t* config)
{
    // Guard clause: Check config pointer
    if (!config) {
        fprintf(stderr, "[ERROR] validate_network_config: NULL config\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_network_config: config is NULL");
        return false;
    }

    // Guard clause: Validate base config layer_sizes
    // WHY: NULL layer_sizes will cause crash during memcpy or neural_network_create
    if (config->base_config.num_layers > 0 && !config->base_config.layer_sizes) {
        fprintf(stderr, "[ERROR] validate_network_config: num_layers=%u but layer_sizes is NULL\n",
                config->base_config.num_layers);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "validate_network_config: config->base_config is NULL");
        return false;
    }

    // Guard clause: Validate spike parameters
    if (config->spike_params.k_factor <= 0.0F) {
        fprintf(stderr, "[ERROR] validate_network_config: invalid k_factor=%f\n", config->spike_params.k_factor);
        return false;
    }
    if (config->spike_params.min_threshold <= 0.0F) {
        fprintf(stderr, "[ERROR] validate_network_config: invalid min_threshold=%f\n", config->spike_params.min_threshold);
        return false;
    }
    if (config->spike_params.max_threshold <= config->spike_params.min_threshold) {
        fprintf(stderr, "[ERROR] validate_network_config: max_threshold=%f <= min_threshold=%f\n",
                config->spike_params.max_threshold, config->spike_params.min_threshold);
        return false;
    }
    if (config->spike_params.sparsity_target < 0.0F || config->spike_params.sparsity_target > 1.0F) {
        fprintf(stderr, "[ERROR] validate_network_config: invalid sparsity_target=%f\n", config->spike_params.sparsity_target);
        return false;
    }

    return true;
}

/**
 * @brief Initializes neuron states with default values
 *
 * WHY: Extracted from creation to reduce nesting. Single pass initialization.
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
static void initialize_neuron_states(adaptive_neuron_state_t* states, uint32_t num_neurons,
                                     float default_threshold)
{
    // Guard clause: Validate input
    if (!states)
        return;

    for (uint32_t i = 0; i < num_neurons; i++) {
        states[i].adaptive_threshold = default_threshold;
        states[i].membrane_potential = 0.0F;
        states[i].spike_count = 0;
        states[i].spike_train = NULL;
        states[i].spike_train_length = 0;
        states[i].activation_mean = 0.0F;
        states[i].activation_variance = 0.0F;
        states[i].sample_count = 0;
    }
}

/**
 * @brief Allocates neuron state array
 *
 * WHY: Extracted allocation for clarity and error handling.
 *
 * COMPLEXITY: O(1) allocation + O(n) initialization
 */
static adaptive_neuron_state_t* allocate_neuron_states(uint32_t num_neurons,
                                                       float default_threshold)
{
    adaptive_neuron_state_t* states = nimcp_calloc(num_neurons, sizeof(adaptive_neuron_state_t));

    // Guard clause: Check allocation
    if (!states) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "states is NULL");

        return NULL;

    }

    initialize_neuron_states(states, num_neurons, default_threshold);
    return states;
}

/**
 * @brief Initializes design pattern components
 *
 * WHY: Extracted initialization of object pool, hash table, and strategy table.
 * Single responsibility - only initializes design patterns.
 *
 * COMPLEXITY: O(1)
 */
static void initialize_design_patterns(adaptive_network_t network)
{
    // Guard clause: Validate input
    if (!network)
        return;

    init_spike_buffer_pool(&network->buffer_pool);
    network->label_table = create_label_hash_table();
    init_spike_strategy_table(&network->strategy_table);
}

//=============================================================================
// Adaptive Network Creation/Destruction
//=============================================================================

/**
 * @brief Creates adaptive neural network (Factory Pattern)
 *
 * WHY: Factory function that orchestrates network creation using multiple
 * helper functions. Each step is extracted to a separate function to eliminate
 * nesting and improve testability.
 *
 * COMPLEXITY: O(n) where n = num_neurons (for state initialization)
 *
 * @param config - Network configuration
 * @return Network handle or NULL on error
 */
adaptive_network_t adaptive_network_create(const adaptive_network_config_t* config)
{

    // Guard clause: Validate configuration
    // C-ADP-13: Use INVALID_PARAM (not NO_MEMORY) — config validation failure
    // is a parameter error, not an allocation failure
    if (!validate_network_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_create: invalid network configuration");
        return NULL;
    }

    // W6-12 FIX: Guard against zero input_size which causes implementation-defined
    // calloc(0) when allocating input_statistics buffer. Enforce minimum of 1.
    if (config->base_config.input_size == 0) {
        fprintf(stderr, "[ERROR] adaptive_network_create: input_size is 0\n");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_create: input_size must be > 0");
        return NULL;
    }

    // Auto-load from checkpoint if enabled (default behavior)
    // WHY: Allow seamless continuation of training from saved state
    // M-6: Removed TOCTOU race (fopen→fclose→load) — call load directly
    //       and handle NULL return (file absent OR corrupt)
    if (config->checkpoint_path && config->auto_load) {
        adaptive_network_t loaded_network = adaptive_network_load(config->checkpoint_path);
        if (loaded_network) {
            return loaded_network;
        }
        // Load returned NULL — file doesn't exist or is corrupt.
        // Fall through to create a fresh network.
    }

    // Allocate network structure
    adaptive_network_t network = nimcp_calloc(1, sizeof(struct adaptive_network_struct));

    // Guard clause: Check allocation
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "network is NULL");

        return NULL;
    }

    // Copy configuration (shallow copy first)
    memcpy(&network->config, config, sizeof(adaptive_network_config_t));

    // M-3: Null out the shallow-copied checkpoint_path pointer to avoid dangling reference.
    // The caller's config may be stack-allocated, so our shallow copy would point at freed memory.
    network->config.checkpoint_path = NULL;

    // Initialize COW fields to NULL/false (Phase COW)
    network->uses_cow_states = false;
    network->cow_states_region = NULL;
    network->cow_states_view = NULL;

    // Initialize EMA tracking fields (-1 = uninitialized, first sample will seed)
    network->ema_grad_norm = -1.0f;
    network->ema_loss = -1.0f;

    // Probe tracking fields: all zero-initialized by nimcp_calloc above.
    // label_accuracy[], confidence_bins[], accuracy_history[], recent_predictions[]
    // all start at zero which is the correct initial state.

    // Deep copy layer_sizes array to avoid dangling pointer
    // WHY: Config may be stack-allocated by caller, so we need our own copy
    // WHAT: Allocate and copy the layer_sizes array if present
    if (config->base_config.num_layers > 0 && config->base_config.layer_sizes) {
        uint32_t* layer_sizes_copy =
            nimcp_calloc(config->base_config.num_layers, sizeof(uint32_t));
        // Guard clause: Check allocation
        if (!layer_sizes_copy) {
            nimcp_free(network);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_network_create: layer_sizes_copy is NULL");
            return NULL;
        }
        memcpy(layer_sizes_copy, config->base_config.layer_sizes,
               config->base_config.num_layers * sizeof(uint32_t));
        network->config.base_config.layer_sizes = layer_sizes_copy;
    } else {
        network->config.base_config.layer_sizes = NULL;
    }

    // Initialize design pattern components
    initialize_design_patterns(network);

    // NOTE: Residual/skip connections require dense projection matrices between
    // mismatched layers. At 2M neurons (200K+ per layer), each projection would
    // need hundreds of GB — infeasible. Left disabled.

    // Create base neural network using our deep-copied config
    // WHY: Use network->config not original config, since we deep-copied layer_sizes
    // WHAT: Pass address of our config which has the corrected layer_sizes pointer
    network->base_network = neural_network_create(&network->config.base_config);
    // Guard clause: Check base network creation
    if (!network->base_network) {
        if (network->config.base_config.layer_sizes) {
            nimcp_free((void*)network->config.base_config.layer_sizes);
        }
        // M-2: Free hash table allocated in initialize_design_patterns() to prevent leak
        if (network->label_table) {
            hash_table_destroy(network->label_table);
        }
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_create: validation failed");
        return NULL;
    }

    // Get number of neurons from base network
    network->num_neurons = config->base_config.num_neurons;

    // Allocate and initialize neuron states
    float default_threshold = config->spike_params.min_threshold;
    network->neuron_states = allocate_neuron_states(network->num_neurons, default_threshold);
    // Guard clause: Check neuron states allocation
    if (!network->neuron_states) {
        neural_network_destroy(network->base_network);
        if (network->config.base_config.layer_sizes) {
            nimcp_free((void*)network->config.base_config.layer_sizes);
        }
        // H-4: Free hash table allocated in initialize_design_patterns() to prevent leak
        if (network->label_table) {
            hash_table_destroy(network->label_table);
        }
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_create: validation failed");
        return NULL;
    }

    // Allocate input statistics buffer
    network->input_statistics = nimcp_calloc(config->base_config.input_size, sizeof(float));
    // Guard clause: Check statistics buffer
    if (!network->input_statistics) {
        nimcp_free(network->neuron_states);
        neural_network_destroy(network->base_network);
        if (network->config.base_config.layer_sizes) {
            nimcp_free((void*)network->config.base_config.layer_sizes);
        }
        // H-4: Free hash table allocated in initialize_design_patterns() to prevent leak
        if (network->label_table) {
            hash_table_destroy(network->label_table);
        }
        nimcp_free(network);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_create: validation failed");
        return NULL;
    }

    // Initialize label map
    network->label_map = NULL;
    network->num_labels = 0;
    network->label_map_capacity = 0;

    // Phase GPU-INF: Frozen state init
    network->frozen = false;

    // Phase GPU: Initialize GPU acceleration (GPU-default, CPU fallback)
    network->gpu_enabled = false;
    network->gpu_ctx = NULL;
    network->gpu_weight_cache = NULL;
    if (gpu_is_available() &&
        network->config.base_config.num_layers >= 2 &&
        network->config.base_config.layer_sizes) {
        network->gpu_ctx = nimcp_gpu_context_create_auto();
        if (network->gpu_ctx) {
            network->gpu_weight_cache = nimcp_gpu_weight_cache_create(
                network->gpu_ctx,
                network->base_network,
                network->config.base_config.layer_sizes,
                network->config.base_config.num_layers);
            if (network->gpu_weight_cache) {
                nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                             network->base_network);
                network->gpu_enabled = true;
                fprintf(stderr, "[GPU] Training enabled for adaptive network (%u layers)\n",
                        network->config.base_config.num_layers);
                NIMCP_LOG_INFO("GPU training enabled for adaptive network (%u layers)",
                               network->config.base_config.num_layers);
            } else {
                nimcp_gpu_context_destroy(network->gpu_ctx);
                network->gpu_ctx = NULL;
            }
        }
    }

    return network;
}

/**
 * @brief Frees neuron state spike trains
 *
 * WHY: Extracted cleanup logic. Single responsibility.
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
static void free_neuron_spike_trains(adaptive_neuron_state_t* states, uint32_t num_neurons)
{
    // Guard clause: Validate input
    if (!states)
        return;

    for (uint32_t i = 0; i < num_neurons; i++) {
        if (states[i].spike_train) {
            nimcp_free(states[i].spike_train);
            states[i].spike_train = NULL;
        }
    }
}

/**
 * @brief Frees label map strings
 *
 * WHY: Extracted cleanup logic for labels.
 *
 * COMPLEXITY: O(n) where n = num_labels
 */
static void free_label_map(char** label_map, uint32_t num_labels)
{
    // Guard clause: Validate input
    if (!label_map)
        return;

    for (uint32_t i = 0; i < num_labels; i++) {
        nimcp_free(label_map[i]);
    }
    nimcp_free(label_map);
}

/**
 * @brief Destroys adaptive neural network and frees all resources
 *
 * WHY: Ensures no memory leaks. Uses helper functions to avoid nesting.
 *
 * COMPLEXITY: O(n + m) where n = num_neurons, m = num_labels
 *
 * @param network - Network to destroy
 */
void adaptive_network_destroy(adaptive_network_t network)
{
    // Guard clause: Validate input
    if (!network)
        return;
    LOG_MODULE_DEBUG(LOG_MODULE, "adaptive_network_destroy: start");

    // Phase Inferentia: Clean up Neuron inference cache
    if (network->neuron_cache) {
        nimcp_neuron_cache_destroy(network->neuron_cache);
        network->neuron_cache = NULL;
    }
    LOG_MODULE_DEBUG(LOG_MODULE, "Neuron cache destroyed");

    // Phase GPU: Clean up GPU resources before base network
    if (network->gpu_weight_cache) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Destroying GPU weight cache...");
        nimcp_gpu_weight_cache_destroy(network->gpu_weight_cache);
        network->gpu_weight_cache = NULL;
    }
    if (network->gpu_ctx) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Destroying GPU context...");
        nimcp_gpu_context_destroy(network->gpu_ctx);
        network->gpu_ctx = NULL;
    }
    LOG_MODULE_DEBUG(LOG_MODULE, "GPU resources destroyed");

    // Destroy base network
    if (network->base_network) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Destroying base network...");
        neural_network_destroy(network->base_network);
        LOG_MODULE_DEBUG(LOG_MODULE, "Base network destroyed");
    }

    // Handle COW cleanup (Phase COW)
    if (network->uses_cow_states) {
        if (network->cow_states_view) {
            page_cow_view_destroy(network->cow_states_view);
        }
        if (network->cow_states_region) {
            page_cow_region_destroy(network->cow_states_region);
        }
    }

    // Free neuron states and their spike trains
    LOG_MODULE_DEBUG(LOG_MODULE, "Freeing neuron states (%u)...", network->num_neurons);
    if (network->neuron_states) {
        free_neuron_spike_trains(network->neuron_states, network->num_neurons);
        nimcp_free(network->neuron_states);
    }
    LOG_MODULE_DEBUG(LOG_MODULE, "Neuron states freed");

    // Free input statistics buffer
    nimcp_free(network->input_statistics);

    // Free label map
    if (network->label_map) {
        free_label_map(network->label_map, network->num_labels);
    }

    // Destroy hash table
    if (network->label_table) {
        hash_table_destroy(network->label_table);
    }

    // Free layer_sizes array (deep copy allocated in create)
    // WHY: We own this memory after deep copy in adaptive_network_create
    if (network->config.base_config.layer_sizes) {
        nimcp_free((void*)network->config.base_config.layer_sizes);
    }

    nimcp_free(network);
}

//=============================================================================
// Forward Pass Helper Functions
//=============================================================================

/**
 * @brief Converts input to spike representation
 *
 * WHY: Extracted spike conversion logic. Single responsibility.
 *
 * COMPLEXITY: O(n) where n = input_size
 *
 * @return Spike-encoded input (caller must free)
 */
static float* convert_input_to_spikes(const float* input, uint32_t input_size, float threshold,
                                      spike_encoding_t encoding)
{
    // Phase MP: Allocate from pool for hot path performance
    float* spike_input = (float*)alloc_hot_buffer(input_size * sizeof(float));
    // Guard clause: Check allocation
    if (!spike_input) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "spike_input is NULL");

        return NULL;

    }

    // Guard clause: Use integer encoding only
    if (encoding == SPIKE_ENCODING_INTEGER) {
        for (uint32_t i = 0; i < input_size; i++) {
            spike_input[i] = adaptive_value_to_spikes(input[i], threshold);
        }
        return spike_input;
    }

    // C-ADP-3: Handle BINARY, TERNARY, BITWISE encodings explicitly instead of
    // silently falling through to PASSTHROUGH copy.
    if (encoding == SPIKE_ENCODING_BINARY) {
        for (uint32_t i = 0; i < input_size; i++) {
            spike_input[i] = (input[i] > threshold) ? 1.0f : 0.0f;
        }
        return spike_input;
    }

    if (encoding == SPIKE_ENCODING_TERNARY) {
        for (uint32_t i = 0; i < input_size; i++) {
            spike_input[i] = (input[i] > threshold) ? 1.0f
                           : (input[i] < -threshold) ? -1.0f
                           : 0.0f;
        }
        return spike_input;
    }

    if (encoding == SPIKE_ENCODING_BITWISE) {
        // Bitwise encoding maps to binary thresholding for float inputs.
        // The integer bitwise encoder operates on spike counts, but for
        // raw float inputs we use the same binary threshold logic.
        for (uint32_t i = 0; i < input_size; i++) {
            spike_input[i] = (input[i] > threshold) ? 1.0f : 0.0f;
        }
        return spike_input;
    }

    // SPIKE_ENCODING_PASSTHROUGH: Copy raw floats directly.
    // This is the default for gradient training (set in brain init).
    memcpy(spike_input, input, input_size * sizeof(float));
    return spike_input;
}

/**
 * @brief Clamps threshold within valid range
 *
 * WHY: Extracted clamping logic. Ensures invariant: threshold in [min, max]
 *
 * COMPLEXITY: O(1)
 *
 * @return Clamped threshold value
 */
static float clamp_threshold(float threshold, float min_threshold, float max_threshold)
{
    if (threshold < min_threshold)
        return min_threshold;
    if (threshold > max_threshold)
        return max_threshold;
    return threshold;
}

/**
 * @brief Adapts neuron threshold based on sparsity target
 *
 * WHY: Extracted adaptation logic. Single responsibility - only adjusts threshold.
 *
 * COMPLEXITY: O(1)
 *
 * @param state - Neuron state to adapt
 * @param current_sparsity - Network's current sparsity
 * @param target_sparsity - Desired sparsity level
 * @param min_threshold - Minimum allowed threshold
 * @param max_threshold - Maximum allowed threshold
 */
static void adapt_neuron_threshold(adaptive_neuron_state_t* state, float current_sparsity,
                                   float target_sparsity, float min_threshold, float max_threshold)
{
    // Guard clause: Validate state
    if (!state)
        return;

    // Adjust threshold based on sparsity
    if (current_sparsity < target_sparsity) {
        // Too many neurons firing, increase threshold
        state->adaptive_threshold *= SPARSITY_ADAPT_INCREASE;
    } else {
        // Too few neurons firing, decrease threshold
        state->adaptive_threshold *= SPARSITY_ADAPT_DECREASE;
    }

    // Clamp to valid range
    state->adaptive_threshold =
        clamp_threshold(state->adaptive_threshold, min_threshold, max_threshold);
}

/**
 * @brief Processes single neuron output with thresholding
 *
 * WHY: Extracted neuron processing logic. Eliminates nested if statements.
 * Single responsibility - processes one neuron.
 *
 * COMPLEXITY: O(1)
 *
 * @param state - Neuron state
 * @param output_value - Neuron output value (modified in place)
 * @param enable_sparsity - Whether to enforce sparsity
 * @return true if neuron is active, false otherwise
 */
static bool process_neuron_output(adaptive_neuron_state_t* state, float* output_value,
                                  bool enable_sparsity)
{
    // Guard clause: Validate inputs
    if (!state || !output_value) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "process_neuron_output: required parameter is NULL (state, output_value)");
        return false;
    }

    bool is_active = (fabsf(*output_value) > state->adaptive_threshold);

    // Apply sparsity if enabled
    if (enable_sparsity && !is_active) {
        *output_value = 0.0F;
        return false;
    }

    // Update spike count
    state->spike_count = adaptive_value_to_spikes(*output_value, state->adaptive_threshold);

    return is_active;
}

/**
 * @brief Processes all neuron outputs and applies adaptive thresholding
 *
 * WHY: Extracted output processing loop. No nested conditions - uses helper functions.
 * Single pass through neurons maintains O(n) complexity.
 *
 * COMPLEXITY: O(n) where n = output_size
 *
 * @return Number of active neurons
 */
/**
 * BUG-11 fix: Added `readonly` parameter so the forward_readonly path can call
 * this without casting away const. When readonly=true, statistics updates and
 * threshold adaptation are skipped (they mutate neuron_states).
 */
static uint32_t process_network_outputs_impl(adaptive_network_t network, float* output,
                                             uint32_t output_size, bool readonly)
{
    // Guard clause: Validate inputs
    if (!network || !output)
        return 0;

    uint32_t active_count = 0;
    const adaptive_spike_params_t* params = &network->config.spike_params;

    // Single pass through neurons - O(n)
    uint32_t num_to_process =
        (output_size < network->num_neurons) ? output_size : network->num_neurons;

    /* Output layer neurons should NOT be sparsified.
     * The output layer produces a dense embedding (e.g., 4096-dim BGE target).
     * Sparsifying it forces 95% to zero, causing mode collapse.
     * Only apply sparsity to hidden layer neurons. */
    uint32_t output_layer_start = 0;
    if (output_size > network->config.base_config.output_size) {
        output_layer_start = output_size - network->config.base_config.output_size;
    }
    {
        static int _diag_count = 0;
        if (_diag_count++ < 3) {
            fprintf(stderr, "[DIAG-SPARSE] output_size=%u base_output=%u output_layer_start=%u enable_sparsity=%d readonly=%d\n",
                    output_size, network->config.base_config.output_size,
                    output_layer_start, network->config.enable_sparsity, readonly);
        }
    }

    for (uint32_t i = 0; i < num_to_process; i++) {
        adaptive_neuron_state_t* state = &network->neuron_states[i];
        bool is_output_neuron = (i >= output_layer_start);

        if (!readonly) {
            // Update statistics (O(1))
            update_statistics(state, output[i]);

            // Adapt threshold if enabled (O(1)) — skip for output neurons
            if (params->enable_adaptation && !is_output_neuron) {
                adapt_neuron_threshold(state, network->running_sparsity, params->sparsity_target,
                                       params->min_threshold, params->max_threshold);
            }
        }

        // M-6: In readonly mode, check threshold without mutating spike_count.
        if (readonly) {
            bool is_active = (fabsf(output[i]) > state->adaptive_threshold);
            if (network->config.enable_sparsity && !is_active && !is_output_neuron) {
                output[i] = 0.0F;
            } else if (is_active || is_output_neuron) {
                active_count++;
            }
        } else {
            if (is_output_neuron) {
                // Output neurons: always active, no sparsity
                active_count++;
            } else {
                // Hidden neurons: normal sparsity enforcement
                bool is_active = process_neuron_output(state, &output[i], network->config.enable_sparsity);
                if (is_active) {
                    active_count++;
                }
            }
        }
    }

    return active_count;
}

/* Backward-compatible wrapper for non-readonly callers */
static uint32_t process_network_outputs(adaptive_network_t network, float* output,
                                        uint32_t output_size)
{
    return process_network_outputs_impl(network, output, output_size, false);
}

/**
 * @brief Updates network's running sparsity estimate
 *
 * WHY: Extracted sparsity update. Uses exponential moving average for stability.
 *
 * COMPLEXITY: O(1)
 */
static void update_running_sparsity(adaptive_network_t network, uint32_t active_count,
                                    uint32_t output_size)
{
    // Guard clause: Validate inputs
    if (!network || output_size == 0)
        return;

    float current_sparsity = 1.0F - ((float) active_count / output_size);
    network->running_sparsity = (1.0F - SPARSITY_EMA_WEIGHT) * network->running_sparsity +
                                SPARSITY_EMA_WEIGHT * current_sparsity;
}

//=============================================================================
// Forward Pass with Adaptive Spiking
//=============================================================================

/**
 * @brief Processes input through adaptive network (main forward pass)
 *
 * WHY: Refactored from 90+ line function with nested ifs to clean orchestration.
 * Each step is a single function call with clear purpose. No nested loops or
 * nested ifs - only guard clauses.
 *
 * ALGORITHM:
 * 1. Validate inputs (O(1))
 * 2. Compute adaptive threshold for input (O(n))
 * 3. Convert input to spikes if needed (O(n))
 * 4. Forward pass through base network (O(n))
 * 5. Process outputs with adaptive thresholding (O(n))
 * 6. Update running sparsity statistics (O(1))
 *
 * COMPLEXITY: O(n) where n = max(input_size, output_size)
 *             Single pass through data - cannot be improved
 *
 * @return Number of active neurons (for sparsity tracking)
 */
uint32_t adaptive_network_forward(adaptive_network_t network, const float* input,
                                  uint32_t input_size, float* output, uint32_t output_size,
                                  uint64_t timestamp)
{
    // Guard clause: Validate inputs
    if (!network || !input || !output)
        return 0;

    /* Diagnostics removed — NeuronCore was the culprit */

    // Phase Inferentia: DISABLED — NeuronCore stub returns success with
    // zero output on non-AWS hardware, hijacking inference and producing
    // all-zero decide_full results. GPU/CPU paths work correctly.
    // TODO: Re-enable when running on actual AWS Inferentia hardware.

    // Phase GPU: GPU-accelerated forward pass
    if (network->gpu_enabled && network->gpu_weight_cache) {
        // Re-upload weights if CPU biological learning modified them
        if (__atomic_load_n(&network->gpu_weight_cache->weights_dirty_on_cpu, __ATOMIC_ACQUIRE)) {
            nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                         network->base_network);
        }

        // Use same fixed threshold as training path (adaptive_network_learn)
        // to ensure train/test consistency. Adaptive threshold can be higher
        // than input values, killing all spikes and producing zero output.
        float gpu_threshold = network->config.spike_params.min_threshold;
        if (gpu_threshold <= 0.0f) gpu_threshold = 0.1f;

        float* spike_input = convert_input_to_spikes(input, input_size, gpu_threshold,
                                                     network->config.spike_params.encoding);
        if (!spike_input) return 0;

        // I-C2: Check GPU forward return value — fall through to CPU on failure
        bool gpu_ok = nimcp_gpu_forward_pass(network->gpu_weight_cache,
                                             spike_input, input_size, output, output_size);

        free_hot_buffer(spike_input);

        {
            static int s_gd = 0;
            if (s_gd++ < 5) {
                uint32_t nz = 0; float mx = 0;
                for (uint32_t di = 0; di < output_size && di < 100; di++) {
                    if (fabsf(output[di]) > 1e-6f) nz++;
                    if (fabsf(output[di]) > mx) mx = fabsf(output[di]);
                }
                uint32_t nz_all = 0;
                for (uint32_t di = 0; di < output_size; di++) {
                    if (fabsf(output[di]) > 1e-6f) nz_all++;
                }
                fprintf(stderr, "[DIAG-GPU] ok=%d nz(100)=%u nz(all)=%u/%u max=%.6f dirty=%d layers=%u\n",
                        gpu_ok, nz, nz_all, output_size, mx,
                        (int)network->gpu_weight_cache->weights_dirty_on_cpu,
                        network->gpu_weight_cache->num_layers);
                fflush(stderr);
            }
        }

        if (gpu_ok) {
            uint32_t active_count = process_network_outputs(network, output, output_size);
            update_running_sparsity(network, active_count, output_size);
            network->total_inferences++;
            return active_count;
        }
        // GPU failed — fall through to CPU path
    }

    // CPU fallback path
    {
        static int s_cpu_warn = 0;
        if (s_cpu_warn++ < 3)
            fprintf(stderr, "[INF-PATH] CPU fallback: gpu_enabled=%d cache=%p in=%u out=%u\n",
                    network->gpu_enabled, (void*)network->gpu_weight_cache,
                    input_size, output_size);
    }

    // Step 1: Use same fixed threshold as training path for consistency
    float input_threshold = network->config.spike_params.min_threshold;
    if (input_threshold <= 0.0f) input_threshold = 0.1f;

    // Step 2: Convert input to spikes if needed
    float* spike_input = convert_input_to_spikes(input, input_size, input_threshold,
                                                 network->config.spike_params.encoding);

    // Guard clause: Check spike input allocation
    if (!spike_input)
        return 0;

    // Step 3: Forward pass through base network
    neural_network_forward(network->base_network, spike_input, input_size, output, output_size);

    free_hot_buffer(spike_input);  // Phase MP: Return to pool

    // Step 4: Process outputs with adaptive thresholding
    uint32_t active_count = process_network_outputs(network, output, output_size);

    // Step 5: Update running sparsity
    update_running_sparsity(network, active_count, output_size);

    // Update statistics
    network->total_inferences++;

    return active_count;
}

/**
 * @brief Read-only forward pass (Phase 3: COW-safe inference)
 *
 * WHAT: Identical to adaptive_network_forward() but without statistics updates
 * WHY:  Enables multiple brains to share network indefinitely during inference
 * HOW:  Skips update_running_sparsity() and total_inferences++
 *
 * PERFORMANCE: Same as forward() - O(s*n) where s=sparsity, n=neurons
 * THREAD SAFETY: Read-only - safe for concurrent access
 *
 * @param network Adaptive network (const - not modified)
 * @param input Input vector
 * @param input_size Input dimension
 * @param output Output buffer
 * @param output_size Output dimension
 * @param timestamp Current time (unused in read-only mode)
 * @return Number of active neurons
 */
uint32_t adaptive_network_forward_readonly(const adaptive_network_t network, const float* input,
                                           uint32_t input_size, float* output, uint32_t output_size,
                                           uint64_t timestamp)
{
    // Guard clause: Validate inputs
    if (!network || !input || !output)
        return 0;

    // Phase Inferentia: DISABLED (see mutable path comment)

    // Phase GPU: GPU-accelerated forward pass (read-only — no statistics update)
    // H-1: In readonly path, don't upload weights (that mutates GPU state).
    //       Fall through to CPU path if weights are dirty on CPU.
    if (network->gpu_enabled && network->gpu_weight_cache &&
        !__atomic_load_n(&network->gpu_weight_cache->weights_dirty_on_cpu, __ATOMIC_ACQUIRE)) {
        // H-1: Apply spike encoding before GPU forward (matches mutable path)
        float fixed_threshold = network->config.spike_params.min_threshold;
        if (fixed_threshold <= 0.0f) fixed_threshold = 0.1f;
        float* spike_input = convert_input_to_spikes(input, input_size, fixed_threshold,
                                                     network->config.spike_params.encoding);
        if (!spike_input) return 0;

        // I-C1 FIX: Check GPU forward return value — fall through to CPU on failure.
        // Previously the return value was unchecked, so if GPU forward failed the output
        // buffer contained garbage/zeros but inference proceeded as if successful.
        bool gpu_ok = nimcp_gpu_forward_pass(network->gpu_weight_cache,
                                             spike_input, input_size, output, output_size);
        free_hot_buffer(spike_input);

        if (gpu_ok) {
            // I-H3: Note — GPU forward pass mutates activation tensors in the weight cache.
            // This is a known limitation of the readonly path when using GPU acceleration.

            /* BUG-11 fix: Use readonly=true to avoid mutating state through const */
            uint32_t active_count = process_network_outputs_impl((adaptive_network_t)network, output, output_size, true);
            return active_count;
        }
        // GPU failed — fall through to CPU path
    }

    // CPU fallback path

    // Step 1: Compute adaptive threshold for input
    float input_threshold =
        adaptive_compute_threshold(input, input_size, network->config.spike_params.k_factor);

    // Step 2: Convert input to spikes if needed
    float* spike_input = convert_input_to_spikes(input, input_size, input_threshold,
                                                 network->config.spike_params.encoding);

    // Guard clause: Check spike input allocation
    if (!spike_input)
        return 0;

    // Step 3: Forward pass through base network
    // Cast away const - neural_network_forward doesn't actually modify network
    // (it only reads weights/structure, writes to output buffer)
    neural_network_forward(network->base_network, spike_input, input_size, output, output_size);

    free_hot_buffer(spike_input);  // Phase MP: Return to pool

    // Step 4: Process outputs with adaptive thresholding
    // BUG-11 fix: Use readonly=true instead of casting away const
    uint32_t active_count = process_network_outputs_impl((adaptive_network_t)network, output, output_size, true);

    // Phase 3: SKIP statistics updates for read-only inference
    // REMOVED: update_running_sparsity(network, active_count, output_size);
    // REMOVED: network->total_inferences++;

    return active_count;
}

/**
 * @brief Raw forward pass -- spike encoding + network forward, NO output thresholding
 *
 * WHAT: Same as adaptive_network_forward() but skips process_network_outputs()
 * WHY:  predict_fast needs raw network output for accurate argmax classification.
 *       The adaptive thresholding zeros out below-threshold outputs which collapses
 *       all predictions to the same class.
 * HOW:  Steps 1-3 of adaptive_network_forward(), skip step 4-5
 *
 * I-H2 FIX: Added GPU forward path. Previously was CPU-only, meaning GPU-trained
 * brains would predict on CPU with different spike encoding thresholds (adaptive
 * vs fixed), causing train/test discrepancy.
 *
 * I-H4 FIX: When GPU is enabled, use the same fixed threshold (min_threshold) that
 * the GPU learning path uses. When GPU is disabled, use adaptive threshold for
 * backward compatibility with CPU-only training. This eliminates the spike encoding
 * threshold mismatch between training and inference.
 */
uint32_t adaptive_network_forward_raw(adaptive_network_t network, const float* input,
                                      uint32_t input_size, float* output, uint32_t output_size)
{
    if (!network || !input || !output)
        return 0;

    // I-H4 FIX: Use fixed threshold when GPU is enabled (matches GPU learning path).
    // When GPU is disabled, use adaptive threshold (matches CPU learning path).
    // This ensures spike encoding is consistent between training and inference.
    float input_threshold;
    if (network->gpu_enabled) {
        // Fixed threshold — same as GPU learning path in adaptive_network_learn()
        input_threshold = network->config.spike_params.min_threshold;
        if (input_threshold <= 0.0f) input_threshold = 0.1f;
    } else {
        // Adaptive threshold — same as CPU learning path
        input_threshold =
            adaptive_compute_threshold(input, input_size, network->config.spike_params.k_factor);
    }

    float* spike_input = convert_input_to_spikes(input, input_size, input_threshold,
                                                 network->config.spike_params.encoding);
    if (!spike_input)
        return 0;

    // I-H2 FIX: GPU forward path for raw inference (no output thresholding).
    // When GPU is enabled and weights are synced, use GPU for inference to match
    // the computational path used during training.
    if (network->gpu_enabled && network->gpu_weight_cache) {
        // Re-upload weights if CPU biological learning modified them
        if (__atomic_load_n(&network->gpu_weight_cache->weights_dirty_on_cpu, __ATOMIC_ACQUIRE)) {
            nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                         network->base_network);
        }

        bool gpu_ok = nimcp_gpu_forward_pass(network->gpu_weight_cache,
                                             spike_input, input_size, output, output_size);
        free_hot_buffer(spike_input);

        if (gpu_ok) {
            // Count non-zero outputs (informational, no thresholding applied)
            uint32_t active = 0;
            for (uint32_t i = 0; i < output_size; i++) {
                if (fabsf(output[i]) > 1e-6f) active++;
            }
            network->total_inferences++;
            return active;
        }
        // GPU failed -- fall through to CPU forward
        spike_input = convert_input_to_spikes(input, input_size, input_threshold,
                                              network->config.spike_params.encoding);
        if (!spike_input) return 0;
    }

    // CPU forward pass through base network
    neural_network_forward(network->base_network, spike_input, input_size, output, output_size);
    free_hot_buffer(spike_input);

    // Count non-zero outputs (informational, no thresholding applied)
    uint32_t active = 0;
    for (uint32_t i = 0; i < output_size; i++) {
        if (fabsf(output[i]) > 1e-6f) active++;
    }

    // W6-07 FIX: Increment total_inferences in CPU path (GPU path already does this).
    network->total_inferences++;

    return active;
}

//=============================================================================
// Sparsity and Pruning
//=============================================================================

float adaptive_network_get_sparsity(adaptive_network_t network)
{
    if (!network)
        return 0.0F;
    return network->running_sparsity;
}

uint32_t adaptive_network_prune(adaptive_network_t network, float threshold)
{
    if (!network)
        return 0;

    /* TODO: Implement network pruning. This requires exposing weight pruning
     * in the neural_network API. Currently a no-op stub. */
    (void)threshold;
    network->num_pruned_synapses = 0;

    return network->num_pruned_synapses;
}

//=============================================================================
// Pattern Learning
//=============================================================================

float adaptive_network_learn(adaptive_network_t network, const training_example_t* example,
                             learning_mode_t mode, float learning_rate)
{
    if (!network || !example) {
        NIMCP_LOGGING_WARN("adaptive_network_learn: NULL network=%p or example=%p", (void*)network, (void*)example);
        return -1.0F;
    }

    // Guard: target_size==0 would cause division by zero in MSE computation.
    // Only enforced for modes that require targets (supervised, distillation, hybrid).
    // Unsupervised and reinforcement modes don't use example->target.
    if (mode != LEARN_MODE_UNSUPERVISED && mode != LEARN_MODE_REINFORCEMENT) {
        if (example->target_size == 0 || !example->target) {
            NIMCP_LOGGING_WARN("adaptive_network_learn: no target (size=%u, ptr=%p)", example->target_size, (void*)example->target);
            return -1.0F;
        }
    }

    // Frozen network rejects learning
    if (network->frozen)
        return 0.0F;

    // Phase GPU: GPU-accelerated learning path
    if (network->gpu_enabled && network->gpu_weight_cache &&
        (mode == LEARN_MODE_SUPERVISED || mode == LEARN_MODE_DISTILLATION || mode == LEARN_MODE_HYBRID)) {
        // 1. Sync weights to GPU if CPU biological learning modified them
        // Use atomic CAS to avoid TOCTOU race: concurrent threads could both
        // see dirty=true and redundantly upload, or one could clear while
        // another skips the upload it needed.
        {
            bool expected_dirty = true;
            if (__atomic_compare_exchange_n(&network->gpu_weight_cache->weights_dirty_on_cpu,
                                            &expected_dirty, false, false,
                                            __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                             network->base_network);
            }
        }

        // 2. Spike-encode input with fixed threshold (preserves magnitude discrimination)
        float fixed_threshold = network->config.spike_params.min_threshold;
        if (fixed_threshold <= 0.0f) fixed_threshold = 0.1f;

        float* spike_input = convert_input_to_spikes(example->input, example->input_size,
                                                     fixed_threshold,
                                                     network->config.spike_params.encoding);
        if (!spike_input) {
            NIMCP_LOGGING_WARN("adaptive_network_learn: spike encoding failed (input_size=%u, threshold=%.4f)", example->input_size, fixed_threshold);
            return -1.0F;
        }

        // 3. GPU forward pass with spike-encoded input
        // M-5: Allocate max(target_size, output_size) to prevent OOB write when
        //       the network's output_size exceeds example->target_size
        uint32_t out_buf_size = example->target_size > network->config.base_config.output_size
            ? example->target_size : network->config.base_config.output_size;
        // Guard against size_t overflow: uint32_t * sizeof(float) can't overflow
        // size_t on 64-bit, but on 32-bit (SIZE_MAX = UINT32_MAX) it could.
        if (out_buf_size > SIZE_MAX / sizeof(float)) {
            free_hot_buffer(spike_input);
            return -1.0F;
        }
        float* output = (float*)alloc_hot_buffer(out_buf_size * sizeof(float));
        if (!output) { free_hot_buffer(spike_input); return -1.0F; }

        // I-C2 FIX: Check GPU forward return value — fall back to CPU on failure.
        // Previously the return value was unchecked; if GPU forward failed, the output
        // buffer contained garbage but loss computation and backprop proceeded.
        // BUG FIX: Use network output_size, NOT example->target_size. The GPU cache
        // validates output_size == last layer size; using target_size (which may differ)
        // caused validation failure → CPU fallback → truncated gradients → mode collapse.
        bool gpu_fwd_ok = nimcp_gpu_forward_pass(network->gpu_weight_cache,
                                                  spike_input, example->input_size,
                                                  output, network->config.base_config.output_size);

        free_hot_buffer(spike_input);

        if (!gpu_fwd_ok) {
            // GPU forward failed — fall through to CPU learning path
            free_hot_buffer(output);
            goto cpu_learn_path;
        }

        // 3. Compute loss — MSE + cosine alignment for distillation,
        //    BCE for classification.
        float loss = 0.0f;
        if (mode == LEARN_MODE_DISTILLATION) {
            // MSE loss: targets are continuous embeddings in [-1, 1]
            for (uint32_t li = 0; li < example->target_size; li++) {
                float o = isfinite(output[li]) ? output[li] : 0.0f;
                float diff = example->target[li] - o;
                loss += diff * diff;
            }
            loss /= (float)example->target_size;
            if (!isfinite(loss)) loss = 0.0f;

            /* Cosine alignment term: penalize directional mismatch between
             * output and target in the first 1024 dims (BERT embedding space).
             * Without this, MSE alone can reduce element-wise error without
             * learning meaningful directional alignment — especially when
             * targets are zero-padded beyond dim 1024.
             * Lambda bumped 0.5 → 2.0 after held-out eval at step 500 showed
             * mean cos=0.02 (9/10 positive but weak). Stronger cosine gradient
             * signal should accelerate alignment relative to MSE-only. */
            {
                const uint32_t EMBED_DIM = 1024;
                uint32_t cos_dim = (example->target_size < EMBED_DIM)
                                 ? example->target_size : EMBED_DIM;
                float dot = 0.0f, norm_o = 0.0f, norm_t = 0.0f;
                for (uint32_t ci = 0; ci < cos_dim; ci++) {
                    float o = isfinite(output[ci]) ? output[ci] : 0.0f;
                    float t = example->target[ci];
                    dot += o * t;
                    norm_o += o * o;
                    norm_t += t * t;
                }
                norm_o = sqrtf(norm_o);
                norm_t = sqrtf(norm_t);
                if (norm_o > 1e-8f && norm_t > 1e-8f) {
                    float cos_sim = dot / (norm_o * norm_t);
                    float cosine_loss = 1.0f - cos_sim;  /* range [0, 2] */
                    loss += 2.0f * cosine_loss;           /* lambda = 2.0 */
                }
            }
        } else {
            // BCE loss: targets are probabilities in [0, 1]
            float eps = 1e-7f;
            for (uint32_t li = 0; li < example->target_size; li++) {
                float raw_o = isfinite(output[li]) ? output[li] : 0.5f;
                float o = fmaxf(eps, fminf(1.0f - eps, raw_o));
                float t = fminf(fmaxf(example->target[li], 0.0f), 1.0f);
                loss -= t * logf(o) + (1.0f - t) * logf(1.0f - o);
            }
            int active_targets = 0;
            for (uint32_t li = 0; li < example->target_size; li++) {
                if (example->target[li] > 0.01f) active_targets++;
            }
            loss /= fmaxf(2.0f * active_targets, 1.0f);
            if (!isfinite(loss) || loss < 0.0f) loss = 0.0f;
        }

        // 3b. Diversity loss (unified anti-collapse) with gradient correction
        float* div_grad = (float*)alloc_hot_buffer(example->target_size * sizeof(float));
        if (div_grad) memset(div_grad, 0, example->target_size * sizeof(float));
        {
            static __thread nimcp_anti_collapse_state_t s_adap_anti_collapse;
            static __thread bool s_adap_ac_inited = false;
            if (!s_adap_ac_inited) {
                nimcp_anti_collapse_init(&s_adap_anti_collapse, NULL);
                s_adap_ac_inited = true;
            }
            float div_loss = nimcp_anti_collapse_diversity_loss(
                &s_adap_anti_collapse, output, div_grad, example->target_size);
            loss += div_loss;
        }

        // 3b½. Cosine alignment gradient — inject into div_grad so it flows
        //       through backprop via the target adjustment.
        if (mode == LEARN_MODE_DISTILLATION && div_grad) {
            const uint32_t EMBED_DIM = 1024;
            uint32_t cos_dim = (example->target_size < EMBED_DIM)
                             ? example->target_size : EMBED_DIM;
            float dot = 0.0f, norm_o = 0.0f, norm_t = 0.0f;
            for (uint32_t ci = 0; ci < cos_dim; ci++) {
                float o = isfinite(output[ci]) ? output[ci] : 0.0f;
                float t = example->target[ci];
                dot += o * t;
                norm_o += o * o;
                norm_t += t * t;
            }
            norm_o = sqrtf(norm_o);
            norm_t = sqrtf(norm_t);
            if (norm_o > 1e-8f && norm_t > 1e-8f) {
                float cos_sim = dot / (norm_o * norm_t);
                float inv_o2 = 1.0f / (norm_o * norm_o);
                float inv_ot = 1.0f / (norm_o * norm_t);
                /* d(1-cos)/d(o[i]) = cos*o[i]/||o||^2 - t[i]/(||o||*||t||)
                 * Added to div_grad with lambda=2.0 (same as loss term). */
                for (uint32_t ci = 0; ci < cos_dim; ci++) {
                    float o = isfinite(output[ci]) ? output[ci] : 0.0f;
                    float g = 2.0f * (cos_sim * o * inv_o2
                                    - example->target[ci] * inv_ot);
                    div_grad[ci] += g;
                }
            }
        }

        // 3c. Gradient-normalized learning rate: scale LR so effective gradient
        //     step has consistent magnitude regardless of raw gradient norm.
        //     Uses EMA grad norm from previous steps as estimate.
        float effective_lr = learning_rate;
        {
            float ema_gn = network->ema_grad_norm;
            if (ema_gn > 1e-4f && isfinite(ema_gn)) {
                /* Target: effective gradient step ≈ learning_rate * target_norm */
                float target_norm = 1.0f;
                effective_lr = learning_rate * (target_norm / ema_gn);
                /* Clamp to prevent extreme LR swings */
                float lr_min = learning_rate * 0.01f;
                float lr_max = learning_rate * 100.0f;
                if (effective_lr < lr_min) effective_lr = lr_min;
                if (effective_lr > lr_max) effective_lr = lr_max;
            }
        }

        // 4. GPU backward pass — weights updated on GPU, then downloaded to CPU.
        // Falls back to CPU backprop if GPU backward fails.
        // Inject diversity gradient into the error signal by adjusting the
        // effective target: target_eff = target - div_grad (pushes output away
        // from recent outputs, preventing mode collapse).
        float* target_with_diversity = NULL;
        if (div_grad) {
            target_with_diversity = (float*)alloc_hot_buffer(example->target_size * sizeof(float));
            if (target_with_diversity) {
                for (uint32_t di = 0; di < example->target_size; di++) {
                    target_with_diversity[di] = example->target[di] - div_grad[di];
                }
            }
        }
        const float* bp_target = target_with_diversity ? target_with_diversity : example->target;
        {
            float grad_norm = 0.0f;
            backprop_layer_grads_t layer_grads = {0};
            uint32_t num_layers = network->config.base_config.num_layers;
            uint32_t* layer_sizes = network->config.base_config.layer_sizes;
            bool gpu_backprop_done = false;

            if (num_layers >= 2 && layer_sizes && network->gpu_weight_cache) {
                /* REVERTED to direct backward pass (no FP16 accumulate, no batch).
                 * FP16 + batch-4 suspected of introducing NaN via numerical
                 * instability. Direct backward is stable — downloads weights
                 * every step via PCIe but produces correct results. */
                gpu_backprop_done = nimcp_gpu_backward_pass(
                    network->gpu_weight_cache,
                    network->base_network,
                    bp_target, output, example->target_size,
                    effective_lr,
                    network->config.base_config.min_weight,
                    network->config.base_config.max_weight,
                    &grad_norm);
            }

            if (!gpu_backprop_done && num_layers >= 2 && layer_sizes) {
                // CPU fallback: sync activations first
                nimcp_gpu_weight_cache_sync_activations(network->gpu_weight_cache,
                                                        network->base_network);
                if (mode == LEARN_MODE_DISTILLATION) {
                    backprop_sparse_full_regression_wd(network->base_network,
                        num_layers, layer_sizes, effective_lr,
                        network->config.base_config.min_weight,
                        network->config.base_config.max_weight,
                        example->target, output, example->target_size,
                        1.0f, ADAPTIVE_WEIGHT_DECAY, div_grad,
                        &grad_norm, &layer_grads);
                } else {
                    backprop_sparse_full_ex2(network->base_network,
                        num_layers, layer_sizes, effective_lr,
                        network->config.base_config.min_weight,
                        network->config.base_config.max_weight,
                        example->target, output, example->target_size,
                        1.0f, ADAPTIVE_WEIGHT_DECAY, div_grad,
                        &grad_norm, &layer_grads);
                }
            }
            network->last_grad_norm = grad_norm;
            network->num_grad_layers = layer_grads.num_layers;
            memcpy(network->last_layer_grad_norms, layer_grads.norms,
                   sizeof(network->last_layer_grad_norms));

            // Update EMA of gradient norm for training stability detection
            // H-5: Guard against +Inf/NaN corrupting the EMA
            if (isfinite(grad_norm)) {
                if (network->ema_grad_norm < 0.0f) {
                    network->ema_grad_norm = grad_norm;  // First iteration: seed
                } else {
                    network->ema_grad_norm = 0.9f * network->ema_grad_norm + 0.1f * grad_norm;
                }
            }
        }

        if (target_with_diversity) free_hot_buffer(target_with_diversity);

        // 6. Mark weights dirty (learning modified synapse weights on CPU)
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);

        // 7. For HYBRID mode: apply biological plasticity after GPU backprop
        //    Uses active-neuron-only sampling for large networks (O(active) not O(N))
        if (mode == LEARN_MODE_HYBRID && network->base_network) {
            float reward = fmaxf(0.0f, 1.0f - loss);
            uint32_t bio_n = neural_network_get_num_neurons(network->base_network);
            if (bio_n <= 100000) {
                neural_network_apply_reward_learning(
                    network->base_network, reward, learning_rate * 0.1f,
                    network->total_learning_steps);
            } else {
                neural_network_apply_reward_learning_active(
                    network->base_network, reward, learning_rate * 0.1f,
                    network->total_learning_steps, 0.01f);
            }
        }

        // --- Probe tracking (GPU path) ---
        {
            uint32_t target_label = 0;
            float target_max = example->target[0];
            uint32_t pred_label = 0;
            float pred_max = output[0];
            for (uint32_t pi = 1; pi < example->target_size; pi++) {
                if (example->target[pi] > target_max) {
                    target_max = example->target[pi];
                    target_label = pi;
                }
                if (output[pi] > pred_max) {
                    pred_max = output[pi];
                    pred_label = pi;
                }
            }
            float probe_conf = fmaxf(0.0f, fminf(1.0f, pred_max));
            bool probe_correct = (pred_label == target_label);
            adaptive_network_update_probe_tracking(network, target_label,
                                                   loss, probe_conf, probe_correct);

            // Update running accuracy EMA — this uses the output[] buffer directly
            // (ground truth from GPU forward pass), unlike the brain_learn code which
            // tried to reconstruct predictions from neuron->state.
            float match_f = probe_correct ? 1.0f : 0.0f;
            float cur = atomic_load_explicit(&network->running_accuracy_ema,
                                              memory_order_relaxed);
            float next = cur * 0.99f + match_f * 0.01f;
            NIMCP_EMA_GUARD(next, match_f);
            atomic_store_explicit(&network->running_accuracy_ema, next,
                                   memory_order_relaxed);
        }

        free_hot_buffer(output);
        free_hot_buffer(div_grad);
        network->total_learning_steps++;

        // L-3: Update EMA of training loss in GPU path (same logic as CPU path)
        // H-5: Guard against +Inf/NaN corrupting the EMA
        if (loss >= 0.0f && isfinite(loss)) {
            if (network->ema_loss < 0.0f) {
                network->ema_loss = loss;  // First iteration: seed
            } else {
                network->ema_loss = 0.9f * network->ema_loss + 0.1f * loss;
            }
            NIMCP_EMA_GUARD(network->ema_loss, loss);
        }

        return loss;
    }

    // CPU fallback path (original code)
cpu_learn_path:
    ;  // empty statement after label (required by C standard before declaration)

    // Forward pass to get current output (Phase MP: use pool)
    // Use raw forward (no sparsity thresholding) so backprop sees true activations
    // H10: Output buffer must be at least max(target_size, output_size) to avoid undersize
    {
    uint32_t out_buf_size = example->target_size > network->config.base_config.output_size
        ? example->target_size : network->config.base_config.output_size;
    float* output = (float*)alloc_hot_buffer(out_buf_size * sizeof(float));
    if (!output) {
        return -1.0F;
    }
    // BUG FIX: Use network output_size for forward pass, not target_size.
    // target_size may be smaller, causing truncated forward → partial gradients → mode collapse.
    adaptive_network_forward_raw(network, example->input, example->input_size, output,
                                 network->config.base_config.output_size);

    // Compute loss based on mode
    float loss = 0.0F;

    switch (mode) {
        case LEARN_MODE_DISTILLATION:
            // MSE loss for continuous embedding targets [-1, 1]
            {
                for (uint32_t i = 0; i < example->target_size; i++) {
                    float o = isfinite(output[i]) ? output[i] : 0.0f;
                    float diff = example->target[i] - o;
                    loss += diff * diff;
                }
                loss /= (float)example->target_size;
                if (!isfinite(loss)) loss = 0.0f;
            }

            // Diversity loss (unified anti-collapse, parity with GPU path)
            {
                static __thread nimcp_anti_collapse_state_t s_cpu_dist_ac;
                static __thread bool s_cpu_dist_ac_inited = false;
                if (!s_cpu_dist_ac_inited) {
                    nimcp_anti_collapse_init(&s_cpu_dist_ac, NULL);
                    s_cpu_dist_ac_inited = true;
                }
                float* div_grad = (float*)alloc_hot_buffer(example->target_size * sizeof(float));
                if (div_grad) memset(div_grad, 0, example->target_size * sizeof(float));
                float div_loss = nimcp_anti_collapse_diversity_loss(
                    &s_cpu_dist_ac, output, div_grad, example->target_size);
                loss += div_loss;

            // Regression backprop: MSE gradients on ALL outputs
            {
                // Gradient-normalized LR (parity with GPU path)
                float cpu_eff_lr = learning_rate;
                {
                    float ema_gn = network->ema_grad_norm;
                    if (ema_gn > 1e-4f && isfinite(ema_gn)) {
                        cpu_eff_lr = learning_rate * (1.0f / ema_gn);
                        float lr_min = learning_rate * 0.01f;
                        float lr_max = learning_rate * 100.0f;
                        if (cpu_eff_lr < lr_min) cpu_eff_lr = lr_min;
                        if (cpu_eff_lr > lr_max) cpu_eff_lr = lr_max;
                    }
                }
                float grad_norm = 0.0f;
                backprop_layer_grads_t layer_grads = {0};
                uint32_t num_layers = network->config.base_config.num_layers;
                uint32_t* layer_sizes = network->config.base_config.layer_sizes;
                if (num_layers >= 2 && layer_sizes) {
                    backprop_sparse_full_regression_wd(network->base_network,
                        num_layers, layer_sizes, cpu_eff_lr,
                        network->config.base_config.min_weight,
                        network->config.base_config.max_weight,
                        example->target, output, example->target_size,
                        1.0f, ADAPTIVE_WEIGHT_DECAY, div_grad,
                        &grad_norm, &layer_grads);
                }
                network->last_grad_norm = grad_norm;
                network->num_grad_layers = layer_grads.num_layers;
                memcpy(network->last_layer_grad_norms, layer_grads.norms,
                       sizeof(network->last_layer_grad_norms));

                if (network->gpu_weight_cache) {
                    __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
                }

                if (isfinite(grad_norm)) {
                    if (network->ema_grad_norm < 0.0f) {
                        network->ema_grad_norm = grad_norm;
                    } else {
                        network->ema_grad_norm = 0.9f * network->ema_grad_norm + 0.1f * grad_norm;
                    }
                    NIMCP_EMA_GUARD(network->ema_grad_norm, grad_norm);
                }
            }
                free_hot_buffer(div_grad);
            }
            break;

        case LEARN_MODE_SUPERVISED:
            // Binary cross-entropy loss (natural pair with sigmoid activation)
            {
                float eps = 1e-7f;
                for (uint32_t i = 0; i < example->target_size; i++) {
                    float raw_o = isfinite(output[i]) ? output[i] : 0.5f;
                    float o = fmaxf(eps, fminf(1.0f - eps, raw_o));
                    float t = fminf(fmaxf(example->target[i], 0.0f), 1.0f);
                    loss -= t * logf(o) + (1.0f - t) * logf(1.0f - o);
                }
                int active_targets = 0;
                for (uint32_t i = 0; i < example->target_size; i++) {
                    if (example->target[i] > 0.01f) active_targets++;
                }
                loss /= fmaxf(2.0f * active_targets, 1.0f);
            }

            // Diversity loss (unified anti-collapse, parity with GPU path)
            {
                static __thread nimcp_anti_collapse_state_t s_cpu_sup_ac;
                static __thread bool s_cpu_sup_ac_inited = false;
                if (!s_cpu_sup_ac_inited) {
                    nimcp_anti_collapse_init(&s_cpu_sup_ac, NULL);
                    s_cpu_sup_ac_inited = true;
                }
                float* div_grad = (float*)alloc_hot_buffer(example->target_size * sizeof(float));
                if (div_grad) memset(div_grad, 0, example->target_size * sizeof(float));
                float div_loss = nimcp_anti_collapse_diversity_loss(
                    &s_cpu_sup_ac, output, div_grad, example->target_size);
                loss += div_loss;

            // =================================================================
            // BIOLOGICAL PLASTICITY: Apply STDP/BCM to synapses
            // =================================================================
            // WHAT: Update synaptic weights using biological learning rules
            // WHY:  Enable Hebbian learning instead of gradient descent
            // HOW:  Compute reward signal → Call neural_network_apply_reward_learning
            //
            // BIOLOGICAL BASIS:
            // - STDP: Hebbian learning ("neurons that fire together wire together")
            // - BCM: Homeostatic plasticity (sliding threshold prevents saturation)
            // - Eligibility traces: Temporal credit assignment (bridge reward delay)
            //
            // COMPLEXITY: O(N×S) where N = neurons, S = synapses per neuron
            // =================================================================

            // Full backpropagation through all layers (sparse-efficient)
            // Delegated to shared kernel with parallel + SIMD optimizations
            {
                // Gradient-normalized LR (parity with GPU path)
                float cpu_eff_lr = learning_rate;
                {
                    float ema_gn = network->ema_grad_norm;
                    if (ema_gn > 1e-4f && isfinite(ema_gn)) {
                        cpu_eff_lr = learning_rate * (1.0f / ema_gn);
                        float lr_min = learning_rate * 0.01f;
                        float lr_max = learning_rate * 100.0f;
                        if (cpu_eff_lr < lr_min) cpu_eff_lr = lr_min;
                        if (cpu_eff_lr > lr_max) cpu_eff_lr = lr_max;
                    }
                }
                float grad_norm = 0.0f;
                backprop_layer_grads_t layer_grads = {0};
                uint32_t num_layers = network->config.base_config.num_layers;
                uint32_t* layer_sizes = network->config.base_config.layer_sizes;
                if (num_layers >= 2 && layer_sizes) {
                    backprop_sparse_full_ex2(network->base_network,
                        num_layers, layer_sizes, cpu_eff_lr,
                        network->config.base_config.min_weight,
                        network->config.base_config.max_weight,
                        example->target, output, example->target_size,
                        1.0f, ADAPTIVE_WEIGHT_DECAY, div_grad,
                        &grad_norm, &layer_grads);
                }
                network->last_grad_norm = grad_norm;
                network->num_grad_layers = layer_grads.num_layers;
                memcpy(network->last_layer_grad_norms, layer_grads.norms,
                       sizeof(network->last_layer_grad_norms));

                // W6-03 FIX: Mark GPU weight cache dirty after CPU backprop modifies weights.
                // Without this, the next GPU-path learn/forward uses stale cached weights.
                if (network->gpu_weight_cache) {
                    __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
                }

                // Update EMA of gradient norm for training stability detection
                // H2-FIX: Guard against NaN/Inf corrupting the EMA (matches GPU path)
                if (isfinite(grad_norm)) {
                    if (network->ema_grad_norm < 0.0f) {
                        network->ema_grad_norm = grad_norm;  // First iteration: seed
                    } else {
                        network->ema_grad_norm = 0.9f * network->ema_grad_norm + 0.1f * grad_norm;
                    }
                    NIMCP_EMA_GUARD(network->ema_grad_norm, grad_norm);
                }
            }
                free_hot_buffer(div_grad);
            }
            break;

        case LEARN_MODE_UNSUPERVISED:
            // Hebbian learning - no explicit target
            // Already handled by base network's plasticity rules during forward pass
            loss = 0.0F;
            // H-3: No backprop gradient to track for unsupervised mode
            network->last_grad_norm = 0.0f;
            break;

        case LEARN_MODE_REINFORCEMENT:
            // Use confidence as reward signal
            loss = 1.0F - example->confidence;

            // Apply reinforcement learning with eligibility traces
            // Reward comes directly from confidence
            {
            neural_network_t base_rl = network->base_network;
            uint64_t time_rl = network->total_learning_steps;
            float rl_reward = example->confidence;  // Direct reward signal

            // Bio-plasticity: active-neuron sampling for large networks
            uint32_t rl_n = neural_network_get_num_neurons(base_rl);
            if (rl_n <= 100000) {
                neural_network_apply_reward_learning(base_rl, rl_reward, learning_rate, time_rl);
            } else {
                neural_network_apply_reward_learning_active(base_rl, rl_reward, learning_rate, time_rl, 0.01f);
            }

            // W6-04 FIX: Mark GPU weight cache dirty after reward learning modifies CPU weights.
            // Without this, the next GPU-path learn/forward uses stale cached weights.
            if (network->gpu_weight_cache) {
                __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
            }
            }
            // H-3: No backprop gradient to track for reinforcement mode
            network->last_grad_norm = 0.0f;
            break;

        case LEARN_MODE_HYBRID:
            // =================================================================
            // HYBRID LEARNING: Backpropagation + Biological Plasticity
            // =================================================================
            // WHAT: Supervised backprop followed by biological learning rules
            // WHY:  Backprop provides fast gradient-based learning, while
            //       biological plasticity (STDP/BCM/eligibility) adds
            //       homeostatic regulation and temporal credit assignment
            // HOW:  Phase 1 = MSE loss + full backprop (same as supervised)
            //       Phase 2 = reward-modulated biological learning (STDP/BCM)
            //
            // BIOLOGICAL BASIS:
            // - Cortical circuits use both error-driven and Hebbian learning
            // - Dopamine-modulated STDP bridges reward with spike timing
            // - BCM prevents weight saturation via sliding threshold
            // =================================================================

            // Phase 1: Binary cross-entropy loss (same as supervised)
            {
                float eps = 1e-7f;
                for (uint32_t i = 0; i < example->target_size; i++) {
                    // M-3: Guard against NaN/Inf output propagating through BCE
                    float raw_o = isfinite(output[i]) ? output[i] : 0.5f;
                    float o = fmaxf(eps, fminf(1.0f - eps, raw_o));
                    // C-ADP-10: Clamp target to [0,1] (same as supervised path)
                    float t = fminf(fmaxf(example->target[i], 0.0f), 1.0f);
                    loss -= t * logf(o) + (1.0f - t) * logf(1.0f - o);
                }
                // BCE loss dilution fix (RL path): same normalization as CPU supervised path
                int active_targets = 0;
                for (uint32_t i = 0; i < example->target_size; i++) {
                    if (example->target[i] > 0.01f) active_targets++;
                }
                loss /= fmaxf(2.0f * active_targets, 1.0f);
            }

            // Diversity loss (unified anti-collapse, parity with GPU path)
            {
                static __thread nimcp_anti_collapse_state_t s_cpu_hyb_ac;
                static __thread bool s_cpu_hyb_ac_inited = false;
                if (!s_cpu_hyb_ac_inited) {
                    nimcp_anti_collapse_init(&s_cpu_hyb_ac, NULL);
                    s_cpu_hyb_ac_inited = true;
                }
                float* div_grad = (float*)alloc_hot_buffer(example->target_size * sizeof(float));
                if (div_grad) memset(div_grad, 0, example->target_size * sizeof(float));
                float div_loss = nimcp_anti_collapse_diversity_loss(
                    &s_cpu_hyb_ac, output, div_grad, example->target_size);
                loss += div_loss;

            // Phase 2: Full backpropagation (delegated to shared kernel)
            {
                // Gradient-normalized LR (parity with GPU path)
                float cpu_eff_lr = learning_rate;
                {
                    float ema_gn = network->ema_grad_norm;
                    if (ema_gn > 1e-4f && isfinite(ema_gn)) {
                        cpu_eff_lr = learning_rate * (1.0f / ema_gn);
                        float lr_min = learning_rate * 0.01f;
                        float lr_max = learning_rate * 100.0f;
                        if (cpu_eff_lr < lr_min) cpu_eff_lr = lr_min;
                        if (cpu_eff_lr > lr_max) cpu_eff_lr = lr_max;
                    }
                }
                float grad_norm = 0.0f;
                backprop_layer_grads_t layer_grads = {0};
                uint32_t num_layers = network->config.base_config.num_layers;
                uint32_t* layer_sizes = network->config.base_config.layer_sizes;
                if (num_layers >= 2 && layer_sizes) {
                    backprop_sparse_full_ex2(network->base_network,
                        num_layers, layer_sizes, cpu_eff_lr,
                        network->config.base_config.min_weight,
                        network->config.base_config.max_weight,
                        example->target, output, example->target_size,
                        1.0f, ADAPTIVE_WEIGHT_DECAY, div_grad,
                        &grad_norm, &layer_grads);

                    // Phase 3: Biological plasticity AFTER backprop
                    //   Active-neuron sampling for large networks (O(active) not O(N))
                    float reward = fmaxf(0.0f, 1.0f - loss);
                    neural_network_t base_hybrid = network->base_network;
                    if (base_hybrid) {
                        uint32_t hybrid_n = neural_network_get_num_neurons(base_hybrid);
                        if (hybrid_n <= 100000) {
                            neural_network_apply_reward_learning(
                                base_hybrid, reward, learning_rate * 0.1f,
                                network->total_learning_steps);
                        } else {
                            neural_network_apply_reward_learning_active(
                                base_hybrid, reward, learning_rate * 0.1f,
                                network->total_learning_steps, 0.01f);
                        }
                    }

                    // Phase 4: Lateral inhibition on output layer
                    if (network->base_network) {
                        uint32_t output_offset = 0;
                        for (uint32_t l = 0; l < num_layers - 1; l++)
                            output_offset += layer_sizes[l];
                        uint32_t output_size = layer_sizes[num_layers - 1];
                        neural_network_apply_lateral_inhibition(
                            network->base_network,
                            output_offset, output_size,
                            LATERAL_INHIBITION_STRENGTH);
                    }
                }
                network->last_grad_norm = grad_norm;
                network->num_grad_layers = layer_grads.num_layers;
                memcpy(network->last_layer_grad_norms, layer_grads.norms,
                       sizeof(network->last_layer_grad_norms));

                // W6-03 FIX: Mark GPU weight cache dirty after CPU HYBRID backprop + bio-plasticity.
                if (network->gpu_weight_cache) {
                    __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
                }

                // Update EMA of gradient norm for training stability detection
                // H2-FIX: Guard against NaN/Inf corrupting the EMA (matches GPU path)
                if (isfinite(grad_norm)) {
                    if (network->ema_grad_norm < 0.0f) {
                        network->ema_grad_norm = grad_norm;  // First iteration: seed
                    } else {
                        network->ema_grad_norm = 0.9f * network->ema_grad_norm + 0.1f * grad_norm;
                    }
                    NIMCP_EMA_GUARD(network->ema_grad_norm, grad_norm);
                }
            }
                free_hot_buffer(div_grad);
            }
            break;
    }

    // --- Probe tracking (CPU path) ---
    // Only for modes with meaningful target/output for classification accuracy.
    if (mode == LEARN_MODE_SUPERVISED || mode == LEARN_MODE_DISTILLATION ||
        mode == LEARN_MODE_HYBRID) {
        uint32_t target_label = 0;
        float target_max = example->target[0];
        uint32_t pred_label = 0;
        float pred_max = output[0];
        for (uint32_t pi = 1; pi < example->target_size; pi++) {
            if (example->target[pi] > target_max) {
                target_max = example->target[pi];
                target_label = pi;
            }
            if (output[pi] > pred_max) {
                pred_max = output[pi];
                pred_label = pi;
            }
        }
        float probe_conf = fmaxf(0.0f, fminf(1.0f, pred_max));
        bool probe_correct = (pred_label == target_label);
        adaptive_network_update_probe_tracking(network, target_label,
                                               loss, probe_conf, probe_correct);

        // Update running accuracy EMA (CPU path, same as GPU path)
        float match_f = probe_correct ? 1.0f : 0.0f;
        float cur = atomic_load_explicit(&network->running_accuracy_ema,
                                          memory_order_relaxed);
        float next = cur * 0.99f + match_f * 0.01f;
        atomic_store_explicit(&network->running_accuracy_ema, next,
                               memory_order_relaxed);
    }

    free_hot_buffer(output);  // Phase MP: Return to pool

    network->total_learning_steps++;

    // Update EMA of training loss for stability detection
    // H2-FIX: Guard against NaN/Inf corrupting the EMA (matches GPU path)
    if (loss >= 0.0f && isfinite(loss)) {
        if (network->ema_loss < 0.0f) {
            network->ema_loss = loss;  // First iteration: seed
        } else {
            network->ema_loss = 0.9f * network->ema_loss + 0.1f * loss;
        }
    }

    return loss;
    }  // end cpu_learn_path block
}

float adaptive_network_learn_batch(adaptive_network_t network, const training_example_t* examples,
                                   uint32_t num_examples, learning_mode_t mode, float learning_rate)
{
    if (!network || !examples || num_examples == 0)
        return -1.0F;

    bool gpu_batch_opt = (network->gpu_enabled && network->gpu_weight_cache &&
        (mode == LEARN_MODE_SUPERVISED || mode == LEARN_MODE_DISTILLATION || mode == LEARN_MODE_HYBRID));

    /* GPU gradient accumulation: run forward+backward for each sample but accumulate
     * gradients on GPU without updating weights. After all samples, apply averaged
     * gradients in one shot. This gives true mini-batch gradient descent. */
    if (gpu_batch_opt && num_examples >= 2) {
        if (__atomic_load_n(&network->gpu_weight_cache->weights_dirty_on_cpu, __ATOMIC_ACQUIRE)) {
            nimcp_gpu_weight_cache_upload(network->gpu_weight_cache, network->base_network);
            __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, false, __ATOMIC_RELEASE);
        }

        float fixed_threshold = network->config.spike_params.min_threshold;
        if (fixed_threshold <= 0.0f) fixed_threshold = 0.1f;

        float total_loss = 0.0f;
        uint32_t successful = 0;

        for (uint32_t i = 0; i < num_examples; i++) {
            const training_example_t* ex = &examples[i];
            if (!ex->input || !ex->target || ex->target_size == 0) continue;

            /* Spike-encode input */
            float* spike_input = convert_input_to_spikes(ex->input, ex->input_size,
                                                          fixed_threshold,
                                                          network->config.spike_params.encoding);
            if (!spike_input) continue;

            uint32_t out_buf_size = ex->target_size > network->config.base_config.output_size
                ? ex->target_size : network->config.base_config.output_size;
            float* output = (float*)alloc_hot_buffer(out_buf_size * sizeof(float));
            if (!output) { free_hot_buffer(spike_input); continue; }

            /* GPU forward pass — use network output_size, not target_size */
            bool fwd_ok = nimcp_gpu_forward_pass(network->gpu_weight_cache,
                                                  spike_input, ex->input_size,
                                                  output, network->config.base_config.output_size);
            free_hot_buffer(spike_input);

            if (!fwd_ok) { free_hot_buffer(output); continue; }

            /* Compute loss */
            float loss = 0.0f;
            if (mode == LEARN_MODE_DISTILLATION) {
                for (uint32_t li = 0; li < ex->target_size; li++) {
                    float o = isfinite(output[li]) ? output[li] : 0.0f;
                    float diff = ex->target[li] - o;
                    loss += diff * diff;
                }
                loss /= (float)ex->target_size;
                if (!isfinite(loss)) loss = 0.0f;
            } else {
                float eps = 1e-7f;
                for (uint32_t li = 0; li < ex->target_size; li++) {
                    float raw_o = isfinite(output[li]) ? output[li] : 0.5f;
                    float o = fmaxf(eps, fminf(1.0f - eps, raw_o));
                    float t = fminf(fmaxf(ex->target[li], 0.0f), 1.0f);
                    loss -= t * logf(o) + (1.0f - t) * logf(1.0f - o);
                }
                int active_targets = 0;
                for (uint32_t li = 0; li < ex->target_size; li++) {
                    if (ex->target[li] > 0.01f) active_targets++;
                }
                loss /= fmaxf(2.0f * active_targets, 1.0f);
                if (!isfinite(loss) || loss < 0.0f) loss = 0.0f;
            }

            /* Diversity loss with gradient — inject into target for GPU backward.
             * Same approach as single-sample path: target_eff = target - div_grad
             * pushes output AWAY from recent outputs, preventing mode collapse. */
            float* batch_div_grad = (float*)alloc_hot_buffer(ex->target_size * sizeof(float));
            if (batch_div_grad) memset(batch_div_grad, 0, ex->target_size * sizeof(float));
            {
                static __thread nimcp_anti_collapse_state_t s_batch_ac;
                static __thread bool s_batch_ac_inited = false;
                if (!s_batch_ac_inited) {
                    nimcp_anti_collapse_init(&s_batch_ac, NULL);
                    s_batch_ac_inited = true;
                }
                float div_loss = nimcp_anti_collapse_diversity_loss(
                    &s_batch_ac, output, batch_div_grad, ex->target_size);
                loss += div_loss;
            }

            /* Build diversity-adjusted target: target - div_grad */
            float* batch_target_eff = ex->target;
            float* batch_target_alloc = NULL;
            if (batch_div_grad) {
                batch_target_alloc = (float*)alloc_hot_buffer(ex->target_size * sizeof(float));
                if (batch_target_alloc) {
                    for (uint32_t di = 0; di < ex->target_size; di++) {
                        batch_target_alloc[di] = ex->target[di] - batch_div_grad[di];
                    }
                    batch_target_eff = batch_target_alloc;
                }
                free_hot_buffer(batch_div_grad);
            }

            /* Gradient-normalized LR for batch accumulation */
            float batch_eff_lr = learning_rate;
            {
                float ema_gn = network->ema_grad_norm;
                if (ema_gn > 1e-4f && isfinite(ema_gn)) {
                    batch_eff_lr = learning_rate * (1.0f / ema_gn);
                    float lr_min = learning_rate * 0.01f;
                    float lr_max = learning_rate * 100.0f;
                    if (batch_eff_lr < lr_min) batch_eff_lr = lr_min;
                    if (batch_eff_lr > lr_max) batch_eff_lr = lr_max;
                }
            }

            /* Accumulate gradients on GPU with diversity-adjusted target */
            nimcp_gpu_backward_accumulate(network->gpu_weight_cache,
                                           batch_target_eff, output, ex->target_size,
                                           batch_eff_lr);
            if (batch_target_alloc) free_hot_buffer(batch_target_alloc);

            if (loss >= 0.0f && isfinite(loss)) {
                total_loss += loss;
                successful++;
            }

            free_hot_buffer(output);
        }

        /* Flush: apply averaged gradients and download weights to CPU */
        float grad_norm = 0.0f;
        nimcp_gpu_gradient_flush_and_sync(network->gpu_weight_cache,
                                          network->base_network,
                                          network->config.base_config.min_weight,
                                          network->config.base_config.max_weight,
                                          &grad_norm);
        network->last_grad_norm = grad_norm;
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
        network->total_learning_steps += successful;

        if (isfinite(grad_norm)) {
            if (network->ema_grad_norm < 0.0f)
                network->ema_grad_norm = grad_norm;
            else
                network->ema_grad_norm = 0.9f * network->ema_grad_norm + 0.1f * grad_norm;
        }

        float avg_loss = (successful > 0) ? (total_loss / successful) : -1.0f;
        if (avg_loss >= 0.0f && isfinite(avg_loss)) {
            if (network->ema_loss < 0.0f)
                network->ema_loss = avg_loss;
            else
                network->ema_loss = 0.9f * network->ema_loss + 0.1f * avg_loss;
            NIMCP_EMA_GUARD(network->ema_loss, avg_loss);
        }

        return avg_loss;
    }

    /* Fallback: per-sample learning (original path, for GPU batch_size=1 or CPU) */
    if (gpu_batch_opt) {
        if (__atomic_load_n(&network->gpu_weight_cache->weights_dirty_on_cpu, __ATOMIC_ACQUIRE)) {
            nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                         network->base_network);
        }
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, false, __ATOMIC_RELEASE);
    }

    float total_loss = 0.0F;
    uint32_t successful = 0;

    for (uint32_t i = 0; i < num_examples; i++) {
        float loss = adaptive_network_learn(network, &examples[i], mode, learning_rate);
        if (loss >= 0.0F && isfinite(loss)) {
            total_loss += loss;
            successful++;
        }

        if (gpu_batch_opt && network->gpu_weight_cache) {
            __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, false, __ATOMIC_RELEASE);
        }
    }

    if (gpu_batch_opt && network->gpu_weight_cache) {
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
    }

    return (successful > 0) ? (total_loss / successful) : -1.0F;
}

float adaptive_network_distill(adaptive_network_t network, const float* input, uint32_t input_size,
                               teacher_function_t teacher_fn, void* teacher_context,
                               float learning_rate)
{
    if (!network || !input || !teacher_fn)
        return -1.0F;

    // Query teacher for target
    float* teacher_output = teacher_fn(input, input_size, teacher_context);
    if (!teacher_output)
        return -1.0F;

    // Create training example from teacher's output
    training_example_t example = {.input = (float*) input,
                                  .input_size = input_size,
                                  .target = teacher_output,
                                  .target_size = network->config.base_config.output_size,
                                  .confidence = 1.0F,  // Assume high confidence in teacher
                                  .label = {0}};

    float loss = adaptive_network_learn(network, &example, LEARN_MODE_DISTILLATION, learning_rate);

    // Free teacher output (caller's responsibility to allocate, our responsibility to free)
    nimcp_free(teacher_output);

    return loss;
}

//=============================================================================
// Model Persistence
//=============================================================================

bool adaptive_network_save(adaptive_network_t network, const char* filepath,
                           serialize_format_t format)
{
    if (!network || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_save: required parameter is NULL (network, filepath)");
        return false;
    }

    // Open file for writing
    FILE* file = fopen(filepath, "wb");
    if (!file) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_save: file is NULL");
        return false;
    }

    bool success = true;

    switch (format) {
        case SERIALIZE_FORMAT_BINARY:
            // Write magic header
            uint32_t magic = 0x4E494D43;    // "NIMC"
            // H3-FIX: Bump to v2.5.1 to include ema_grad_norm + ema_loss fields
            uint32_t version = 0x00020501;  // v2.5.1

            FWRITE_CHECKED(&magic, sizeof(uint32_t), 1, file);
            FWRITE_CHECKED(&version, sizeof(uint32_t), 1, file);

            // WHAT: Write configuration (excluding pointer fields)
            // WHY:  Can't serialize pointers directly - they're invalid when loaded
            // HOW:  Write num_layers, then layer_sizes array data separately

            // Write base_config fields (safe ones)
            FWRITE_CHECKED(&network->config.base_config.num_neurons, sizeof(uint32_t), 1, file);
            FWRITE_CHECKED(&network->config.base_config.ei_ratio, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.learning_rate, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.hebbian_rate, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.stdp_window, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.homeostatic_rate, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.target_activity, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.adaptation_rate, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.refractory_period, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.min_weight, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.max_weight, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.base_config.update_interval, sizeof(uint32_t), 1, file);
            FWRITE_CHECKED(&network->config.base_config.input_size, sizeof(uint32_t), 1, file);
            FWRITE_CHECKED(&network->config.base_config.output_size, sizeof(uint32_t), 1, file);
            FWRITE_CHECKED(&network->config.base_config.num_layers, sizeof(uint32_t), 1, file);

            // Write layer_sizes array separately
            if (network->config.base_config.num_layers > 0 && network->config.base_config.layer_sizes) {
                FWRITE_CHECKED(network->config.base_config.layer_sizes,
                       sizeof(uint32_t),
                       network->config.base_config.num_layers,
                       file);
            }

            // Continue with base_config booleans and enums
            // W6-11 NOTE (LOW): ABI-dependent checkpoint field sizes — sizeof(bool),
            // sizeof(enum), and struct padding are implementation-defined. Checkpoints are
            // NOT portable across compilers/platforms. A future checkpoint version should use
            // fixed-width types (uint8_t for bools, uint32_t for enums) to enable portability.
            // Changing now would break backward compatibility with existing checkpoints.
            FWRITE_CHECKED(&network->config.base_config.enable_stdp, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.base_config.enable_hebbian, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.base_config.enable_oja, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.base_config.enable_homeostasis, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.base_config.neuron_model, sizeof(neuron_model_type_t), 1, file);
            // Skip model_params pointer (NULL in most cases, complex to serialize)
            FWRITE_CHECKED(&network->config.base_config.integration_method, sizeof(ode_integration_method_t), 1, file);
            FWRITE_CHECKED(&network->config.base_config.enable_bcm, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.base_config.enable_eligibility, sizeof(bool), 1, file);

            // Write spike_params struct
            FWRITE_CHECKED(&network->config.spike_params, sizeof(adaptive_spike_params_t), 1, file);

            // Write remaining adaptive config fields
            FWRITE_CHECKED(&network->config.enable_sparsity, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.pruning_threshold, sizeof(float), 1, file);
            FWRITE_CHECKED(&network->config.update_frequency, sizeof(uint32_t), 1, file);

            // Skip checkpoint_path pointer (don't need to save this)
            FWRITE_CHECKED(&network->config.auto_load, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.auto_save, sizeof(bool), 1, file);
            FWRITE_CHECKED(&network->config.auto_save_interval, sizeof(uint32_t), 1, file);

            // Write neuron count
            FWRITE_CHECKED(&network->num_neurons, sizeof(uint32_t), 1, file);

            // Write neuron states (M-4: field-by-field to skip pointer fields)
            for (uint32_t i = 0; i < network->num_neurons; i++) {
                adaptive_neuron_state_t* ns = &network->neuron_states[i];
                FWRITE_CHECKED(&ns->membrane_potential, sizeof(float), 1, file);
                FWRITE_CHECKED(&ns->adaptive_threshold, sizeof(float), 1, file);
                FWRITE_CHECKED(&ns->spike_count, sizeof(int32_t), 1, file);
                // Skip spike_train pointer — not meaningful across processes
                FWRITE_CHECKED(&ns->spike_train_length, sizeof(uint32_t), 1, file);
                FWRITE_CHECKED(&ns->activation_mean, sizeof(float), 1, file);
                FWRITE_CHECKED(&ns->activation_variance, sizeof(float), 1, file);
                FWRITE_CHECKED(&ns->sample_count, sizeof(uint32_t), 1, file);
            }

            // Write label map
            // W6-06 FIX: Guard against NULL label_map entries which would crash strlen()
            FWRITE_CHECKED(&network->num_labels, sizeof(uint32_t), 1, file);
            for (uint32_t i = 0; i < network->num_labels; i++) {
                if (!network->label_map[i]) {
                    // Write empty placeholder for NULL entries
                    uint32_t label_len = 1;
                    char nul = '\0';
                    FWRITE_CHECKED(&label_len, sizeof(uint32_t), 1, file);
                    FWRITE_CHECKED(&nul, 1, 1, file);
                    continue;
                }
                uint32_t label_len = strlen(network->label_map[i]) + 1;
                FWRITE_CHECKED(&label_len, sizeof(uint32_t), 1, file);
                FWRITE_CHECKED(network->label_map[i], label_len, 1, file);
            }

            // Write statistics
            FWRITE_CHECKED(&network->total_inferences, sizeof(uint64_t), 1, file);
            FWRITE_CHECKED(&network->total_learning_steps, sizeof(uint64_t), 1, file);
            FWRITE_CHECKED(&network->running_sparsity, sizeof(float), 1, file);

            // H3-FIX: Persist EMA tracking values (new in v2.5.1)
            // Without these, checkpoint restore resets EMA to -1 (uninitialized),
            // causing incorrect stability detection until the EMA warms up again.
            // C-ADP-2: Copy volatile fields to local vars for fwrite (volatile float*
            // is not implicitly convertible to const void*)
            {
                float tmp_ema_gn = network->ema_grad_norm;
                float tmp_ema_l = network->ema_loss;
                FWRITE_CHECKED(&tmp_ema_gn, sizeof(float), 1, file);
                FWRITE_CHECKED(&tmp_ema_l, sizeof(float), 1, file);
            }

            // Write synaptic weights from base_network (CRITICAL: enables weight persistence)
            if (network->base_network) {
                synapse_metadata_pool_t save_m_pool = neural_network_get_synapse_metadata_pool(network->base_network);
                uint32_t base_num_neurons = neural_network_get_num_neurons(network->base_network);
                FWRITE_CHECKED(&base_num_neurons, sizeof(uint32_t), 1, file);

                for (uint32_t i = 0; i < base_num_neurons; i++) {
                    neuron_t* neuron = neural_network_get_neuron(network->base_network, i);
                    // C-1: Write zero/default data for NULL neurons instead of skipping,
                    // because load expects sequential entries for every neuron index
                    if (!neuron) {
                        uint32_t zero_syn = 0;
                        FWRITE_CHECKED(&zero_syn, sizeof(uint32_t), 1, file);
                        // Write default neuron state fields matching the non-NULL path
                        float zero_f = 0.0f;
                        uint64_t zero_u64 = 0;
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // state
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // bias
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // threshold
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // adaptation
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // calcium_concentration
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // plasticity_rate
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // homeostatic_factor
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // avg_activity
                        FWRITE_CHECKED(&zero_f, sizeof(float), 1, file);  // weight_norm
                        learning_rule_t zero_lr = 0;
                        FWRITE_CHECKED(&zero_lr, sizeof(learning_rule_t), 1, file);
                        activation_type_t zero_at = 0;
                        FWRITE_CHECKED(&zero_at, sizeof(activation_type_t), 1, file);
                        oja_params_t zero_oja = {0};
                        FWRITE_CHECKED(&zero_oja, sizeof(oja_params_t), 1, file);
                        stdp_params_t zero_stdp = {0};
                        FWRITE_CHECKED(&zero_stdp, sizeof(stdp_params_t), 1, file);
                        homeostatic_params_t zero_homeo = {0};
                        FWRITE_CHECKED(&zero_homeo, sizeof(homeostatic_params_t), 1, file);
                        FWRITE_CHECKED(&zero_u64, sizeof(uint64_t), 1, file);  // last_spike
                        FWRITE_CHECKED(&zero_u64, sizeof(uint64_t), 1, file);  // last_update
                        neuron_model_type_t zero_mt = 0;
                        FWRITE_CHECKED(&zero_mt, sizeof(neuron_model_type_t), 1, file);
                        continue;
                    }

                    // C-1: Count non-NULL synapse handles before writing the count.
                    // The load side reads exactly num_syn synapse records sequentially,
                    // so we must not skip any entries or the file position will misalign.
                    uint32_t total_syn = NEURON_OUT_COUNT(neuron);
                    uint32_t num_syn = 0;
                    for (uint32_t j = 0; j < total_syn; j++) {
                        if (NEURON_OUT_HANDLE(neuron, j)) num_syn++;
                    }
                    FWRITE_CHECKED(&num_syn, sizeof(uint32_t), 1, file);

                    // Write each synapse (weight and key plasticity data)
                    for (uint32_t j = 0; j < total_syn; j++) {
                        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, j);
                        if (!h) continue;
                        synapse_t* syn = sparse_synapse_get_metadata(save_m_pool, h);
                        FWRITE_CHECKED(&h->target_neuron_id, sizeof(uint32_t), 1, file);
                        FWRITE_CHECKED(&h->weight, sizeof(float), 1, file);
                        float plasticity = syn ? syn->plasticity : 0.0f;
                        float trace = syn ? syn->trace : 0.0f;
                        float strength = h->strength;
                        float meta_plasticity = syn ? syn->meta_plasticity : 0.0f;
                        float last_change = syn ? syn->last_change : 0.0f;
                        uint64_t last_active = syn ? syn->last_active : 0;
                        bool enable_stp = false;
                        stp_state_t stp_save = {0};
                        if (syn) {
                            synapse_cold_t* save_cold = SYNAPSE_COLD(network->base_network, syn);
                            if (save_cold) {
                                enable_stp = save_cold->enable_stp;
                                if (enable_stp) stp_save = save_cold->stp;
                            }
                        }
                        FWRITE_CHECKED(&plasticity, sizeof(float), 1, file);
                        FWRITE_CHECKED(&trace, sizeof(float), 1, file);
                        FWRITE_CHECKED(&strength, sizeof(float), 1, file);
                        FWRITE_CHECKED(&meta_plasticity, sizeof(float), 1, file);
                        FWRITE_CHECKED(&last_change, sizeof(float), 1, file);
                        FWRITE_CHECKED(&last_active, sizeof(uint64_t), 1, file);
                        FWRITE_CHECKED(&enable_stp, sizeof(bool), 1, file);
                        if (enable_stp) {
                            FWRITE_CHECKED(&stp_save, sizeof(stp_state_t), 1, file);
                        }
                    }

                    // Write full neuron state (CRITICAL: enables exact training resume)
                    FWRITE_CHECKED(&neuron->state, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->bias, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->threshold, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->adaptation, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->calcium_concentration, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->plasticity_rate, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->homeostatic_factor, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->avg_activity, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->weight_norm, sizeof(float), 1, file);
                    FWRITE_CHECKED(&neuron->learning_rule, sizeof(learning_rule_t), 1, file);
                    FWRITE_CHECKED(&neuron->activation_type, sizeof(activation_type_t), 1, file);
                    FWRITE_CHECKED(&neuron->oja_params, sizeof(oja_params_t), 1, file);
                    FWRITE_CHECKED(&neuron->stdp_params, sizeof(stdp_params_t), 1, file);
                    FWRITE_CHECKED(&neuron->homeostatic, sizeof(homeostatic_params_t), 1, file);
                    FWRITE_CHECKED(&neuron->last_spike, sizeof(uint64_t), 1, file);
                    FWRITE_CHECKED(&neuron->last_update, sizeof(uint64_t), 1, file);
                    FWRITE_CHECKED(&neuron->model_type, sizeof(neuron_model_type_t), 1, file);
                    // Note: neuron.model (neuron_model_state_t) is a pointer to opaque state
                    // TODO: If model is not NULL, need to serialize model-specific state
                }
            } else {
                // No base network - write 0 neurons
                uint32_t zero = 0;
                FWRITE_CHECKED(&zero, sizeof(uint32_t), 1, file);
            }

            break;

        case SERIALIZE_FORMAT_JSON:
        case SERIALIZE_FORMAT_SAFETENSORS:
            // TODO: Implement JSON and SafeTensors formats
            success = false;
            break;
    }

write_done:
    fclose(file);
    return success;
}

adaptive_network_t adaptive_network_load(const char* filepath)
{
    if (!filepath) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "filepath is NULL");

        return NULL;

    }

    FILE* file = fopen(filepath, "rb");
    if (!file) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "file is NULL");

        return NULL;

    }

    /* 16 MB read buffer + kernel read-ahead for fast checkpoint I/O */
    {
        char* buf = (char*)malloc(16 * 1024 * 1024);
        if (buf) setvbuf(file, buf, _IOFBF, 16 * 1024 * 1024);
#ifdef __linux__
        int fd = fileno(file);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0) {
                posix_fadvise(fd, 0, st.st_size, POSIX_FADV_SEQUENTIAL);
                posix_fadvise(fd, 0, st.st_size, POSIX_FADV_WILLNEED);
            }
        }
#endif
    }

    // Read magic header
    uint32_t magic = 0;
    uint32_t version = 0;
    if (fread(&magic, sizeof(uint32_t), 1, file) != 1 || magic != 0x4E494D43) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
        return NULL;
    }

    if (fread(&version, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
        return NULL;
    }

    // H-6: Validate checkpoint version — v2.5.0 and v2.5.1 formats are supported.
    // v2.5.1 adds ema_grad_norm and ema_loss fields after statistics.
    if (version != 0x00020500 && version != 0x00020501) {
        fprintf(stderr, "WARNING: Checkpoint version 0x%08X not supported (expected 0x00020500 or 0x00020501)\n", version);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: unsupported checkpoint version");
        return NULL;
    }

    // WHAT: Read configuration (excluding pointer fields)
    // WHY:  Pointers were serialized separately
    // HOW:  Read individual fields, then reconstruct layer_sizes array

    adaptive_network_config_t config;
    memset(&config, 0, sizeof(adaptive_network_config_t));

    // Read base_config fields
    // M-1: Use checked reads to detect file misalignment from partial I/O failures
    if (fread(&config.base_config.num_neurons, sizeof(uint32_t), 1, file) != 1) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
        return NULL;
    }
    // Validate num_neurons: max 10M neurons to prevent corrupt data from causing crashes
    if (config.base_config.num_neurons > 10000000) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: num_neurons exceeds maximum (10M)");
        return NULL;
    }
    {
        bool cfg_ok = true;
        cfg_ok = cfg_ok && (fread(&config.base_config.ei_ratio, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.learning_rate, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.hebbian_rate, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.stdp_window, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.homeostatic_rate, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.target_activity, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.adaptation_rate, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.refractory_period, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.min_weight, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.max_weight, sizeof(float), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.update_interval, sizeof(uint32_t), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.input_size, sizeof(uint32_t), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.output_size, sizeof(uint32_t), 1, file) == 1);
        cfg_ok = cfg_ok && (fread(&config.base_config.num_layers, sizeof(uint32_t), 1, file) == 1);
        if (!cfg_ok) {
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "adaptive_network_load: failed to read base config fields");
            return NULL;
        }
    }

    // Validate dimensions: prevent corrupt data from causing crashes
    if (config.base_config.input_size > 1000000 ||
        config.base_config.output_size > 1000000) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: input/output size exceeds maximum (1M)");
        return NULL;
    }

    // Read layer_sizes array separately
    uint32_t* layer_sizes = NULL;
    // Validate num_layers to prevent corrupt data from causing massive allocations
    if (config.base_config.num_layers > 1024) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: num_layers exceeds maximum (1024)");
        return NULL;
    }
    if (config.base_config.num_layers > 0) {
        layer_sizes = nimcp_malloc(config.base_config.num_layers * sizeof(uint32_t));
        if (!layer_sizes) {
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_network_load: layer_sizes is NULL");
            return NULL;
        }
        if (fread(layer_sizes, sizeof(uint32_t), config.base_config.num_layers, file) != config.base_config.num_layers) {
            nimcp_free(layer_sizes);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
            return NULL;
        }
        config.base_config.layer_sizes = layer_sizes;
    }

    // M-1: Read remaining base_config fields with checked reads
    {
        bool cfg2_ok = true;
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_stdp, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_hebbian, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_oja, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_homeostasis, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.neuron_model, sizeof(neuron_model_type_t), 1, file) == 1);
        config.base_config.model_params = NULL;  // Not serialized
        cfg2_ok = cfg2_ok && (fread(&config.base_config.integration_method, sizeof(ode_integration_method_t), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_bcm, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.base_config.enable_eligibility, sizeof(bool), 1, file) == 1);

        // Read spike_params
        cfg2_ok = cfg2_ok && (fread(&config.spike_params, sizeof(adaptive_spike_params_t), 1, file) == 1);

        // Read remaining adaptive config fields
        cfg2_ok = cfg2_ok && (fread(&config.enable_sparsity, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.pruning_threshold, sizeof(float), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.update_frequency, sizeof(uint32_t), 1, file) == 1);

        config.checkpoint_path = NULL;  // Not serialized, will be NULL
        cfg2_ok = cfg2_ok && (fread(&config.auto_load, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.auto_save, sizeof(bool), 1, file) == 1);
        cfg2_ok = cfg2_ok && (fread(&config.auto_save_interval, sizeof(uint32_t), 1, file) == 1);
        if (!cfg2_ok) {
            if (layer_sizes) nimcp_free(layer_sizes);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "adaptive_network_load: failed to read remaining config fields");
            return NULL;
        }
    }

    // Create network with reconstructed config
    adaptive_network_t network = adaptive_network_create(&config);

    // Free temporary layer_sizes array (create function makes its own copy)
    if (layer_sizes) {
        nimcp_free(layer_sizes);
    }

    if (!network) {
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_load: network is NULL");
        return NULL;
    }

    struct timespec _ts_load_start, _ts_load_end;
    clock_gettime(CLOCK_MONOTONIC, &_ts_load_start);
    #define LOAD_ELAPSED_S() ({ struct timespec _now; clock_gettime(CLOCK_MONOTONIC, &_now); \
        (_now.tv_sec - _ts_load_start.tv_sec) + (_now.tv_nsec - _ts_load_start.tv_nsec) * 1e-9; })
    fprintf(stderr, "[LOAD-TIMING] config+create: %.1fs\n", LOAD_ELAPSED_S());

    // Read neuron count
    uint32_t num_neurons = 0;
    if (fread(&num_neurons, sizeof(uint32_t), 1, file) != 1) {
        adaptive_network_destroy(network);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
        return NULL;
    }

    // Validate neuron count against network capacity
    if (num_neurons > 10000000) {
        adaptive_network_destroy(network);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: saved neuron count exceeds maximum (10M)");
        return NULL;
    }

    // C-1: Bounds check — saved neuron count must not exceed the network's
    // allocated neuron_states array, otherwise fread would write past the end
    if (num_neurons > network->num_neurons) {
        adaptive_network_destroy(network);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: saved neuron count exceeds network capacity");
        return NULL;
    }

    // Read neuron states (M-4: field-by-field matching save format, pointer skipped)
    for (uint32_t i = 0; i < num_neurons; i++) {
        adaptive_neuron_state_t* ns = &network->neuron_states[i];
        bool ns_ok = true;
        ns_ok = ns_ok && (fread(&ns->membrane_potential, sizeof(float), 1, file) == 1);
        ns_ok = ns_ok && (fread(&ns->adaptive_threshold, sizeof(float), 1, file) == 1);
        ns_ok = ns_ok && (fread(&ns->spike_count, sizeof(int32_t), 1, file) == 1);
        // spike_train pointer was not serialized — keep it NULL from calloc
        ns->spike_train = NULL;
        ns_ok = ns_ok && (fread(&ns->spike_train_length, sizeof(uint32_t), 1, file) == 1);
        ns->spike_train_length = 0;  // No spike train data in checkpoint
        ns_ok = ns_ok && (fread(&ns->activation_mean, sizeof(float), 1, file) == 1);
        ns_ok = ns_ok && (fread(&ns->activation_variance, sizeof(float), 1, file) == 1);
        ns_ok = ns_ok && (fread(&ns->sample_count, sizeof(uint32_t), 1, file) == 1);
        if (!ns_ok) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
            return NULL;
        }

        // C-ADP-6: Validate loaded neuron state floats for NaN/Inf from corrupt checkpoints
        if (!isfinite(ns->adaptive_threshold) || ns->adaptive_threshold <= 0.0f)
            ns->adaptive_threshold = network->config.spike_params.min_threshold;
        if (!isfinite(ns->activation_mean))
            ns->activation_mean = 0.0f;
        if (!isfinite(ns->activation_variance))
            ns->activation_variance = 0.0f;
        if (!isfinite(ns->membrane_potential))
            ns->membrane_potential = 0.0f;
    }

    fprintf(stderr, "[LOAD-TIMING] neuron_states: %.1fs (%u neurons)\n", LOAD_ELAPSED_S(), num_neurons);

    // Read label map
    uint32_t num_labels = 0;
    if (fread(&num_labels, sizeof(uint32_t), 1, file) != 1) {
        adaptive_network_destroy(network);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
        return NULL;
    }

    // Validate num_labels to prevent corrupt data from causing crashes
    if (num_labels > 100000) {
        adaptive_network_destroy(network);
        fclose(file);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "adaptive_network_load: num_labels exceeds maximum (100K)");
        return NULL;
    }

    // W6-01 FIX: Allocate label_map for the CAPACITY (next power-of-2), not just num_labels.
    // Previously allocated num_labels entries but set label_map_capacity to the next
    // power-of-2, so adding a new label when num_labels < capacity would skip realloc
    // and write past the allocation (heap overflow).
    // M-malloc0: Guard against nimcp_malloc(0) when num_labels==0
    if (num_labels > 0) {
        uint32_t cap = 16;
        while (cap < num_labels) cap *= 2;
        // C-ADP-1: Use nimcp_calloc to zero the ENTIRE allocation. Previously
        // nimcp_malloc + partial memset only zeroed slots [num_labels, cap), leaving
        // slots [0, num_labels) uninitialized. If the label reading loop fails
        // partway, destroy() would free garbage pointers in uninitialized slots.
        network->label_map = nimcp_calloc(cap, sizeof(char*));
        // C-3: NULL check after label_map allocation to prevent NULL dereference
        if (!network->label_map) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "adaptive_network_load: failed to allocate label_map");
            return NULL;
        }
        network->label_map_capacity = cap;
    } else {
        network->label_map = NULL;
        network->label_map_capacity = 0;
    }
    network->num_labels = num_labels;

    for (uint32_t i = 0; i < num_labels; i++) {
        uint32_t label_len = 0;
        if (fread(&label_len, sizeof(uint32_t), 1, file) != 1) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
            return NULL;
        }

        // Validate label length
        if (label_len > 65536) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "adaptive_network_load: label_len exceeds maximum (64K)");
            return NULL;
        }

        // M-2: Reject label_len==0 which would cause nimcp_malloc(0)
        if (label_len == 0) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "adaptive_network_load: label_len is 0");
            return NULL;
        }

        network->label_map[i] = nimcp_malloc(label_len);
        // H8: NULL check on per-label malloc during checkpoint load
        if (!network->label_map[i]) {
            adaptive_network_destroy(network);
            fclose(file);
            return NULL;
        }
        if (fread(network->label_map[i], label_len, 1, file) != 1) {
            adaptive_network_destroy(network);
            fclose(file);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_load: validation failed");
            return NULL;
        }

        // M-1: Verify loaded label string is null-terminated to prevent buffer overrun
        // in strlen/strcmp/hash calls. Corrupt checkpoint data could omit the terminator.
        if (network->label_map[i][label_len - 1] != '\0') {
            network->label_map[i][label_len - 1] = '\0';
            fprintf(stderr, "WARNING: Label %u missing null terminator, truncated\n", i);
        }
    }

    // C2-FIX: label_map_capacity already set during allocation above (W6-01 fix).
    // The capacity is allocated as next power-of-2 >= num_labels, and label_map
    // is allocated for the full capacity, not just num_labels.

    // H-1: Rebuild label hash table from loaded label_map entries.
    // Labels were loaded into label_map[] above but never inserted into the
    // hash table that was freshly created (empty) during adaptive_network_create().
    if (network->label_table && network->label_map) {
        for (uint32_t i = 0; i < network->num_labels; i++) {
            if (network->label_map[i]) {
                hash_table_insert_label(network->label_table, network->label_map[i], i);
            }
        }
    }

    // W6-02 FIX: On partial stats read, reset to defaults and return WITHOUT attempting
    // to read weights. A partial read leaves the file position misaligned, so all
    // subsequent reads (EMA, weights) would parse from the wrong position.
    {
        bool stats_ok = true;
        stats_ok = stats_ok && (fread(&network->total_inferences, sizeof(uint64_t), 1, file) == 1);
        stats_ok = stats_ok && (fread(&network->total_learning_steps, sizeof(uint64_t), 1, file) == 1);
        stats_ok = stats_ok && (fread(&network->running_sparsity, sizeof(float), 1, file) == 1);
        if (!stats_ok) {
            fprintf(stderr, "WARNING: Failed to read statistics, skipping weight load\n");
            network->total_inferences = 0;
            network->total_learning_steps = 0;
            network->running_sparsity = 0.0f;
            fclose(file);
            return network;
        }
    }

    // H3-FIX: Read EMA tracking values (v2.5.1+ only)
    // Old v2.5.0 checkpoints don't have these fields — keep the -1 (uninitialized)
    // defaults set by adaptive_network_create().
    if (version >= 0x00020501) {
        bool ema_ok = true;
        // C-ADP-2: Cast volatile away for fread — fread needs non-volatile void*
        float tmp_ema_grad = 0.0f, tmp_ema_loss = 0.0f;
        ema_ok = ema_ok && (fread(&tmp_ema_grad, sizeof(float), 1, file) == 1);
        ema_ok = ema_ok && (fread(&tmp_ema_loss, sizeof(float), 1, file) == 1);
        if (!ema_ok) {
            fprintf(stderr, "WARNING: Failed to read EMA fields from checkpoint, using defaults\n");
            network->ema_grad_norm = -1.0f;
            network->ema_loss = -1.0f;
        } else {
            network->ema_grad_norm = tmp_ema_grad;
            network->ema_loss = tmp_ema_loss;
        }

        // C-ADP-5: Validate loaded EMA values for NaN/Inf from corrupt checkpoints
        if (!isfinite(network->ema_grad_norm)) network->ema_grad_norm = -1.0f;
        if (!isfinite(network->ema_loss)) network->ema_loss = -1.0f;
    }

    // Read synaptic weights from base_network (CRITICAL: restores learned weights)
    uint32_t base_num_neurons = 0;
    if (fread(&base_num_neurons, sizeof(uint32_t), 1, file) == 1 && base_num_neurons > 0) {
        if (network->base_network) {
            // Validate neuron count matches
            uint32_t current_num_neurons = neural_network_get_num_neurons(network->base_network);
            if (base_num_neurons != current_num_neurons) {
                fprintf(stderr, "WARNING: Saved neuron count (%u) != current (%u), skipping weight load\n",
                        base_num_neurons, current_num_neurons);
                fclose(file);
                return network;
            }

            fprintf(stderr, "[LOAD-TIMING] pre-synapse-loop: %.1fs\n", LOAD_ELAPSED_S());

            // C-2: Track load errors from inner synapse loop so we can break out of
            // the outer neuron loop too, preventing file position misalignment
            bool load_error = false;
            uint64_t total_synapses_read = 0, total_synapses_added = 0, total_synapses_dropped = 0;

            for (uint32_t i = 0; i < base_num_neurons; i++) {
                neuron_t* neuron = neural_network_get_neuron(network->base_network, i);
                if (!neuron) {
                    fprintf(stderr, "WARNING: Failed to get neuron %u\n", i);
                    load_error = true;
                    break;
                }

                // Read number of synapses
                uint32_t num_synapses = 0;
                if (fread(&num_synapses, sizeof(uint32_t), 1, file) != 1) {
                    fprintf(stderr, "WARNING: Failed to read synapse count for neuron %u\n", i);
                    load_error = true;
                    break;
                }

                // H-2: Upper bound validation on num_synapses to detect corrupt data
                if (num_synapses > 1000000) {
                    fprintf(stderr, "WARNING: num_synapses %u exceeds max for neuron %u\n",
                            num_synapses, i);
                    load_error = true;
                    break;
                }

                // Clear existing synapses and load from file
                sparse_synapse_pool_t h_pool = neural_network_get_synapse_handle_pool(network->base_network);
                synapse_metadata_pool_t m_pool = neural_network_get_synapse_metadata_pool(network->base_network);

                // Actually clear existing outgoing synapses before loading saved ones
                sparse_synapse_storage_cleanup(h_pool, &neuron->outgoing);
                sparse_synapse_storage_init(&neuron->outgoing);

                /* Bulk read all synapse data for this neuron in one fread.
                 * Per-synapse: target_id(4) + weight(4) + plasticity(4) + trace(4)
                 *            + strength(4) + meta_plasticity(4) + last_change(4)
                 *            + last_active(8) + enable_stp(1) = 37 bytes base
                 * STP synapses add sizeof(stp_state_t) but we handle those separately.
                 * Reduces 9 fread calls per synapse to 1 fread per neuron. */
                #define SYNAPSE_BASE_SIZE 37
                uint8_t* bulk_buf = NULL;
                size_t bulk_size = (size_t)num_synapses * SYNAPSE_BASE_SIZE;
                if (num_synapses > 0) {
                    bulk_buf = (uint8_t*)nimcp_malloc(bulk_size);
                    if (!bulk_buf) { load_error = true; break; }
                    if (fread(bulk_buf, 1, bulk_size, file) != bulk_size) {
                        nimcp_free(bulk_buf);
                        load_error = true; break;
                    }
                }

                for (uint32_t j = 0; j < num_synapses; j++) {
                    uint8_t* p = bulk_buf + (size_t)j * SYNAPSE_BASE_SIZE;
                    uint32_t target_id;  memcpy(&target_id, p, 4); p += 4;
                    float weight;        memcpy(&weight, p, 4); p += 4;
                    float plasticity;    memcpy(&plasticity, p, 4); p += 4;
                    float trace;         memcpy(&trace, p, 4); p += 4;
                    float strength;      memcpy(&strength, p, 4); p += 4;
                    float meta_plasticity; memcpy(&meta_plasticity, p, 4); p += 4;
                    float last_change;   memcpy(&last_change, p, 4); p += 4;
                    uint64_t last_active; memcpy(&last_active, p, 8); p += 8;
                    bool enable_stp;     memcpy(&enable_stp, p, 1);

                    stp_state_t stp_data = {0};
                    if (enable_stp) {
                        if (fread(&stp_data, sizeof(stp_state_t), 1, file) != 1) {
                            nimcp_free(bulk_buf); load_error = true; break;
                        }
                    }

                    // Add synapse WITHOUT metadata — lazy allocation during training.
                    // This prevents the metadata pool from exploding during load
                    // (was allocating 100M+ slots → 13GB OOM).
                    // Metadata is allocated on-demand when plasticity needs it.
                    total_synapses_read++;
                    bool needs_metadata = (plasticity != 0.0f || trace != 0.0f ||
                                           meta_plasticity != 0.0f || enable_stp);
                    int add_rc;
                    if (needs_metadata) {
                        add_rc = sparse_synapse_add_with_metadata(h_pool, m_pool,
                                    &neuron->outgoing, target_id, weight, 0);
                    } else {
                        add_rc = sparse_synapse_add(h_pool,
                                    &neuron->outgoing, target_id, weight);
                    }
                    if (add_rc == 0) {
                        total_synapses_added++;
                        uint32_t out_count = NEURON_OUT_COUNT(neuron);
                        if (out_count == 0) continue;  /* guard against underflow */
                        uint32_t idx = out_count - 1;
                        synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, idx);
                        if (h) h->strength = strength;
                        if (needs_metadata) {
                            synapse_t* syn = h ? sparse_synapse_get_metadata(m_pool, h) : NULL;
                            if (syn) {
                                syn->plasticity = plasticity;
                                syn->trace = trace;
                                syn->strength = strength;
                                syn->meta_plasticity = meta_plasticity;
                                syn->last_change = last_change;
                                syn->last_active = last_active;
                                if (enable_stp) {
                                    synapse_cold_t* load_cold = SYNAPSE_ENSURE_COLD(network->base_network, syn);
                                    if (load_cold) {
                                        load_cold->enable_stp = enable_stp;
                                        load_cold->stp = stp_data;
                                    }
                                }
                            }
                        }
                    } else {
                        total_synapses_dropped++;
                    }
                }
                // Free bulk read buffer
                if (bulk_buf) { nimcp_free(bulk_buf); bulk_buf = NULL; }
                #undef SYNAPSE_BASE_SIZE

                // C-2: If inner synapse loop broke due to read failure (EOF/truncation),
                // stop loading further neurons but keep what we have — a truncated
                // checkpoint with 36% of weights is far better than no weights at all.
                if (load_error) {
                    fprintf(stderr, "WARNING: Checkpoint truncated at neuron %u/%u — "
                            "keeping %lu synapses already loaded\n",
                            i, base_num_neurons, (unsigned long)total_synapses_added);
                    break;
                }

                // Restore full neuron state — non-fatal if truncated (synapses are the
                // critical learned data; state fields default to init values)
                bool state_ok = true;
                if (fread(&neuron->state, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->bias, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->threshold, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->adaptation, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->calcium_concentration, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->plasticity_rate, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->homeostatic_factor, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->avg_activity, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->weight_norm, sizeof(float), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->learning_rule, sizeof(learning_rule_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->activation_type, sizeof(activation_type_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->oja_params, sizeof(oja_params_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->stdp_params, sizeof(stdp_params_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->homeostatic, sizeof(homeostatic_params_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->last_spike, sizeof(uint64_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->last_update, sizeof(uint64_t), 1, file) != 1) { state_ok = false; }
                if (state_ok && fread(&neuron->model_type, sizeof(neuron_model_type_t), 1, file) != 1) { state_ok = false; }
                if (!state_ok) {
                    fprintf(stderr, "WARNING: Neuron state truncated at neuron %u/%u — "
                            "synapses OK, using default state for remaining neurons\n",
                            i, base_num_neurons);
                    break;
                }
            }
            fprintf(stderr, "[LOAD-TIMING] synapse-loop: %.1fs\n", LOAD_ELAPSED_S());
            fprintf(stderr, "[CHECKPOINT] Synapse restore: read=%lu added=%lu dropped=%lu (%.1f%% success)\n",
                    (unsigned long)total_synapses_read, (unsigned long)total_synapses_added,
                    (unsigned long)total_synapses_dropped,
                    total_synapses_read > 0 ? 100.0 * total_synapses_added / total_synapses_read : 0.0);
            // Truncation is non-fatal — partial weights are better than none
            if (load_error) {
                fprintf(stderr, "WARNING: Partial weight restoration — continuing with %lu loaded synapses\n",
                        (unsigned long)total_synapses_added);
            }
        }
    }

    fclose(file);

    fprintf(stderr, "[LOAD-TIMING] pre-rebuild: %.1fs\n", LOAD_ELAPSED_S());
    // Rebuild incoming synapses from outgoing for forward pass
    if (network->base_network) {
        neural_network_rebuild_incoming(network->base_network);
        fprintf(stderr, "[LOAD-TIMING] rebuild-incoming: %.1fs\n", LOAD_ELAPSED_S());

        // POST-LOAD BACKBONE REPAIR: Verify both input→hidden and hidden→output
        // connections exist. Old checkpoints may be missing either direction due to
        // the broken metadata pool (fixed-size 10K) or incomplete prior repairs.
        // Without input→hidden, hidden neurons get bias-only activation (input-blind).
        // Without hidden→output, sparse_weights[last_layer] is NULL (bias-only output).
        // Either missing direction causes constant output regardless of input.
        uint32_t num_layers = network->config.base_config.num_layers;
        const uint32_t* layer_sizes = network->config.base_config.layer_sizes;
        if (num_layers >= 3 && layer_sizes) {
            uint32_t input_size = layer_sizes[0];
            uint32_t output_size = layer_sizes[num_layers - 1];
            uint32_t output_start = 0;
            for (uint32_t l = 0; l < num_layers - 1; l++) {
                output_start += layer_sizes[l];
            }
            uint32_t total_hidden = 0;
            for (uint32_t l = 1; l < num_layers - 1; l++) {
                total_hidden += layer_sizes[l];
            }
            uint32_t hidden_start = input_size;

            // Use same backbone parameters as fresh creation (nimcp_neuralnet.c)
            uint32_t backbone_target = output_size * 16;
            if (backbone_target < 1024) backbone_target = 1024;
            if (backbone_target > 32768) backbone_target = 32768;
            uint32_t backbone = (total_hidden < backbone_target) ? total_hidden : backbone_target;
            uint32_t step = (total_hidden > 0) ? (total_hidden / backbone) : 1;
            if (step == 0) step = 1;

            int needs_rebuild = 0;

            // --- Check input→hidden connections ---
            uint32_t hidden_with_incoming = 0;
            uint32_t hidden_check = (backbone < 100) ? backbone : 100;
            for (uint32_t b = 0; b < hidden_check; b++) {
                uint32_t hidden_id = hidden_start + b * step;
                if (hidden_id >= hidden_start + total_hidden) break;
                neuron_t* n = neural_network_get_neuron(network->base_network, hidden_id);
                if (n && NEURON_IN_COUNT(n) > 0) hidden_with_incoming++;
            }

            if (hidden_with_incoming < hidden_check / 2 && total_hidden > 0) {
                fprintf(stderr, "[CHECKPOINT] Backbone hidden neurons have zero incoming "
                        "connections from input — adding input→hidden wiring\n");
                float input_scale = 1.0F / sqrtf((float)input_size);
                uint32_t ih_conns = 0;

                for (uint32_t b = 0; b < backbone; b++) {
                    uint32_t hidden_id = hidden_start + b * step;
                    if (hidden_id >= hidden_start + total_hidden) break;
                    for (uint32_t i = 0; i < input_size; i++) {
                        float weight = (((float)rand() / RAND_MAX) * 2.0F - 1.0F) * input_scale;
                        neural_network_add_connection(network->base_network, i, hidden_id, weight);
                        ih_conns++;
                    }
                }
                fprintf(stderr, "[CHECKPOINT] Backbone repair: %u input→hidden connections "
                        "added (%u backbone neurons × %u inputs)\n",
                        ih_conns, backbone, input_size);
                needs_rebuild = 1;
            }

            // --- Check hidden→output connections ---
            uint32_t output_with_incoming = 0;
            uint32_t output_check = (output_size < 100) ? output_size : 100;
            for (uint32_t o = 0; o < output_check; o++) {
                neuron_t* n = neural_network_get_neuron(network->base_network, output_start + o);
                if (n && NEURON_IN_COUNT(n) > 0) output_with_incoming++;
            }

            if (output_with_incoming < output_check / 2 && total_hidden > 0) {
                fprintf(stderr, "[CHECKPOINT] Output layer has zero incoming connections — "
                        "adding backbone hidden→output wiring\n");
                float backbone_scale = 1.0F / sqrtf((float)backbone);
                uint32_t ho_conns = 0;

                for (uint32_t b = 0; b < backbone; b++) {
                    uint32_t hidden_id = hidden_start + b * step;
                    if (hidden_id >= hidden_start + total_hidden) break;
                    for (uint32_t o = 0; o < output_size; o++) {
                        uint32_t output_id = output_start + o;
                        uint32_t net_size = neural_network_get_num_neurons(network->base_network);
                        if (output_id >= net_size) break;
                        float weight = (((float)rand() / RAND_MAX) * 2.0F - 1.0F) * backbone_scale;
                        neural_network_add_connection(network->base_network, hidden_id, output_id, weight);
                        ho_conns++;
                    }
                }
                fprintf(stderr, "[CHECKPOINT] Backbone repair: %u hidden→output connections "
                        "added (%u backbone neurons × %u outputs)\n",
                        ho_conns, backbone, output_size);
                needs_rebuild = 1;
            }

            if (needs_rebuild) {
                neural_network_rebuild_incoming(network->base_network);
            }
        }
    }

    // H11: Clean up GPU resources from create() before re-initializing
    if (network->gpu_weight_cache) {
        nimcp_gpu_weight_cache_destroy(network->gpu_weight_cache);
        network->gpu_weight_cache = NULL;
    }
    if (network->gpu_ctx) {
        nimcp_gpu_context_destroy(network->gpu_ctx);
        network->gpu_ctx = NULL;
    }
    // Phase GPU: Initialize GPU acceleration for loaded network
    network->gpu_enabled = false;
    if (gpu_is_available() &&
        network->config.base_config.num_layers >= 2 &&
        network->config.base_config.layer_sizes) {
        network->gpu_ctx = nimcp_gpu_context_create_auto();
        if (network->gpu_ctx) {
            network->gpu_weight_cache = nimcp_gpu_weight_cache_create(
                network->gpu_ctx,
                network->base_network,
                network->config.base_config.layer_sizes,
                network->config.base_config.num_layers);
            if (network->gpu_weight_cache) {
                nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                             network->base_network);
                network->gpu_enabled = true;
            } else {
                nimcp_gpu_context_destroy(network->gpu_ctx);
                network->gpu_ctx = NULL;
            }
        }
    }

    return network;
}

size_t adaptive_network_get_size(adaptive_network_t network)
{
    if (!network)
        return 0;

    size_t size = sizeof(struct adaptive_network_struct);
    size += network->num_neurons * sizeof(adaptive_neuron_state_t);
    size += network->config.base_config.input_size * sizeof(float);

    // Add label map size
    // M-8: Guard against NULL label_map or NULL entries
    if (network->label_map) {
        for (uint32_t i = 0; i < network->num_labels; i++) {
            if (network->label_map[i]) {
                size += strlen(network->label_map[i]) + 1;
            }
        }
    }

    // Add base network size (approximate)
    size += (size_t)network->num_neurons * network->config.base_config.input_size * sizeof(float);

    return size;
}

//=============================================================================
// Interpretability & Analysis
//=============================================================================

bool adaptive_network_analyze_activation(adaptive_network_t network, const float* input,
                                         uint32_t input_size, activation_analysis_t* analysis)
{
    if (!network || !input || !analysis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_analyze_activation: required parameter is NULL (network, input, analysis)");
        return false;
    }

    // Forward pass (Phase MP: use pool)
    float* output = (float*)alloc_hot_buffer(network->config.base_config.output_size * sizeof(float));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_network_analyze_activation: output is NULL");
        return false;
    }
    uint32_t active_count = adaptive_network_forward(network, input, input_size, output,
                                                     network->config.base_config.output_size, 0);

    analysis->num_active_neurons = active_count;
    analysis->sparsity = network->running_sparsity;

    // H-3: Early return when active_count==0 to avoid nimcp_malloc(0)
    if (active_count == 0) {
        analysis->active_neuron_ids = NULL;
        analysis->activation_strengths = NULL;
        analysis->confidence = 0.0f;
        free_hot_buffer(output);
        return true;
    }

    // Allocate arrays for active neurons
    analysis->active_neuron_ids = nimcp_malloc(active_count * sizeof(uint32_t));
    analysis->activation_strengths = nimcp_malloc(active_count * sizeof(float));

    if (!analysis->active_neuron_ids || !analysis->activation_strengths) {
        nimcp_free(analysis->active_neuron_ids);
        nimcp_free(analysis->activation_strengths);
        free_hot_buffer(output);  // Phase MP: Return to pool
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "adaptive_network_analyze_activation: required parameter is NULL (analysis->active_neuron_ids, analysis->activation_strengths)");
        return false;
    }

    // Collect active neurons
    // M-4: Use min(output_size, num_neurons) as single loop bound to prevent OOB on both output[] and neuron_states[]
    uint32_t loop_bound = network->config.base_config.output_size;
    if (network->num_neurons < loop_bound) {
        loop_bound = network->num_neurons;
    }
    uint32_t idx = 0;
    for (uint32_t i = 0; i < loop_bound && idx < active_count; i++) {
        if (fabsf(output[i]) > network->neuron_states[i].adaptive_threshold) {
            analysis->active_neuron_ids[idx] = i;
            analysis->activation_strengths[idx] = output[i];
            idx++;
        }
    }

    // Compute confidence (based on max activation)
    float max_activation = 0.0F;
    for (uint32_t i = 0; i < active_count; i++) {
        if (fabsf(analysis->activation_strengths[i]) > max_activation) {
            max_activation = fabsf(analysis->activation_strengths[i]);
        }
    }
    analysis->confidence = fminf(max_activation / 10.0F, 1.0F);  // Normalize to [0,1]

    free_hot_buffer(output);  // Phase MP: Return to pool
    return true;
}

uint32_t adaptive_network_rank_neurons(adaptive_network_t network, neuron_importance_t* rankings,
                                       uint32_t max_rankings)
{
    if (!network || !rankings)
        return 0;

    uint32_t num_to_rank =
        (max_rankings < network->num_neurons) ? max_rankings : network->num_neurons;

    // Compute importance based on activation statistics
    for (uint32_t i = 0; i < num_to_rank; i++) {
        rankings[i].neuron_id = i;
        rankings[i].avg_activation = network->neuron_states[i].activation_mean;
        rankings[i].activation_count = network->neuron_states[i].sample_count;

        // Importance = activation_mean * sqrt(activation_count) / variance
        float variance = (network->neuron_states[i].sample_count > 1)
                             ? network->neuron_states[i].activation_variance /
                                   (network->neuron_states[i].sample_count - 1)
                             : 1.0F;

        float act_count = (float) rankings[i].activation_count;
        float importance = rankings[i].avg_activation *
                           sqrtf(act_count > 0.0f ? act_count : 0.0f) / (variance + 1e-6F);
        rankings[i].importance = isfinite(importance) ? importance : 0.0f;

        rankings[i].most_active_for = NULL;  // TODO: Track pattern associations
    }

    // Sort by importance (descending) using consolidated nimcp_sort
    nimcp_sort(rankings, num_to_rank, sizeof(neuron_importance_t),
               compare_neuron_importance_desc);

    return num_to_rank;
}

uint32_t adaptive_network_explain(adaptive_network_t network, const float* input,
                                  uint32_t input_size, char* explanation, uint32_t max_length)
{
    if (!network || !input || !explanation || max_length == 0)
        return 0;

    // Analyze activation
    activation_analysis_t analysis;
    if (!adaptive_network_analyze_activation(network, input, input_size, &analysis)) {
        return 0;
    }

    // Generate explanation
    int written = snprintf(explanation, max_length,
                           "Activated %u/%u neurons (%.1f%% sparse). "
                           "Confidence: %.2f. "
                           "Top contributors: ",
                           analysis.num_active_neurons, network->config.base_config.output_size,
                           analysis.sparsity * 100.0F, analysis.confidence);

    // M-1: Check snprintf return value — negative on encoding error,
    //       >= max_length means output was truncated (no room for more)
    if (written < 0 || (uint32_t)written >= max_length) {
        nimcp_free(analysis.active_neuron_ids);
        nimcp_free(analysis.activation_strengths);
        return (written < 0) ? 0 : (uint32_t)written;
    }

    // Add top 3 neurons
    uint32_t top_count = (analysis.num_active_neurons < 3) ? analysis.num_active_neurons : 3;
    for (uint32_t i = 0; i < top_count; i++) {
        int added = snprintf(explanation + written, max_length - written, "N%u(%.2f) ",
                            analysis.active_neuron_ids[i], analysis.activation_strengths[i]);
        if (added < 0 || (uint32_t)(written + added) >= max_length) {
            // Truncated — stop adding more neurons
            if (added > 0) written += added;
            break;
        }
        written += added;
    }

    // Free analysis arrays
    nimcp_free(analysis.active_neuron_ids);
    nimcp_free(analysis.activation_strengths);

    return written;
}

//=============================================================================
// Performance Statistics
//=============================================================================

bool adaptive_network_get_performance(adaptive_network_t network, network_performance_t* stats)
{
    if (!network || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_get_performance: required parameter is NULL (network, stats)");
        return false;
    }

    stats->total_inferences = network->total_inferences;
    stats->total_learning_steps = network->total_learning_steps;
    stats->avg_sparsity = network->running_sparsity;
    stats->avg_inference_time_us = network->running_inference_time_us;
    stats->avg_learning_time_us = network->running_learning_time_us;
    stats->memory_usage_bytes = adaptive_network_get_size(network);
    stats->accuracy = 0.0F;  // TODO: Track validation accuracy
    stats->num_pruned_synapses = network->num_pruned_synapses;

    return true;
}

void adaptive_network_reset_stats(adaptive_network_t network)
{
    if (!network)
        return;

    network->total_inferences = 0;
    network->total_learning_steps = 0;
    network->running_sparsity = 0.0F;
    network->running_inference_time_us = 0.0F;
    network->running_learning_time_us = 0.0F;
}

//=============================================================================
// Introspection & Network State Access (NIMCP 2.5 Consciousness APIs)
//=============================================================================

/**
 * WHAT: Get total number of neurons
 * WHY: Introspection needs to know network size
 * HOW: Return cached neuron count
 */
uint32_t adaptive_network_get_neuron_count(adaptive_network_t network)
{
    if (!network)
        return 0;
    return network->num_neurons;
}

/**
 * WHAT: Get activation level of specific neuron
 * WHY: Introspection needs to query individual neuron states
 * HOW: Access base network neuron state
 */
bool adaptive_network_get_neuron_activation(adaptive_network_t network, uint32_t neuron_id,
                                            float* activation)
{
    if (!network || !activation || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_get_neuron_activation: required parameter is NULL (network, activation)");
        return false;
    }

    /* WHAT: Get neuron state from base network */
    return neural_network_get_neuron_state(network->base_network, neuron_id, activation);
}

/**
 * WHAT: Get list of active neurons above threshold
 * WHY: Introspection brain_get_active_population() needs this
 * HOW: Scan all neurons, collect those above threshold
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
uint32_t adaptive_network_get_active_neurons(adaptive_network_t network, float threshold,
                                             uint32_t* neuron_ids, float* activations,
                                             uint32_t max_neurons)
{
    if (!network || !neuron_ids || !activations || max_neurons == 0) {
        return 0;
    }

    /* WHAT: Scan neurons and collect active ones */
    /* NOTE: Biological potentials need normalization */
    const float REST_POTENTIAL = -65.0F;
    const float PEAK_POTENTIAL = 30.0F;

    uint32_t count = 0;
    for (uint32_t i = 0; i < network->num_neurons && count < max_neurons; i++) {
        float raw_activation = 0.0F;
        if (neural_network_get_neuron_state(network->base_network, i, &raw_activation)) {
            /* WHAT: Normalize to 0-1 range */
            float normalized =
                (raw_activation - REST_POTENTIAL) / (PEAK_POTENTIAL - REST_POTENTIAL);
            if (normalized < 0.0F)
                normalized = 0.0F;
            if (normalized > 1.0F)
                normalized = 1.0F;

            if (normalized >= threshold) {
                neuron_ids[count] = i;
                activations[count] = normalized;
                count++;
            }
        }
    }

    return count;
}

/**
 * WHAT: Get connection count for a neuron
 * WHY: Introspection needs neuron topology info
 * HOW: Access base network neuron synapse count
 */
bool adaptive_network_get_connection_count(adaptive_network_t network, uint32_t neuron_id,
                                           uint32_t* num_connections)
{
    if (!network || !num_connections || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_get_connection_count: required parameter is NULL (network, num_connections)");
        return false;
    }

    /* WHAT: Access base network to get neuron */
    /* NOTE: This requires adding a new API to neural_network or
     * making neurons publicly accessible. For now, estimate from
     * network configuration. */

    /* TODO: Add neural_network_get_neuron_info() API */
    /* For now, return average connections based on sparsity */
    uint32_t avg_connections =
        (uint32_t) (network->num_neurons * (1.0F - network->config.spike_params.sparsity_target));
    *num_connections = avg_connections;

    return true;
}

/**
 * WHAT: Get total weight for a neuron
 * WHY: Introspection needs neuron importance metrics
 * HOW: Sum absolute values of connection weights
 */
bool adaptive_network_get_total_weight(adaptive_network_t network, uint32_t neuron_id,
                                       float* total_weight)
{
    if (!network || !total_weight || neuron_id >= network->num_neurons) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_get_total_weight: required parameter is NULL (network, total_weight)");
        return false;
    }

    /* TODO: Add neural_network_get_neuron_weights() API */
    /* For now, estimate based on neuron activation and network stats */
    float activation = 0.0F;
    if (neural_network_get_neuron_state(network->base_network, neuron_id, &activation)) {
        /* Estimate: active neurons have higher total weights */
        /* Use absolute value since total weight should be non-negative */
        *total_weight = fabsf(activation) * 10.0F; /* Rough estimate */
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adaptive_network_get_total_weight: validation failed");
    return false;
}

/**
 * WHAT: Get base network handle
 * WHY: Allow consciousness APIs direct access when needed
 * HOW: Return internal base_network pointer
 *
 * WARNING: This exposes internals - use carefully!
 */
neural_network_t adaptive_network_get_base_network(adaptive_network_t network)
{
    if (!network) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;

    }
    return network->base_network;
}

//=============================================================================
// GPU Inference Accessors
//=============================================================================

void adaptive_network_set_gpu_context(adaptive_network_t network, struct nimcp_gpu_context_s* ctx)
{
    if (!network) return;
    // Don't destroy existing — brain owns the context, not us
    network->gpu_ctx = ctx;
}

void adaptive_network_set_gpu_weight_cache(adaptive_network_t network, struct nimcp_gpu_weight_cache_s* cache)
{
    if (!network) return;
    // Destroy existing cache if we owned one
    if (network->gpu_weight_cache && network->gpu_weight_cache != cache) {
        nimcp_gpu_weight_cache_destroy(network->gpu_weight_cache);
    }
    network->gpu_weight_cache = cache;
}

struct nimcp_gpu_weight_cache_s* adaptive_network_get_gpu_weight_cache(adaptive_network_t network)
{
    if (!network) return NULL;
    return network->gpu_weight_cache;
}

void adaptive_network_set_gpu_enabled(adaptive_network_t network, bool enabled)
{
    if (!network) return;
    network->gpu_enabled = enabled;
}

bool adaptive_network_is_gpu_enabled(adaptive_network_t network)
{
    if (!network) return false;
    return network->gpu_enabled;
}

void adaptive_network_mark_gpu_weights_dirty(adaptive_network_t network)
{
    if (!network) return;
    if (network->gpu_weight_cache) {
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
    }
}

void adaptive_network_reset_ema(adaptive_network_t network)
{
    if (!network) return;
    /* C11 _Atomic float supports direct assignment with sequential consistency */
    network->ema_grad_norm = -1.0f;
    network->ema_loss = -1.0f;
}

void adaptive_network_rebuild_gpu_cache(adaptive_network_t network)
{
    if (!network || !network->gpu_enabled) return;

    /* Destroy old cache */
    if (network->gpu_weight_cache) {
        nimcp_gpu_weight_cache_destroy(network->gpu_weight_cache);
        network->gpu_weight_cache = NULL;
    }

    /* Recreate from scratch using current network state */
    if (network->gpu_ctx &&
        network->config.base_config.num_layers >= 2 &&
        network->config.base_config.layer_sizes &&
        network->base_network) {
        network->gpu_weight_cache = nimcp_gpu_weight_cache_create(
            network->gpu_ctx,
            network->base_network,
            network->config.base_config.layer_sizes,
            network->config.base_config.num_layers);
        if (network->gpu_weight_cache) {
            nimcp_gpu_weight_cache_upload(network->gpu_weight_cache,
                                         network->base_network);
            fprintf(stderr, "[REINIT] GPU weight cache rebuilt\n");
        }
    }
}

void adaptive_network_invalidate_gpu_structure(adaptive_network_t network)
{
    if (!network || !network->gpu_weight_cache) return;
    if (network->gpu_weight_cache->connected_dst_valid &&
        network->gpu_weight_cache->connected_dst) {
        uint32_t num_transitions = network->gpu_weight_cache->num_layers - 1;
        for (uint32_t l = 0; l < num_transitions; l++) {
            nimcp_free(network->gpu_weight_cache->connected_dst[l]);
            network->gpu_weight_cache->connected_dst[l] = NULL;
        }
        network->gpu_weight_cache->connected_dst_valid = false;
    }
    __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, true, __ATOMIC_RELEASE);
}

//=============================================================================
// Neuron (Inferentia) Inference Accessors
//=============================================================================

void adaptive_network_set_neuron_cache(adaptive_network_t network, struct nimcp_neuron_inference_cache* cache)
{
    if (!network) return;
    // Destroy existing cache if we owned one
    if (network->neuron_cache && network->neuron_cache != cache) {
        nimcp_neuron_cache_destroy(network->neuron_cache);
    }
    network->neuron_cache = cache;
}

void adaptive_network_set_neuron_enabled(adaptive_network_t network, bool enabled)
{
    if (!network) return;
    network->neuron_enabled = enabled;
}

bool adaptive_network_is_neuron_enabled(adaptive_network_t network)
{
    if (!network) return false;
    return network->neuron_enabled;
}

//=============================================================================
// Frozen Network Support
//=============================================================================

void adaptive_network_freeze(adaptive_network_t network)
{
    if (!network) return;

    // Mark frozen
    network->frozen = true;

    // Clear dirty flag — weights are final
    if (network->gpu_weight_cache) {
        __atomic_store_n(&network->gpu_weight_cache->weights_dirty_on_cpu, false, __ATOMIC_RELEASE);
    }
}

bool adaptive_network_is_frozen(adaptive_network_t network)
{
    if (!network) return false;
    return network->frozen;
}

float adaptive_network_get_last_grad_norm(adaptive_network_t network)
{
    if (!network) return 0.0f;
    return network->last_grad_norm;
}

float adaptive_network_get_running_accuracy(adaptive_network_t network)
{
    if (!network) return 0.0f;
    return atomic_load_explicit(&network->running_accuracy_ema, memory_order_relaxed);
}

uint32_t adaptive_network_get_layer_grad_norms(adaptive_network_t network,
                                                float* out_norms,
                                                uint32_t max_layers)
{
    if (!network || !out_norms || max_layers == 0) return 0;
    uint32_t n = network->num_grad_layers;
    if (n > max_layers) n = max_layers;
    if (n > BP_MAX_GRAD_LAYERS) n = BP_MAX_GRAD_LAYERS;
    memcpy(out_norms, network->last_layer_grad_norms, n * sizeof(float));
    return n;
}

float adaptive_network_get_ema_grad_norm(adaptive_network_t network)
{
    if (!network) return 0.0f;
    return network->ema_grad_norm;
}

float adaptive_network_get_ema_loss(adaptive_network_t network)
{
    if (!network) return 0.0f;
    return network->ema_loss;
}

/**
 * WHAT: Compute weight statistics by sampling backbone neurons
 * WHY:  Real-time monitoring of whether weights are actually updating
 * HOW:  Sample every Nth neuron's outgoing synapses, compute L2 norm,
 *       mean absolute weight, max weight, and total sampled connections
 */
void adaptive_network_weight_stats(adaptive_network_t network,
                                   float* out_weight_l2_norm,
                                   float* out_mean_abs_weight,
                                   float* out_max_abs_weight,
                                   uint64_t* out_num_sampled_synapses)
{
    if (!network || !network->base_network) {
        if (out_weight_l2_norm) *out_weight_l2_norm = 0.0f;
        if (out_mean_abs_weight) *out_mean_abs_weight = 0.0f;
        if (out_max_abs_weight) *out_max_abs_weight = 0.0f;
        if (out_num_sampled_synapses) *out_num_sampled_synapses = 0;
        return;
    }

    neural_network_t nn = network->base_network;
    uint32_t total = neural_network_get_num_neurons(nn);

    // Sample every Nth neuron to keep this O(sample_size) not O(total_synapses)
    // For 1.5M neurons, step=100 gives ~15K neurons sampled
    uint32_t step = total > 15000 ? total / 15000 : 1;

    double sum_sq = 0.0;
    double sum_abs = 0.0;
    float max_abs = 0.0f;
    uint64_t count = 0;

    for (uint32_t i = 0; i < total; i += step) {
        neuron_t* n = neural_network_get_neuron(nn, i);
        if (!n) continue;
        // Read from INCOMING handles — these are the weights that the GPU cache
        // and backprop use. Outgoing handles are stale after backprop updates.
        uint32_t nin = NEURON_IN_COUNT(n);
        for (uint32_t j = 0; j < nin; j++) {
            synapse_handle_t* h = NEURON_IN_HANDLE(n, j);
            if (!h) continue;
            float w = h->weight;
            float aw = w < 0 ? -w : w;
            sum_sq += (double)w * (double)w;
            sum_abs += (double)aw;
            if (aw > max_abs) max_abs = aw;
            count++;
        }
    }

    if (out_weight_l2_norm) *out_weight_l2_norm = (float)sqrt(sum_sq);
    if (out_mean_abs_weight) *out_mean_abs_weight = count > 0 ? (float)(sum_abs / (double)count) : 0.0f;
    if (out_max_abs_weight) *out_max_abs_weight = max_abs;
    if (out_num_sampled_synapses) *out_num_sampled_synapses = count;
}

//=============================================================================
// Learning Quality Probes
//=============================================================================

/**
 * WHAT: Update probe tracking state after a learn step
 * WHY:  Enable per-label accuracy, confidence calibration, prediction diversity
 *       and learning velocity tracking for conversational readiness assessment
 * HOW:  Atomic increments on per-label and per-bin counters; no mutex needed
 *
 * COMPLEXITY: O(1) — constant-time counter updates
 */
void adaptive_network_update_probe_tracking(adaptive_network_t network,
                                            uint32_t label_index,
                                            float loss,
                                            float confidence,
                                            bool was_correct)
{
    if (!network)
        return;

    (void)loss;  // Reserved for future use (e.g., loss-weighted metrics)

    // --- Per-label accuracy ---
    if (label_index < 2048) {
        atomic_fetch_add_explicit(&network->label_accuracy[label_index].total,
                                  1, memory_order_relaxed);
        if (was_correct) {
            atomic_fetch_add_explicit(&network->label_accuracy[label_index].correct,
                                      1, memory_order_relaxed);
        }
        // Track high-water mark of labels seen
        uint32_t cur_size = atomic_load_explicit(&network->label_accuracy_size,
                                                 memory_order_relaxed);
        while (label_index >= cur_size) {
            // CAS to bump label_accuracy_size if we're the one that discovers a new label
            if (atomic_compare_exchange_weak_explicit(&network->label_accuracy_size,
                                                      &cur_size, label_index + 1,
                                                      memory_order_relaxed,
                                                      memory_order_relaxed))
                break;
            // CAS failed — cur_size was reloaded by the CAS, retry loop
        }
    }

    // --- Confidence calibration bins ---
    if (isfinite(confidence)) {
        float clamped = fmaxf(0.0f, fminf(confidence, 1.0f));
        uint32_t bin = (uint32_t)(clamped * 10.0f);
        if (bin >= 10) bin = 9;  // [0.9, 1.0] maps to bin 9
        atomic_fetch_add_explicit(&network->confidence_bins[bin].total,
                                  1, memory_order_relaxed);
        if (was_correct) {
            atomic_fetch_add_explicit(&network->confidence_bins[bin].correct,
                                      1, memory_order_relaxed);
        }
    }

    // --- Prediction diversity (track which labels are predicted) ---
    if (label_index < 2048) {
        atomic_fetch_add_explicit(&network->recent_predictions[label_index],
                                  1, memory_order_relaxed);
        atomic_fetch_add_explicit(&network->recent_prediction_total,
                                  1, memory_order_relaxed);
    }

    // --- Learning velocity: record rolling overall accuracy ---
    // Snapshot overall accuracy every 256 learn steps to keep history sparse
    uint32_t total_steps = (uint32_t)network->total_learning_steps;
    if ((total_steps & 0xFF) == 0 && total_steps > 0) {
        // Compute current global accuracy from label_accuracy counters
        uint32_t nlabels = atomic_load_explicit(&network->label_accuracy_size,
                                                memory_order_relaxed);
        uint64_t sum_correct = 0, sum_total = 0;
        for (uint32_t i = 0; i < nlabels && i < 2048; i++) {
            sum_correct += atomic_load_explicit(&network->label_accuracy[i].correct,
                                                memory_order_relaxed);
            sum_total += atomic_load_explicit(&network->label_accuracy[i].total,
                                              memory_order_relaxed);
        }
        float acc = (sum_total > 0) ? (float)sum_correct / (float)sum_total : 0.0f;

        uint32_t idx = atomic_load_explicit(&network->accuracy_history_idx,
                                            memory_order_relaxed);
        network->accuracy_history[idx & 63] = acc;
        atomic_store_explicit(&network->accuracy_history_idx,
                              (idx + 1) & 63, memory_order_relaxed);
        uint32_t cnt = atomic_load_explicit(&network->accuracy_history_count,
                                            memory_order_relaxed);
        if (cnt < 64) {
            atomic_store_explicit(&network->accuracy_history_count,
                                  cnt + 1, memory_order_relaxed);
        }
    }
}

/**
 * WHAT: Get per-label accuracy for a specific label index
 * WHY:  Enable fine-grained per-domain accuracy reporting
 * HOW:  Read atomic counters, compute ratio
 *
 * COMPLEXITY: O(1)
 */
float adaptive_network_get_label_accuracy(adaptive_network_t network,
                                          uint32_t label_index)
{
    if (!network || label_index >= 2048)
        return 0.0f;
    uint32_t total = atomic_load_explicit(&network->label_accuracy[label_index].total,
                                          memory_order_relaxed);
    if (total == 0)
        return 0.0f;
    uint32_t correct = atomic_load_explicit(&network->label_accuracy[label_index].correct,
                                            memory_order_relaxed);
    return (float)correct / (float)total;
}

/**
 * WHAT: Compute comprehensive learning quality metrics
 * WHY:  Aggregate per-label accuracy, calibration, velocity, diversity, growth
 *       into a single struct for probe/dashboard consumption
 * HOW:
 *   - mean/worst label accuracy: iterate tracked labels
 *   - confidence calibration: MAE across 10 bins (|bin_accuracy - bin_midpoint|)
 *   - learning velocity: linear regression slope over accuracy_history window
 *   - prediction entropy: Shannon entropy -sum(p_i * log2(p_i))
 *   - synapse growth: delta from prev_synapse_count
 *
 * COMPLEXITY: O(num_labels) for accuracy scan, O(64) for velocity, O(2048) for entropy
 */
void adaptive_network_learning_quality(adaptive_network_t network,
                                       adaptive_learning_quality_t* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(adaptive_learning_quality_t));

    if (!network)
        return;

    // --- Per-label accuracy (mean and worst) ---
    uint32_t nlabels = atomic_load_explicit(&network->label_accuracy_size,
                                            memory_order_relaxed);
    if (nlabels > 2048) nlabels = 2048;
    out->num_labels_tracked = nlabels;

    if (nlabels > 0) {
        double acc_sum = 0.0;
        float worst = 1.0f;
        for (uint32_t i = 0; i < nlabels; i++) {
            uint32_t total = atomic_load_explicit(&network->label_accuracy[i].total,
                                                  memory_order_relaxed);
            float acc = 0.0f;
            if (total > 0) {
                uint32_t correct = atomic_load_explicit(&network->label_accuracy[i].correct,
                                                        memory_order_relaxed);
                acc = (float)correct / (float)total;
            }
            acc_sum += acc;
            if (acc < worst)
                worst = acc;
        }
        out->mean_label_accuracy = (float)(acc_sum / (double)nlabels);
        out->worst_label_accuracy = worst;
    }

    // --- Confidence calibration (ECE — Expected Calibration Error) ---
    {
        double mae_sum = 0.0;
        uint32_t bins_with_data = 0;
        for (uint32_t b = 0; b < 10; b++) {
            uint32_t total = atomic_load_explicit(&network->confidence_bins[b].total,
                                                  memory_order_relaxed);
            if (total == 0) continue;
            uint32_t correct = atomic_load_explicit(&network->confidence_bins[b].correct,
                                                    memory_order_relaxed);
            float bin_accuracy = (float)correct / (float)total;
            float bin_midpoint = (float)b * 0.1f + 0.05f;
            float err = bin_accuracy - bin_midpoint;
            mae_sum += (err < 0.0f) ? -err : err;
            bins_with_data++;
        }
        out->confidence_calibration = (bins_with_data > 0)
            ? (float)(mae_sum / (double)bins_with_data)
            : 0.0f;
    }

    // --- Learning velocity (linear regression slope over accuracy_history) ---
    {
        uint32_t count = atomic_load_explicit(&network->accuracy_history_count,
                                              memory_order_relaxed);
        if (count >= 2) {
            uint32_t head = atomic_load_explicit(&network->accuracy_history_idx,
                                                 memory_order_relaxed);
            // Reconstruct oldest-to-newest order from circular buffer
            // head points to the NEXT write position, so the oldest entry is at head
            // if the buffer is full, otherwise at index 0
            double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
            for (uint32_t i = 0; i < count; i++) {
                // Read from oldest to newest
                uint32_t buf_idx = (count < 64)
                    ? i
                    : (head + i) & 63;
                double x = (double)i;
                double y = (double)network->accuracy_history[buf_idx];
                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_x2 += x * x;
            }
            double n = (double)count;
            double denom = n * sum_x2 - sum_x * sum_x;
            if (denom > 1e-12) {
                out->learning_velocity = (float)((n * sum_xy - sum_x * sum_y) / denom);
            }
        }
    }

    // --- Prediction entropy (Shannon entropy of recent prediction distribution) ---
    {
        uint32_t total = atomic_load_explicit(&network->recent_prediction_total,
                                              memory_order_relaxed);
        if (total > 1) {  // W4-22: require >1 to avoid degenerate entropy
            double entropy = 0.0;
            double inv_total = 1.0 / (double)total;
            for (uint32_t i = 0; i < 2048; i++) {
                uint32_t cnt = atomic_load_explicit(&network->recent_predictions[i],
                                                    memory_order_relaxed);
                if (cnt == 0) continue;
                double p = (double)cnt * inv_total;
                entropy -= p * log2(p);
            }
            out->prediction_entropy = (float)entropy;
        }
    }

    // --- Synapse growth ---
    {
        // Count total synapses across all neurons (sampled for speed)
        neural_network_t nn = network->base_network;
        uint64_t current_count = 0;
        if (nn) {
            uint32_t total_neurons = neural_network_get_num_neurons(nn);
            // Sample every Nth neuron and extrapolate (same strategy as weight_stats)
            uint32_t step = total_neurons > 15000 ? total_neurons / 15000 : 1;
            uint64_t sampled_synapses = 0;
            uint32_t sampled_neurons = 0;
            for (uint32_t i = 0; i < total_neurons; i += step) {
                neuron_t* n = neural_network_get_neuron(nn, i);
                if (!n) continue;
                sampled_synapses += NEURON_OUT_COUNT(n);
                sampled_neurons++;
            }
            if (sampled_neurons > 0 && step > 1) {
                // Extrapolate from sample
                current_count = sampled_synapses * (uint64_t)step;
            } else {
                current_count = sampled_synapses;
            }
        }
        uint64_t prev = atomic_load_explicit(&network->prev_synapse_count,
                                             memory_order_relaxed);
        // Signed delta: growth can be negative (pruning)
        out->synapse_growth = current_count - prev;
        // Update prev for next probe call
        atomic_store_explicit(&network->prev_synapse_count,
                              current_count, memory_order_relaxed);
    }
}

/**
 * WHAT: Get network configuration (read-only)
 * WHY: Enable brain resize to read current config for cloning
 * HOW: Return pointer to internal config structure
 *
 * Phase 2.8: Brain Dynamic Resizing Support
 * COMPLEXITY: O(1) - direct pointer access
 *
 * @param network Adaptive network
 * @return Pointer to config (read-only), or NULL on error
 */
const adaptive_network_config_t* adaptive_network_get_config(adaptive_network_t network)
{
    /* Guard: Validate network handle */
    if (!network) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "network is NULL");

        return NULL;
    }

    /* WHAT: Return pointer to internal config */
    /* NOTE: Returned as const to prevent modification */
    /* WHY: Config should only be changed during creation or explicit reconfigure */
    return &network->config;
}

/**
 * WHAT: Get number of neurons in the adaptive network
 * WHY: Enable topology analysis and community detection
 * HOW: Return neuron count from base network
 *
 * COMPLEXITY: O(1) - direct field access
 *
 * @param network Adaptive network
 * @return Number of neurons, or 0 on error
 */
uint32_t adaptive_network_get_num_neurons(adaptive_network_t network)
{
    /* Guard: Validate network handle */
    if (!network) {
        return 0;
    }

    /* Guard: Validate base network */
    if (!network->base_network) {
        return 0;
    }

    /* Return neuron count from base network */
    return neural_network_get_num_neurons(network->base_network);
}

/**
 * WHAT: Get synapse weight between two neurons
 * WHY: Enable community detection to analyze connectivity patterns
 * HOW: Access neuron synapses to find connection weight
 *
 * COMPLEXITY: O(s) where s = synapses per neuron (typically < 100)
 *
 * @param network Adaptive network
 * @param from_neuron Source neuron ID
 * @param to_neuron Target neuron ID
 * @return Synapse weight, or 0.0f if no connection exists
 */
float adaptive_network_get_synapse_weight(adaptive_network_t network, uint32_t from_neuron, uint32_t to_neuron)
{
    /* Guard: Validate network */
    if (!network || !network->base_network) {
        return 0.0F;
    }

    /* Guard: Validate neuron IDs */
    uint32_t num_neurons = neural_network_get_num_neurons(network->base_network);
    if (from_neuron >= num_neurons || to_neuron >= num_neurons) {
        return 0.0F;
    }

    /* Get source neuron */
    neuron_t* from = neural_network_get_neuron(network->base_network, from_neuron);
    if (!from) {
        return 0.0F;
    }

    /* Search for synapse to target neuron */
    for (uint32_t i = 0; i < NEURON_OUT_COUNT(from); i++) {
        synapse_handle_t* h = NEURON_OUT_HANDLE(from, i);
        if (h && h->target_neuron_id == to_neuron) {
            return h->weight;
        }
    }

    /* No connection found */
    return 0.0F;
}

//=============================================================================
// Phase COW: Copy-on-Write Support for Training Snapshots
//=============================================================================

/**
 * @brief Enable COW-backed state storage for the adaptive network
 *
 * WHAT: Convert neuron states to COW-backed storage
 * WHY:  Enable O(1) snapshots for training rollback and checkpointing
 * HOW:  Create COW region from existing states, redirect pointer
 *
 * @param network Adaptive network
 * @return true on success, false if already using COW or on failure
 *
 * COMPLEXITY: O(n) where n = neuron_states size (one-time copy)
 */
bool adaptive_network_enable_cow_states(adaptive_network_t network)
{
    if (!network || !network->neuron_states) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_enable_cow_states: required parameter is NULL (network, network->neuron_states)");
        return false;
    }

    if (network->uses_cow_states) {
        return true;  // Already enabled
    }

    // Calculate states size
    size_t states_size = network->num_neurons * sizeof(adaptive_neuron_state_t);
    size_t aligned_size = page_cow_align_size(states_size);

    // Create COW region with existing states
    page_cow_config_t config = page_cow_default_config(aligned_size);
    config.enable_tracking = true;

    network->cow_states_region = page_cow_region_create(&config, network->neuron_states);
    if (!network->cow_states_region) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_enable_cow_states: network->cow_states_region is NULL");
        return false;
    }

    // Create view
    network->cow_states_view = page_cow_view_create(network->cow_states_region);
    if (!network->cow_states_view) {
        page_cow_region_destroy(network->cow_states_region);
        network->cow_states_region = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_enable_cow_states: network->cow_states_view is NULL");
        return false;
    }

    // Free original states and point to COW view
    nimcp_free(network->neuron_states);
    network->neuron_states = (adaptive_neuron_state_t*)page_cow_view_write(network->cow_states_view);
    network->uses_cow_states = true;

    return network->neuron_states != NULL;
}

/**
 * @brief Create instant snapshot of network state for rollback
 *
 * WHAT: O(1) snapshot of neuron states
 * WHY:  Enable rollback during training (e.g., failed batch)
 * HOW:  Page-level COW snapshot, copies only on subsequent writes
 *
 * @param network Adaptive network (must have COW enabled)
 * @return Snapshot handle, or NULL on failure
 *
 * COMPLEXITY: O(1) - No data copying
 */
page_cow_snapshot_t adaptive_network_snapshot_states(adaptive_network_t network)
{
    if (!network || !network->uses_cow_states || !network->cow_states_view) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_snapshot_states: required parameter is NULL (network, network->uses_cow_states, network->cow_states_view)");
        return NULL;
    }

    return page_cow_snapshot_create(network->cow_states_view);
}

/**
 * @brief Restore network state from snapshot
 *
 * WHAT: Rollback neuron states to snapshot state
 * WHY:  Recover from failed training iterations
 * HOW:  Page-level restore, discards private pages
 *
 * @param network Adaptive network
 * @param snapshot Snapshot to restore from
 * @return true on success
 *
 * COMPLEXITY: O(num_modified_pages)
 */
bool adaptive_network_restore_states(adaptive_network_t network,
                                     page_cow_snapshot_t snapshot)
{
    if (!network || !network->uses_cow_states || !network->cow_states_view || !snapshot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adaptive_network_restore_states: required parameter is NULL (network, network->uses_cow_states, network->cow_states_view, snapshot)");
        return false;
    }

    bool success = page_cow_snapshot_restore(network->cow_states_view, snapshot);
    if (success) {
        // Re-acquire writable pointer after restore
        network->neuron_states = (adaptive_neuron_state_t*)page_cow_view_write(network->cow_states_view);
    }

    return success;
}

/**
 * @brief Get memory savings from COW state sharing
 *
 * @param network Adaptive network
 * @return Bytes saved by sharing, or 0 if not using COW
 */
size_t adaptive_network_get_cow_savings(adaptive_network_t network)
{
    if (!network || !network->uses_cow_states || !network->cow_states_view) {
        return 0;
    }

    return page_cow_view_get_memory_saved(network->cow_states_view);
}
