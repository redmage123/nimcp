/**
 * @file nimcp_tensor_fp16.h
 * @brief Mixed Precision (FP16/BF16) Tensor Operations for NIMCP
 *
 * WHAT: GPU kernels for half-precision tensor operations with FP32 master weights
 * WHY:  2-3x speedup on modern GPUs with minimal accuracy loss
 * HOW:  Uses CUDA half-precision intrinsics and Tensor Cores
 *
 * ARCHITECTURE:
 * - FP16/BF16 for compute operations (memory bandwidth and compute efficiency)
 * - FP32 master weights for accurate gradient accumulation
 * - Dynamic loss scaling for gradient underflow prevention
 * - Automatic mixed precision (AMP) context for operation-specific precision
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: torch.cuda.amp.autocast(), GradScaler
 * - JAX: jax.numpy.float16 dtypes, manual scaling
 * - TensorFlow: tf.keras.mixed_precision.Policy
 *
 * BIOLOGICAL GROUNDING:
 * - Neural precision varies by brain region (low precision in motor, high in IT)
 * - Metabolic efficiency parallels computational efficiency
 * - Precision trade-offs mirror biological noise tolerance
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_TENSOR_FP16_H
#define NIMCP_TENSOR_FP16_H

// Include GPU context BEFORE extern "C" block
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
// Constants
//=============================================================================

#define MP_DEFAULT_INIT_SCALE        65536.0f   /**< Initial loss scale (2^16) */
#define MP_DEFAULT_GROWTH_FACTOR     2.0f       /**< Scale growth factor */
#define MP_DEFAULT_BACKOFF_FACTOR    0.5f       /**< Scale backoff on overflow */
#define MP_DEFAULT_GROWTH_INTERVAL   2000       /**< Steps between growth attempts */
#define MP_MIN_SCALE                 1.0f       /**< Minimum loss scale */
#define MP_MAX_SCALE                 (float)(1ULL << 24)  /**< Maximum loss scale (2^24) */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mixed precision data types
 */
typedef enum {
    MP_DTYPE_FP32 = 0,              /**< Full precision (baseline) */
    MP_DTYPE_FP16,                  /**< IEEE half precision (GPU optimized) */
    MP_DTYPE_BF16,                  /**< Brain float16 (better range, less precision) */
    MP_DTYPE_TF32,                  /**< TensorFloat-32 (Ampere+ GPUs) */
    MP_DTYPE_COUNT
} nimcp_mp_dtype_t;

/**
 * @brief Operator categories for autocasting
 */
typedef enum {
    MP_OP_COMPUTE = 0,              /**< Compute-intensive (FP16) - matmul, conv */
    MP_OP_REDUCE,                   /**< Reduction ops (FP32) - softmax, loss */
    MP_OP_NORMALIZE,                /**< Normalization (FP32) - layernorm, batchnorm */
    MP_OP_ELEMENTWISE,              /**< Element-wise ops (FP16) - activation, add */
    MP_OP_PRESERVE,                 /**< Preserve input dtype */
    MP_OP_CATEGORY_COUNT
} nimcp_mp_op_category_t;

/**
 * @brief Scaling mode for loss scaling
 */
typedef enum {
    MP_SCALING_NONE = 0,            /**< No scaling (FP32 only) */
    MP_SCALING_STATIC,              /**< Fixed scale factor */
    MP_SCALING_DYNAMIC,             /**< Dynamic scaling with backoff */
} nimcp_mp_scaling_mode_t;

//=============================================================================
// Mixed Precision Tensor
//=============================================================================

/**
 * @brief Mixed precision tensor wrapper
 *
 * WHAT: Wraps FP16/BF16 compute tensor with optional FP32 master copy
 * WHY:  Enables accurate gradient updates with efficient compute
 * HOW:  Forward pass uses fp16_data, optimizer updates fp32_master
 */
