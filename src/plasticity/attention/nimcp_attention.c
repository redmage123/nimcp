/**
 * @file nimcp_attention.c
 * @brief Implementation of biology-based multihead attention
 *
 * WHAT: Cortical column-inspired parallel attention with thalamic gating
 * WHY:  Enable selective, biologically-plausible focus on relevant features
 * HOW:  Multiple specialized attention heads process input in parallel,
 *       modulated by thalamic gate for top-down control
 *
 * DESIGN: Strict Single Responsibility Principle
 * - Each function does ONE thing
 * - No nested if/for structures
 * - Early returns with guard clauses
 * - Extract helper functions liberally
 */

#include "plasticity/attention/nimcp_attention.h"
#include "plasticity/nimcp_plasticity_constants.h"
#include "cognitive/attention/nimcp_attention_snn_bridge.h"
#include "cognitive/attention/nimcp_attention_plasticity_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory_pool.h"  // Phase MP: Memory pool for hot paths
#include "utils/memory/nimcp_page_cow.h"     // Phase COW: Copy-on-write for weight sharing
#include "utils/containers/nimcp_vector.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/encoding/nimcp_positional_encoding.h"
#include "utils/algorithms/nimcp_monte_carlo.h"  // MC integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "security/nimcp_security.h"
#include "core/brain/nimcp_brain_kg_helpers.h"  // KG self-awareness integration
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: shared event + read helpers
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/geometry/nimcp_differential_geometry.h"

//=============================================================================
// Monte Carlo Integration - GPU acceleration with CPU fallback
//=============================================================================

static __thread uint32_t g_attention_mc_seed = 0;

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

/* Module-level GPU resources - initialized on first use */
static nimcp_gpu_context_t* g_attention_gpu_ctx = NULL;
static qmc_gpu_rng_t g_attention_gpu_rng = NULL;
static bool g_attention_gpu_init_attempted = false;

/**
 * @brief Initialize GPU resources for attention MC operations
 */
static bool attention_init_gpu_mc(void) {
    if (g_attention_gpu_init_attempted) {
        return g_attention_gpu_rng != NULL;
    }
    g_attention_gpu_init_attempted = true;

    if (!qmc_gpu_is_available()) {
        return false;  /* GPU unavailable - normal fallback to CPU */
    }

    g_attention_gpu_ctx = nimcp_gpu_context_create_auto();
    if (!g_attention_gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_init_gpu_mc: g_attention_gpu_ctx is NULL");
        return false;
    }

    g_attention_gpu_rng = qmc_gpu_rng_create(g_attention_gpu_ctx, 4096, 0);
    if (!g_attention_gpu_rng) {
        nimcp_gpu_context_destroy(g_attention_gpu_ctx);
        g_attention_gpu_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_init_gpu_mc: g_attention_gpu_rng is NULL");
        return false;
    }

    return true;
}

static inline bool attention_has_gpu_mc(void) {
    if (!g_attention_gpu_init_attempted) {
        attention_init_gpu_mc();
    }
    return g_attention_gpu_rng != NULL;
}

#else  /* !NIMCP_ENABLE_CUDA */
static inline bool attention_has_gpu_mc(void) { return false; }
#endif /* NIMCP_ENABLE_CUDA */

/* WHAT: Quantum attention bridge integration
 * WHY:  O(√N) speedup for attention head selection
 * HOW:  Header-only implementation activated via define
 * NOTE: Disabled due to API mismatch with actual quantum_attention implementation
 *       The bridge expects Grover-based selection, but the implementation uses
 *       ternary logic and quantum annealing. Needs API alignment.
 */
/* TODO: Fix quantum bridge API to match nimcp_quantum_attention.h
#define NIMCP_ATTENTION_QUANTUM_BRIDGE_IMPLEMENTATION
#include "plasticity/attention/nimcp_attention_quantum_bridge.h"
*/

#define LOG_MODULE "plasticity_attention"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(attention)

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

//=============================================================================
// Deferred Callback Buffer (bounded, overflow-safe)
//=============================================================================

/**
 * WHAT: Maximum number of registered callback subscribers
 * WHY:  Bounded to prevent unbounded growth of subscriber list
 */
#define ATTENTION_MAX_SUBSCRIBERS 16

/**
 * WHAT: Single deferred callback entry queued during forward pass
 * WHY:  Store event data for invocation after computation completes
 * HOW:  Populated during forward pass, invoked after critical section
 */
typedef struct {
    attention_event_type_t event_type;
    uint32_t head_index;
    float value;
} deferred_attention_event_t;

/**
 * WHAT: Subscriber registration entry
 * WHY:  Track registered callback functions with their user data
 */
typedef struct {
    attention_deferred_callback_t callback;
    void* user_data;
    bool active;
} attention_subscriber_t;

//=============================================================================
// Memory Pool for Attention Workspace (Phase MP)
//=============================================================================

/**
 * @brief Memory pool for attention forward pass workspace
 *
 * WHAT: Global memory pool for projection and score buffers
 * WHY:  Forward pass allocates multiple workspace buffers - O(1) vs O(log n)
 * HOW:  Lazily initialized pool with blocks sized for typical attention dims
 */
#define ATTENTION_POOL_BLOCK_SIZE 16384  // 16KB - fits 4096 floats
#define ATTENTION_POOL_NUM_BLOCKS 128    // 128 concurrent buffers

/**
 * Pool initialization states for lock-free state machine
 *
 * WHAT: Three-state atomic flag for pool initialization
 * WHY:  atomic_flag spinlock had a race: the flag_clear (release) could reorder
 *       with atomic_store of the pool pointer, allowing a thread to see the pool
 *       as non-NULL before the underlying memory was fully visible. Using explicit
 *       memory ordering on a three-state atomic eliminates this.
 * HOW:  UNINIT -> INITIALIZING (CAS) -> READY (store with release)
 *       Waiters spin on state == INITIALIZING with acquire loads
 */
#define POOL_STATE_UNINIT       0
#define POOL_STATE_INITIALIZING 1
#define POOL_STATE_READY        2

static _Atomic(memory_pool_t) g_attention_pool = NULL;
static atomic_int g_pool_init_state = POOL_STATE_UNINIT;

/**
 * @brief Get or create the attention memory pool
 *
 * WHAT: Thread-safe lazy initialization of global attention memory pool
 * WHY:  Prevents race condition where two threads both create pools,
 *       causing memory leak from first pool being orphaned. Also prevents
 *       use of partially-initialized pool via proper memory ordering.
 * HOW:  Three-state atomic CAS pattern with explicit acquire/release ordering:
 *       1. Fast path: acquire-load pool pointer (non-NULL = ready)
 *       2. Slow path: CAS state UNINIT -> INITIALIZING (only one winner)
 *       3. Winner creates pool, stores with release ordering
 *       4. Losers spin until state transitions to READY
 *
 * MEMORY ORDERING:
 * - atomic_load_explicit(&g_attention_pool, memory_order_acquire) ensures
 *   all writes from the initializing thread are visible when pool != NULL
 * - atomic_store_explicit(&g_attention_pool, pool, memory_order_release) ensures
 *   pool internals are fully written before pointer becomes visible
 * - CAS uses memory_order_acq_rel for both the state transition and visibility
 */
static memory_pool_t get_attention_pool(void) {
    /* Fast path: pool already initialized - acquire ensures we see all init writes */
    memory_pool_t pool = atomic_load_explicit(&g_attention_pool, memory_order_acquire);
    if (pool != NULL) {
        return pool;
    }

    /* Slow path: try to become the initializer */
    int expected = POOL_STATE_UNINIT;
    if (atomic_compare_exchange_strong_explicit(
            &g_pool_init_state, &expected, POOL_STATE_INITIALIZING,
            memory_order_acq_rel, memory_order_acquire)) {
        /* We won the CAS - we are the initializer */
        memory_pool_config_t config = memory_pool_default_config(
            ATTENTION_POOL_BLOCK_SIZE, ATTENTION_POOL_NUM_BLOCKS);
        pool = memory_pool_create(&config);

        /* Release-store the pool pointer so other threads see fully-init'd pool */
        atomic_store_explicit(&g_attention_pool, pool, memory_order_release);

        /* Mark state as READY so spinning threads can proceed */
        atomic_store_explicit(&g_pool_init_state, POOL_STATE_READY, memory_order_release);
        return pool;
    }

    /* Another thread is initializing - spin until ready */
    while (atomic_load_explicit(&g_pool_init_state, memory_order_acquire) != POOL_STATE_READY) {
        /* Yield CPU to avoid burning cycles in tight spin */
#if defined(__x86_64__) || defined(__i386__)
        __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__)
        __asm__ volatile("yield" ::: "memory");
#endif
    }

    /* Acquire-load the now-initialized pool */
    return atomic_load_explicit(&g_attention_pool, memory_order_acquire);
}

/**
 * @brief Allocate workspace buffer from pool or heap
 *
 * P1 fix: Falls back to heap allocation if pool acquisition fails
 * WHY:  Prevents unnecessary failures under high concurrent load
 */
static void* alloc_attention_buffer(size_t size) {
    if (size <= ATTENTION_POOL_BLOCK_SIZE) {
        memory_pool_t pool = get_attention_pool();
        if (pool) {
            void* buf = memory_pool_acquire(pool);
            if (buf) return buf;
            /* Fall through to heap allocation if pool exhausted */
        }
    }
    return nimcp_malloc(size);
}

/**
 * @brief Free workspace buffer to pool or heap
 *
 * P1 fix: Uses atomic acquire-load for thread-safe pool access
 */
