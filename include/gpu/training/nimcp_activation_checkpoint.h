//=============================================================================
// nimcp_activation_checkpoint.h - High-Level Activation Checkpointing API
//=============================================================================
/**
 * @file nimcp_activation_checkpoint.h
 * @brief High-Level API for Activation Checkpointing in Sequential Models
 *
 * WHAT: Simplified checkpointing API for common sequential model patterns
 * WHY:  Makes gradient checkpointing easy to use without managing low-level details
 * HOW:  Wraps nimcp_gradient_checkpoint with convenience functions for sequential models
 *
 * ARCHITECTURE:
 *
 *   Sequential Model with Checkpointing:
 *
 *   FORWARD:
 *   Input --> [Layer 0] --> [Layer 1] --> ... --> [Layer N-1] --> Output
 *                |            |                      |
 *             [SAVE]       [FREE]                 [SAVE]
 *           (checkpoint)                        (checkpoint)
 *
 *   BACKWARD:
 *   d_Input <-- [Layer 0] <-- [Layer 1] <-- ... <-- [Layer N-1] <-- d_Output
 *                  ^            ^                      ^
 *               [LOAD]     [RECOMPUTE]              [LOAD]
 *
 * USAGE EXAMPLE:
 *
 *   // Create sequential checkpoint context
 *   nimcp_sequential_checkpoint_t* ckpt = nimcp_sequential_checkpoint_create(
 *       gpu_ctx, num_layers, memory_budget);
 *
 *   // Forward pass with checkpointing
 *   nimcp_gpu_tensor_t* output = nimcp_sequential_checkpoint_forward(
 *       ckpt, input, my_layer_forward, my_ctx);
 *
 *   // Backward pass with automatic recomputation
 *   nimcp_sequential_checkpoint_backward(
 *       ckpt, grad_output, my_layer_backward, my_ctx);
 *
 *   // Clean up
 *   nimcp_sequential_checkpoint_destroy(ckpt);
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_ACTIVATION_CHECKPOINT_H
#define NIMCP_ACTIVATION_CHECKPOINT_H

// Include GPU context BEFORE extern "C" block - CUDA headers contain C++
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/training/nimcp_gradient_checkpoint.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Layer Function Signatures
//=============================================================================

/**
 * @brief Function signature for sequential layer forward pass
 *
 * @param layer_idx Index of the layer (0 to num_layers-1)
 * @param ctx User-provided context
 * @param input Input tensor to the layer
 * @param output Output tensor from the layer (pre-allocated)
 */
typedef void (*nimcp_seq_layer_forward_fn_t)(
    int layer_idx,
    void* ctx,
    nimcp_gpu_tensor_t* input,
    nimcp_gpu_tensor_t* output
);

/**
 * @brief Function signature for sequential layer backward pass
 *
 * @param layer_idx Index of the layer (0 to num_layers-1)
 * @param ctx User-provided context
 * @param grad_output Gradient of loss w.r.t. layer output
 * @param grad_input Gradient of loss w.r.t. layer input (pre-allocated)
 */
typedef void (*nimcp_seq_layer_backward_fn_t)(
    int layer_idx,
    void* ctx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input
);

/**
 * @brief Function signature for getting output shape of a layer
 *
 * @param layer_idx Index of the layer
 * @param ctx User-provided context
 * @param input_dims Input dimensions
 * @param input_ndim Number of input dimensions
 * @param output_dims Output: output dimensions (caller provides buffer)
 * @param output_ndim Output: number of output dimensions
 */
typedef void (*nimcp_seq_layer_shape_fn_t)(
    int layer_idx,
    void* ctx,
    const size_t* input_dims,
    uint32_t input_ndim,
    size_t* output_dims,
    uint32_t* output_ndim
);

//=============================================================================
// Sequential Checkpoint Configuration
//=============================================================================

/**
 * @brief Configuration for sequential checkpointing
 */
