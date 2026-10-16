//=============================================================================
// nimcp_gradient_checkpoint.h - Gradient Checkpointing for Memory Optimization
//=============================================================================
/**
 * @file nimcp_gradient_checkpoint.h
 * @brief GPU Gradient Checkpointing System for Memory-Efficient Training
 *
 * WHAT: Gradient checkpointing to trade computation for memory during training
 * WHY:  Enables training of larger models by reducing activation memory
 * HOW:  Selectively saves activations at checkpoint boundaries and recomputes
 *       intermediate activations during backward pass
 *
 * ARCHITECTURE:
 *
 *   FORWARD PASS (with checkpointing):
 *   +---------+     +---------+     +---------+     +---------+
 *   | Layer 0 | --> | Layer 1 | --> | Layer 2 | --> | Layer 3 |
 *   +---------+     +---------+     +---------+     +---------+
 *       |               |               |               |
 *       v               x               v               x
 *    [SAVE]          [FREE]          [SAVE]          [FREE]
 *   (checkpoint)                   (checkpoint)
 *
 *   BACKWARD PASS (with recomputation):
 *   +---------+     +---------+     +---------+     +---------+
 *   | Layer 0 | --> | Layer 1 | --> | Layer 2 | --> | Layer 3 |
 *   +---------+     +---------+     +---------+     +---------+
 *       ^               ^               ^               ^
 *    [LOAD]        [RECOMPUTE]      [LOAD]        [RECOMPUTE]
 *
 * STRATEGIES:
 * - SQRT: Checkpoint every sqrt(N) layers (O(sqrt(N)) memory, O(N*sqrt(N)) compute)
 * - EVERY_N: Checkpoint every N layers (configurable trade-off)
 * - SELECTIVE: User-specified checkpoint layers
 * - MEMORY_BUDGET: Auto-select checkpoints based on memory budget
 *
 * MEMORY SAVINGS:
 * - Without checkpointing: O(N) memory for N layers
 * - With sqrt checkpointing: O(sqrt(N)) memory
 * - Trade-off: One extra forward pass per segment during backward
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GRADIENT_CHECKPOINT_H
#define NIMCP_GRADIENT_CHECKPOINT_H

// Include GPU context BEFORE extern "C" block - CUDA headers contain C++
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Checkpoint Strategy Types
//=============================================================================

/**
 * @brief Checkpoint strategy for selecting which layers to checkpoint
 */
typedef enum {
    CKPT_STRATEGY_NONE = 0,        /**< No checkpointing (keep all activations) */
    CKPT_STRATEGY_SQRT = 1,        /**< Checkpoint every sqrt(N) layers */
    CKPT_STRATEGY_EVERY_N = 2,     /**< Checkpoint every N layers */
    CKPT_STRATEGY_SELECTIVE = 3,   /**< User-specified checkpoint layers */
    CKPT_STRATEGY_MEMORY_BUDGET = 4 /**< Auto-select based on memory budget */
} nimcp_checkpoint_strategy_t;

/**
 * @brief Checkpoint state for a layer
 */
typedef enum {
    CKPT_STATE_NONE = 0,           /**< Not checkpointed */
    CKPT_STATE_SAVED = 1,          /**< Activation saved */
    CKPT_STATE_FREED = 2,          /**< Activation freed (will recompute) */
    CKPT_STATE_RECOMPUTING = 3     /**< Currently recomputing */
} nimcp_checkpoint_state_t;

//=============================================================================
// Forward Function Signature
//=============================================================================

/**
 * @brief Function pointer for forward pass computation
 *
 * @param segment_ctx Opaque context for the segment
 * @param input Input tensor to the segment
 * @param output Output tensor from the segment (pre-allocated)
 */