static void free_attention_buffer(void* buf) {
    if (!buf) return;
    memory_pool_t pool = atomic_load_explicit(&g_attention_pool, memory_order_acquire);
    if (pool && memory_pool_owns(pool, buf)) {
        memory_pool_release(pool, buf);
    } else {
        nimcp_free(buf);
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * WHAT: Single attention head (cortical column)
 * WHY:  Processes input in specialized representational space
 */
struct attention_head_struct {
    attention_head_config_t config;

    /* WHAT: Learned projection matrices (Q/K/V)
     * WHY:  Project input to query/key/value spaces
     */
    float* query_weights;     // [input_dim × key_dim]
    float* key_weights;       // [input_dim × key_dim]
    float* value_weights;     // [input_dim × value_dim]
    float* output_weights;    // [value_dim × output_dim]

    /* WHAT: Copy-on-Write support for weight sharing (Phase COW)
     * WHY:  Enable O(1) model cloning with lazy copying on write
     */
    bool uses_cow;                        // True if weights use COW
    page_cow_region_t cow_region;         // COW region for combined weights
    page_cow_view_t cow_view;             // View into COW region
    size_t cow_weights_offset[4];         // Offsets: query, key, value, output
};

/**
 * WHAT: Multihead attention system (cortical column network)
 * WHY:  Coordinate multiple parallel attention streams
 */
struct multihead_attention_struct {
    multihead_attention_config_t config;

    /* WHAT: Array of attention heads (cortical columns)
     * WHY:  Each head specializes in different features
     */
    attention_head_t* heads;

    /* WHAT: Thalamic gate state
     * WHY:  Top-down attention modulation
     */
    atomic_uint_fast32_t gate_signal;  // Fixed-point: [0, 1000] → [0.0, 1.0]

    /* WHAT: Statistics (atomic for thread-safe monitoring)
     * WHY:  Track system behavior without locks
     */
    atomic_uint_fast64_t forward_calls;
    atomic_uint_fast32_t avg_entropy_scaled;  // Fixed-point entropy
    atomic_uint_fast32_t avg_gate_scaled;     // Fixed-point gate

    /* WHAT: Positional encoding encoder (Strategy Pattern)
     * WHY:  Support RoPE and ALiBi position encoding strategies
     * HOW:  Created based on config.pe_type, applied in forward pass
     */
    nimcp_pos_encoder_t* pos_encoder;

    /* WHAT: Quantum attention bridge for accelerated head selection
     * WHY:  O(√N) speedup using Grover-inspired amplitude amplification
     * HOW:  Bridge selects high-scoring heads before attention computation
     * NOTE: Disabled until quantum bridge API matches quantum_attention implementation
     */
    /* attention_quantum_bridge_t* quantum_bridge; */

    /* WHAT: Internal Knowledge Graph integration (self-awareness)
     * WHY:  Enable attention to query its own structure and targets
     */
    kg_module_context_t kg_context;
    bool kg_connected;

    /* WHAT: SNN and Plasticity bridge integration
     * WHY:  Enable spike-based attention and attention-modulated learning
     */
    attention_snn_bridge_t* snn_bridge;
    attention_plasticity_bridge_t* plasticity_bridge;
    bool bridges_enabled;

    /* WHAT: Deferred callback buffer (bounded, overflow-safe)
     * WHY:  Queue attention events during forward pass, invoke after completion
     * HOW:  Fixed-size buffer with overflow counter; excess events are dropped
     *       and counted rather than causing buffer overflow
     */
    attention_subscriber_t subscribers[ATTENTION_MAX_SUBSCRIBERS];
    uint32_t num_subscribers;
    deferred_attention_event_t deferred_events[ATTENTION_MAX_DEFERRED_CALLBACKS];
    uint32_t num_deferred;
    atomic_uint_fast64_t deferred_drop_count;  // Overflow counter (thread-safe)
};

//=============================================================================
// Helper Functions - Matrix Operations
//=============================================================================

/**
 * WHAT: Matrix-vector multiplication: y = A * x
 * WHY:  Core linear algebra operation for projections
 * @param matrix Matrix A [m × n]
 * @param vector Vector x [n]
 * @param output Output y [m]
 * @param m Number of rows
 * @param n Number of columns
 * PERFORMANCE: ~O(m*n), SIMD-friendly loop order
 * SRP: Only does matrix-vector multiplication
 */
static void matmul_mv(const float* matrix, const float* vector,
                     float* output, uint32_t m, uint32_t n)
{
    /* WHAT: Guard against NULL pointers
     * WHY:  Prevent segfaults, enable early return
     */
    if (!matrix || !vector || !output) {
        return;
    }

    /* WHAT: Compute each output element
     * WHY:  y[i] = sum(A[i][j] * x[j])
     * NOTE: Single loop, no nesting
     */
    for (uint32_t i = 0; i < m; i++) {
        float sum = 0.0F;
        const float* row = matrix + (i * n);

        /* WHAT: Dot product of row with vector
         * WHY:  This is what matrix-vector multiplication means
         * PERFORMANCE: Compiler can auto-vectorize this
         */
        for (uint32_t j = 0; j < n; j++) {
            sum += row[j] * vector[j];
        }

        output[i] = sum;
    }
}

/**
 * WHAT: Apply softmax to vector in-place
 * WHY:  Convert attention scores to probability distribution
 * @param vector Vector to softmax [n]
 * @param n Vector length
 * @param temperature Softmax temperature (biological gain)
 * PERFORMANCE: ~O(n), numerically stable
 * SRP: Only does softmax transformation
 */
static void softmax_inplace(float* vector, uint32_t n, float temperature)
{
    /* WHAT: Guard clauses
     * WHY:  Early returns avoid nested ifs
     */
    if (!vector || n == 0) {
        return;
    }

    if (temperature <= 0.0F) {
        temperature = 1.0F;
    }

    /* WHAT: Find maximum for numerical stability
     * WHY:  Prevents overflow in exp()
     */
    float max_val = vector[0];
    for (uint32_t i = 1; i < n; i++) {
        if (vector[i] > max_val) {
            max_val = vector[i];
        }
    }

    /* WHAT: Compute exp(scaled values) and sum
     * WHY:  Numerically stable softmax computation
     */
    float sum = 0.0F;
    for (uint32_t i = 0; i < n; i++) {
        float scaled = (vector[i] - max_val) / temperature;
        vector[i] = expf(scaled);
        sum += vector[i];
    }

    /* WHAT: Normalize by sum
     * WHY:  Make it a probability distribution
     */
    if (sum > 0.0F) {
        for (uint32_t i = 0; i < n; i++) {
            vector[i] /= sum;
        }
    }
}

/**
 * WHAT: Apply element-wise scalar multiplication
 * WHY:  Scale vector by thalamic gate or salience
 * @param vector Vector to scale [n]
 * @param scalar Scaling factor
 * @param n Vector length
 * PERFORMANCE: ~O(n)
 * SRP: Only does element-wise scaling
 */
static void scale_vector(float* vector, float scalar, uint32_t n)
{
    if (!vector) {
        return;
    }

    for (uint32_t i = 0; i < n; i++) {
        vector[i] *= scalar;
    }
}

//=============================================================================
// Helper Functions - Projection & Attention
//=============================================================================

/**
 * WHAT: Project input sequence to query space
 * WHY:  Transform input for attention computation
 * @param head Attention head with query weights
 * @param input Input sequence [seq_len × input_dim]
 * @param seq_len Sequence length
 * @param output Query buffer [seq_len × key_dim]
 * PERFORMANCE: ~O(seq_len * input_dim * key_dim)
 * SRP: Only does query projection
 */
static void project_to_query(attention_head_t head,
                            const float* input,
                            uint32_t seq_len,
                            float* output)
{
    if (!head || !input || !output) {
        return;
    }

    const uint32_t input_dim = head->config.input_dim;
    const uint32_t key_dim = head->config.key_dim;

    /* WHAT: Project each token to query space
     * WHY:  Q = Input * W_q
     * NOTE: Single loop, no nesting
     */
    for (uint32_t t = 0; t < seq_len; t++) {
        const float* token = input + (t * input_dim);
        float* query = output + (t * key_dim);
        matmul_mv(head->query_weights, token, query, key_dim, input_dim);
    }
}

/**
 * WHAT: Project input sequence to key space
 * WHY:  Transform input for attention computation
 * SRP: Only does key projection
 */
static void project_to_key(attention_head_t head,
                          const float* input,
                          uint32_t seq_len,
                          float* output)
{
    if (!head || !input || !output) {
        return;
    }

    const uint32_t input_dim = head->config.input_dim;
    const uint32_t key_dim = head->config.key_dim;

    for (uint32_t t = 0; t < seq_len; t++) {
        const float* token = input + (t * input_dim);
        float* key = output + (t * key_dim);
        matmul_mv(head->key_weights, token, key, key_dim, input_dim);
    }
}

/**
 * WHAT: Project input sequence to value space
 * WHY:  Create values to be weighted by attention
 * SRP: Only does value projection
 */
static void project_to_value(attention_head_t head,
                            const float* input,
                            uint32_t seq_len,
                            float* output)
{
    if (!head || !input || !output) {
        return;
    }

    const uint32_t input_dim = head->config.input_dim;
    const uint32_t value_dim = head->config.value_dim;

    for (uint32_t t = 0; t < seq_len; t++) {
        const float* token = input + (t * input_dim);
        float* value = output + (t * value_dim);
        matmul_mv(head->value_weights, token, value, value_dim, input_dim);
    }
}

/**
 * WHAT: Compute attention scores for single query
 * WHY:  Measure relevance of each key to query
 * @param query Query vector [key_dim]
 * @param keys All key vectors [seq_len × key_dim]
 * @param seq_len Number of keys
 * @param key_dim Key dimension
 * @param scores Output scores [seq_len]
 * PERFORMANCE: ~O(seq_len * key_dim)
 * SRP: Only computes raw attention scores
 */
static void compute_attention_scores(const float* query,
                                    const float* keys,
                                    uint32_t seq_len,
                                    uint32_t key_dim,
                                    float* scores)
{
    if (!query || !keys || !scores) {
        return;
    }

    /* For small key dimensions (≤ 64), use Riemannian inner product with a
     * data-dependent metric instead of flat dot product. The metric is built
     * from the query vector: G_ii = 1 + |q_i| (adaptive weighting).
     * This makes attention curvature-aware: dimensions where the query has
     * strong activation get higher metric weight, producing more selective
     * attention scores that focus on informative feature dimensions. */
    const float scale = 1.0F / sqrtf((float)key_dim);

    if (key_dim <= DIFFGEO_MAX_DIM) {
        riemannian_metric_t* attn_metric = riemannian_metric_create(key_dim);
        if (attn_metric) {
            /* Build diagonal metric from query magnitude */
            for (uint32_t d = 0; d < key_dim; d++) {
                attn_metric->g[d * key_dim + d] = 1.0f + fabsf(query[d]);
            }

            for (uint32_t i = 0; i < seq_len; i++) {
                const float* key = keys + (i * key_dim);
                scores[i] = riemannian_inner_product(attn_metric, query, key) * scale;
            }
            riemannian_metric_destroy(attn_metric);
            return;
        }
    }

    /* Fallback: standard scaled dot-product attention */
    for (uint32_t i = 0; i < seq_len; i++) {
        const float* key = keys + (i * key_dim);
        scores[i] = nimcp_vector_dot_product(query, key, key_dim) * scale;
    }
}

/**
 * WHAT: Apply attention weights to values
 * WHY:  Weight values by attention distribution
 * @param attention_weights Attention distribution [seq_len]
 * @param values Value vectors [seq_len × value_dim]
 * @param seq_len Sequence length
 * @param value_dim Value dimension
 * @param output Weighted output [value_dim]
 * PERFORMANCE: ~O(seq_len * value_dim)
 * SRP: Only does weighted sum of values
 */
static void apply_attention_to_values(const float* attention_weights,
                                     const float* values,
                                     uint32_t seq_len,
                                     uint32_t value_dim,
                                     float* output)
{
    if (!attention_weights || !values || !output) {
        return;
    }

    /* WHAT: Initialize output to zero
     * WHY:  We'll accumulate weighted values
     */
    memset(output, 0, value_dim * sizeof(float));

    /* WHAT: Accumulate weighted values
     * WHY:  output = sum(attention[i] * value[i])
     * NOTE: Single loop, no nesting
     */
    for (uint32_t i = 0; i < seq_len; i++) {
        const float weight = attention_weights[i];
        const float* value = values + (i * value_dim);

        for (uint32_t d = 0; d < value_dim; d++) {
            output[d] += weight * value[d];
        }
    }
}

//=============================================================================
// Helper Functions - Biological Modulation
//=============================================================================

/**
 * WHAT: Apply salience-based weighting to attention scores
 * WHY:  Amplify attention to biologically important stimuli
 * @param scores Attention scores [seq_len]
 * @param salience Salience values [seq_len]
 * @param seq_len Sequence length
 * PERFORMANCE: ~O(seq_len)
 * SRP: Only applies salience weighting
 */
static void apply_salience_weighting(float* scores,
                                    const float* salience,
                                    uint32_t seq_len)
{
    if (!scores || !salience) {
        return;
    }

    /* WHAT: Multiply each score by salience
     * WHY:  Boost attention to salient features
     */
    for (uint32_t i = 0; i < seq_len; i++) {
        scores[i] *= salience[i];
    }
}

/**
 * WHAT: Apply thalamic gate to attention output
 * WHY:  Top-down modulation of attention strength
 * @param output Attention output [output_dim]
 * @param gate_signal Gate opening [0.0-1.0]
 * @param output_dim Output dimension
 * PERFORMANCE: ~O(output_dim)
 * SRP: Only applies gating
 */
static void apply_thalamic_gate(float* output,
                               float gate_signal,
                               uint32_t output_dim)
{
    if (!output) {
        return;
    }

    /* WHAT: Guard against invalid gate
     * WHY:  Clamp to valid range
     */
    if (gate_signal < 0.0F) {
        gate_signal = 0.0F;
    }
    if (gate_signal > 1.0F) {
        gate_signal = 1.0F;
    }

    /* WHAT: Scale output by gate
     * WHY:  Gate controls information flow (like thalamus)
     */
    scale_vector(output, gate_signal, output_dim);
}

//=============================================================================
// Deferred Callback Buffer Helpers
//=============================================================================

/**
 * WHAT: Queue a deferred attention event for post-forward-pass invocation
 * WHY:  Avoid invoking user callbacks during critical computation paths
 * HOW:  Append to bounded buffer; if full, increment drop counter instead
 *
 * @param mha Multihead attention system
 * @param event_type Type of attention event
 * @param head_index Index of the triggering head
 * @param value Event-specific value
 * @return 0 on success, -1 if buffer is full (event dropped)
 *
 * THREAD_SAFETY: Not thread-safe, called only from forward pass (single-threaded)
 */
static int attention_defer_event(struct multihead_attention_struct* mha,
                                 attention_event_type_t event_type,
                                 uint32_t head_index,
                                 float value)
{
    if (!mha) {
        return -1;
    }

    /* WHAT: Check buffer capacity before writing
     * WHY:  Prevent buffer overflow - drop event if full
     */
    if (mha->num_deferred >= ATTENTION_MAX_DEFERRED_CALLBACKS) {
        atomic_fetch_add(&mha->deferred_drop_count, 1);
        NIMCP_LOGGING_WARN("Deferred callback buffer full (%u/%u), dropping event type=%d head=%u",
                           mha->num_deferred, (uint32_t)ATTENTION_MAX_DEFERRED_CALLBACKS,
                           (int)event_type, head_index);
        return -1;
    }

    /* WHAT: Append event to deferred buffer
     * WHY:  Will be invoked after forward pass completes
     */
    mha->deferred_events[mha->num_deferred].event_type = event_type;
    mha->deferred_events[mha->num_deferred].head_index = head_index;
    mha->deferred_events[mha->num_deferred].value = value;
    mha->num_deferred++;

    return 0;
}

/**
 * WHAT: Invoke all deferred callbacks and clear buffer
 * WHY:  Process queued events after forward pass completes
 * HOW:  Iterate events, dispatch to all active subscribers, clear buffer
 *
 * @param mha Multihead attention system
 *
 * THREAD_SAFETY: Not thread-safe, called after forward pass completes
 */
static void attention_flush_deferred(struct multihead_attention_struct* mha)
{
    if (!mha || mha->num_deferred == 0) {
        return;
    }

    for (uint32_t i = 0; i < mha->num_deferred; i++) {
        const deferred_attention_event_t* evt = &mha->deferred_events[i];

        for (uint32_t s = 0; s < mha->num_subscribers; s++) {
            if (mha->subscribers[s].active && mha->subscribers[s].callback) {
                mha->subscribers[s].callback(
                    evt->event_type,
                    evt->head_index,
                    evt->value,
                    mha->subscribers[s].user_data
                );
            }
        }
    }

    /* WHAT: Clear deferred buffer after all events dispatched
     * WHY:  Ready for next forward pass
     */
    mha->num_deferred = 0;
}

//=============================================================================
// Attention Head Implementation
//=============================================================================

/**
 * WHAT: Initialize projection matrices with Xavier initialization
 * WHY:  Proper initialization prevents vanishing/exploding gradients
 * SRP: Only initializes weights
 */
static bool initialize_weights(attention_head_t head)
{
    if (!head) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "initialize_weights: head is NULL");
        return false;
    }

    /* Ensure MC seed is initialized */
    if (g_attention_mc_seed == 0) {
        g_attention_mc_seed = mc_seed_from_time();
    }

    const uint32_t input_dim = head->config.input_dim;
    const uint32_t key_dim = head->config.key_dim;
    const uint32_t value_dim = head->config.value_dim;
    const uint32_t output_dim = head->config.output_dim;

    /* WHAT: Xavier initialization scale
     * WHY:  Maintains variance across layers
     */
    const float query_scale = sqrtf(2.0F / (input_dim + key_dim));
    const float key_scale = sqrtf(2.0F / (input_dim + key_dim));
    const float value_scale = sqrtf(2.0F / (input_dim + value_dim));
    const float output_scale = sqrtf(2.0F / (value_dim + output_dim));

    /* WHAT: Initialize each weight matrix using MC sampling
     * WHY:  Random initialization for symmetry breaking
     * NOTE: Single-level loops, no nesting
     */
    for (uint32_t i = 0; i < input_dim * key_dim; i++) {
        head->query_weights[i] = (mc_random_uniform(&g_attention_mc_seed) - 0.5F) * query_scale;
    }

    for (uint32_t i = 0; i < input_dim * key_dim; i++) {
        head->key_weights[i] = (mc_random_uniform(&g_attention_mc_seed) - 0.5F) * key_scale;
    }

    for (uint32_t i = 0; i < input_dim * value_dim; i++) {
        head->value_weights[i] = (mc_random_uniform(&g_attention_mc_seed) - 0.5F) * value_scale;
    }

    for (uint32_t i = 0; i < value_dim * output_dim; i++) {
        head->output_weights[i] = (mc_random_uniform(&g_attention_mc_seed) - 0.5F) * output_scale;
    }

    return true;
}

