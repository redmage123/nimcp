//=============================================================================
// nimcp_training_bridge.h - GPU Weight Cache & Forward Pass Bridge
//=============================================================================
/**
 * @file nimcp_training_bridge.h
 * @brief Bridge between neuron/synapse AoS layout and GPU tensor operations
 *
 * WHAT: Extracts contiguous weight matrices from neuron_t/synapse_t structs,
 *       runs forward pass + loss computation on GPU, writes results back.
 * WHY:  Neural network uses Array-of-Structures (each neuron has incoming_synapses[]),
 *       but GPU GEMM needs contiguous weight matrices. This bridge layer handles
 *       the AoS <-> matrix conversion.
 * HOW:  Upload: iterate neurons per layer, extract weight*strength into row-major matrix.
 *       Download: write GPU matrix values back to synapse structs.
 *
 * ARCHITECTURE:
 *   CPU                           GPU
 *   -----                         -----
 *   input data ---upload--->  [input tensor]
 *                                  |
 *                             GEMV per layer: W[l] @ a[l] + b[l]
 *                             Activation (sigmoid/tanh/relu)
 *                             MSE loss
 *                                  |
 *   output, loss <--download--     |
 *        |
 *   reward = 1/(1+loss)
 *   Sync activations -> neuron states
 *   STDP/BCM/eligibility (CPU)
 *   Mark weights_dirty = true
 *
 * @version 1.0
 * @date 2026
 */

#ifndef NIMCP_TRAINING_BRIDGE_H
#define NIMCP_TRAINING_BRIDGE_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/sparse/nimcp_sparse_gpu.h"
#include "gpu/tensor/nimcp_amp_autocast.h"
#include "core/neuralnet/nimcp_neuralnet.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// GPU Weight Cache Structure
//=============================================================================

/**
 * @brief Cache of GPU tensors mirroring neural network weight matrices
 *
 * Maintains GPU-resident copies of weight matrices extracted from the
 * neuron/synapse AoS layout. Tracks dirty state so weights are only
 * re-uploaded when CPU biological learning modifies them.
 */
typedef struct nimcp_gpu_weight_cache_s {
    nimcp_gpu_context_t* ctx;              /**< GPU context for operations */

    uint32_t num_layers;                   /**< Number of layers (from config) */
    uint32_t* layer_sizes;                 /**< Size of each layer (owned copy) */
    uint32_t* layer_offsets;               /**< Neuron index offset per layer */

    // Per-layer GPU tensors (num_layers - 1 transitions)
    nimcp_sparse_tensor_t** sparse_weights; /**< W[l] = CSR sparse (backward/plasticity) */
    nimcp_sparse_tensor_t** bsr_weights;    /**< W[l] = BSR sparse (forward pass, tensor cores) */
    nimcp_sparse_ctx_t* sparse_ctx;         /**< cuSPARSE context for sparse operations */
    nimcp_gpu_tensor_t** biases;           /**< b[l] = (layer_sizes[l+1]) */
    nimcp_gpu_tensor_t** activations;      /**< a[l] = (layer_sizes[l]) per-layer output */

    // Per-layer activation types
    activation_type_t* layer_activations;  /**< Activation function per layer */

    // Sync state
    bool weights_dirty_on_cpu;             /**< CPU modified weights, need re-upload */
    uint64_t last_sync_step;               /**< Last step weights were synced */

    // Host-side scratch buffers (reused across uploads)
    float* host_coo_values;                /**< COO/CSR values scratch [max_nnz] */
    int*   host_coo_row_idx;               /**< COO row indices scratch [max_nnz] */
    int*   host_coo_col_idx;               /**< COO/CSR col indices scratch [max_nnz] */
    int*   host_csr_row_ptrs;              /**< CSR row pointers scratch [max_rows+1] */
    size_t host_coo_capacity;              /**< Allocated COO entry capacity */
    size_t host_csr_row_ptrs_capacity;     /**< Allocated row_ptrs capacity */
    float* host_bias_buf;                  /**< Largest bias vector buffer */
    float* host_activation_buf;            /**< Largest activation vector buffer */
    size_t host_bias_buf_size;             /**< Size of bias buffer in floats */
    size_t host_activation_buf_size;       /**< Size of activation buffer in floats */

    // Per-transition connected neuron indices (for fast upload of sparse networks)
    // Only neurons with NEURON_IN_COUNT > 0 appear in these lists.
    // Built on first upload, reused on subsequent uploads.
    uint32_t** connected_dst;              /**< Per-transition: local indices of connected dst neurons */
    uint32_t*  num_connected_dst;          /**< Per-transition: count of connected dst neurons */
    bool       connected_dst_valid;        /**< True once connected_dst lists are built */

    // Gradient accumulation buffers (for mini-batch training)
    float**    d_weight_grad_accum;        /**< Per-transition GPU weight gradient accum [nnz each] */
    float**    d_bias_grad_accum;          /**< Per-transition GPU bias gradient accum [layer_size each] */
    uint32_t   grad_accum_count;           /**< Number of samples accumulated since last flush */
    bool       grad_accum_initialized;     /**< True once accum buffers are allocated */

    // Mixed precision (AMP) support
    nimcp_autocast_ctx_t* autocast_ctx;    /**< Autocast context for FP16 mixed precision (NULL if disabled) */
    bool       mixed_precision_enabled;    /**< True if mixed precision training is active */

    // Gradient checkpointing (trade compute for memory)
    // WHY: Reduces peak activation memory from O(L) to O(sqrt(L)) by only storing
    //       activations at checkpoint layers, recomputing intermediates during backward.
    // HOW: During forward pass, only keep activations at every checkpoint_interval-th
    //       layer. During backward, recompute intermediate activations from the nearest
    //       checkpoint before computing gradients for that segment.
    bool       gradient_checkpointing;     /**< Enable gradient checkpointing */
    uint32_t   checkpoint_interval;        /**< Save activations every N layers (default: 2) */
    nimcp_gpu_tensor_t** checkpoint_activations; /**< Saved activations at checkpoint layers only */
    bool*      is_checkpoint_layer;        /**< Per-layer: true if this layer is a checkpoint boundary */
    uint32_t   num_checkpoint_layers;      /**< Number of checkpoint boundary layers */
} nimcp_gpu_weight_cache_t;

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Create GPU weight cache from a neural network
 *
 * Allocates GPU tensors for all layer weight matrices, bias vectors,
 * and activation vectors. Does NOT upload weights (call upload separately).
 *
 * @param ctx GPU context (must be valid)
 * @param net Neural network to mirror
 * @param layer_sizes Array of layer sizes (num_layers elements)
 * @param num_layers Number of layers
 * @return Weight cache or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_weight_cache_t* nimcp_gpu_weight_cache_create(
    nimcp_gpu_context_t* ctx,
    neural_network_t net,
    const uint32_t* layer_sizes,
    uint32_t num_layers
);