typedef struct nimcp_seq_checkpoint_config {
    nimcp_checkpoint_strategy_t strategy;  /**< Checkpointing strategy */
    int checkpoint_every_n;                /**< For EVERY_N strategy */
    size_t memory_budget;                  /**< Target memory budget (0=unlimited) */
    bool preserve_rng;                     /**< Preserve RNG state for dropout */
    bool enable_profiling;                 /**< Enable timing profiling */
    bool verbose;                          /**< Verbose logging */
    bool auto_configure;                   /**< Auto-configure based on memory */
} nimcp_seq_checkpoint_config_t;

/**
 * @brief Initialize default configuration
 *
 * @param config Configuration structure to initialize
 */
NIMCP_EXPORT void nimcp_seq_checkpoint_config_init(nimcp_seq_checkpoint_config_t* config);

//=============================================================================
// Sequential Checkpoint Context
//=============================================================================

/**
 * @brief Context for sequential model checkpointing
 *
 * WHAT: Manages checkpointing for a sequential stack of layers
 * WHY:  Simplifies checkpointing for common sequential model architectures
 * HOW:  Wraps low-level checkpoint context with layer-aware management
 */
typedef struct nimcp_sequential_checkpoint {
    // Core checkpoint context
    nimcp_checkpoint_ctx_t* ckpt_ctx;       /**< Underlying checkpoint context */
    nimcp_gpu_context_t* gpu_ctx;           /**< GPU context */

    // Layer configuration
    int num_layers;                         /**< Number of layers */
    void** layer_contexts;                  /**< Per-layer user contexts */
    size_t* layer_output_sizes;             /**< Output size per layer (bytes) */

    // Shape function for dynamic shapes
    nimcp_seq_layer_shape_fn_t shape_fn;    /**< Optional shape query function */
    void* shape_ctx;                        /**< Context for shape function */

    // Activation storage
    nimcp_gpu_tensor_t** activations;       /**< Stored activations per layer */
    bool* activation_stored;                /**< Whether activation is stored */

    // Current forward state
    nimcp_gpu_tensor_t* current_input;      /**< Current forward input */
    nimcp_gpu_tensor_t* current_output;     /**< Current forward output */
    int current_layer;                      /**< Current layer being processed */

    // Configuration
    nimcp_seq_checkpoint_config_t config;   /**< Configuration */

    // Statistics
    size_t total_activation_memory;         /**< Total activation memory used */
    size_t saved_memory;                    /**< Memory saved by checkpointing */
    int recompute_count;                    /**< Number of recomputations */
    double recompute_time_ms;               /**< Total recomputation time */
} nimcp_sequential_checkpoint_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create sequential checkpoint context
 *
 * @param gpu_ctx GPU context for memory operations
 * @param num_layers Number of layers in the sequential model
 * @param memory_budget Target memory budget (0 for auto)
 * @return Sequential checkpoint context or NULL on failure
 */
NIMCP_EXPORT nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    size_t memory_budget
);

/**
 * @brief Create sequential checkpoint context with configuration
 *
 * @param gpu_ctx GPU context
 * @param num_layers Number of layers
 * @param config Configuration options
 * @return Sequential checkpoint context or NULL on failure
 */
NIMCP_EXPORT nimcp_sequential_checkpoint_t* nimcp_sequential_checkpoint_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    int num_layers,
    const nimcp_seq_checkpoint_config_t* config
);

/**
 * @brief Destroy sequential checkpoint context
 *
 * @param ckpt Context to destroy
 */
NIMCP_EXPORT void nimcp_sequential_checkpoint_destroy(nimcp_sequential_checkpoint_t* ckpt);