attention_head_t attention_head_create(const attention_head_config_t* config)
{
    /* WHAT: Validate configuration
     * WHY:  Early return prevents allocation if invalid
     */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_create: config is NULL");
        NIMCP_LOGGING_ERROR("Attention head config is NULL");
        return NULL;
    }

    if (config->input_dim == 0 || config->output_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_head_create: invalid dimensions");
        NIMCP_LOGGING_ERROR("Invalid dimensions in attention head config");
        return NULL;
    }

    /* WHAT: Allocate head structure
     * WHY:  Need storage for head state
     */
    attention_head_t head = nimcp_malloc(sizeof(struct attention_head_struct));
    if (!head) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_create: failed to allocate head");
        NIMCP_LOGGING_ERROR("Failed to allocate attention head");
        return NULL;
    }

    memcpy(&head->config, config, sizeof(attention_head_config_t));

    /* Initialize COW fields to NULL/false (Phase COW) */
    head->uses_cow = false;
    head->cow_region = NULL;
    head->cow_view = NULL;
    memset(head->cow_weights_offset, 0, sizeof(head->cow_weights_offset));

    /* WHAT: Allocate weight matrices
     * WHY:  Need storage for learned parameters
     */
    const uint32_t input_dim = config->input_dim;
    const uint32_t key_dim = config->key_dim;
    const uint32_t value_dim = config->value_dim;
    const uint32_t output_dim = config->output_dim;

    head->query_weights = nimcp_malloc(input_dim * key_dim * sizeof(float));
    head->key_weights = nimcp_malloc(input_dim * key_dim * sizeof(float));
    head->value_weights = nimcp_malloc(input_dim * value_dim * sizeof(float));
    head->output_weights = nimcp_malloc(value_dim * output_dim * sizeof(float));

    /* WHAT: Check all allocations succeeded
     * WHY:  Early cleanup if any allocation failed
     */
    if (!head->query_weights || !head->key_weights ||
        !head->value_weights || !head->output_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_create: failed to allocate weights");
        attention_head_destroy(head);
        return NULL;
    }

    /* WHAT: Initialize weights
     * WHY:  Proper initialization for training
     */
    if (!initialize_weights(head)) {
        attention_head_destroy(head);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "attention_head_create: initialize_weights is NULL");
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("Created attention head: input_dim=%u, output_dim=%u",
                       input_dim, output_dim);

    return head;
}

void attention_head_destroy(attention_head_t head)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return if nothing to destroy
     */
    if (!head) {
        return;
    }

    /* WHAT: Handle COW cleanup (Phase COW)
     * WHY:  COW views/regions must be destroyed before freeing structure
     */
    if (head->uses_cow) {
        if (head->cow_view) {
            page_cow_view_destroy(head->cow_view);
        }
        // Only destroy region if this head owns it (region creator)
        // Clone heads share the region, they only have views
        if (head->cow_region && !head->cow_view) {
            page_cow_region_destroy(head->cow_region);
        }
    } else {
        /* WHAT: Free all allocated resources
         * WHY:  Prevent memory leaks
         * NOTE: nimcp_free handles NULL gracefully
         */
        nimcp_free(head->query_weights);
        nimcp_free(head->key_weights);
        nimcp_free(head->value_weights);
        nimcp_free(head->output_weights);
    }

    nimcp_free(head);
}

bool attention_head_forward(attention_head_t head,
                           const float* query,
                           const float* key,
                           const float* value,
                           uint32_t sequence_length,
                           float* output,
                           float* attention_weights,
                           nimcp_pos_encoder_t* pos_encoder,
                           uint32_t head_idx)
{
    /* WHAT: Validate all inputs
     * WHY:  Early returns prevent crashes
     */
    if (!head) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_forward: head is NULL");
        return false;
    }
    if (!query || !key || !value || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_forward: input/output is NULL");
        return false;
    }

    if (sequence_length == 0) {
        NIMCP_LOGGING_ERROR("Zero sequence length");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_head_forward: sequence_length is zero");
        return false;
    }

    /* WHAT: Get dimensions from config
     * WHY:  Used throughout computation
     */
    const uint32_t key_dim = head->config.key_dim;
    const uint32_t value_dim = head->config.value_dim;
    const uint32_t output_dim = head->config.output_dim;
    const float temperature = head->config.temperature;

    /* WHAT: Determine if using positional encoding
     * WHY:  Need to know PE type for RoPE vs ALiBi processing
     */
    const bool use_rope = pos_encoder &&
                         (nimcp_pos_get_type(pos_encoder) == NIMCP_POS_ROTARY);
    const bool use_alibi = pos_encoder &&
                          (nimcp_pos_get_type(pos_encoder) == NIMCP_POS_ALIBI);

    /* WHAT: Allocate temporary buffers from pool (Phase MP)
     * WHY:  Need workspace for Q/K/V projections - use pool for O(1) allocation
     */
    float* query_proj = alloc_attention_buffer(sequence_length * key_dim * sizeof(float));
    float* key_proj = alloc_attention_buffer(sequence_length * key_dim * sizeof(float));
    float* value_proj = alloc_attention_buffer(sequence_length * value_dim * sizeof(float));
    float* scores = alloc_attention_buffer(sequence_length * sizeof(float));
    float* output_proj = alloc_attention_buffer(value_dim * sizeof(float));

    /* WHAT: Allocate RoPE buffers if needed
     * WHY:  RoPE requires rotated Q/K before attention computation
     */
    float* query_rope = NULL;
    float* key_rope = NULL;
    if (use_rope) {
        query_rope = alloc_attention_buffer(sequence_length * key_dim * sizeof(float));
        key_rope = alloc_attention_buffer(sequence_length * key_dim * sizeof(float));
    }

    /* WHAT: Check allocations
     * WHY:  Early cleanup if allocation failed
     */
    if (!query_proj || !key_proj || !value_proj || !scores || !output_proj ||
        (use_rope && (!query_rope || !key_rope))) {
        free_attention_buffer(query_proj);
        free_attention_buffer(key_proj);
        free_attention_buffer(value_proj);
        free_attention_buffer(scores);
        free_attention_buffer(output_proj);
        free_attention_buffer(query_rope);
        free_attention_buffer(key_rope);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_head_forward: operation failed");
        return false;
    }

    /* WHAT: Project inputs to Q/K/V spaces
     * WHY:  Transform for attention computation
     */
    project_to_query(head, query, sequence_length, query_proj);
    project_to_key(head, key, sequence_length, key_proj);
    project_to_value(head, value, sequence_length, value_proj);

    /* WHAT: Apply RoPE rotation to Q/K if enabled
     * WHY:  Inject relative position information via rotation
     * HOW:  Rotate each Q/K vector by position-dependent angle
     */
    const float* query_final = query_proj;
    const float* key_final = key_proj;

    if (use_rope) {
        for (uint32_t pos = 0; pos < sequence_length; pos++) {
            const float* q_vec = query_proj + (pos * key_dim);
            const float* k_vec = key_proj + (pos * key_dim);
            float* q_out = query_rope + (pos * key_dim);
            float* k_out = key_rope + (pos * key_dim);

            int result = nimcp_pos_rope_apply(pos_encoder, q_vec, k_vec,
                                             pos, q_out, k_out);
            if (result != NIMCP_POS_SUCCESS) {
                NIMCP_LOGGING_ERROR("RoPE application failed at position %u", pos);
                free_attention_buffer(query_proj);
                free_attention_buffer(key_proj);
                free_attention_buffer(value_proj);
                free_attention_buffer(scores);
                free_attention_buffer(output_proj);
                free_attention_buffer(query_rope);
                free_attention_buffer(key_rope);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_head_forward: validation failed");
                return false;
            }
        }
        query_final = query_rope;
        key_final = key_rope;
    }

    /* WHAT: Compute attention for each query token
     * WHY:  Each output depends on all inputs via attention
     * NOTE: Single loop, no nesting
     */
    for (uint32_t t = 0; t < sequence_length; t++) {
        const float* query_vec = query_final + (t * key_dim);

        /* WHAT: Compute attention scores
         * WHY:  Measure relevance of each key to this query
         * HOW:  Use rotated Q/K if RoPE is enabled
         */
        compute_attention_scores(query_vec, key_final, sequence_length,
                                key_dim, scores);

        /* WHAT: Add ALiBi bias if enabled
         * WHY:  Inject position information via additive bias
         * HOW:  bias[i][j] = -slope * |i - j|
         */
        if (use_alibi) {
            /* WHAT: Get ALiBi slope for this head
             * WHY:  Each head has different distance decay rate
             * HOW:  Slopes computed geometrically by encoder
             */
            float head_slope = 0.0F;
            // ALiBi bias: scores[j] += -slope * |t - j|
            for (uint32_t j = 0; j < sequence_length; j++) {
                int distance = (int)t - (int)j;
                if (distance < 0) distance = -distance;
                // Compute slope: 2^(-8 * (head_idx+1) / num_heads)
                // Simplified: use head_idx directly for slope scaling
                head_slope = powf(2.0F, -8.0F * ((float)(head_idx + 1) / 8.0F));
                scores[j] += -head_slope * (float)distance;
            }
        }

        /* WHAT: Apply softmax to get attention distribution
         * WHY:  Convert scores to probabilities
         */
        softmax_inplace(scores, sequence_length, temperature);

        /* WHAT: Store attention weights if requested
         * WHY:  Useful for visualization and debugging
         */
        if (attention_weights) {
            memcpy(attention_weights + (t * sequence_length),
                  scores, sequence_length * sizeof(float));
        }

        /* WHAT: Apply attention to values
         * WHY:  Weighted combination of values
         */
        apply_attention_to_values(scores, value_proj, sequence_length,
                                 value_dim, output_proj);

        /* WHAT: Project to output space
         * WHY:  Final linear transformation
         */
        float* output_vec = output + (t * output_dim);
        matmul_mv(head->output_weights, output_proj, output_vec,
                 output_dim, value_dim);
    }

    /* WHAT: Free temporary buffers (Phase MP: return to pool)
     * WHY:  Prevent memory leaks
     */
    free_attention_buffer(query_proj);
    free_attention_buffer(key_proj);
    free_attention_buffer(value_proj);
    free_attention_buffer(scores);
    free_attention_buffer(output_proj);
    free_attention_buffer(query_rope);
    free_attention_buffer(key_rope);

    return true;
}

