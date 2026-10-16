/**
 * @file nimcp_lnn_parallel.h
 * @brief Parallelization primitives for LNN computation
 * @version 1.0.0
 * @date 2025-12-20
 *
 * WHAT: Multi-level parallelization for LNN forward/backward computation
 * WHY:  Exploit batch, layer, neuron, and ODE-level parallelism for performance
 * HOW:  Thread pool for batch parallelism, SIMD for neuron ops, pipeline for layers
 *
 * PARALLELIZATION LEVELS:
 * =======================
 *
 * 1. BATCH PARALLELISM
 *    - Different sequences processed on different threads
 *    - Each thread has a cloned network or thread-local state
 *    - Gradients reduced across threads after backward pass
 *
 * 2. LAYER PIPELINE
 *    - Overlap computation across layers
 *    - Producer-consumer pattern with ring buffer
 *    - Layer L processes timestep T while Layer L+1 processes timestep T-1
 *
 * 3. NEURON SIMD
 *    - Vectorized operations on neuron state/tau/weights
 *    - Uses tensor operations which internally use SIMD
 *    - Auto-detects AVX2/AVX-512 capabilities
 *
 * 4. ODE PARALLEL
 *    - Parallel evaluation of RK4 stages (k1, k2, k3, k4)
 *    - Limited benefit due to dependencies, mostly SIMD-level
 *
 * ARCHITECTURE:
 * =============
 *
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    BATCH PARALLELISM                             │
 * │  Thread 0   │  Thread 1   │  Thread 2   │  Thread 3   │  ...    │
 * │  Batch[0:8] │  Batch[8:16]│ Batch[16:24]│ Batch[24:32]│         │
 * └─────────────────────────────────────────────────────────────────┘
 *                              ↓
 * ┌─────────────────────────────────────────────────────────────────┐
 * │              PER-THREAD LAYER PIPELINE                           │
 * │  Time T     │  Time T+1   │  Time T+2   │  Time T+3   │         │
 * │  Layer 0    │  Layer 0    │  Layer 0    │  Layer 0    │         │
 * │      ↓      │      ↓      │      ↓      │      ↓      │         │
 * │  Layer 1    │  Layer 1    │  Layer 1    │  Layer 1    │         │
 * │      ↓      │      ↓      │      ↓      │      ↓      │         │
 * │  Layer 2    │  Layer 2    │  Layer 2    │  Layer 2    │         │
 * └─────────────────────────────────────────────────────────────────┘
 *                              ↓
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                   NEURON-LEVEL SIMD                              │
 * │  AVX-512: 16 neurons per vector operation                       │
 * │  x_new = x + dt * (-x/τ + activation(W*input + bias))           │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * USAGE EXAMPLE:
 * ==============
 *
 * ```c
 * // Initialize global parallelization
 * lnn_parallel_init(4);  // 4 threads
 *
 * // Create batch parallel context
 * lnn_parallel_config_t config;
 * lnn_parallel_config_default(&config);
 * config.enable_batch_parallel = true;
 * config.enable_simd = true;
 *
 * lnn_batch_parallel_ctx_t* ctx = lnn_batch_parallel_create(network, &config);
 *
 * // Forward pass on batch
 * nimcp_tensor_t* inputs = ...;   // [batch_size, seq_len, n_inputs]
 * nimcp_tensor_t* outputs = ...;  // [batch_size, seq_len, n_outputs]
 * lnn_batch_parallel_forward(ctx, inputs, outputs, batch_size, seq_len, dt);
 *
 * // Backward pass on batch
 * nimcp_tensor_t* loss_grads = ...;
 * lnn_batch_parallel_backward(ctx, loss_grads, batch_size);
 *
 * lnn_batch_parallel_destroy(ctx);
 * lnn_parallel_shutdown();
 * ```
 *
 * NIMCP STANDARDS:
 * ================
 * - Guard clauses (early returns)
 * - Helper functions < 50 lines
 * - WHAT/WHY/HOW documentation
 * - Thread-safe operations
 * - nimcp_malloc/nimcp_free
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LNN_PARALLEL_H
#define NIMCP_LNN_PARALLEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "lnn/nimcp_lnn_types.h"
#include "utils/tensor/nimcp_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct lnn_batch_parallel_ctx_s lnn_batch_parallel_ctx_t;
typedef struct lnn_pipeline_ctx_s lnn_pipeline_ctx_t;

/*=============================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Parallel execution configuration
 */