typedef struct nimcp_mp_tensor_s {
    nimcp_gpu_tensor_t* fp16_data;      /**< FP16/BF16 for compute */
    nimcp_gpu_tensor_t* fp32_master;    /**< FP32 master weights (optional) */
    nimcp_mp_dtype_t compute_dtype;     /**< FP16 or BF16 */
    bool has_master;                    /**< Whether master weights exist */
    bool owns_compute;                  /**< Whether we own fp16_data */
    bool owns_master;                   /**< Whether we own fp32_master */
} nimcp_mp_tensor_t;

/**
 * @brief Loss scaler for gradient scaling (dynamic loss scaling)
 *
 * WHAT: Manages scale factor for gradient scaling
 * WHY:  Prevents gradient underflow in FP16
 * HOW:  Grows scale when stable, backs off on overflow
 */
typedef struct nimcp_loss_scaler_s {
    float scale;                        /**< Current loss scale */
    float growth_factor;                /**< Scale growth (default 2.0) */
    float backoff_factor;               /**< Scale reduction (default 0.5) */
    float min_scale;                    /**< Minimum allowed scale */
    float max_scale;                    /**< Maximum allowed scale */
    int growth_interval;                /**< Steps between growth attempts */
    int consecutive_ok;                 /**< Steps without overflow */
    bool dynamic;                       /**< Enable dynamic scaling */

    // Statistics
    uint64_t total_steps;               /**< Total training steps */
    uint64_t overflow_count;            /**< Number of overflow events */
    uint64_t scale_increases;           /**< Scale increase events */
    uint64_t scale_decreases;           /**< Scale decrease events */
} nimcp_loss_scaler_t;

/**
 * @brief AMP (Automatic Mixed Precision) context
 *
 * WHAT: Context for automatic mixed precision training
 * WHY:  Manages precision settings and loss scaling
 * HOW:  Thread-local autocast state with op-specific overrides
 */
typedef struct nimcp_amp_context_s {
    nimcp_gpu_context_t* gpu_ctx;       /**< GPU context */
    nimcp_loss_scaler_t* scaler;        /**< Loss scaler */
    nimcp_mp_dtype_t default_dtype;     /**< Default compute dtype */
    bool enabled;                       /**< Enable AMP */
    bool autocasting;                   /**< Currently in autocast region */

    // Op-specific precision overrides
    nimcp_mp_dtype_t matmul_dtype;      /**< GEMM ops - typically FP16 */
    nimcp_mp_dtype_t conv_dtype;        /**< Convolution - typically FP16 */
    nimcp_mp_dtype_t norm_dtype;        /**< Normalization - always FP32 */
    nimcp_mp_dtype_t softmax_dtype;     /**< Softmax - always FP32 */
    nimcp_mp_dtype_t loss_dtype;        /**< Loss computation - always FP32 */

    // Hardware capabilities
    bool tensor_cores_available;        /**< Tensor Cores available */
    bool bf16_supported;                /**< BF16 hardware support */
    int compute_capability_major;       /**< GPU SM major version */
    int compute_capability_minor;       /**< GPU SM minor version */

    // Statistics
    uint64_t fp16_ops;                  /**< FP16 operations count */
    uint64_t fp32_ops;                  /**< FP32 operations count */
    uint64_t tensor_core_ops;           /**< Tensor Core operations */
} nimcp_amp_context_t;

//=============================================================================
// Mixed Precision Tensor Lifecycle
//=============================================================================

/**
 * @brief Create mixed precision tensor from FP32 tensor
 *
 * @param ctx GPU context
 * @param fp32_tensor Source FP32 tensor
 * @param compute_dtype Target compute dtype (FP16 or BF16)
 * @param keep_master Keep FP32 master copy
 * @return Mixed precision tensor or NULL on failure
 */
NIMCP_EXPORT nimcp_mp_tensor_t* nimcp_mp_tensor_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* fp32_tensor,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master
);

/**
 * @brief Create mixed precision tensor from dimensions
 *
 * @param ctx GPU context
 * @param dims Dimension sizes
 * @param ndim Number of dimensions
 * @param compute_dtype Compute dtype
 * @param keep_master Keep FP32 master
 * @return Mixed precision tensor or NULL
 */
NIMCP_EXPORT nimcp_mp_tensor_t* nimcp_mp_tensor_create_empty(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master
);