//=============================================================================
// Multihead Attention Implementation
//=============================================================================

multihead_attention_t multihead_attention_create(const multihead_attention_config_t* config)
{
    /* WHAT: Validate configuration
     * WHY:  Early return prevents allocation if invalid
     */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_create: config is NULL");
        return NULL;
    }
    if (!attention_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_create: invalid config");
        NIMCP_LOGGING_ERROR("Invalid multihead attention config");
        return NULL;
    }

    /* WHAT: Allocate system structure
     * WHY:  Need storage for multihead state
     */
    multihead_attention_t mha = nimcp_malloc(sizeof(struct multihead_attention_struct));
    if (!mha) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multihead_attention_create: failed to allocate mha");
        NIMCP_LOGGING_ERROR("Failed to allocate multihead attention");
        return NULL;
    }

    memcpy(&mha->config, config, sizeof(multihead_attention_config_t));

    /* WHAT: Initialize atomic counters
     * WHY:  Lock-free statistics tracking
     */
    atomic_init(&mha->forward_calls, 0);
    atomic_init(&mha->avg_entropy_scaled, 0);
    atomic_init(&mha->gate_signal, (uint32_t)(config->gate_bias * 1000.0F));

    /* WHAT: Initialize deferred callback buffer
     * WHY:  Start with empty buffer and no subscribers
     */
    memset(mha->subscribers, 0, sizeof(mha->subscribers));
    mha->num_subscribers = 0;
    mha->num_deferred = 0;
    atomic_init(&mha->deferred_drop_count, 0);

    /* WHAT: Initialize positional encoder to NULL
     * WHY:  Created on-demand via multihead_attention_set_pe_type()
     */
    mha->pos_encoder = NULL;

    /* WHAT: Allocate attention heads
     * WHY:  Need multiple parallel heads (cortical columns)
     */
    mha->heads = nimcp_malloc(config->num_heads * sizeof(attention_head_t));
    if (!mha->heads) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multihead_attention_create: failed to allocate heads array");
        nimcp_free(mha);
        return NULL;
    }

    /* WHAT: Create each attention head
     * WHY:  Each head specializes in different features
     * NOTE: Single loop, no nesting
     */
    const uint32_t head_dim = config->input_dim / config->num_heads;

    for (uint32_t i = 0; i < config->num_heads; i++) {
        attention_head_config_t head_config = {
            .input_dim = config->input_dim,
            .output_dim = head_dim,
            .key_dim = head_dim,
            .value_dim = head_dim,
            .temperature = 1.0F,
            .dropout_rate = 0.0F
        };

        mha->heads[i] = attention_head_create(&head_config);

        /* WHAT: Check if head creation failed
         * WHY:  Early cleanup if any head fails
         */
        if (!mha->heads[i]) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multihead_attention_create: failed to create head %u", i);
            multihead_attention_destroy(mha);
            return NULL;
        }
    }

    /* WHAT: Create positional encoder if enabled
     * WHY:  Support RoPE or ALiBi positional encoding
     * HOW:  Create encoder based on config.pe_type
     */
    if (config->use_positional_encoding) {
        if (!multihead_attention_set_pe_type(mha, config->pe_type)) {
            NIMCP_LOGGING_WARN("Failed to create positional encoder, continuing without PE");
            mha->config.use_positional_encoding = false;
        }
    }

    /* WHAT: Quantum attention integration placeholder
     * WHY:  Reserved for future quantum acceleration
     * NOTE: Currently disabled due to API mismatch between quantum bridge
     *       and actual quantum_attention implementation
     */
    if (config->enable_quantum_attention) {
        NIMCP_LOGGING_WARN("Quantum attention requested but not yet integrated (API mismatch)");
        mha->config.enable_quantum_attention = false;
    }

    /* WHAT: Create SNN and Plasticity bridges
     * WHY:  Enable spike-based attention and attention-modulated learning
     */
    attention_snn_config_t snn_config = attention_snn_config_default();
    snn_config.num_heads = config->num_heads;
    mha->snn_bridge = attention_snn_create(&snn_config);

    attention_plasticity_config_t plasticity_config = attention_plasticity_config_default();
    mha->plasticity_bridge = attention_plasticity_create(&plasticity_config);

    mha->bridges_enabled = (mha->snn_bridge != NULL && mha->plasticity_bridge != NULL);
    if (!mha->bridges_enabled) {
        NIMCP_LOGGING_WARN("SNN/Plasticity bridges not fully created, continuing without bridge integration");
    }

    NIMCP_LOGGING_INFO("Created multihead attention: num_heads=%u, input_dim=%u, pe=%s, quantum=%s",
                      config->num_heads, config->input_dim,
                      config->use_positional_encoding ?
                      nimcp_pos_type_to_string(config->pe_type) : "none",
                      config->enable_quantum_attention ? "enabled" : "disabled");

    return mha;
}

void multihead_attention_destroy(multihead_attention_t mha)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return if nothing to destroy
     */
    if (!mha) {
        return;
    }

    /* WHAT: Destroy all attention heads
     * WHY:  Free head resources
     */
    if (mha->heads) {
        for (uint32_t i = 0; i < mha->config.num_heads; i++) {
            attention_head_destroy(mha->heads[i]);
        }
        nimcp_free(mha->heads);
    }

    /* WHAT: Destroy positional encoder
     * WHY:  Free PE resources
     */
    if (mha->pos_encoder) {
        nimcp_pos_encoder_destroy(mha->pos_encoder);
    }

    /* WHAT: Destroy SNN and Plasticity bridges
     * WHY:  Free bridge resources before freeing main struct
     */
    if (mha->snn_bridge) {
        attention_snn_destroy(mha->snn_bridge);
    }
    if (mha->plasticity_bridge) {
        attention_plasticity_destroy(mha->plasticity_bridge);
    }

    /* NOTE: Quantum bridge cleanup would go here when integrated */

    nimcp_free(mha);
}

bool multihead_attention_forward(multihead_attention_t mha,
                                const float* input,
                                uint32_t sequence_length,
                                const float* salience,
                                float* output)
{
    /* WHAT: Validate inputs
     * WHY:  Early returns prevent crashes
     */
    if (!mha) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_forward: mha is NULL");
        return false;
    }
    if (!input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_forward: input/output is NULL");
        return false;
    }

    if (sequence_length == 0 || sequence_length > mha->config.sequence_length) {
        NIMCP_LOGGING_ERROR("Invalid sequence length");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_forward: sequence_length is zero");
        return false;
    }

    /* WHAT: Get configuration values
     * WHY:  Used throughout computation
     */
    const uint32_t num_heads = mha->config.num_heads;
    const uint32_t input_dim = mha->config.input_dim;
    const uint32_t output_dim = mha->config.output_dim;
    const uint32_t head_dim = input_dim / num_heads;

    /* WHAT: Get thalamic gate signal
     * WHY:  Top-down attention modulation
     */
    const uint32_t gate_scaled = atomic_load(&mha->gate_signal);
    const float gate = (float)gate_scaled / 1000.0F;

    /* WHAT: Allocate temporary buffers from pool (Phase MP)
     * WHY:  Need workspace for head outputs and attention weights - use pool for O(1)
     */
    float* head_outputs = alloc_attention_buffer(num_heads * sequence_length * head_dim * sizeof(float));
    float* attention_weights = alloc_attention_buffer(sequence_length * sequence_length * sizeof(float));

    if (!head_outputs || !attention_weights) {
        free_attention_buffer(head_outputs);
        free_attention_buffer(attention_weights);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "multihead_attention_forward: allocation failed (head_outputs, attention_weights)");
        return false;
    }

    /* NOTE: Quantum head selection would go here when integrated */

    /* WHAT: Process each head independently
     * WHY:  Each head computes attention in parallel (logically)
     * NOTE: Single loop, no nesting
     */
    for (uint32_t h = 0; h < num_heads; h++) {
        float* head_output = head_outputs + (h * sequence_length * head_dim);

        /* WHAT: Capture attention weights from first head for entropy calculation
         * WHY:  Track attention focus characteristics
         */
        float* weights_ptr = (h == 0) ? attention_weights : NULL;

        /* WHAT: Run forward pass for this head
         * WHY:  Compute attention-weighted representation
         * HOW:  Pass positional encoder if enabled
         */
        bool success = attention_head_forward(mha->heads[h],
                                             input, input, input,
                                             sequence_length,
                                             head_output,
                                             weights_ptr,
                                             mha->pos_encoder,
                                             h);

        /* WHAT: Check if forward pass failed
         * WHY:  Early cleanup and return on error
         */
        if (!success) {
            free_attention_buffer(head_outputs);
            free_attention_buffer(attention_weights);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_forward: success is NULL");
            return false;
        }

        /* WHAT: Apply thalamic gate if enabled
         * WHY:  Top-down modulation of attention
         */
        if (mha->config.use_thalamic_gate) {
            apply_thalamic_gate(head_output, gate, sequence_length * head_dim);
        }

        /* WHAT: Apply salience weighting if provided
         * WHY:  Boost biologically important features
         */
        if (mha->config.use_salience_weighting && salience) {
            for (uint32_t t = 0; t < sequence_length; t++) {
                float* token_output = head_output + (t * head_dim);
                scale_vector(token_output, salience[t], head_dim);
            }
        }
    }

    /* WHAT: Concatenate head outputs
     * WHY:  Combine representations from all heads
     * NOTE: Single loop, no nesting
     */
    for (uint32_t t = 0; t < sequence_length; t++) {
        float* token_output = output + (t * output_dim);

        for (uint32_t h = 0; h < num_heads; h++) {
            const float* head_output = head_outputs + (h * sequence_length * head_dim) + (t * head_dim);
            memcpy(token_output + (h * head_dim), head_output, head_dim * sizeof(float));
        }
    }

    /* WHAT: Compute and update entropy statistics
     * WHY:  Track attention focus characteristics
     */
    float entropy = attention_compute_entropy(attention_weights, sequence_length);
    uint32_t entropy_scaled = (uint32_t)(entropy * 1000.0F);

    /* WHAT: Update running average of entropy (simple moving average)
     * WHY:  Track typical attention behavior over time
     */
    uint32_t old_entropy = atomic_load(&mha->avg_entropy_scaled);
    uint64_t call_count = atomic_load(&mha->forward_calls);
    uint32_t new_entropy = (old_entropy * call_count + entropy_scaled) / (call_count + 1);
    atomic_store(&mha->avg_entropy_scaled, new_entropy);

    /* WHAT: Update statistics
     * WHY:  Track system behavior
     */
    atomic_fetch_add(&mha->forward_calls, 1);

    /* WHAT: Queue deferred events if subscribers exist
     * WHY:  Notify external modules of attention state changes
     * HOW:  Check conditions, queue events, flush after buffers freed
     */
    if (mha->num_subscribers > 0) {
        /* Reset deferred buffer for this forward pass */
        mha->num_deferred = 0;

        /* Queue entropy spike event if entropy exceeds threshold (2.0) */
        if (entropy > 2.0F) {
            attention_defer_event(mha, ATTENTION_EVENT_ENTROPY_SPIKE, 0, entropy);
        }
    }

    /* WHAT: Free temporary buffers
     * WHY:  Prevent memory leaks
     */
    free_attention_buffer(head_outputs);
    free_attention_buffer(attention_weights);

    /* WHAT: Flush deferred callbacks after buffers freed
     * WHY:  User callbacks may allocate memory; invoke outside critical path
     */
    attention_flush_deferred(mha);

    return true;
}

