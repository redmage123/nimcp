/**
 * @file nimcp_amp_autocast.h
 * @brief Automatic Mixed Precision (AMP) Autocast Context API
 *
 * WHAT: Context manager pattern for automatic precision selection
 * WHY:  Simplifies mixed precision training with automatic dtype selection
 * HOW:  Thread-local autocast state with operation-specific precision rules
 *
 * USAGE PATTERN:
 * ```c
 * // Create AMP context
 * nimcp_autocast_ctx_t* autocast = nimcp_autocast_create(gpu_ctx, AUTOCAST_FP16);
 *
 * // Enter autocast region
 * nimcp_autocast_begin(autocast);
 *
 * // Operations automatically use appropriate precision
 * nimcp_autocast_matmul(autocast, A, B, C);  // Uses FP16
 * nimcp_autocast_softmax(autocast, x, y);    // Uses FP32 for stability
 *
 * // Exit autocast region
 * nimcp_autocast_end(autocast);
 *
 * // Cleanup
 * nimcp_autocast_destroy(autocast);
 * ```
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: torch.cuda.amp.autocast(device_type, dtype, enabled)
 * - JAX: Manual dtype management with jax.lax.convert_element_type
 * - TensorFlow: tf.keras.mixed_precision.Policy("mixed_float16")
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_AMP_AUTOCAST_H
#define NIMCP_AMP_AUTOCAST_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/tensor/nimcp_tensor_fp16.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

//=============================================================================
// Constants
//=============================================================================

#define AUTOCAST_MAX_NESTING      16    /**< Maximum nesting depth */
#define AUTOCAST_CACHE_SIZE       256   /**< Cached tensor conversions */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Autocast precision mode
 */
typedef enum {
    AUTOCAST_DISABLED = 0,          /**< No autocasting (FP32) */
    AUTOCAST_FP16,                  /**< FP16 compute with FP32 accumulation */
    AUTOCAST_BF16,                  /**< BF16 compute (Ampere+) */
    AUTOCAST_TF32,                  /**< TF32 for Tensor Cores (Ampere+) */
} nimcp_autocast_mode_t;

/**
 * @brief Operation type for precision selection
 */
typedef enum {
    AUTOCAST_OP_MATMUL = 0,         /**< Matrix multiplication - FP16 */
    AUTOCAST_OP_CONV,               /**< Convolution - FP16 */
    AUTOCAST_OP_ATTENTION,          /**< Attention mechanism - FP16 */
    AUTOCAST_OP_LINEAR,             /**< Linear layer - FP16 */
    AUTOCAST_OP_EMBEDDING,          /**< Embedding lookup - FP16 */
    AUTOCAST_OP_SOFTMAX,            /**< Softmax - FP32 (stable) */
    AUTOCAST_OP_LAYERNORM,          /**< Layer normalization - FP32 (stable) */
    AUTOCAST_OP_BATCHNORM,          /**< Batch normalization - FP32 (stable) */
    AUTOCAST_OP_GROUPNORM,          /**< Group normalization - FP32 (stable) */
    AUTOCAST_OP_LOSS,               /**< Loss computation - FP32 (stable) */
    AUTOCAST_OP_REDUCTION,          /**< Sum/Mean reduction - FP32 (stable) */
    AUTOCAST_OP_ACTIVATION,         /**< Activation functions - FP16 */
    AUTOCAST_OP_ELEMENTWISE,        /**< Element-wise ops - FP16 */
    AUTOCAST_OP_CUSTOM,             /**< Custom operation */
    AUTOCAST_OP_COUNT
} nimcp_autocast_op_t;

//=============================================================================
// Autocast Context Structure
//=============================================================================

/**
 * @brief Cached tensor for autocast conversions
 */
typedef struct nimcp_autocast_cache_entry_s {
    const nimcp_gpu_tensor_t* original;  /**< Original tensor */
    nimcp_gpu_tensor_t* converted;       /**< Converted tensor */
    nimcp_mp_dtype_t target_dtype;       /**< Target dtype */
    uint64_t last_used;                  /**< LRU timestamp */
    bool valid;                          /**< Entry is valid */
} nimcp_autocast_cache_entry_t;

/**
 * @brief Autocast context for automatic precision selection
 *
 * WHAT: Manages automatic precision selection for operations
 * WHY:  Simplifies mixed precision training workflow
 * HOW:  Maintains state and caches converted tensors
 */