/**
 * @brief Reset checkpoint context for new training iteration
 *
 * Clears all stored activations and resets statistics.
 *
 * @param ckpt Sequential checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_reset(nimcp_sequential_checkpoint_t* ckpt);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set per-layer context
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @param layer_ctx User context for this layer
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_set_layer_ctx(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    void* layer_ctx
);

/**
 * @brief Set layer output size for memory planning
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @param output_size Output tensor size in bytes
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_set_layer_size(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    size_t output_size
);

/**
 * @brief Set shape function for dynamic shape inference
 *
 * @param ckpt Sequential checkpoint context
 * @param shape_fn Shape inference function
 * @param ctx Context to pass to shape function
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_set_shape_fn(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_seq_layer_shape_fn_t shape_fn,
    void* ctx
);

/**
 * @brief Configure checkpointing strategy
 *
 * @param ckpt Sequential checkpoint context
 * @param strategy Checkpointing strategy
 * @param checkpoint_n Checkpoint interval (for EVERY_N strategy)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_configure(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_checkpoint_strategy_t strategy,
    int checkpoint_n
);

/**
 * @brief Auto-configure based on layer sizes and memory budget
 *
 * @param ckpt Sequential checkpoint context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_auto_configure(
    nimcp_sequential_checkpoint_t* ckpt
);

//=============================================================================
// Forward Pass API
//=============================================================================

/**
 * @brief Run forward pass with checkpointing
 *
 * Executes all layers in sequence, saving activations at checkpoint boundaries.
 * Non-checkpointed activations are freed immediately after use.
 *
 * @param ckpt Sequential checkpoint context
 * @param input Input tensor to the first layer
 * @param layer_forward Forward function for each layer
 * @param forward_ctx Context to pass to forward function
 * @return Output tensor from the last layer
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx
);

/**
 * @brief Run single layer forward with checkpointing management
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index to run
 * @param input Input tensor
 * @param layer_forward Forward function
 * @param forward_ctx Context to pass to forward function
 * @return Output tensor (owned by context, do not free)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_forward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* input,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx
);

//=============================================================================
// Backward Pass API
//=============================================================================

/**
 * @brief Run backward pass with automatic recomputation
 *
 * Executes backward pass through all layers in reverse order.
 * Automatically recomputes activations that were freed during forward pass.
 *
 * @param ckpt Sequential checkpoint context
 * @param grad_output Gradient of loss w.r.t. final output
 * @param layer_backward Backward function for each layer
 * @param backward_ctx Context to pass to backward function
 * @return Gradient of loss w.r.t. input (or NULL if not needed)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx
);

/**
 * @brief Run backward pass with separate forward function for recomputation
 *
 * @param ckpt Sequential checkpoint context
 * @param grad_output Gradient of loss w.r.t. final output
 * @param layer_forward Forward function for recomputation
 * @param layer_backward Backward function
 * @param forward_ctx Context for forward function
 * @param backward_ctx Context for backward function
 * @return Gradient of loss w.r.t. input
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_with_recompute(
    nimcp_sequential_checkpoint_t* ckpt,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_forward_fn_t layer_forward,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* forward_ctx,
    void* backward_ctx
);

/**
 * @brief Run single layer backward with checkpoint management
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @param grad_output Gradient w.r.t. layer output
 * @param layer_backward Backward function
 * @param backward_ctx Context
 * @return Gradient w.r.t. layer input
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_backward_layer(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* grad_output,
    nimcp_seq_layer_backward_fn_t layer_backward,
    void* backward_ctx
);

//=============================================================================
// Activation Management API
//=============================================================================

/**
 * @brief Get activation for a layer (recomputes if necessary)
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index (0 = input, num_layers = output)
 * @param layer_forward Forward function for recomputation
 * @param forward_ctx Context for forward function
 * @return Activation tensor (owned by context)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_sequential_checkpoint_get_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_seq_layer_forward_fn_t layer_forward,
    void* forward_ctx
);

/**
 * @brief Check if activation is currently stored
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @return true if activation is stored
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_has_activation(
    const nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx
);

/**
 * @brief Manually store activation for a layer
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @param activation Activation tensor to store (will be cloned)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_store_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx,
    nimcp_gpu_tensor_t* activation
);

/**
 * @brief Free stored activation for a layer
 *
 * @param ckpt Sequential checkpoint context
 * @param layer_idx Layer index
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_free_activation(
    nimcp_sequential_checkpoint_t* ckpt,
    int layer_idx
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get checkpointing statistics
 *
 * @param ckpt Sequential checkpoint context
 * @param total_memory Output: total activation memory used
 * @param saved_memory Output: memory saved by checkpointing
 * @param recompute_count Output: number of recomputations
 * @param recompute_time_ms Output: total recomputation time
 */