bool multihead_attention_set_gate(multihead_attention_t mha, float gate_signal)
{
    /* WHAT: Validate inputs
     * WHY:  Early return on invalid input
     */
    if (!mha) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_set_gate: mha is NULL");
        return false;
    }

    /* WHAT: Clamp gate to valid range
     * WHY:  Prevent invalid values
     */
    if (gate_signal < 0.0F) {
        gate_signal = 0.0F;
    }
    if (gate_signal > 1.0F) {
        gate_signal = 1.0F;
    }

    /* WHAT: Store as fixed-point atomic
     * WHY:  Lock-free thread-safe update
     */
    const uint32_t gate_scaled = (uint32_t)(gate_signal * 1000.0F);
    atomic_store(&mha->gate_signal, gate_scaled);

    return true;
}

bool multihead_attention_get_stats(multihead_attention_t mha, attention_stats_t* stats)
{
    /* WHAT: Validate inputs
     * WHY:  Early return on invalid input
     */
    if (!mha || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_get_stats: parameter is NULL");
        return false;
    }

    /* WHAT: Read atomic counters
     * WHY:  Lock-free thread-safe read
     */
    stats->forward_calls = atomic_load(&mha->forward_calls);
    stats->avg_gate_activation = (float)atomic_load(&mha->gate_signal) / 1000.0F;
    stats->active_heads = mha->config.num_heads;
    stats->avg_attention_entropy = (float)atomic_load(&mha->avg_entropy_scaled) / 1000.0F;
    stats->computation_time_ms = 0.0F;  // TODO: Add timing

    return true;
}

void multihead_attention_reset_stats(multihead_attention_t mha)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return if nothing to reset
     */
    if (!mha) {
        return;
    }

    /* WHAT: Reset all atomic counters
     * WHY:  Clear accumulated statistics
     */
    atomic_store(&mha->forward_calls, 0);
    atomic_store(&mha->avg_entropy_scaled, 0);
}

//=============================================================================
// Deferred Callback Public API
//=============================================================================

/**
 * WHAT: Register a callback subscriber for attention events
 * WHY:  Allow external modules to subscribe to attention state changes
 * HOW:  Find free slot in subscriber array, register callback and user_data
 */
int multihead_attention_register_callback(multihead_attention_t mha,
                                          attention_deferred_callback_t callback,
                                          void* user_data)
{
    if (!mha || !callback) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "multihead_attention_register_callback: mha or callback is NULL");
        return -1;
    }

    if (mha->num_subscribers >= ATTENTION_MAX_SUBSCRIBERS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "multihead_attention_register_callback: max subscribers reached (%u)",
            (unsigned)ATTENTION_MAX_SUBSCRIBERS);
        return -1;
    }

    mha->subscribers[mha->num_subscribers].callback = callback;
    mha->subscribers[mha->num_subscribers].user_data = user_data;
    mha->subscribers[mha->num_subscribers].active = true;
    mha->num_subscribers++;

    return 0;
}

/**
 * WHAT: Get number of deferred callbacks dropped due to buffer overflow
 * WHY:  Monitor if the deferred buffer size is sufficient
 */
uint64_t attention_get_deferred_drop_count(multihead_attention_t mha)
{
    if (!mha) {
        return 0;
    }
    return atomic_load(&mha->deferred_drop_count);
}

/**
 * WHAT: Reset the deferred callback drop counter
 * WHY:  Allow periodic monitoring of drop rate
 */
void attention_reset_deferred_drop_count(multihead_attention_t mha)
{
    if (!mha) {
        return;
    }
    atomic_store(&mha->deferred_drop_count, 0);
}

//=============================================================================
// Utility Functions
//=============================================================================

float attention_compute_entropy(const float* attention_weights, uint32_t sequence_length)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return prevents crash
     */
    if (!attention_weights || sequence_length == 0) {
        return 0.0F;
    }

    /* WHAT: Compute entropy: H = -sum(p * log(p))
     * WHY:  Measure attention focus (lower = more focused)
     */
    float entropy = 0.0F;

    for (uint32_t i = 0; i < sequence_length; i++) {
        const float p = attention_weights[i];

        /* WHAT: Skip zero/negligible probabilities
         * WHY:  Avoid log(0) = -inf and denormal performance degradation
         */
        if (p > NIMCP_DENORMAL_THRESHOLD) {
            entropy -= p * logf(p);
        }
    }

    return entropy;
}

bool attention_validate_config(const multihead_attention_config_t* config)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return if no config
     */
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_validate_config: config is NULL");
        return false;
    }

    /* WHAT: Check required fields are non-zero
     * WHY:  Prevent division by zero and invalid allocations
     */
    if (config->num_heads == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_validate_config: config->num_heads is zero");
        return false;
    }

    if (config->input_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_validate_config: config->input_dim is zero");
        return false;
    }

    if (config->output_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_validate_config: config->output_dim is zero");
        return false;
    }

    if (config->sequence_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_validate_config: config->sequence_length is zero");
        return false;
    }

    /* WHAT: Check input_dim is divisible by num_heads
     * WHY:  Each head needs equal dimension
     */
    if (config->input_dim % config->num_heads != 0) {
        return false;
    }

    return true;
}

//=============================================================================
// Phase 11: Attention-Working Memory Coordination
//=============================================================================

/**
 * WHAT: Get current attention strength for working memory gating
 * WHY:  Attended items should be more salient in working memory
 * HOW:  Read atomic gate signal, convert from fixed-point to float
 *
 * COMPLEXITY: O(1)
 */
float multihead_attention_get_strength(multihead_attention_t mha)
{
    // Guard: NULL check
    if (!mha) {
        return 0.0F;
    }

    // Guard: Check if attention has been used (forward_calls > 0)
    uint64_t calls = atomic_load(&mha->forward_calls);
    if (calls == 0) {
        // No forward passes yet, return neutral strength
        return 0.5F;
    }

    // Read atomic gate signal (fixed-point [0, 1000] → [0.0, 1.0])
    uint32_t gate_scaled = atomic_load(&mha->avg_gate_scaled);
    float gate_strength = (float)gate_scaled / 1000.0F;

    // Ensure bounded [0.0, 1.0]
    if (gate_strength > 1.0F) {
        gate_strength = 1.0F;
    }
    if (gate_strength < 0.0F) {
        gate_strength = 0.0F;
    }

    return gate_strength;
}

//=============================================================================
// Positional Encoding Integration (Strategy Pattern)
//=============================================================================

/**
 * @brief Set positional encoding type for multihead attention
 *
 * WHAT: Create or replace positional encoder with specified type
 * WHY:  Enable runtime switching between RoPE and ALiBi strategies
 * HOW:  Destroy old encoder, create new one based on type
 *
 * COMPLEXITY: O(1) for encoder creation
 * SRP: Only handles encoder lifecycle management
 */
bool multihead_attention_set_pe_type(multihead_attention_t mha,
                                     nimcp_pos_encoding_type_t pe_type)
{
    /* WHAT: Guard clause for NULL
     * WHY:  Early return prevents crash
     */
    if (!mha) {
        NIMCP_LOGGING_ERROR("NULL multihead attention in set_pe_type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_set_pe_type: mha is NULL");
        return false;
    }

    /* WHAT: Validate PE type is supported
     * WHY:  Only RoPE and ALiBi are integrated
     */
    if (pe_type != NIMCP_POS_ROTARY && pe_type != NIMCP_POS_ALIBI) {
        NIMCP_LOGGING_ERROR("Unsupported PE type: %d (only RoPE and ALiBi supported)",
                           pe_type);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_set_pe_type: validation failed");
        return false;
    }

    /* WHAT: Destroy existing encoder if present
     * WHY:  Prevent memory leak when changing type
     */
    if (mha->pos_encoder) {
        nimcp_pos_encoder_destroy(mha->pos_encoder);
        mha->pos_encoder = NULL;
    }

    /* WHAT: Create unified config structure
     * WHY:  nimcp_pos_encoder_create takes unified config
     */
    nimcp_pos_config_t pe_config = {0};
    pe_config.type = pe_type;

    /* WHAT: Configure based on PE type
     * WHY:  Different types need different parameters
     * HOW:  RoPE needs base frequency, ALiBi needs num_heads
     */
    if (pe_type == NIMCP_POS_ROTARY) {
        /* WHAT: Configure RoPE encoder
         * WHY:  Rotary position embedding for Q/K rotation
         */
        pe_config.config.rope.base.max_seq_length = mha->config.sequence_length;
        pe_config.config.rope.base.embedding_dim = mha->config.input_dim / mha->config.num_heads;
        pe_config.config.rope.base.cache_enabled = true;
        pe_config.config.rope.base.thread_safe = false;  // Single-threaded forward pass
        pe_config.config.rope.rope_base = mha->config.rope_base > 0.0F ?
                                          mha->config.rope_base : NIMCP_ROPE_DEFAULT_BASE;
        pe_config.config.rope.rope_scaling = 1.0F;
        pe_config.config.rope.rope_dim = 0;  // Apply to all dimensions
        pe_config.config.rope.use_ntk_scaling = false;

        NIMCP_LOGGING_DEBUG("Creating RoPE encoder: seq_len=%u, dim=%u, base=%.1f",
                           pe_config.config.rope.base.max_seq_length,
                           pe_config.config.rope.base.embedding_dim,
                           pe_config.config.rope.rope_base);

    } else if (pe_type == NIMCP_POS_ALIBI) {
        /* WHAT: Configure ALiBi encoder
         * WHY:  Attention with Linear Biases for position
         */
        pe_config.config.alibi.base.max_seq_length = mha->config.sequence_length;
        pe_config.config.alibi.base.embedding_dim = 0;  // ALiBi doesn't use embeddings
        pe_config.config.alibi.base.cache_enabled = true;
        pe_config.config.alibi.base.thread_safe = false;
        pe_config.config.alibi.num_heads = mha->config.num_heads;
        pe_config.config.alibi.slope_base = mha->config.alibi_slope_base > 0.0F ?
                                            mha->config.alibi_slope_base : 2.0F;
        pe_config.config.alibi.use_symmetric = true;  // Bidirectional attention

        NIMCP_LOGGING_DEBUG("Creating ALiBi encoder: seq_len=%u, num_heads=%u, slope_base=%.1f",
                           pe_config.config.alibi.base.max_seq_length,
                           pe_config.config.alibi.num_heads,
                           pe_config.config.alibi.slope_base);
    }

    /* WHAT: Create positional encoder
     * WHY:  Factory function for any PE type
     */
    mha->pos_encoder = nimcp_pos_encoder_create(&pe_config);
    if (!mha->pos_encoder) {
        NIMCP_LOGGING_ERROR("Failed to create positional encoder");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_set_pe_type: mha->pos_encoder is NULL");
        return false;
    }

    /* WHAT: Update config to reflect new PE type
     * WHY:  Keep config in sync with actual encoder
     */
    mha->config.pe_type = pe_type;
    mha->config.use_positional_encoding = true;

    NIMCP_LOGGING_INFO("Set positional encoding type: %s",
                      nimcp_pos_type_to_string(pe_type));

    return true;
}