typedef struct nimcp_autocast_ctx_s {
    nimcp_gpu_context_t* gpu_ctx;        /**< GPU context */
    nimcp_autocast_mode_t mode;          /**< Current mode */

    // Autocast state
    bool enabled;                        /**< Autocasting enabled */
    int nesting_level;                   /**< Nesting depth */
    nimcp_autocast_mode_t mode_stack[AUTOCAST_MAX_NESTING]; /**< Nested modes */

    // Per-operation precision settings
    nimcp_mp_dtype_t op_dtypes[AUTOCAST_OP_COUNT]; /**< Dtype per op type */
    bool op_force_fp32[AUTOCAST_OP_COUNT];         /**< Force FP32 for op */

    // Tensor conversion cache
    nimcp_autocast_cache_entry_t cache[AUTOCAST_CACHE_SIZE];
    int cache_count;                     /**< Number of cached entries */
    uint64_t cache_timestamp;            /**< LRU counter */

    // Loss scaler integration
    nimcp_loss_scaler_t* scaler;         /**< Associated loss scaler */
    bool owns_scaler;                    /**< Whether we own the scaler */

    // Statistics
    uint64_t casts_performed;            /**< Total casts performed */
    uint64_t cache_hits;                 /**< Cache hits */
    uint64_t cache_misses;               /**< Cache misses */
    uint64_t fp16_ops;                   /**< FP16 operations */
    uint64_t fp32_ops;                   /**< FP32 operations */
} nimcp_autocast_ctx_t;

/**
 * @brief Autocast configuration
 */
typedef struct nimcp_autocast_config_s {
    nimcp_autocast_mode_t mode;          /**< Precision mode */
    bool enable_caching;                 /**< Enable tensor caching */
    bool enable_scaler;                  /**< Enable loss scaler */
    float init_scale;                    /**< Initial loss scale */

    // Per-operation overrides (NULL = use defaults)
    nimcp_mp_dtype_t* op_overrides;      /**< Custom op dtypes */
    int num_overrides;                   /**< Number of overrides */
} nimcp_autocast_config_t;

//=============================================================================
// Autocast Context Lifecycle
//=============================================================================

/**
 * @brief Create autocast context with default settings
 *
 * @param gpu_ctx GPU context
 * @param mode Precision mode
 * @return Autocast context or NULL
 */
NIMCP_EXPORT nimcp_autocast_ctx_t* nimcp_autocast_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_autocast_mode_t mode
);

/**
 * @brief Create autocast context with custom configuration
 *
 * @param gpu_ctx GPU context
 * @param config Configuration
 * @return Autocast context or NULL
 */
NIMCP_EXPORT nimcp_autocast_ctx_t* nimcp_autocast_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    const nimcp_autocast_config_t* config
);

/**
 * @brief Get default configuration
 *
 * @param config Output configuration
 * @param mode Precision mode
 */
NIMCP_EXPORT void nimcp_autocast_default_config(
    nimcp_autocast_config_t* config,
    nimcp_autocast_mode_t mode
);

/**
 * @brief Destroy autocast context
 *
 * @param ctx Context to destroy
 */
NIMCP_EXPORT void nimcp_autocast_destroy(nimcp_autocast_ctx_t* ctx);

//=============================================================================
// Autocast Region Control
//=============================================================================

/**
 * @brief Enter autocast region
 *
 * Enables automatic precision selection for subsequent operations.
 * Can be nested; inner calls preserve the current mode.
 *
 * @param ctx Autocast context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_autocast_begin(nimcp_autocast_ctx_t* ctx);

/**
 * @brief Enter autocast region with specific mode
 *
 * @param ctx Autocast context
 * @param mode Precision mode for this region
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_autocast_begin_with_mode(
    nimcp_autocast_ctx_t* ctx,
    nimcp_autocast_mode_t mode
);

/**
 * @brief Exit autocast region
 *
 * Restores previous autocast state (for nested calls).
 *
 * @param ctx Autocast context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_autocast_end(nimcp_autocast_ctx_t* ctx);

/**
 * @brief Check if autocasting is active
 *
 * @param ctx Autocast context
 * @return true if in autocast region
 */
NIMCP_EXPORT bool nimcp_autocast_is_active(const nimcp_autocast_ctx_t* ctx);

/**
 * @brief Get current autocast mode
 *
 * @param ctx Autocast context
 * @return Current mode
 */
NIMCP_EXPORT nimcp_autocast_mode_t nimcp_autocast_get_mode(
    const nimcp_autocast_ctx_t* ctx
);

//=============================================================================
// Precision Query API
//=============================================================================

/**
 * @brief Get appropriate dtype for operation
 *
 * Returns the recommended dtype based on autocast mode and operation type.
 *
 * @param ctx Autocast context
 * @param op Operation type
 * @return Recommended dtype
 */
NIMCP_EXPORT nimcp_mp_dtype_t nimcp_autocast_get_op_dtype(
    const nimcp_autocast_ctx_t* ctx,
    nimcp_autocast_op_t op
);

/**
 * @brief Override dtype for specific operation
 *
 * @param ctx Autocast context
 * @param op Operation type
 * @param dtype Dtype to use
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_autocast_set_op_dtype(
    nimcp_autocast_ctx_t* ctx,
    nimcp_autocast_op_t op,
    nimcp_mp_dtype_t dtype
);

/**
 * @brief Force FP32 for specific operation
 *
 * @param ctx Autocast context
 * @param op Operation type
 * @param force_fp32 Whether to force FP32
 */
NIMCP_EXPORT void nimcp_autocast_force_fp32(
    nimcp_autocast_ctx_t* ctx,
    nimcp_autocast_op_t op,
    bool force_fp32
);

//=============================================================================
// Automatic Tensor Casting
//=============================================================================