/**
 * @brief Destroy mixed precision tensor
 *
 * @param tensor Tensor to destroy (can be NULL)
 */
NIMCP_EXPORT void nimcp_mp_tensor_destroy(nimcp_mp_tensor_t* tensor);

/**
 * @brief Sync compute tensor from master (after optimizer update)
 *
 * @param ctx GPU context
 * @param tensor Mixed precision tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_mp_tensor_sync_compute(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* tensor
);

/**
 * @brief Sync master tensor from compute (for checkpointing)
 *
 * @param ctx GPU context
 * @param tensor Mixed precision tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_mp_tensor_sync_master(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* tensor
);

//=============================================================================
// Loss Scaler API
//=============================================================================

/**
 * @brief Create loss scaler with default settings
 *
 * @param dynamic Enable dynamic scaling
 * @return Loss scaler or NULL
 */
NIMCP_EXPORT nimcp_loss_scaler_t* nimcp_loss_scaler_create(bool dynamic);

/**
 * @brief Create loss scaler with custom settings
 *
 * @param init_scale Initial scale factor
 * @param growth_factor Scale growth factor
 * @param backoff_factor Scale backoff factor
 * @param growth_interval Steps between growth
 * @param dynamic Enable dynamic scaling
 * @return Loss scaler or NULL
 */
NIMCP_EXPORT nimcp_loss_scaler_t* nimcp_loss_scaler_create_custom(
    float init_scale,
    float growth_factor,
    float backoff_factor,
    int growth_interval,
    bool dynamic
);

/**
 * @brief Destroy loss scaler
 *
 * @param scaler Scaler to destroy
 */
NIMCP_EXPORT void nimcp_loss_scaler_destroy(nimcp_loss_scaler_t* scaler);

/**
 * @brief Scale loss for backward pass
 *
 * @param scaler Loss scaler
 * @param loss Original loss value
 * @return Scaled loss
 */
NIMCP_EXPORT float nimcp_loss_scaler_scale(nimcp_loss_scaler_t* scaler, float loss);

/**
 * @brief Unscale gradients before optimizer step
 *
 * @param ctx GPU context
 * @param scaler Loss scaler
 * @param gradients FP32 gradient tensor
 * @return true if gradients are valid (no overflow)
 */
NIMCP_EXPORT bool nimcp_loss_scaler_unscale(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients
);

/**
 * @brief Unscale FP16 gradients
 *
 * @param ctx GPU context
 * @param scaler Loss scaler
 * @param gradients FP16 gradient tensor
 * @return true if gradients are valid
 */
NIMCP_EXPORT bool nimcp_loss_scaler_unscale_fp16(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients
);

/**
 * @brief Update scale factor after step
 *
 * @param scaler Loss scaler
 * @param gradients_valid Whether gradients were valid (no overflow)
 */
NIMCP_EXPORT void nimcp_loss_scaler_update(
    nimcp_loss_scaler_t* scaler,
    bool gradients_valid
);

/**
 * @brief Get current scale
 *
 * @param scaler Loss scaler
 * @return Current scale factor
 */
NIMCP_EXPORT float nimcp_loss_scaler_get_scale(const nimcp_loss_scaler_t* scaler);

/**
 * @brief Check if step should be skipped due to overflow
 *
 * @param scaler Loss scaler
 * @param gradients_valid Gradient validity flag
 * @return true if optimizer step should be skipped
 */
NIMCP_EXPORT bool nimcp_loss_scaler_should_skip(
    const nimcp_loss_scaler_t* scaler,
    bool gradients_valid
);

//=============================================================================
// AMP Context API
//=============================================================================

/**
 * @brief Create AMP context
 *
 * @param gpu_ctx GPU context
 * @param compute_dtype Default compute dtype
 * @param enable_scaler Enable dynamic loss scaling
 * @return AMP context or NULL
 */
NIMCP_EXPORT nimcp_amp_context_t* nimcp_amp_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_mp_dtype_t compute_dtype,
    bool enable_scaler
);

/**
 * @brief Destroy AMP context
 *
 * @param ctx AMP context
 */
NIMCP_EXPORT void nimcp_amp_destroy(nimcp_amp_context_t* ctx);