/**
 * @brief Apply RoPE rotation to query and key projections
 *
 * WHAT: Rotate Q/K vectors by position-dependent angles
 * WHY:  Inject relative position information into attention
 * HOW:  Call nimcp_pos_rope_apply for each position
 *
 * COMPLEXITY: O(seq_len * key_dim)
 * SRP: Only applies RoPE transformation
 */
bool multihead_attention_apply_rope(multihead_attention_t mha,
                                   const float* query_proj,
                                   const float* key_proj,
                                   uint32_t seq_length,
                                   uint32_t key_dim,
                                   float* query_out,
                                   float* key_out)
{
    /* WHAT: Guard clauses for NULL pointers
     * WHY:  Early returns prevent crashes
     */
    if (!mha || !query_proj || !key_proj || !query_out || !key_out) {
        NIMCP_LOGGING_ERROR("NULL parameter in apply_rope");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_apply_rope: required parameter is NULL (mha, query_proj, key_proj, query_out, key_out)");
        return false;
    }

    /* WHAT: Check encoder exists and is RoPE type
     * WHY:  RoPE functions only work on RoPE encoder
     */
    if (!mha->pos_encoder) {
        NIMCP_LOGGING_ERROR("No positional encoder initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_apply_rope: mha->pos_encoder is NULL");
        return false;
    }

    if (nimcp_pos_get_type(mha->pos_encoder) != NIMCP_POS_ROTARY) {
        NIMCP_LOGGING_ERROR("Encoder is not RoPE type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_apply_rope: validation failed");
        return false;
    }

    /* WHAT: Apply RoPE to each position in sequence
     * WHY:  Each token needs position-dependent rotation
     * NOTE: Single loop, no nesting
     */
    for (uint32_t pos = 0; pos < seq_length; pos++) {
        const float* query_vec = query_proj + (pos * key_dim);
        const float* key_vec = key_proj + (pos * key_dim);
        float* query_out_vec = query_out + (pos * key_dim);
        float* key_out_vec = key_out + (pos * key_dim);

        /* WHAT: Apply rotation to Q/K pair
         * WHY:  RoPE rotates query and key by same position angles
         * HOW:  Uses precomputed rotation matrices in encoder
         */
        int result = nimcp_pos_rope_apply(mha->pos_encoder,
                                          query_vec, key_vec,
                                          pos,
                                          query_out_vec, key_out_vec);

        if (result != NIMCP_POS_SUCCESS) {
            NIMCP_LOGGING_ERROR("RoPE application failed at position %u", pos);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_apply_rope: validation failed");
            return false;
        }
    }

    return true;
}

/**
 * @brief Get ALiBi attention bias matrix
 *
 * WHAT: Compute position-dependent bias for attention scores
 * WHY:  ALiBi adds linear distance penalty to encourage local attention
 * HOW:  Call nimcp_pos_alibi_get_bias to compute bias matrix
 *
 * COMPLEXITY: O(num_heads * seq_length^2)
 * SRP: Only retrieves bias matrix from encoder
 */
bool multihead_attention_get_alibi_bias(multihead_attention_t mha,
                                       uint32_t seq_length,
                                       float* bias_out)
{
    /* WHAT: Guard clauses for NULL pointers
     * WHY:  Early returns prevent crashes
     */
    if (!mha || !bias_out) {
        NIMCP_LOGGING_ERROR("NULL parameter in get_alibi_bias");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_get_alibi_bias: required parameter is NULL (mha, bias_out)");
        return false;
    }

    /* WHAT: Check encoder exists and is ALiBi type
     * WHY:  ALiBi functions only work on ALiBi encoder
     */
    if (!mha->pos_encoder) {
        NIMCP_LOGGING_ERROR("No positional encoder initialized");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_get_alibi_bias: mha->pos_encoder is NULL");
        return false;
    }

    if (nimcp_pos_get_type(mha->pos_encoder) != NIMCP_POS_ALIBI) {
        NIMCP_LOGGING_ERROR("Encoder is not ALiBi type");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_get_alibi_bias: validation failed");
        return false;
    }

    /* WHAT: Get bias matrix from encoder
     * WHY:  Encoder pre-computes geometric slope-based biases
     * HOW:  bias[h][i][j] = -slope[h] * |i - j|
     */
    int result = nimcp_pos_alibi_get_bias(mha->pos_encoder,
                                          seq_length,
                                          bias_out);

    if (result != NIMCP_POS_SUCCESS) {
        NIMCP_LOGGING_ERROR("Failed to get ALiBi bias matrix");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_get_alibi_bias: validation failed");
        return false;
    }

    return true;
}

//=============================================================================
// Phase COW: Copy-on-Write Support for Weight Sharing
//=============================================================================

/**
 * @brief Calculate total weight buffer size for all attention matrices
 *
 * WHAT: Compute total bytes needed for combined Q/K/V/O weights
 * WHY:  Single contiguous COW region for all weights
 * HOW:  Sum of all weight matrix sizes, page-aligned
 */
static size_t calculate_cow_weights_size(const attention_head_config_t* config)
{
    if (!config) return 0;

    size_t query_size = config->input_dim * config->key_dim * sizeof(float);
    size_t key_size = config->input_dim * config->key_dim * sizeof(float);
    size_t value_size = config->input_dim * config->value_dim * sizeof(float);
    size_t output_size = config->value_dim * config->output_dim * sizeof(float);

    return page_cow_align_size(query_size + key_size + value_size + output_size);
}

/**
 * @brief Create attention head with COW-backed weight storage
 *
 * WHAT: Create attention head using page-level COW for weights
 * WHY:  Enable O(1) model cloning with lazy weight copying
 * HOW:  Store all weights in single COW region, set pointers via view
 *
 * @param config Attention head configuration
 * @param initial_weights Initial weights to copy (NULL = random init)
 * @return Attention head with COW weights, or NULL on failure
 *
 * COMPLEXITY: O(weights_size) for initial copy
 * MEMORY: weights_size bytes in COW region
 *
 * EXAMPLE:
 * ```c
 * // Create COW-backed attention head for fine-tuning
 * attention_head_t head = attention_head_create_cow(&config, pretrained_weights);
 *
 * // Clone for parallel training (instant, shares pages)
 * attention_head_t clone = attention_head_clone_cow(head);
 * ```
 */
attention_head_t attention_head_create_cow(const attention_head_config_t* config,
                                           const float* initial_weights)
{
    if (!config) {
        NIMCP_LOGGING_ERROR("COW attention head config is NULL");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_create_cow: config is NULL");
        return NULL;
    }

    if (config->input_dim == 0 || config->output_dim == 0) {
        NIMCP_LOGGING_ERROR("Invalid dimensions in COW attention head config");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "attention_head_create_cow: config->input_dim is zero");
        return NULL;
    }

    // Calculate weight sizes and offsets
    size_t query_size = config->input_dim * config->key_dim * sizeof(float);
    size_t key_size = config->input_dim * config->key_dim * sizeof(float);
    size_t value_size = config->input_dim * config->value_dim * sizeof(float);
    size_t output_size = config->value_dim * config->output_dim * sizeof(float);
    size_t total_size = calculate_cow_weights_size(config);

    // Allocate head structure
    attention_head_t head = nimcp_malloc(sizeof(struct attention_head_struct));
    if (!head) {
        NIMCP_LOGGING_ERROR("Failed to allocate COW attention head");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_create_cow: head is NULL");
        return NULL;
    }

    memcpy(&head->config, config, sizeof(attention_head_config_t));

    // Initialize COW fields
    head->uses_cow = true;
    head->cow_weights_offset[0] = 0;                           // query
    head->cow_weights_offset[1] = query_size;                  // key
    head->cow_weights_offset[2] = query_size + key_size;       // value
    head->cow_weights_offset[3] = query_size + key_size + value_size;  // output

    // Create COW region for weights
    page_cow_config_t cow_config = page_cow_default_config(total_size);
    cow_config.enable_tracking = true;

    head->cow_region = page_cow_region_create(&cow_config, initial_weights);
    if (!head->cow_region) {
        NIMCP_LOGGING_ERROR("Failed to create COW region for attention weights");
        nimcp_free(head);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_create_cow: head->cow_region is NULL");
        return NULL;
    }

    // Create view into region
    head->cow_view = page_cow_view_create(head->cow_region);
    if (!head->cow_view) {
        NIMCP_LOGGING_ERROR("Failed to create COW view for attention weights");
        page_cow_region_destroy(head->cow_region);
        nimcp_free(head);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_create_cow: head->cow_view is NULL");
        return NULL;
    }

    // Set weight pointers from view (read-only initially)
    const float* base = (const float*)page_cow_view_read(head->cow_view);
    if (!base) {
        page_cow_view_destroy(head->cow_view);
        page_cow_region_destroy(head->cow_region);
        nimcp_free(head);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_create_cow: base is NULL");
        return NULL;
    }

    // Point to offsets within COW region (cast away const, COW handles writes)
    head->query_weights = (float*)((char*)base + head->cow_weights_offset[0]);
    head->key_weights = (float*)((char*)base + head->cow_weights_offset[1]);
    head->value_weights = (float*)((char*)base + head->cow_weights_offset[2]);
    head->output_weights = (float*)((char*)base + head->cow_weights_offset[3]);

    // Initialize weights if no initial data provided
    if (!initial_weights) {
        // Get writable pointer (will COW on write)
        float* writable_base = (float*)page_cow_view_write(head->cow_view);
        if (writable_base) {
            head->query_weights = (float*)((char*)writable_base + head->cow_weights_offset[0]);
            head->key_weights = (float*)((char*)writable_base + head->cow_weights_offset[1]);
            head->value_weights = (float*)((char*)writable_base + head->cow_weights_offset[2]);
            head->output_weights = (float*)((char*)writable_base + head->cow_weights_offset[3]);
            initialize_weights(head);
        }
    }

    NIMCP_LOGGING_INFO("Created COW attention head: input_dim=%u, output_dim=%u, cow_size=%zu",
                       config->input_dim, config->output_dim, total_size);

    return head;
}

/**
 * @brief Clone attention head with COW semantics
 *
 * WHAT: Create O(1) clone sharing weight pages with source
 * WHY:  Instant model cloning for fine-tuning or parallel training
 * HOW:  Create new view into same COW region, COW on first write
 *
 * @param source Attention head to clone (must use COW)
 * @return Cloned attention head, or NULL on failure
 *
 * COMPLEXITY: O(1) - No weight copying
 * MEMORY: ~64 bytes for view metadata
 *
 * EXAMPLE:
 * ```c
 * // Clone for each training worker (instant)
 * for (int i = 0; i < num_workers; i++) {
 *     workers[i].head = attention_head_clone_cow(base_head);
 *     // All workers share pages until they write
 * }
 * ```
 */