/**
 * @brief Destroy GPU weight cache and free all GPU tensors
 *
 * @param cache Weight cache to destroy (NULL-safe)
 */
NIMCP_EXPORT void nimcp_gpu_weight_cache_destroy(nimcp_gpu_weight_cache_t* cache);

//=============================================================================
// Weight Synchronization
//=============================================================================

/**
 * @brief Upload weights from CPU neuron/synapse structs to GPU tensors
 *
 * Extracts effective weight (weight * strength) from each synapse's
 * incoming_synapses array and builds row-major matrices for GPU GEMV.
 * Also uploads neuron biases.
 *
 * @param cache Weight cache
 * @param net Neural network source
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_upload(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net
);

/**
 * @brief Download weights from GPU tensors back to CPU synapse structs
 *
 * Writes GPU weight matrix values back to synapse weight fields.
 * Divides by synapse strength to preserve the strength factor separately.
 *
 * @param cache Weight cache
 * @param net Neural network destination
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_download(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net
);

/**
 * @brief Sync GPU activation values back to neuron state fields
 *
 * After GPU forward pass, downloads activation tensors and writes
 * values to neuron->state so CPU biological learning (STDP/BCM)
 * can read current neuron activations.
 *
 * @param cache Weight cache with computed activations
 * @param net Neural network to update
 */
NIMCP_EXPORT void nimcp_gpu_weight_cache_sync_activations(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net
);

//=============================================================================
// GPU Compute Operations
//=============================================================================

/**
 * @brief Run forward pass on GPU
 *
 * Uploads input, runs per-layer GEMV + bias + activation + clamp,
 * downloads output. Uses existing GPU tensor API functions.
 *
 * @param cache Weight cache (weights must be uploaded)
 * @param input Input data (host memory)
 * @param input_size Input vector size
 * @param output Output buffer (host memory, pre-allocated)
 * @param output_size Output vector size
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_forward_pass(
    nimcp_gpu_weight_cache_t* cache,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Compute MSE loss on GPU
 *
 * @param cache Weight cache (for GPU context)
 * @param output Network output (host memory)
 * @param target Target values (host memory)
 * @param size Vector size
 * @return MSE loss value, or -1.0f on error
 */
NIMCP_EXPORT float nimcp_gpu_compute_loss(
    nimcp_gpu_weight_cache_t* cache,
    const float* output,
    const float* target,
    uint32_t size
);

//=============================================================================
// Batched GPU Forward Pass
//=============================================================================