/**
 * @brief Cast input tensor to appropriate dtype for operation
 *
 * Automatically casts tensor based on current autocast mode and operation.
 * Uses caching to avoid redundant conversions.
 *
 * @param ctx Autocast context
 * @param tensor Input tensor
 * @param op Operation type
 * @return Casted tensor (may be same as input if no cast needed)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_cast_input(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    nimcp_autocast_op_t op
);

/**
 * @brief Cast output tensor to FP32 (for stability)
 *
 * Used after operations that produce FP16 output but need FP32 for
 * subsequent operations.
 *
 * @param ctx Autocast context
 * @param tensor Output tensor (FP16)
 * @return FP32 tensor
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_cast_output_fp32(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* tensor
);

/**
 * @brief Clear tensor conversion cache
 *
 * Frees all cached tensors. Call when memory is tight or
 * at the end of a training step.
 *
 * @param ctx Autocast context
 */
NIMCP_EXPORT void nimcp_autocast_clear_cache(nimcp_autocast_ctx_t* ctx);

//=============================================================================
// Autocast Operations (Convenience Wrappers)
//=============================================================================

/**
 * @brief Autocast matrix multiplication
 *
 * Automatically casts inputs to FP16 and performs GEMM.
 *
 * @param ctx Autocast context
 * @param A First matrix
 * @param B Second matrix
 * @param C Output matrix (will be created if NULL)
 * @param alpha Scale for A @ B
 * @param beta Scale for C
 * @param trans_a Transpose A
 * @param trans_b Transpose B
 * @return Output tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_matmul(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* A,
    nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

/**
 * @brief Autocast softmax (always FP32 for stability)
 *
 * @param ctx Autocast context
 * @param x Input tensor
 * @param out Output tensor (will be created if NULL)
 * @return Output tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_softmax(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief Autocast layer normalization (FP32 for stability)
 *
 * @param ctx Autocast context
 * @param x Input tensor
 * @param gamma Scale parameter
 * @param beta Shift parameter
 * @param out Output tensor
 * @param eps Epsilon for numerical stability
 * @return Output tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_layernorm(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* gamma,
    nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps
);

/**
 * @brief Autocast activation function
 *
 * @param ctx Autocast context
 * @param x Input tensor
 * @param out Output tensor
 * @param activation Activation type (0=ReLU, 1=GELU, 2=SiLU, 3=Sigmoid, 4=Tanh)
 * @return Output tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_autocast_activation(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int activation
);

//=============================================================================
// Loss Scaling Integration
//=============================================================================

/**
 * @brief Get loss scaler from autocast context
 *
 * @param ctx Autocast context
 * @return Loss scaler or NULL if not enabled
 */
NIMCP_EXPORT nimcp_loss_scaler_t* nimcp_autocast_get_scaler(
    nimcp_autocast_ctx_t* ctx
);

/**
 * @brief Attach loss scaler to autocast context
 *
 * @param ctx Autocast context
 * @param scaler Loss scaler (context takes ownership)
 */
NIMCP_EXPORT void nimcp_autocast_set_scaler(
    nimcp_autocast_ctx_t* ctx,
    nimcp_loss_scaler_t* scaler
);

/**
 * @brief Scale loss for backward pass
 *
 * @param ctx Autocast context
 * @param loss Original loss
 * @return Scaled loss
 */
NIMCP_EXPORT float nimcp_autocast_scale_loss(
    nimcp_autocast_ctx_t* ctx,
    float loss
);

/**
 * @brief Unscale gradients and check for overflow
 *
 * @param ctx Autocast context
 * @param gradients Gradient tensor
 * @return true if gradients are valid (no overflow)
 */
NIMCP_EXPORT bool nimcp_autocast_unscale_gradients(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* gradients
);

/**
 * @brief Update loss scale after optimizer step
 *
 * @param ctx Autocast context
 * @param gradients_valid Whether gradients were valid
 */
NIMCP_EXPORT void nimcp_autocast_update_scale(
    nimcp_autocast_ctx_t* ctx,
    bool gradients_valid
);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get autocast statistics
 *
 * @param ctx Autocast context
 * @param casts_performed Output: total casts
 * @param cache_hits Output: cache hits
 * @param cache_misses Output: cache misses
 */
NIMCP_EXPORT void nimcp_autocast_get_stats(
    const nimcp_autocast_ctx_t* ctx,
    uint64_t* casts_performed,
    uint64_t* cache_hits,
    uint64_t* cache_misses
);

/**
 * @brief Reset autocast statistics
 *
 * @param ctx Autocast context
 */
NIMCP_EXPORT void nimcp_autocast_reset_stats(nimcp_autocast_ctx_t* ctx);

/**
 * @brief Get operation type name
 *
 * @param op Operation type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_autocast_op_name(nimcp_autocast_op_t op);

/**
 * @brief Get autocast mode name
 *
 * @param mode Autocast mode
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_autocast_mode_name(nimcp_autocast_mode_t mode);

/**
 * @brief Print autocast configuration
 *
 * @param ctx Autocast context
 */
NIMCP_EXPORT void nimcp_autocast_print_config(const nimcp_autocast_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_AMP_AUTOCAST_H */