/**
 * @brief Enter autocast region
 *
 * @param ctx AMP context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_amp_autocast_enter(nimcp_amp_context_t* ctx);

/**
 * @brief Exit autocast region
 *
 * @param ctx AMP context
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_amp_autocast_exit(nimcp_amp_context_t* ctx);

/**
 * @brief Check if in autocast region
 *
 * @param ctx AMP context
 * @return true if autocasting
 */
NIMCP_EXPORT bool nimcp_amp_is_autocasting(const nimcp_amp_context_t* ctx);

/**
 * @brief Get dtype for operation category
 *
 * @param ctx AMP context
 * @param category Operation category
 * @return Recommended dtype
 */
NIMCP_EXPORT nimcp_mp_dtype_t nimcp_amp_get_dtype(
    const nimcp_amp_context_t* ctx,
    nimcp_mp_op_category_t category
);

/**
 * @brief Cast tensor to autocast dtype
 *
 * @param ctx AMP context
 * @param tensor Input tensor
 * @param category Operation category
 * @return Casted tensor (may be same if no cast needed)
 */
NIMCP_EXPORT nimcp_gpu_tensor_t* nimcp_amp_cast_tensor(
    nimcp_amp_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    nimcp_mp_op_category_t category
);

//=============================================================================
// FP16 Conversion Kernels
//=============================================================================

/**
 * @brief Convert FP32 tensor to FP16
 *
 * @param ctx GPU context
 * @param src FP32 source tensor
 * @param dst FP16 destination tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_fp32_to_fp16(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst
);

/**
 * @brief Convert FP16 tensor to FP32
 *
 * @param ctx GPU context
 * @param src FP16 source tensor
 * @param dst FP32 destination tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_fp16_to_fp32(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst
);

/**
 * @brief Convert FP32 tensor to BF16
 *
 * @param ctx GPU context
 * @param src FP32 source tensor
 * @param dst BF16 destination tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_fp32_to_bf16(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst
);

/**
 * @brief Convert BF16 tensor to FP32
 *
 * @param ctx GPU context
 * @param src BF16 source tensor
 * @param dst FP32 destination tensor
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_bf16_to_fp32(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* src,
    nimcp_gpu_tensor_t* dst
);

//=============================================================================
// FP16 Element-wise Operations
//=============================================================================

/**
 * @brief FP16 element-wise addition
 */