attention_head_t attention_head_clone_cow(attention_head_t source)
{
    if (!source) {
        NIMCP_LOGGING_ERROR("Cannot clone NULL attention head");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_clone_cow: source is NULL");
        return NULL;
    }

    if (!source->uses_cow || !source->cow_view) {
        NIMCP_LOGGING_ERROR("Cannot COW-clone non-COW attention head");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_clone_cow: required parameter is NULL (source->uses_cow, source->cow_view)");
        return NULL;
    }

    // Allocate new head structure
    attention_head_t clone = nimcp_malloc(sizeof(struct attention_head_struct));
    if (!clone) {
        NIMCP_LOGGING_ERROR("Failed to allocate COW clone head");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_clone_cow: clone is NULL");
        return NULL;
    }

    // Copy config and COW offsets
    memcpy(&clone->config, &source->config, sizeof(attention_head_config_t));
    memcpy(clone->cow_weights_offset, source->cow_weights_offset,
           sizeof(clone->cow_weights_offset));

    // Mark as COW but without owning the region
    clone->uses_cow = true;
    clone->cow_region = NULL;  // Don't own region, source does

    // Create COW clone of the view
    clone->cow_view = page_cow_view_clone(source->cow_view);
    if (!clone->cow_view) {
        NIMCP_LOGGING_ERROR("Failed to clone COW view");
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_clone_cow: clone->cow_view is NULL");
        return NULL;
    }

    // Set weight pointers from cloned view
    const float* base = (const float*)page_cow_view_read(clone->cow_view);
    if (!base) {
        page_cow_view_destroy(clone->cow_view);
        nimcp_free(clone);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_clone_cow: base is NULL");
        return NULL;
    }

    clone->query_weights = (float*)((char*)base + clone->cow_weights_offset[0]);
    clone->key_weights = (float*)((char*)base + clone->cow_weights_offset[1]);
    clone->value_weights = (float*)((char*)base + clone->cow_weights_offset[2]);
    clone->output_weights = (float*)((char*)base + clone->cow_weights_offset[3]);

    NIMCP_LOGGING_DEBUG("Created COW clone of attention head");

    return clone;
}

/**
 * @brief Create instant snapshot of attention weights for rollback
 *
 * WHAT: O(1) snapshot of current weight state
 * WHY:  Enable rollback during training (e.g., failed batch)
 * HOW:  Page-level COW snapshot, copies only on subsequent writes
 *
 * @param head Attention head (must use COW)
 * @return Snapshot handle, or NULL on failure
 *
 * COMPLEXITY: O(1) - No weight copying
 */
page_cow_snapshot_t attention_head_snapshot_weights(attention_head_t head)
{
    if (!head || !head->uses_cow || !head->cow_view) {
        NIMCP_LOGGING_ERROR("Cannot snapshot non-COW attention head");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_snapshot_weights: required parameter is NULL (head, head->uses_cow, head->cow_view)");
        return NULL;
    }

    page_cow_snapshot_t snapshot = page_cow_snapshot_create(head->cow_view);
    if (!snapshot) {
        NIMCP_LOGGING_ERROR("Failed to create weight snapshot");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "attention_head_snapshot_weights: snapshot is NULL");
        return NULL;
    }

    NIMCP_LOGGING_DEBUG("Created attention weight snapshot");
    return snapshot;
}

/**
 * @brief Restore attention weights from snapshot
 *
 * WHAT: Rollback weights to snapshot state
 * WHY:  Recover from failed training iterations
 * HOW:  Page-level restore, discards private pages
 *
 * @param head Attention head to restore
 * @param snapshot Snapshot to restore from
 * @return true on success
 *
 * COMPLEXITY: O(num_modified_pages)
 */
bool attention_head_restore_weights(attention_head_t head,
                                    page_cow_snapshot_t snapshot)
{
    if (!head || !head->uses_cow || !head->cow_view || !snapshot) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "attention_head_restore_weights: required parameter is NULL (head, head->uses_cow, head->cow_view, snapshot)");
        return false;
    }

    bool success = page_cow_snapshot_restore(head->cow_view, snapshot);
    if (success) {
        NIMCP_LOGGING_DEBUG("Restored attention weights from snapshot");
    }

    return success;
}

/**
 * @brief Get memory savings from COW weight sharing
 *
 * @param head Attention head (must use COW)
 * @return Bytes saved by sharing, or 0 if not using COW
 */
size_t attention_head_get_cow_savings(attention_head_t head)
{
    if (!head || !head->uses_cow || !head->cow_view) {
        return 0;
    }

    return page_cow_view_get_memory_saved(head->cow_view);
}

//=============================================================================
// Hard Ternary Attention Implementation
//=============================================================================

/**
 * @brief Internal structure for ternary attention context
 */
struct ternary_attention_ctx_s {
    ternary_attention_config_t config;
    ternary_attention_stats_t stats;
    float current_temperature;
    uint32_t current_step;
};

int ternary_attention_default_config(ternary_attention_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    memset(config, 0, sizeof(ternary_attention_config_t));

    config->focus_threshold = 0.7f;
    config->suppress_threshold = 0.3f;
    config->use_top_k = false;
    config->top_k = 5;
    config->focus_gain = 1.5f;
    config->suppress_gain = 0.1f;
    config->temperature_annealing = false;
    config->initial_temperature = 1.0f;
    config->final_temperature = 0.01f;
    config->annealing_steps = 10000;

    return 0;
}

ternary_attention_ctx_t* ternary_attention_create(
    const ternary_attention_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return NULL;
    }

    ternary_attention_ctx_t* ctx = nimcp_calloc(1, sizeof(ternary_attention_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "ctx is NULL");

        return NULL;
    }

    memcpy(&ctx->config, config, sizeof(ternary_attention_config_t));
    memset(&ctx->stats, 0, sizeof(ternary_attention_stats_t));
    ctx->current_temperature = config->initial_temperature;
    ctx->current_step = 0;

    return ctx;
}

void ternary_attention_destroy(ternary_attention_ctx_t* ctx)
{
    if (ctx) {
        nimcp_free(ctx);
    }
}

int ternary_attention_discretize(
    ternary_attention_ctx_t* ctx,
    const float* soft_attention,
    uint32_t seq_length,
    ternary_attention_state_t* ternary_out)
{
    if (!ctx || !soft_attention || !ternary_out || seq_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ternary_attention_discretize: required parameter is NULL (ctx, soft_attention, ternary_out)");
        return -1;
    }

    float focus_thresh = ctx->config.focus_threshold;
    float suppress_thresh = ctx->config.suppress_threshold;

    /* Temperature scaling: adjust thresholds based on temperature */
    if (ctx->config.temperature_annealing && ctx->current_temperature > 0.01f) {
        float temp_scale = 1.0f / ctx->current_temperature;
        /* As temperature decreases, thresholds become more extreme */
        focus_thresh = 0.5f + (focus_thresh - 0.5f) * temp_scale;
        suppress_thresh = 0.5f - (0.5f - suppress_thresh) * temp_scale;
        /* Clamp to valid range */
        if (focus_thresh > 0.99f) focus_thresh = 0.99f;
        if (suppress_thresh < 0.01f) suppress_thresh = 0.01f;
    }

    /* Use top-k if configured */
    if (ctx->config.use_top_k) {
        return ternary_attention_top_k(soft_attention, seq_length,
                                       ctx->config.top_k, ternary_out);
    }

    /* Threshold-based discretization */
    uint64_t n_focus = 0, n_neutral = 0, n_suppress = 0;

    for (uint32_t i = 0; i < seq_length; i++) {
        float attn = soft_attention[i];

        if (attn >= focus_thresh) {
            ternary_out[i] = ATTENTION_FOCUS;
            n_focus++;
        } else if (attn <= suppress_thresh) {
            ternary_out[i] = ATTENTION_SUPPRESS;
            n_suppress++;
        } else {
            ternary_out[i] = ATTENTION_NEUTRAL;
            n_neutral++;
        }
    }

    /* Update statistics */
    ctx->stats.n_focus += n_focus;
    ctx->stats.n_neutral += n_neutral;
    ctx->stats.n_suppress += n_suppress;

    uint64_t total = ctx->stats.n_focus + ctx->stats.n_neutral + ctx->stats.n_suppress;
    if (total > 0) {
        ctx->stats.focus_ratio = (float)ctx->stats.n_focus / (float)total;
        ctx->stats.suppress_ratio = (float)ctx->stats.n_suppress / (float)total;
        ctx->stats.sparsity = 1.0f - ctx->stats.focus_ratio;
    }

    return 0;
}

int ternary_attention_apply(
    ternary_attention_ctx_t* ctx,
    const float* values,
    const ternary_attention_state_t* ternary_attention,
    uint32_t seq_length,
    uint32_t dim,
    float* output)
{
    if (!ctx || !values || !ternary_attention || !output ||
        seq_length == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ternary_attention_apply: operation failed");
        return -1;
    }

    float focus_gain = ctx->config.focus_gain;
    float suppress_gain = ctx->config.suppress_gain;

    for (uint32_t t = 0; t < seq_length; t++) {
        float gain;

        switch (ternary_attention[t]) {
            case ATTENTION_FOCUS:
                gain = focus_gain;
                break;
            case ATTENTION_SUPPRESS:
                gain = suppress_gain;
                break;
            case ATTENTION_NEUTRAL:
            default:
                gain = 1.0f;
                break;
        }

        /* Apply gain to all features at this position */
        const float* input_vec = values + (t * dim);
        float* output_vec = output + (t * dim);

        for (uint32_t d = 0; d < dim; d++) {
            output_vec[d] = input_vec[d] * gain;
        }
    }

    return 0;
}

int ternary_attention_backward(
    ternary_attention_ctx_t* ctx,
    const float* grad_output,
    const float* soft_attention,
    uint32_t seq_length,
    uint32_t dim,
    float* grad_attention)
{
    if (!ctx || !grad_output || !soft_attention || !grad_attention ||
        seq_length == 0 || dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "ternary_attention_backward: operation failed");
        return -1;
    }

    /* Straight-through estimator: sum gradient over features */
    for (uint32_t t = 0; t < seq_length; t++) {
        float grad_sum = 0.0f;

        /* Sum gradients across feature dimension */
        const float* grad_vec = grad_output + (t * dim);
        for (uint32_t d = 0; d < dim; d++) {
            grad_sum += grad_vec[d];
        }

        /* Pass through: gradient for attention is sum of output gradients */
        grad_attention[t] = grad_sum;
    }

    return 0;
}

void ternary_attention_update_temperature(
    ternary_attention_ctx_t* ctx,
    uint32_t step)
{
    if (!ctx || !ctx->config.temperature_annealing) {
        return;
    }

    ctx->current_step = step;

    if (step >= ctx->config.annealing_steps) {
        ctx->current_temperature = ctx->config.final_temperature;
        return;
    }

    /* Linear annealing */
    float progress = (float)step / (float)ctx->config.annealing_steps;
    float temp_range = ctx->config.initial_temperature - ctx->config.final_temperature;
    ctx->current_temperature = ctx->config.initial_temperature - (progress * temp_range);

    /* Update stats */
    ctx->stats.avg_temperature = ctx->current_temperature;
}

float ternary_attention_get_temperature(const ternary_attention_ctx_t* ctx)
{
    if (!ctx) {
        return 1.0f;
    }
    return ctx->current_temperature;
}

int ternary_attention_get_stats(
    const ternary_attention_ctx_t* ctx,
    ternary_attention_stats_t* stats)
{
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ternary_attention_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    memcpy(stats, &ctx->stats, sizeof(ternary_attention_stats_t));
    return 0;
}