typedef struct {
    /* Threading */
    uint32_t n_threads;              /**< Number of worker threads (0 = auto) */

    /* Parallelism levels */
    bool enable_batch_parallel;      /**< Parallelize across batch items */
    bool enable_layer_pipeline;      /**< Pipeline layer execution */
    bool enable_simd;                /**< Use SIMD for neuron operations */
    bool enable_ode_parallel;        /**< Parallel ODE function evaluations */

    /* Batch parallelism */
    uint32_t batch_chunk_size;       /**< Items per thread (0 = auto) */

    /* SIMD */
    uint32_t neuron_simd_width;      /**< Vector width (0 = auto-detect) */

    /* Pipeline */
    uint32_t pipeline_depth;         /**< Pipeline stages (0 = n_layers) */
} lnn_parallel_config_t;

/*=============================================================================
 * Global Initialization
 *===========================================================================*/

/**
 * @brief Initialize global LNN parallelization system
 *
 * WHAT: Set up thread pool and detect SIMD capabilities
 * WHY:  Required before using parallel LNN operations
 * HOW:  Create thread pool, query CPU features, allocate shared resources
 *
 * @param n_threads Number of worker threads (0 = CPU count)
 * @return 0 on success, negative on error
 */
int lnn_parallel_init(uint32_t n_threads);

/**
 * @brief Shutdown global LNN parallelization system
 *
 * WHAT: Free thread pool and shared resources
 * WHY:  Clean shutdown
 * HOW:  Join threads, free memory
 */
void lnn_parallel_shutdown(void);

/**
 * @brief Get number of available worker threads
 *
 * @return Thread count
 */
uint32_t lnn_parallel_get_num_threads(void);

/**
 * @brief Initialize default parallel configuration
 *
 * WHAT: Fill config with sensible defaults
 * WHY:  Convenience for common use cases
 * HOW:  Set auto thread count, enable batch parallel, enable SIMD
 *
 * @param config Output configuration
 * @return 0 on success
 */
int lnn_parallel_config_default(lnn_parallel_config_t* config);

/*=============================================================================
 * Batch Parallelism
 *===========================================================================*/

/**
 * @brief Create batch parallel context
 *
 * WHAT: Allocate context for parallel batch processing
 * WHY:  Process multiple sequences simultaneously
 * HOW:  Clone network per thread, create task queue
 *
 * @param network LNN network to parallelize
 * @param config Parallel configuration
 * @return Context handle or NULL on failure
 */
lnn_batch_parallel_ctx_t* lnn_batch_parallel_create(
    lnn_network_t* network,
    const lnn_parallel_config_t* config
);

/**
 * @brief Destroy batch parallel context
 *
 * WHAT: Free all parallel resources
 * WHY:  Clean shutdown
 * HOW:  Destroy per-thread networks, free task queue
 *
 * @param ctx Batch parallel context
 */
void lnn_batch_parallel_destroy(lnn_batch_parallel_ctx_t* ctx);

/**
 * @brief Execute parallel forward pass on batch
 *
 * WHAT: Process batch items in parallel across threads
 * WHY:  Exploit batch-level parallelism for throughput
 * HOW:  Divide batch into chunks, submit to thread pool, gather outputs
 *
 * @param ctx Batch parallel context
 * @param inputs Input tensor [batch_size, seq_len, n_inputs]
 * @param outputs Output tensor [batch_size, seq_len, n_outputs] (pre-allocated)
 * @param batch_size Number of sequences in batch
 * @param seq_len Sequence length
 * @param dt Time step per sample
 * @return 0 on success, negative on error
 */
int lnn_batch_parallel_forward(
    lnn_batch_parallel_ctx_t* ctx,
    const nimcp_tensor_t* inputs,
    nimcp_tensor_t* outputs,
    uint32_t batch_size,
    uint32_t seq_len,
    float dt
);

/**
 * @brief Execute parallel backward pass on batch
 *
 * WHAT: Compute gradients in parallel, reduce across threads
 * WHY:  Parallelize backpropagation for speed
 * HOW:  Per-thread backward pass, accumulate gradients with atomics or locks
 *
 * @param ctx Batch parallel context
 * @param loss_grads Gradient of loss [batch_size, seq_len, n_outputs]
 * @param batch_size Number of sequences in batch
 * @return 0 on success, negative on error
 */
int lnn_batch_parallel_backward(
    lnn_batch_parallel_ctx_t* ctx,
    const nimcp_tensor_t* loss_grads,
    uint32_t batch_size
);

/*=============================================================================
 * SIMD Operations
 *===========================================================================*/

/**
 * @brief SIMD-optimized layer forward pass
 *
 * WHAT: Vectorized forward computation for layer
 * WHY:  Exploit SIMD for neuron-level parallelism
 * HOW:  Use tensor operations which internally use SIMD
 *
 * @param layer LNN layer
 * @param input Input tensor [n_inputs]
 * @param output Output tensor [n_neurons]
 * @param dt Time step
 * @return 0 on success
 */
int lnn_layer_forward_simd(
    lnn_layer_t* layer,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    float dt
);