NIMCP_EXPORT bool nimcp_fp16_add(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 element-wise multiplication
 */
NIMCP_EXPORT bool nimcp_fp16_mul(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 scalar multiplication
 */
NIMCP_EXPORT bool nimcp_fp16_scale(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float scale,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 fused multiply-add: out = a * b + c
 */
NIMCP_EXPORT bool nimcp_fp16_fma(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    const nimcp_gpu_tensor_t* c,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// FP16 GEMM Operations
//=============================================================================

/**
 * @brief FP16 GEMM using Tensor Cores if available
 *
 * @param ctx GPU context
 * @param A First matrix (M x K)
 * @param B Second matrix (K x N)
 * @param C Output matrix (M x N)
 * @param alpha Scale for A @ B
 * @param beta Scale for C
 * @param trans_a Transpose A
 * @param trans_b Transpose B
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_fp16_gemm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

/**
 * @brief FP16 batched GEMM
 */
NIMCP_EXPORT bool nimcp_fp16_gemm_batched(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* A,
    const nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha,
    float beta,
    bool trans_a,
    bool trans_b
);

//=============================================================================
// FP16 Activation Functions
//=============================================================================

/**
 * @brief FP16 ReLU activation
 */
NIMCP_EXPORT bool nimcp_fp16_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 GELU activation
 */
NIMCP_EXPORT bool nimcp_fp16_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 Sigmoid activation
 */
NIMCP_EXPORT bool nimcp_fp16_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 Tanh activation
 */
NIMCP_EXPORT bool nimcp_fp16_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 SiLU/Swish activation
 */
NIMCP_EXPORT bool nimcp_fp16_silu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

//=============================================================================
// Numerically Stable Operations (Compute in FP32, Store FP16)
//=============================================================================

/**
 * @brief Numerically stable softmax in FP16
 *
 * Computes in FP32 internally for stability, outputs FP16
 */
NIMCP_EXPORT bool nimcp_fp16_softmax_stable(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out
);

/**
 * @brief FP16 LayerNorm with stable accumulation
 */
NIMCP_EXPORT bool nimcp_fp16_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps
);

/**
 * @brief FP16 BatchNorm with stable accumulation
 */
NIMCP_EXPORT bool nimcp_fp16_batchnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* running_mean,
    const nimcp_gpu_tensor_t* running_var,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps
);

//=============================================================================
// Loss Scaling Kernels
//=============================================================================

/**
 * @brief Scale FP16 gradients
 */
NIMCP_EXPORT bool nimcp_fp16_scale_gradients(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grads,
    float scale
);

/**
 * @brief Check for inf/nan in FP16 tensor
 *
 * @param ctx GPU context
 * @param data FP16 tensor
 * @param found_inf Output: non-zero if inf/nan found
 * @return true on success (check found_inf for result)
 */
NIMCP_EXPORT bool nimcp_fp16_check_inf_nan(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* data,
    int* found_inf
);

//=============================================================================
// Mixed Precision Optimizer Support
//=============================================================================

/**
 * @brief Adam update with mixed precision
 *
 * Updates FP32 master weights, syncs to FP16 compute weights
 *
 * @param ctx GPU context
 * @param mp_tensor Mixed precision tensor (contains master and compute)
 * @param gradients FP16 gradients
 * @param m First moment (FP32)
 * @param v Second moment (FP32)
 * @param lr Learning rate
 * @param beta1 First moment decay
 * @param beta2 Second moment decay
 * @param eps Epsilon for numerical stability
 * @param weight_decay Weight decay coefficient
 * @param step Current step number (1-indexed)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_mp_adam_update(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* mp_tensor,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* m,
    nimcp_gpu_tensor_t* v,
    float lr,
    float beta1,
    float beta2,
    float eps,
    float weight_decay,
    int step
);

/**
 * @brief SGD with momentum, mixed precision
 */
NIMCP_EXPORT bool nimcp_mp_sgd_update(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* mp_tensor,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* momentum_buffer,
    float lr,
    float momentum,
    float weight_decay,
    bool nesterov
);

//=============================================================================
// Hardware Capability Detection
//=============================================================================

/**
 * @brief Check if Tensor Cores are available
 *
 * @param ctx GPU context
 * @return true if Tensor Cores available (SM 7.0+)
 */
NIMCP_EXPORT bool nimcp_tensor_cores_available(nimcp_gpu_context_t* ctx);

/**
 * @brief Check if BF16 is supported
 *
 * @param ctx GPU context
 * @return true if BF16 supported (SM 8.0+)
 */
NIMCP_EXPORT bool nimcp_bf16_supported(nimcp_gpu_context_t* ctx);

/**
 * @brief Get recommended compute dtype for hardware
 *
 * @param ctx GPU context
 * @return FP16 or BF16 based on hardware
 */
NIMCP_EXPORT nimcp_mp_dtype_t nimcp_get_recommended_dtype(nimcp_gpu_context_t* ctx);

/**
 * @brief Get mixed precision statistics
 *
 * @param ctx AMP context
 * @param fp16_ops Output: FP16 operation count
 * @param fp32_ops Output: FP32 operation count
 * @param tc_ops Output: Tensor Core operation count
 */
NIMCP_EXPORT void nimcp_amp_get_stats(
    const nimcp_amp_context_t* ctx,
    uint64_t* fp16_ops,
    uint64_t* fp32_ops,
    uint64_t* tc_ops
);

/**
 * @brief Get dtype name string
 *
 * @param dtype Data type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* nimcp_mp_dtype_name(nimcp_mp_dtype_t dtype);

/**
 * @brief Get dtype size in bytes
 *
 * @param dtype Data type
 * @return Size in bytes
 */
NIMCP_EXPORT size_t nimcp_mp_dtype_size(nimcp_mp_dtype_t dtype);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TENSOR_FP16_H */