typedef void (*nimcp_checkpoint_forward_fn_t)(
    void* segment_ctx,
    nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Function pointer for backward pass computation
 *
 * @param segment_ctx Opaque context for the segment
 * @param grad_output Gradient of loss w.r.t. segment output
 * @param grad_input Gradient of loss w.r.t. segment input (pre-allocated)
 */
typedef void (*nimcp_checkpoint_backward_fn_t)(
    void* segment_ctx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

//=============================================================================
// Checkpoint Segment Structure
//=============================================================================

/**
 * @brief A checkpoint segment (a recomputable unit of layers)
 *
 * WHAT: Represents a contiguous group of layers between checkpoints
 * WHY:  Enables efficient recomputation during backward pass
 * HOW:  Stores boundary activations and forward function for recomputation
 */
typedef struct nimcp_checkpoint_segment {
    int start_layer;                    /**< First layer index in segment */
    int end_layer;                      /**< Last layer index in segment */

    nimcp_gpu_tensor_t* saved_input;    /**< Saved input to this segment */
    nimcp_gpu_tensor_t* saved_output;   /**< Saved output (if terminal) */

    bool needs_recompute;               /**< Whether segment needs recomputation */
    bool input_is_checkpoint;           /**< Input is a checkpoint boundary */
    bool output_is_checkpoint;          /**< Output is a checkpoint boundary */

    size_t memory_saved;                /**< Bytes saved by not storing activations */
    size_t activation_size;             /**< Total activation size for segment */

    // Forward/backward function pointers for recomputation
    nimcp_checkpoint_forward_fn_t forward_fn;   /**< Forward pass function */
    nimcp_checkpoint_backward_fn_t backward_fn; /**< Backward pass function */
    void* segment_ctx;                  /**< Opaque context for functions */

    // RNG state for reproducible dropout
    bool preserve_rng;                  /**< Save RNG state for dropout */
    uint64_t rng_state_before;          /**< RNG state before forward */
    uint64_t rng_state_after;           /**< RNG state after forward */

    // Timing statistics
    double forward_time_ms;             /**< Forward pass time */
    double recompute_time_ms;           /**< Recomputation time */
} nimcp_checkpoint_segment_t;

//=============================================================================
// Layer Activation Info
//=============================================================================

/**
 * @brief Information about a single layer's activation
 */
typedef struct nimcp_layer_activation_info {
    int layer_idx;                      /**< Layer index */
    size_t activation_size;             /**< Size in bytes */
    nimcp_checkpoint_state_t state;     /**< Current checkpoint state */
    nimcp_gpu_tensor_t* activation;     /**< Stored activation (or NULL) */
    bool is_checkpoint_boundary;        /**< Is this a checkpoint boundary? */
    int segment_idx;                    /**< Which segment this belongs to */
} nimcp_layer_activation_info_t;

//=============================================================================
// Checkpoint Context Structure
//=============================================================================

/**
 * @brief Main checkpoint context (manages all checkpointing state)
 *
 * WHAT: Central manager for gradient checkpointing during training
 * WHY:  Coordinates checkpoint placement, activation storage, and recomputation
 * HOW:  Maintains segment list, tracks memory, and orchestrates recomputation
 */
typedef struct nimcp_checkpoint_ctx {
    // GPU context
    nimcp_gpu_context_t* gpu_ctx;       /**< GPU context for memory ops */

    // Strategy configuration
    nimcp_checkpoint_strategy_t strategy; /**< Checkpointing strategy */
    int checkpoint_every_n;             /**< For EVERY_N strategy */
    int* selective_layers;              /**< For SELECTIVE strategy */
    int num_selective_layers;           /**< Number of selective layers */

    // Layer information
    int total_layers;                   /**< Total number of layers */
    nimcp_layer_activation_info_t* layer_info; /**< Per-layer info */

    // Segment management
    nimcp_checkpoint_segment_t* segments; /**< Array of segments */
    int num_segments;                   /**< Number of segments */
    int max_segments;                   /**< Capacity of segments array */

    // Memory management
    size_t memory_budget;               /**< Target memory budget (bytes) */
    size_t current_memory;              /**< Current activation memory usage */
    size_t peak_memory;                 /**< Peak memory without checkpointing */
    size_t saved_memory;                /**< Total memory saved by checkpointing */

    // Activation tensor pool for efficient allocation
    nimcp_gpu_tensor_t** tensor_pool;   /**< Pool of reusable tensors */
    int pool_size;                      /**< Size of tensor pool */
    int pool_capacity;                  /**< Capacity of tensor pool */
    bool* pool_in_use;                  /**< Which pool slots are in use */

    // Statistics
    int recompute_count;                /**< Number of recomputations performed */
    double total_recompute_time_ms;     /**< Total recomputation time */
    double total_forward_time_ms;       /**< Total forward pass time */
    uint64_t forward_pass_count;        /**< Number of forward passes */
    uint64_t backward_pass_count;       /**< Number of backward passes */

    // State flags
    bool in_forward_pass;               /**< Currently in forward pass */
    bool in_backward_pass;              /**< Currently in backward pass */
    bool configured;                    /**< Checkpointing is configured */

    // Debug/profiling
    bool enable_profiling;              /**< Enable timing profiling */
    bool verbose;                       /**< Verbose logging */
} nimcp_checkpoint_ctx_t;

//=============================================================================
// Checkpointed Function Wrapper
//=============================================================================

/**
 * @brief Wrapper for a function that should be checkpointed
 *
 * WHAT: Encapsulates a forward/backward function pair for checkpointing
 * WHY:  Allows PyTorch-style checkpoint.checkpoint() functionality
 * HOW:  Saves input, runs forward, frees intermediate, recomputes on backward
 */
typedef struct nimcp_checkpointed_fn {
    nimcp_checkpoint_forward_fn_t forward;   /**< Forward function */
    nimcp_checkpoint_backward_fn_t backward; /**< Backward function */
    void* ctx;                          /**< User context */

    bool preserve_rng;                  /**< Save/restore RNG state */
    uint64_t rng_state;                 /**< Saved RNG state */

    nimcp_gpu_tensor_t* saved_input;    /**< Saved input for recomputation */
    nimcp_gpu_tensor_t* cached_output;  /**< Cached output (during forward) */

    bool is_checkpointed;               /**< Whether this fn is checkpointed */
} nimcp_checkpointed_fn_t;

//=============================================================================
// Memory Estimation Structure
//=============================================================================

/**
 * @brief Estimates for different checkpointing strategies
 */
typedef struct nimcp_checkpoint_estimate {
    size_t no_checkpoint_memory;        /**< Memory without checkpointing */
    size_t sqrt_checkpoint_memory;      /**< Memory with sqrt strategy */
    size_t every_n_memory[10];          /**< Memory for N=1,2,3,...,10 */

    double no_checkpoint_overhead;      /**< Compute overhead (1.0 = baseline) */
    double sqrt_recompute_overhead;     /**< Extra compute with sqrt strategy */
    double every_n_overhead[10];        /**< Extra compute for N=1,...,10 */

    int optimal_n;                      /**< Optimal N for EVERY_N strategy */
    size_t optimal_n_memory;            /**< Memory with optimal N */
    double optimal_n_overhead;          /**< Overhead with optimal N */
} nimcp_checkpoint_estimate_t;

//=============================================================================
// Context Lifecycle API
//=============================================================================

/**
 * @brief Create a checkpoint context
 *
 * @param gpu_ctx GPU context for memory operations
 * @param strategy Initial checkpointing strategy
 * @param total_layers Total number of layers in the model
 * @param memory_budget Target memory budget (0 for no limit)
 * @return Checkpoint context or NULL on failure
 */
NIMCP_EXPORT nimcp_checkpoint_ctx_t* nimcp_checkpoint_ctx_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_checkpoint_strategy_t strategy,
    int total_layers,
    size_t memory_budget
);

/**
 * @brief Destroy a checkpoint context and free all resources
 *
 * @param ctx Context to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_checkpoint_ctx_destroy(nimcp_checkpoint_ctx_t* ctx);

/**
 * @brief Reset checkpoint context for new training iteration
 *
 * Clears all stored activations and resets statistics.
 *
 * @param ctx Checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_ctx_reset(nimcp_checkpoint_ctx_t* ctx);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Configure checkpointing strategy
 *
 * @param ctx Checkpoint context
 * @param strategy Checkpointing strategy
 * @param checkpoint_every_n For EVERY_N strategy, checkpoint interval
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_configure(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_every_n
);

/**
 * @brief Configure selective checkpointing
 *
 * @param ctx Checkpoint context
 * @param layer_indices Array of layer indices to checkpoint
 * @param num_layers Number of layers in array
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_configure_selective(
    nimcp_checkpoint_ctx_t* ctx,
    const int* layer_indices,
    int num_layers
);

/**
 * @brief Auto-configure checkpointing based on memory budget
 *
 * Analyzes layer activation sizes and selects optimal checkpoint placement
 * to stay within the specified memory budget.
 *
 * @param ctx Checkpoint context
 * @param available_memory Available GPU memory in bytes
 * @param layer_activation_sizes Array of activation sizes per layer
 * @param num_layers Number of layers
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_auto_configure(
    nimcp_checkpoint_ctx_t* ctx,
    size_t available_memory,
    const size_t* layer_activation_sizes,
    int num_layers
);

/**
 * @brief Set layer activation size for memory planning
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @param size_bytes Activation size in bytes
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_set_layer_size(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    size_t size_bytes
);

//=============================================================================
// Checkpoint Operations API
//=============================================================================

/**
 * @brief Mark a layer as a checkpoint boundary
 *
 * Called during forward pass to save activation if this is a checkpoint.
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @param activation Activation tensor to potentially save
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_mark_layer(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx,
    nimcp_gpu_tensor_t* activation
);

/**
 * @brief Check if a layer's activation should be saved
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @return true if activation should be saved
 */
NIMCP_EXPORT bool nimcp_checkpoint_should_save(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx
);

/**
 * @brief Get saved activation for a layer (or trigger recompute)
 *
 * During backward pass, returns the activation for the specified layer.
 * If not checkpointed, triggers recomputation from nearest checkpoint.
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @return Activation tensor (owned by context, do not free)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_checkpoint_get_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx
);

/**
 * @brief Register forward function for a segment
 *
 * @param ctx Checkpoint context
 * @param start_layer First layer of segment
 * @param end_layer Last layer of segment
 * @param forward_fn Forward function for recomputation
 * @param fn_ctx Context to pass to forward function
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_register_forward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_forward_fn_t forward_fn,
    void* fn_ctx
);

/**
 * @brief Register backward function for a segment
 *
 * @param ctx Checkpoint context
 * @param start_layer First layer of segment
 * @param end_layer Last layer of segment
 * @param backward_fn Backward function
 * @param fn_ctx Context to pass to backward function
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_register_backward(
    nimcp_checkpoint_ctx_t* ctx,
    int start_layer,
    int end_layer,
    nimcp_checkpoint_backward_fn_t backward_fn,
    void* fn_ctx
);

/**
 * @brief Trigger recomputation for a segment
 *
 * Explicitly recomputes activations for the specified segment.
 *
 * @param ctx Checkpoint context
 * @param segment_idx Segment index
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_recompute_segment(
    nimcp_checkpoint_ctx_t* ctx,
    int segment_idx
);

/**
 * @brief Free activation for a layer (after backward pass)
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_free_activation(
    nimcp_checkpoint_ctx_t* ctx,
    int layer_idx
);

//=============================================================================
// Checkpointed Function API
//=============================================================================

/**
 * @brief Create a checkpointed function wrapper
 *
 * @param forward Forward function
 * @param backward Backward function
 * @param fn_ctx User context
 * @param preserve_rng Whether to save/restore RNG state
 * @return Checkpointed function wrapper
 */
NIMCP_EXPORT nimcp_checkpointed_fn_t* nimcp_checkpointed_fn_create(
    nimcp_checkpoint_forward_fn_t forward,
    nimcp_checkpoint_backward_fn_t backward,
    void* fn_ctx,
    bool preserve_rng
);

/**
 * @brief Destroy a checkpointed function wrapper
 *
 * @param fn Function wrapper to destroy
 */
NIMCP_EXPORT void nimcp_checkpointed_fn_destroy(nimcp_checkpointed_fn_t* fn);

/**
 * @brief Execute checkpointed function forward pass
 *
 * Like PyTorch's checkpoint.checkpoint() - saves input, runs forward,
 * but allows intermediate activations to be freed.
 *
 * @param ctx Checkpoint context
 * @param fn Checkpointed function wrapper
 * @param input Input tensor
 * @return Output tensor
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_checkpoint_function(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* input
);

/**
 * @brief Execute checkpointed function backward pass
 *
 * Recomputes forward pass from saved input, then runs backward.
 *
 * @param ctx Checkpoint context
 * @param fn Checkpointed function wrapper
 * @param grad_output Gradient of loss w.r.t. output
 * @param grad_input Output: gradient of loss w.r.t. input
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_function_backward(
    nimcp_checkpoint_ctx_t* ctx,
    nimcp_checkpointed_fn_t* fn,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

//=============================================================================
// Forward/Backward Pass Control
//=============================================================================

/**
 * @brief Begin forward pass (enables checkpointing)
 *
 * @param ctx Checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_begin_forward(nimcp_checkpoint_ctx_t* ctx);

/**
 * @brief End forward pass
 *
 * @param ctx Checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_end_forward(nimcp_checkpoint_ctx_t* ctx);

/**
 * @brief Begin backward pass (enables recomputation)
 *
 * @param ctx Checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_begin_backward(nimcp_checkpoint_ctx_t* ctx);

/**
 * @brief End backward pass
 *
 * @param ctx Checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_end_backward(nimcp_checkpoint_ctx_t* ctx);

//=============================================================================
// Memory Estimation API
//=============================================================================

/**
 * @brief Estimate memory requirements for different strategies
 *
 * @param layer_sizes Array of activation sizes per layer (bytes)
 * @param num_layers Number of layers
 * @param estimate Output: estimated memory for each strategy
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_estimate(
    const size_t* layer_sizes,
    int num_layers,
    nimcp_checkpoint_estimate_t* estimate
);

/**
 * @brief Get recommended strategy for memory budget
 *
 * @param layer_sizes Array of activation sizes per layer
 * @param num_layers Number of layers
 * @param memory_budget Target memory budget
 * @param strategy Output: recommended strategy
 * @param checkpoint_n Output: N value if EVERY_N recommended
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_checkpoint_recommend_strategy(
    const size_t* layer_sizes,
    int num_layers,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get checkpoint statistics
 *
 * @param ctx Checkpoint context
 * @param current_memory Output: current activation memory
 * @param peak_memory Output: peak memory without checkpointing
 * @param saved_memory Output: memory saved by checkpointing
 * @param recompute_count Output: number of recomputations
 * @param recompute_time_ms Output: total recomputation time
 */
NIMCP_EXPORT void nimcp_checkpoint_get_stats(
    const nimcp_checkpoint_ctx_t* ctx,
    size_t* current_memory,
    size_t* peak_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms
);

/**
 * @brief Print checkpoint statistics to stdout
 *
 * @param ctx Checkpoint context
 */
NIMCP_EXPORT void nimcp_checkpoint_print_stats(const nimcp_checkpoint_ctx_t* ctx);

/**
 * @brief Get checkpoint info as string
 *
 * @param ctx Checkpoint context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT int nimcp_checkpoint_get_info_string(
    const nimcp_checkpoint_ctx_t* ctx,
    char* buffer,
    size_t size
);

/**
 * @brief Enable/disable profiling
 *
 * @param ctx Checkpoint context
 * @param enable Enable profiling
 */
NIMCP_EXPORT void nimcp_checkpoint_set_profiling(
    nimcp_checkpoint_ctx_t* ctx,
    bool enable
);

/**
 * @brief Enable/disable verbose logging
 *
 * @param ctx Checkpoint context
 * @param verbose Enable verbose logging
 */
NIMCP_EXPORT void nimcp_checkpoint_set_verbose(
    nimcp_checkpoint_ctx_t* ctx,
    bool verbose
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get the checkpoint interval for sqrt strategy
 *
 * @param num_layers Total number of layers
 * @return Checkpoint interval (approximately sqrt(num_layers))
 */
NIMCP_EXPORT int nimcp_checkpoint_sqrt_interval(int num_layers);

/**
 * @brief Check if layer should be checkpointed with current strategy
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @return true if layer should be checkpointed
 */
NIMCP_EXPORT bool nimcp_checkpoint_is_checkpoint_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx
);

/**
 * @brief Get segment index containing a layer
 *
 * @param ctx Checkpoint context
 * @param layer_idx Layer index
 * @return Segment index or -1 if not found
 */
NIMCP_EXPORT int nimcp_checkpoint_get_segment_for_layer(
    const nimcp_checkpoint_ctx_t* ctx,
    int layer_idx
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_GRADIENT_CHECKPOINT_H