/**
 * @brief SIMD-optimized ODE step
 *
 * WHAT: Vectorized ODE integration
 * WHY:  Process multiple neurons simultaneously
 * HOW:  Vector operations on state/tau tensors
 *
 * @param x State tensor [n_neurons] (updated in-place)
 * @param tau Time constant tensor [n_neurons]
 * @param input Input tensor [n_neurons]
 * @param dt Time step
 * @param method ODE solver method
 * @return 0 on success
 */
int lnn_ode_step_simd(
    nimcp_tensor_t* x,
    const nimcp_tensor_t* tau,
    const nimcp_tensor_t* input,
    float dt,
    lnn_ode_method_t method
);

/**
 * @brief SIMD-optimized activation function
 *
 * WHAT: Vectorized activation application
 * WHY:  Faster than scalar loop
 * HOW:  Tensor element-wise operations
 *
 * @param x Input/output tensor (updated in-place)
 * @param act Activation function type
 * @return 0 on success
 */
int lnn_activation_simd(nimcp_tensor_t* x, lnn_activation_t act);

/**
 * @brief SIMD-optimized matrix multiplication
 *
 * WHAT: Vectorized matmul
 * WHY:  Core operation in LNN forward/backward
 * HOW:  Use tensor matmul (internally optimized)
 *
 * @param A Matrix A
 * @param B Matrix B
 * @param C Output matrix C = A @ B (pre-allocated)
 * @return 0 on success
 */
int lnn_matmul_simd(
    const nimcp_tensor_t* A,
    const nimcp_tensor_t* B,
    nimcp_tensor_t* C
);

/*=============================================================================
 * Layer Pipeline
 *===========================================================================*/

/**
 * @brief Create layer pipeline context
 *
 * WHAT: Set up producer-consumer pipeline for layers
 * WHY:  Overlap layer computations for latency hiding
 * HOW:  Ring buffer between layers, condition variables for sync
 *
 * @param network LNN network
 * @param pipeline_depth Number of in-flight items (0 = n_layers)
 * @return Pipeline context or NULL on failure
 */
lnn_pipeline_ctx_t* lnn_pipeline_create(
    lnn_network_t* network,
    uint32_t pipeline_depth
);

/**
 * @brief Destroy layer pipeline context
 *
 * WHAT: Free pipeline resources
 * WHY:  Clean shutdown
 * HOW:  Flush pipeline, free buffers
 *
 * @param ctx Pipeline context
 */
void lnn_pipeline_destroy(lnn_pipeline_ctx_t* ctx);

/**
 * @brief Submit input to pipeline
 *
 * WHAT: Push input into pipeline for processing
 * WHY:  Asynchronous layer processing
 * HOW:  Enqueue to ring buffer, signal producer
 *
 * @param ctx Pipeline context
 * @param input Input tensor [n_inputs]
 * @param dt Time step
 * @return 0 on success, blocks if pipeline full
 */
int lnn_pipeline_submit(
    lnn_pipeline_ctx_t* ctx,
    const nimcp_tensor_t* input,
    float dt
);

/**
 * @brief Get output from pipeline
 *
 * WHAT: Retrieve processed output
 * WHY:  Retrieve result of pipeline computation
 * HOW:  Dequeue from output buffer, block if not ready
 *
 * @param ctx Pipeline context
 * @param output Output tensor [n_outputs] (pre-allocated)
 * @param timeout_ms Timeout in milliseconds (-1 = infinite)
 * @return 0 on success, negative on timeout/error
 */
int lnn_pipeline_get_output(
    lnn_pipeline_ctx_t* ctx,
    nimcp_tensor_t* output,
    int timeout_ms
);

/**
 * @brief Flush pipeline
 *
 * WHAT: Wait for all in-flight items to complete
 * WHY:  Ensure all submitted inputs are processed
 * HOW:  Drain pipeline, wait for completion
 *
 * @param ctx Pipeline context
 * @return 0 on success
 */
int lnn_pipeline_flush(lnn_pipeline_ctx_t* ctx);

/*=============================================================================
 * SIMD Capability Detection
 *===========================================================================*/

/**
 * @brief Detect optimal SIMD width for neuron operations
 *
 * WHAT: Query CPU features and return vector width
 * WHY:  Auto-configure for available SIMD
 * HOW:  Check for AVX-512 (16), AVX2 (8), SSE (4)
 *
 * @return SIMD width (floats per vector)
 */
uint32_t lnn_parallel_detect_simd_width(void);

/**
 * @brief Check if AVX2 is available
 *
 * @return true if AVX2 supported
 */
bool lnn_parallel_has_avx2(void);

/**
 * @brief Check if AVX-512 is available
 *
 * @return true if AVX-512 supported
 */
bool lnn_parallel_has_avx512(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_PARALLEL_H */