NIMCP_EXPORT void nimcp_sequential_checkpoint_get_stats(
    const nimcp_sequential_checkpoint_t* ckpt,
    size_t* total_memory,
    size_t* saved_memory,
    int* recompute_count,
    double* recompute_time_ms
);

/**
 * @brief Print checkpointing statistics
 *
 * @param ckpt Sequential checkpoint context
 */
NIMCP_EXPORT void nimcp_sequential_checkpoint_print_stats(
    const nimcp_sequential_checkpoint_t* ckpt
);

/**
 * @brief Get checkpoint info as string
 *
 * @param ckpt Sequential checkpoint context
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written
 */
NIMCP_EXPORT int nimcp_sequential_checkpoint_get_info_string(
    const nimcp_sequential_checkpoint_t* ckpt,
    char* buffer,
    size_t size
);

//=============================================================================
// Transformer Block Checkpointing (Specialized)
//=============================================================================

/**
 * @brief Transformer block structure for checkpointing
 */
typedef struct nimcp_transformer_block {
    int num_heads;                          /**< Number of attention heads */
    int embed_dim;                          /**< Embedding dimension */
    int ffn_dim;                            /**< FFN intermediate dimension */
    bool pre_norm;                          /**< Pre-LayerNorm (GPT-2 style) */
    float dropout_p;                        /**< Dropout probability */
} nimcp_transformer_block_t;

/**
 * @brief Context for transformer checkpointing
 */
typedef struct nimcp_transformer_checkpoint {
    nimcp_sequential_checkpoint_t* seq_ckpt; /**< Underlying sequential context */
    int num_blocks;                          /**< Number of transformer blocks */
    nimcp_transformer_block_t* blocks;       /**< Block configurations */

    // Specialized storage
    nimcp_gpu_tensor_t** attention_scores;   /**< Stored attention scores */
    nimcp_gpu_tensor_t** ffn_intermediate;   /**< Stored FFN intermediates */
} nimcp_transformer_checkpoint_t;

/**
 * @brief Create transformer checkpoint context
 *
 * @param gpu_ctx GPU context
 * @param num_blocks Number of transformer blocks
 * @param memory_budget Memory budget
 * @return Transformer checkpoint context
 */
NIMCP_EXPORT nimcp_transformer_checkpoint_t* nimcp_transformer_checkpoint_create(
    nimcp_gpu_context_t* gpu_ctx,
    int num_blocks,
    size_t memory_budget
);

/**
 * @brief Configure transformer block parameters
 *
 * @param ckpt Transformer checkpoint context
 * @param block_idx Block index
 * @param block Block configuration
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_transformer_checkpoint_configure_block(
    nimcp_transformer_checkpoint_t* ckpt,
    int block_idx,
    const nimcp_transformer_block_t* block
);

/**
 * @brief Destroy transformer checkpoint context
 *
 * @param ckpt Context to destroy
 */
NIMCP_EXPORT void nimcp_transformer_checkpoint_destroy(
    nimcp_transformer_checkpoint_t* ckpt
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Estimate memory savings for different configurations
 *
 * @param num_layers Number of layers
 * @param layer_sizes Array of layer output sizes
 * @param no_ckpt_memory Output: memory without checkpointing
 * @param sqrt_ckpt_memory Output: memory with sqrt strategy
 * @param optimal_n Output: optimal N for EVERY_N strategy
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_estimate_memory(
    int num_layers,
    const size_t* layer_sizes,
    size_t* no_ckpt_memory,
    size_t* sqrt_ckpt_memory,
    int* optimal_n
);

/**
 * @brief Get recommended checkpoint strategy for memory budget
 *
 * @param num_layers Number of layers
 * @param layer_sizes Array of layer output sizes
 * @param memory_budget Target memory budget
 * @param strategy Output: recommended strategy
 * @param checkpoint_n Output: N value for EVERY_N
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_sequential_checkpoint_recommend(
    int num_layers,
    const size_t* layer_sizes,
    size_t memory_budget,
    nimcp_checkpoint_strategy_t* strategy,
    int* checkpoint_n
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ACTIVATION_CHECKPOINT_H