int ternary_attention_top_k(
    const float* soft_attention,
    uint32_t seq_length,
    uint32_t k,
    ternary_attention_state_t* ternary_out)
{
    if (!soft_attention || !ternary_out || seq_length == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "ternary_attention_top_k: required parameter is NULL (soft_attention, ternary_out)");
        return -1;
    }

    /* Clamp k to valid range */
    if (k > seq_length) {
        k = seq_length;
    }
    if (k == 0) {
        k = 1;
    }

    /* Find k-th largest value using partial sort approach */
    /* Simple O(n*k) algorithm suitable for small k */
    float* values_copy = nimcp_malloc(seq_length * sizeof(float));
    if (!values_copy) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "values_copy is NULL");

        return -1;
    }

    memcpy(values_copy, soft_attention, seq_length * sizeof(float));

    /* Initialize all to SUPPRESS */
    for (uint32_t i = 0; i < seq_length; i++) {
        ternary_out[i] = ATTENTION_SUPPRESS;
    }

    /* Find top-k and mark as FOCUS */
    for (uint32_t round = 0; round < k; round++) {
        float max_val = -1e30f;
        uint32_t max_idx = 0;

        for (uint32_t i = 0; i < seq_length; i++) {
            if (values_copy[i] > max_val) {
                max_val = values_copy[i];
                max_idx = i;
            }
        }

        ternary_out[max_idx] = ATTENTION_FOCUS;
        values_copy[max_idx] = -1e30f;  /* Mark as used */
    }

    nimcp_free(values_copy);
    return 0;
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Connect multihead attention to internal knowledge graph
 *
 * WHAT: Initialize KG context for self-awareness queries
 * WHY:  Enable attention to query its targets and configurations
 * HOW:  Use KG helper functions to establish connection
 *
 * @param mha Multihead attention instance
 * @param brain Brain instance for KG access
 * @return true if connected (or KG gracefully disabled), false on error
 */
bool multihead_attention_connect_kg(multihead_attention_t mha, brain_t brain)
{
    if (!mha) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_connect_kg: mha is NULL");
        return false;
    }

    int result = kg_module_init(&mha->kg_context, brain, "Multihead_Attention");

    if (result != 0) {
        NIMCP_LOGGING_ERROR("Failed to initialize KG context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_connect_kg: validation failed");
        return false;
    }

    if (!kg_is_available(&mha->kg_context)) {
        mha->kg_connected = false;
        NIMCP_LOGGING_INFO("KG disabled, attention graceful degradation");
        return true;
    }

    mha->kg_connected = true;
    NIMCP_LOGGING_INFO("Connected to internal KG for attention self-awareness");

    return true;
}

/**
 * @brief Query attention targets from KG
 *
 * WHAT: Retrieve list of modules that attention can focus on
 * WHY:  Enable self-awareness of what attention can target
 * HOW:  Query KG for connected nodes
 *
 * @param mha Multihead attention instance
 * @return Number of potential targets found (0 if KG not connected)
 */
int multihead_attention_query_targets(multihead_attention_t mha)
{
    if (!mha || !mha->kg_connected) {
        return 0;
    }

    if (!kg_is_available(&mha->kg_context)) {
        return 0;
    }

    brain_kg_edge_list_t* outgoing = kg_get_outgoing_safe(&mha->kg_context);
    if (!outgoing) {
        return 0;
    }

    int count = (int)outgoing->count;
    NIMCP_LOGGING_DEBUG("Attention has %d potential KG targets", count);

    brain_kg_edge_list_destroy(outgoing);
    return count;
}

/**
 * @brief Query attention's self-knowledge from KG
 *
 * WHAT: Query KG for structural self-knowledge about attention
 * WHY:  Enable introspection of attention configuration
 * HOW:  Find self node and retrieve metadata
 *
 * @param mha Multihead attention instance
 * @return true if self-knowledge is available, false otherwise
 */
bool multihead_attention_query_self_knowledge(multihead_attention_t mha)
{
    if (!mha || !mha->kg_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "multihead_attention_query_self_knowledge: required parameter is NULL (mha, mha->kg_connected)");
        return false;
    }

    if (!kg_has_node(&mha->kg_context)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_query_self_knowledge: kg_has_node is NULL");
        return false;
    }

    const brain_kg_node_t* self = kg_get_node_safe(
        &mha->kg_context,
        mha->kg_context.self_node_id
    );

    if (self) {
        NIMCP_LOGGING_DEBUG("Attention self-knowledge: name=%s, state=%d",
                           self->name, self->state);
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "multihead_attention_query_self_knowledge: validation failed");
    return false;
}

/**
 * @brief W9-kg read-path: query recent salience events from the KG.
 *
 * WHAT: Count prior `cog_attention_event_salience_*` nodes in the KG and
 *       emit a fresh salience event for the current forward pass.
 * WHY:  Lets the attention hot-path reason about KG-visible attention
 *       history, biases gain toward modules with frequent prior salience,
 *       and publishes this pass's salience so downstream W16 consumers
 *       (brain_decide, middleware attention_focus) can see it.
 * HOW:  Uses the shared W9-kg helpers which admin-elevate internally.
 *       Returns the count of salience events visible *before* this call.
 *
 * @param mha Multihead attention instance
 * @param head_idx Head that produced the salience
 * @param salience Computed salience score for this head
 * @return Number of historical salience events, 0 if KG unavailable.
 */
uint32_t multihead_attention_query_salience_events(multihead_attention_t mha,
                                                   uint32_t head_idx,
                                                   float salience)
{
    if (!mha) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    /* Guard: no brain -> graceful no-op (attention may still run pre-init). */
    if (!brain) return 0;

    uint32_t prior = w9kg_query_attention_salience_count(brain);
    w9kg_emit_attention_salience(brain, head_idx, salience);
    return prior;
}

//=============================================================================
// Monte Carlo Integration API - GPU default with CPU fallback
//=============================================================================

/**
 * @brief Apply stochastic attention dropout using MC sampling
 *
 * WHAT: Randomly zero attention weights for regularization
 * WHY:  Prevent overfitting, encourage diverse attention patterns
 * HOW:  GPU: Parallel random sampling (default for seq_length >= 256)
 *       CPU: Sequential sampling (fallback)
 *
 * @param attention_weights Attention weights to modify in-place
 * @param seq_length Sequence length
 * @param dropout_rate Probability of dropping [0, 1]
 */
void attention_apply_dropout_mc(float* attention_weights,
                                 uint32_t seq_length,
                                 float dropout_rate)
{
    if (!attention_weights || seq_length == 0 || dropout_rate <= 0.0f) {
        return;
    }

    float scale = 1.0f / (1.0f - dropout_rate);  /* Inverted dropout */

#ifdef NIMCP_ENABLE_CUDA
    /* Try GPU acceleration first (default for large sequences) */
    if (attention_has_gpu_mc() && seq_length >= 256) {
        size_t dims[] = {seq_length};
        nimcp_gpu_tensor_t* samples = nimcp_gpu_tensor_create(
            g_attention_gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (samples) {
            if (qmc_gpu_sample_uniform(g_attention_gpu_ctx, g_attention_gpu_rng, samples)) {
                float* h_samples = nimcp_calloc(seq_length, sizeof(float));
                if (h_samples) {
                    cudaMemcpy(h_samples, samples->data, seq_length * sizeof(float), cudaMemcpyDeviceToHost);

                    for (uint32_t i = 0; i < seq_length; i++) {
                        if (h_samples[i] < dropout_rate) {
                            attention_weights[i] = 0.0f;
                        } else {
                            attention_weights[i] *= scale;
                        }
                    }

                    nimcp_free(h_samples);
                    nimcp_gpu_tensor_destroy(samples);
                    return;
                }
            }
            nimcp_gpu_tensor_destroy(samples);
        }
    }
#endif

    /* CPU fallback */
    if (g_attention_mc_seed == 0) {
        g_attention_mc_seed = mc_seed_from_time();
    }

    for (uint32_t i = 0; i < seq_length; i++) {
        if (mc_random_uniform(&g_attention_mc_seed) < dropout_rate) {
            attention_weights[i] = 0.0f;
        } else {
            attention_weights[i] *= scale;
        }
    }
}

/**
 * @brief Sample attention head using softmax MC sampling
 *
 * WHAT: Probabilistically select attention head based on importance
 * WHY:  Enable stochastic head pruning during inference
 * HOW:  Apply softmax to head importance, sample from distribution
 *
 * @param head_importances Array of head importance scores
 * @param num_heads Number of attention heads
 * @param temperature Softmax temperature (higher = more random)
 * @return Index of selected head
 */
uint32_t attention_sample_head_mc(const float* head_importances,
                                   uint32_t num_heads,
                                   float temperature)
{
    if (!head_importances || num_heads == 0 || temperature <= 0.0f) {
        return 0;
    }

    if (g_attention_mc_seed == 0) {
        g_attention_mc_seed = mc_seed_from_time();
    }

    /* Find max for numerical stability */
    float max_imp = head_importances[0];
    for (uint32_t i = 1; i < num_heads; i++) {
        if (head_importances[i] > max_imp) {
            max_imp = head_importances[i];
        }
    }

    /* Compute softmax probabilities */
    float sum_exp = 0.0f;
    float* probs = (float*)nimcp_malloc(num_heads * sizeof(float));
    if (!probs) {
        return 0;
    }

    for (uint32_t i = 0; i < num_heads; i++) {
        probs[i] = expf((head_importances[i] - max_imp) / temperature);
        sum_exp += probs[i];
    }

    /* Sample from distribution */
    float r = mc_random_uniform(&g_attention_mc_seed) * sum_exp;
    float cumulative = 0.0f;

    for (uint32_t i = 0; i < num_heads; i++) {
        cumulative += probs[i];
        if (r < cumulative) {
            nimcp_free(probs);
            return i;
        }
    }

    nimcp_free(probs);
    return num_heads - 1;
}

/**
 * @brief Add noise to attention scores for exploration
 *
 * WHAT: Add Gaussian noise to attention for diverse patterns
 * WHY:  Enable exploration during attention computation
 * HOW:  GPU: Parallel noise generation (default for seq_length >= 256)
 *       CPU: Sequential noise generation (fallback)
 *
 * @param attention_scores Scores to modify in-place
 * @param seq_length Sequence length
 * @param noise_scale Standard deviation of noise
 */
void attention_add_exploration_noise_mc(float* attention_scores,
                                         uint32_t seq_length,
                                         float noise_scale)
{
    if (!attention_scores || seq_length == 0 || noise_scale <= 0.0f) {
        return;
    }

#ifdef NIMCP_ENABLE_CUDA
    /* Try GPU acceleration first (default for large sequences) */
    if (attention_has_gpu_mc() && seq_length >= 256) {
        size_t dims[] = {seq_length};
        nimcp_gpu_tensor_t* noise_tensor = nimcp_gpu_tensor_create(
            g_attention_gpu_ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);

        if (noise_tensor) {
            if (qmc_gpu_sample_normal(g_attention_gpu_ctx, g_attention_gpu_rng,
                                      noise_tensor, 0.0f, noise_scale)) {
                float* h_noise = nimcp_calloc(seq_length, sizeof(float));
                if (h_noise) {
                    cudaMemcpy(h_noise, noise_tensor->data, seq_length * sizeof(float), cudaMemcpyDeviceToHost);

                    for (uint32_t i = 0; i < seq_length; i++) {
                        attention_scores[i] += h_noise[i];
                    }

                    nimcp_free(h_noise);
                    nimcp_gpu_tensor_destroy(noise_tensor);
                    return;
                }
            }
            nimcp_gpu_tensor_destroy(noise_tensor);
        }
    }
#endif

    /* CPU fallback */
    if (g_attention_mc_seed == 0) {
        g_attention_mc_seed = mc_seed_from_time();
    }

    for (uint32_t i = 0; i < seq_length; i++) {
        float noise = mc_random_normal(&g_attention_mc_seed, 0.0f, noise_scale);
        attention_scores[i] += noise;
    }
}

/**
 * @brief Get thread-local MC seed for attention module
 *
 * @return Pointer to thread-local seed
 */
uint32_t* attention_get_mc_seed(void)
{
    if (g_attention_mc_seed == 0) {
        g_attention_mc_seed = mc_seed_from_time();
    }
    return &g_attention_mc_seed;
}