/**
 * @brief Run batched forward pass on GPU
 *
 * Processes multiple input vectors in a single GPU operation using SpMM
 * (sparse matrix-matrix multiply) instead of per-sample SpMV.
 *
 * @param cache Weight cache (weights must be uploaded)
 * @param inputs Input data row-major [batch_size x input_size] (host memory)
 * @param batch_size Number of samples in batch
 * @param input_size Input vector size (must match layer_sizes[0])
 * @param outputs Output buffer row-major [batch_size x output_size] (host memory)
 * @param output_size Output vector size (must match layer_sizes[last])
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_forward_pass_batch(
    nimcp_gpu_weight_cache_t* cache,
    const float* inputs,
    uint32_t batch_size,
    uint32_t input_size,
    float* outputs,
    uint32_t output_size
);

//=============================================================================
// GPU Backward Pass
//=============================================================================

/**
 * @brief Run sparse backward pass on GPU, updating weight cache and syncing back
 *
 * WHAT: GPU-accelerated backpropagation using the existing weight cache
 * WHY:  Replaces CPU backprop_sparse_full for ~3-4x training speedup
 * HOW:  Runs GPU sparse kernels, then downloads weights to CPU synapse structs
 *
 * @param cache Weight cache (must have valid sparse weights + activations)
 * @param net Neural network (for weight writeback)
 * @param target Target vector (host memory)
 * @param output Network output (host memory)
 * @param target_size Target vector size
 * @param learning_rate Learning rate
 * @param min_weight Minimum weight clamp
 * @param max_weight Maximum weight clamp
 * @param out_grad_norm Output gradient norm
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_backward_pass(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net,
    const float* target,
    const float* output,
    uint32_t target_size,
    float learning_rate,
    float min_weight, float max_weight,
    float* out_grad_norm
);

//=============================================================================
// GPU Gradient Accumulation (Mini-Batch Training)
//=============================================================================

/**
 * @brief Accumulate gradients from one sample (no weight update)
 *
 * Lazily allocates GPU gradient buffers on first call. Runs GPU backward pass
 * but writes gradients to accumulation buffers instead of updating weights.
 *
 * @param cache Weight cache
 * @param target Target vector (host memory)
 * @param output Network output (host memory)
 * @param target_size Target vector size
 * @param learning_rate Learning rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_backward_accumulate(
    nimcp_gpu_weight_cache_t* cache,
    const float* target,
    const float* output,
    uint32_t target_size,
    float learning_rate
);

/**
 * @brief Apply accumulated gradients to weights, then download to CPU
 *
 * Divides accumulated gradients by sample count, applies to weights,
 * downloads updated weights to CPU synapse structs, resets accumulators.
 *
 * @param cache Weight cache
 * @param net Neural network (for weight writeback)
 * @param min_weight Minimum weight clamp
 * @param max_weight Maximum weight clamp
 * @param out_grad_norm Output gradient norm
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_gradient_flush_and_sync(
    nimcp_gpu_weight_cache_t* cache,
    neural_network_t net,
    float min_weight, float max_weight,
    float* out_grad_norm
);

//=============================================================================
// Mixed Precision (AMP) Support
//=============================================================================

/**
 * @brief Enable or disable FP16 mixed precision on the GPU weight cache
 *
 * Creates an autocast context for FP16 compute with FP32 storage.
 * When enabled, backward accumulation and gradient flush operations
 * are wrapped in autocast begin/end regions for automatic precision
 * selection.
 *
 * @param cache Weight cache
 * @param enable true to enable, false to disable
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_enable_mixed_precision(
    nimcp_gpu_weight_cache_t* cache,
    bool enable
);

/**
 * @brief Check if mixed precision is enabled on the weight cache
 *
 * @param cache Weight cache
 * @return true if mixed precision is active
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_is_mixed_precision(
    const nimcp_gpu_weight_cache_t* cache
);

//=============================================================================
// Gradient Checkpointing Control
//=============================================================================

/**
 * @brief Enable or disable gradient checkpointing on a weight cache
 *
 * When enabled, the forward pass only retains activations at every
 * checkpoint_interval-th layer. During backward pass, intermediate
 * activations are recomputed from the nearest checkpoint boundary.
 * This trades ~1 extra forward pass per segment for O(sqrt(L)) memory
 * instead of O(L).
 *
 * MEMORY SAVINGS:
 *   Without checkpointing: stores activations for ALL L layers
 *   With checkpointing (interval=2): stores ~L/2 activations, recomputes rest
 *   With checkpointing (interval=sqrt(L)): stores ~sqrt(L) activations
 *
 * @param cache Weight cache
 * @param enable true to enable, false to disable
 * @param checkpoint_interval Layers between checkpoints (0 = auto = every 2 layers)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_set_gradient_checkpointing(
    nimcp_gpu_weight_cache_t* cache,
    bool enable,
    uint32_t checkpoint_interval
);

/**
 * @brief Check if gradient checkpointing is enabled on the weight cache
 *
 * @param cache Weight cache
 * @return true if gradient checkpointing is active
 */
NIMCP_EXPORT bool nimcp_gpu_weight_cache_is_gradient_checkpointing(
    const nimcp_gpu_weight_cache_t* cache
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRAINING_BRIDGE_H */
