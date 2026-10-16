/**
 * @file nimcp_mixed_precision.h
 * @brief Mixed Precision Training (AMP) for NIMCP
 *
 * WHAT: Automatic Mixed Precision training with FP16/BF16 compute and FP32 storage
 * WHY:  2-3x speedup on modern GPUs with minimal accuracy loss
 * HOW:  Dynamic loss scaling, operator autocasting, and master weight maintenance
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch AMP: torch.cuda.amp.autocast(), GradScaler
 * - JAX AMP: jax.numpy.float16 dtypes, manual scaling
 * - TensorFlow: tf.keras.mixed_precision.Policy
 *
 * NIMCP APPROACH:
 * - Builds on existing gradient_manager scaling infrastructure
 * - Integrates with tensor layer for dtype management
 * - Bio-async enabled for distributed mixed precision
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

#ifndef NIMCP_MIXED_PRECISION_H
#define NIMCP_MIXED_PRECISION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"
#include "middleware/training/nimcp_gradient_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define AMP_DEFAULT_INIT_SCALE        65536.0f   /**< Initial loss scale (2^16) */
#define AMP_DEFAULT_GROWTH_FACTOR     2.0f       /**< Scale growth factor */
#define AMP_DEFAULT_BACKOFF_FACTOR    0.5f       /**< Scale backoff on overflow */
#define AMP_DEFAULT_GROWTH_INTERVAL   2000       /**< Steps between growth attempts */
#define AMP_MIN_SCALE                 1.0f       /**< Minimum loss scale */
#define AMP_MAX_SCALE                 (float)(1ULL << 24)  /**< Maximum loss scale (2^24) */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Mixed precision data types
 *
 * COMPARISON:
 * - PyTorch: torch.float16, torch.bfloat16
 * - JAX: jnp.float16, jnp.bfloat16
 * - TensorFlow: tf.float16, tf.bfloat16
 */
typedef enum {
    AMP_DTYPE_FP32 = 0,             /**< Full precision (baseline) */
    AMP_DTYPE_FP16,                 /**< IEEE half precision (GPU optimized) */
    AMP_DTYPE_BF16,                 /**< Brain float16 (better range, less precision) */
    AMP_DTYPE_TF32,                 /**< TensorFloat-32 (Ampere+ GPUs) */
    AMP_DTYPE_FP8_E4M3,             /**< FP8 e4m3 (Hopper+ forward pass) */
    AMP_DTYPE_FP8_E5M2,             /**< FP8 e5m2 (Hopper+ backward pass) */
    AMP_DTYPE_TERNARY,              /**< Ternary {-1, 0, +1} extreme quantization */
    AMP_DTYPE_COUNT
} amp_dtype_t;

/**
 * @brief Operator categories for autocasting
 *
 * COMPARISON (PyTorch autocast categories):
 * - FP16: matmul, conv, linear, attention
 * - FP32: softmax, layer_norm, batch_norm, loss
 * - Promote: operations that need highest precision of inputs
 */
typedef enum {
    AMP_OP_CATEGORY_COMPUTE = 0,    /**< Compute-intensive (FP16) */
    AMP_OP_CATEGORY_REDUCE,         /**< Reduction ops (FP32) */
    AMP_OP_CATEGORY_NORMALIZE,      /**< Normalization (FP32) */
    AMP_OP_CATEGORY_LOSS,           /**< Loss computation (FP32) */
    AMP_OP_CATEGORY_PROMOTE,        /**< Promote to highest input */
    AMP_OP_CATEGORY_PRESERVE,       /**< Preserve input dtype */
    AMP_OP_CATEGORY_COUNT
} amp_op_category_t;

/**
 * @brief Scaling mode for loss scaling
 */
typedef enum {
    AMP_SCALING_NONE = 0,           /**< No scaling (FP32 only) */
    AMP_SCALING_STATIC,             /**< Fixed scale factor */
    AMP_SCALING_DYNAMIC,            /**< Dynamic scaling with backoff */
    AMP_SCALING_MODE_COUNT
} amp_scaling_mode_t;

/**
 * @brief Overflow handling strategy
 */
typedef enum {
    AMP_OVERFLOW_SKIP = 0,          /**< Skip update on overflow */
    AMP_OVERFLOW_RETRY,             /**< Reduce scale and retry */
    AMP_OVERFLOW_CLAMP,             /**< Clamp gradients and continue */
    AMP_OVERFLOW_STRATEGY_COUNT
} amp_overflow_strategy_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Loss scaling configuration
 *
 * COMPARISON:
 * - PyTorch GradScaler: init_scale, growth_factor, backoff_factor, growth_interval
 * - JAX: Manual implementation required
 * - TensorFlow: LossScaleOptimizer with DynamicLossScale
 */
typedef struct {
    amp_scaling_mode_t mode;         /**< Scaling mode */
    float init_scale;                /**< Initial scale factor */
    float growth_factor;             /**< Scale growth multiplier */
    float backoff_factor;            /**< Scale reduction on overflow */
    uint32_t growth_interval;        /**< Steps between growth attempts */
    float min_scale;                 /**< Minimum allowed scale */
    float max_scale;                 /**< Maximum allowed scale */
    amp_overflow_strategy_t overflow_strategy; /**< Overflow handling */
} amp_scaling_config_t;

/**
 * @brief Autocasting configuration
 *
 * COMPARISON:
 * - PyTorch: torch.autocast(device_type, dtype, enabled, cache_enabled)
 * - JAX: Manual dtype management
 * - TensorFlow: mixed_precision.set_global_policy()
 */
typedef struct {
    amp_dtype_t compute_dtype;       /**< Dtype for compute ops */
    amp_dtype_t storage_dtype;       /**< Dtype for parameters (usually FP32) */
    bool enabled;                    /**< Enable autocasting */
    bool cache_enabled;              /**< Cache autocast decisions */

    /* Per-layer overrides */
    amp_dtype_t* layer_overrides;    /**< Per-layer dtype overrides */
    uint32_t num_layers;             /**< Number of layers */

    /* Operation category settings */
    amp_dtype_t category_dtypes[AMP_OP_CATEGORY_COUNT]; /**< Dtype per category */
} amp_autocast_config_t;

/**
 * @brief Master weights configuration
 *
 * WHAT: Maintain FP32 copies of weights for accurate updates
 * WHY:  FP16 weight updates can lose precision for small gradients
 * HOW:  Store FP32 master, cast to FP16 for compute, update FP32 master
 */
typedef struct {
    bool use_master_weights;         /**< Maintain FP32 master weights */
    bool lazy_cast;                  /**< Lazy casting (on-demand) */
    bool fuse_update_cast;           /**< Fuse weight update and cast */
} amp_master_weights_config_t;

/**
 * @brief Complete AMP configuration
 */
typedef struct {
    amp_scaling_config_t scaling;    /**< Loss scaling config */
    amp_autocast_config_t autocast;  /**< Autocasting config */
    amp_master_weights_config_t master_weights; /**< Master weights config */

    /* Integration */
    bool integrate_gradient_manager; /**< Use existing gradient_manager */
    bool verbose;                    /**< Print AMP status */
    bool track_statistics;           /**< Track dtype statistics */
} amp_config_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief AMP training statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t overflow_count;         /**< Number of overflow events */
    uint64_t underflow_count;        /**< Number of underflow events */
    uint64_t skipped_steps;          /**< Steps skipped due to overflow */

    /* Scaling statistics */
    float current_scale;             /**< Current loss scale */
    float min_scale_reached;         /**< Minimum scale reached */
    float max_scale_reached;         /**< Maximum scale reached */
    uint64_t scale_increases;        /**< Scale increase events */
    uint64_t scale_decreases;        /**< Scale decrease events */

    /* Performance statistics */
    uint64_t fp16_ops;               /**< FP16 operations performed */
    uint64_t fp32_ops;               /**< FP32 operations performed */
    float fp16_ratio;                /**< Ratio of FP16 ops */
    double memory_saved_bytes;       /**< Estimated memory savings */
} amp_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief AMP context (opaque)
 */
typedef struct amp_ctx_s amp_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default AMP configuration
 *
 * WHAT: Initialize config with sensible defaults
 * WHY:  Easy setup for common use case
 * HOW:  FP16 compute, FP32 storage, dynamic scaling
 *
 * DEFAULTS:
 * - compute_dtype: FP16
 * - scaling_mode: DYNAMIC
 * - init_scale: 65536 (2^16)
 * - growth_factor: 2.0
 * - growth_interval: 2000 steps
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int amp_default_config(amp_config_t* config);

/**
 * @brief Get BF16-optimized configuration
 *
 * WHAT: Configuration optimized for BF16 training
 * WHY:  BF16 has better range, may not need scaling
 * HOW:  BF16 compute, no loss scaling (usually stable)
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int amp_bf16_config(amp_config_t* config);

/**
 * @brief Create AMP context
 *
 * WHAT: Allocate and initialize AMP training context
 * WHY:  Set up mixed precision infrastructure
 * HOW:  Create scaling state, autocast tables, master weight buffers
 *
 * @param config AMP configuration
 * @return AMP context or NULL on failure
 */
amp_ctx_t* amp_create(const amp_config_t* config);

/**
 * @brief Destroy AMP context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void amp_destroy(amp_ctx_t* ctx);

/**
 * @brief Connect AMP to gradient manager
 *
 * WHAT: Integrate with existing gradient manager
 * WHY:  Leverage existing scaling infrastructure
 * HOW:  Share scaling state with gradient manager
 *
 * @param ctx AMP context
 * @param grad_manager Gradient manager to integrate
 * @return 0 on success, negative on error
 */
int amp_connect_gradient_manager(
    amp_ctx_t* ctx,
    nimcp_gradient_manager_ctx_t* grad_manager
);

//=============================================================================
// Autocast API
//=============================================================================

/**
 * @brief Enter autocast region
 *
 * WHAT: Enable autocasting for subsequent operations
 * WHY:  Automatically select optimal dtypes per operation
 * HOW:  Set thread-local autocast state
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * with torch.autocast(device_type='cuda', dtype=torch.float16):
 *     output = model(input)
 * ```
 *
 * NIMCP usage:
 * ```c
 * amp_autocast_enter(ctx);
 * output = model_forward(model, input);
 * amp_autocast_exit(ctx);
 * ```
 *
 * @param ctx AMP context
 * @return 0 on success, negative on error
 */
int amp_autocast_enter(amp_ctx_t* ctx);

/**
 * @brief Exit autocast region
 *
 * @param ctx AMP context
 * @return 0 on success, negative on error
 */
int amp_autocast_exit(amp_ctx_t* ctx);

/**
 * @brief Check if autocasting is active
 *
 * @param ctx AMP context
 * @return true if in autocast region
 */
bool amp_is_autocasting(const amp_ctx_t* ctx);

/**
 * @brief Get dtype for operation category
 *
 * WHAT: Query dtype to use for operation type
 * WHY:  Allow operations to self-configure
 * HOW:  Look up category in autocast config
 *
 * @param ctx AMP context
 * @param category Operation category
 * @return Recommended dtype
 */
amp_dtype_t amp_get_op_dtype(const amp_ctx_t* ctx, amp_op_category_t category);

/**
 * @brief Cast tensor to autocast dtype
 *
 * WHAT: Convert tensor to appropriate mixed precision dtype
 * WHY:  Automatic dtype management
 * HOW:  Check autocast state, apply conversion
 *
 * @param ctx AMP context
 * @param tensor Tensor to cast
 * @param category Operation category
 * @return Casted tensor (may be same if no cast needed)
 */
nimcp_tensor_t* amp_autocast_tensor(
    amp_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    amp_op_category_t category
);

//=============================================================================
// Loss Scaling API
//=============================================================================

/**
 * @brief Scale loss for backward pass
 *
 * WHAT: Multiply loss by scale factor before backward
 * WHY:  Prevent gradient underflow in FP16
 * HOW:  loss_scaled = loss * scale
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * scaler.scale(loss).backward()
 * ```
 *
 * @param ctx AMP context
 * @param loss Loss value
 * @return Scaled loss
 */
float amp_scale_loss(amp_ctx_t* ctx, float loss);

/**
 * @brief Unscale gradients before optimizer step
 *
 * WHAT: Divide gradients by scale factor
 * WHY:  Restore true gradient magnitudes
 * HOW:  grad = grad / scale, check for overflow
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * scaler.unscale_(optimizer)
 * ```
 *
 * @param ctx AMP context
 * @param gradients Gradient array (modified in place)
 * @param count Number of gradients
 * @return true if gradients are valid (no overflow)
 */
bool amp_unscale_gradients(
    amp_ctx_t* ctx,
    float* gradients,
    size_t count
);

/**
 * @brief Unscale gradient tensor
 *
 * @param ctx AMP context
 * @param grad_tensor Gradient tensor (modified in place)
 * @return true if gradients are valid
 */
bool amp_unscale_tensor(amp_ctx_t* ctx, nimcp_tensor_t* grad_tensor);

/**
 * @brief Update scale factor after step
 *
 * WHAT: Adjust loss scale based on gradient health
 * WHY:  Dynamic scaling adapts to training dynamics
 * HOW:  Increase if stable, decrease on overflow
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * scaler.update()
 * ```
 *
 * @param ctx AMP context
 * @param gradients_valid Whether gradients were valid
 */
void amp_update_scale(amp_ctx_t* ctx, bool gradients_valid);

/**
 * @brief Get current loss scale
 *
 * @param ctx AMP context
 * @return Current scale factor
 */
float amp_get_scale(const amp_ctx_t* ctx);

/**
 * @brief Set loss scale manually
 *
 * @param ctx AMP context
 * @param scale New scale factor
 * @return 0 on success, negative on error
 */
int amp_set_scale(amp_ctx_t* ctx, float scale);

//=============================================================================
// Master Weights API
//=============================================================================

/**
 * @brief Create master weight copy
 *
 * WHAT: Create FP32 copy of weights for accurate updates
 * WHY:  Accumulate small updates in FP32
 * HOW:  Allocate FP32 buffer, copy weights
 *
 * @param ctx AMP context
 * @param weights FP16/BF16 weights
 * @param count Number of weights
 * @return Master weight buffer or NULL on failure
 */
float* amp_create_master_weights(
    amp_ctx_t* ctx,
    const void* weights,
    size_t count,
    amp_dtype_t weight_dtype
);

/**
 * @brief Update master weights and cast back
 *
 * WHAT: Apply gradient update to FP32 master, cast to FP16
 * WHY:  Accurate updates with efficient compute weights
 * HOW:  master -= lr * grad; weights = cast(master)
 *
 * @param ctx AMP context
 * @param master_weights FP32 master weights
 * @param compute_weights FP16/BF16 compute weights
 * @param gradients FP32 gradients
 * @param count Number of weights
 * @param learning_rate Learning rate
 * @return 0 on success, negative on error
 */
int amp_update_master_weights(
    amp_ctx_t* ctx,
    float* master_weights,
    void* compute_weights,
    const float* gradients,
    size_t count,
    float learning_rate
);

/**
 * @brief Sync compute weights from master
 *
 * WHAT: Cast master weights to compute dtype
 * WHY:  Keep compute weights in sync after update
 * HOW:  compute = cast(master, compute_dtype)
 *
 * @param ctx AMP context
 * @param master_weights FP32 master weights
 * @param compute_weights Output compute weights
 * @param count Number of weights
 * @param compute_dtype Target dtype
 * @return 0 on success, negative on error
 */
int amp_sync_compute_weights(
    amp_ctx_t* ctx,
    const float* master_weights,
    void* compute_weights,
    size_t count,
    amp_dtype_t compute_dtype
);

//=============================================================================
// Training Step API
//=============================================================================

/**
 * @brief Complete AMP training step
 *
 * WHAT: Orchestrate scaled backward and optimizer step
 * WHY:  Convenience API for complete AMP workflow
 * HOW:  Scale loss, backward, unscale, step if valid, update scale
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * scaler.scale(loss).backward()
 * scaler.unscale_(optimizer)
 * # Optionally clip gradients
 * scaler.step(optimizer)
 * scaler.update()
 * ```
 *
 * @param ctx AMP context
 * @param loss Training loss value
 * @param gradients Gradient array
 * @param params Parameter array
 * @param count Number of parameters
 * @param learning_rate Learning rate
 * @param step_performed Output: whether step was performed
 * @return 0 on success, negative on error
 */
int amp_step(
    amp_ctx_t* ctx,
    float loss,
    float* gradients,
    float* params,
    size_t count,
    float learning_rate,
    bool* step_performed
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get AMP statistics
 *
 * @param ctx AMP context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int amp_get_stats(const amp_ctx_t* ctx, amp_stats_t* stats);

/**
 * @brief Reset AMP statistics
 *
 * @param ctx AMP context
 */
void amp_reset_stats(amp_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get dtype name
 *
 * @param dtype Dtype enum
 * @return String name
 */
const char* amp_dtype_name(amp_dtype_t dtype);

/**
 * @brief Get dtype size in bytes
 *
 * @param dtype Dtype enum
 * @return Size in bytes
 */
size_t amp_dtype_size(amp_dtype_t dtype);

/**
 * @brief Check if dtype is supported by hardware
 *
 * @param dtype Dtype to check
 * @return true if supported
 */
bool amp_dtype_supported(amp_dtype_t dtype);

/**
 * @brief Check if BF16 is available
 *
 * @return true if BF16 supported (Ampere+ GPU or AVX-512 CPU)
 */
bool amp_bf16_available(void);

/**
 * @brief Cast between dtypes
 *
 * @param src Source buffer
 * @param src_dtype Source dtype
 * @param dst Destination buffer
 * @param dst_dtype Destination dtype
 * @param count Number of elements
 * @return 0 on success, negative on error
 */
int amp_cast(
    const void* src,
    amp_dtype_t src_dtype,
    void* dst,
    amp_dtype_t dst_dtype,
    size_t count
);

/**
 * @brief Validate AMP configuration
 *
 * @param config Configuration to validate
 * @return 0 if valid, negative error code
 */
int amp_validate_config(const amp_config_t* config);

//=============================================================================
// Ternary Precision API
//=============================================================================

/**
 * @brief Ternary precision configuration
 *
 * WHAT: Configuration for ternary {-1, 0, +1} extreme quantization
 * WHY:  Maximum memory efficiency with ~32x compression vs FP32
 * HOW:  Threshold-based ternarization with learned scaling
 *
 * BIOLOGICAL GROUNDING:
 * - Synaptic weights discretize to LTP/baseline/LTD states
 * - Action potentials are all-or-nothing with refractory period
 * - Neural coding efficiency maximized with sparse ternary representations
 *
 * COMPARISON:
 * - BinaryConnect (Courbariaux 2015): Binary {-1, +1} weights
 * - Ternary Weight Networks (Li 2016): Adds zero state
 * - Trained Ternary Quantization (Zhu 2016): Learned thresholds
 */
typedef struct {
    float threshold_ratio;           /**< Threshold as ratio of max value (default: 0.5) */
    bool use_learned_threshold;      /**< Learn threshold during training */
    float positive_scale;            /**< Scale for +1 values */
    float negative_scale;            /**< Scale for -1 values */
    bool symmetric_thresholds;       /**< Use same threshold for +/- */
    bool use_ste;                    /**< Straight-through estimator for backprop */
    float ste_clip_value;            /**< Gradient clipping value for STE */
    bool layer_wise_thresholds;      /**< Different thresholds per layer */
    bool channel_wise_thresholds;    /**< Different thresholds per channel */
} amp_ternary_config_t;

/**
 * @brief Ternary precision statistics
 *
 * WHAT: Statistics for ternary precision training
 * WHY:  Monitor sparsity, threshold adaptation, and gradient health
 */
typedef struct {
    uint64_t total_elements;         /**< Total elements processed */
    uint64_t positive_count;         /**< Elements quantized to +1 */
    uint64_t zero_count;             /**< Elements quantized to 0 */
    uint64_t negative_count;         /**< Elements quantized to -1 */
    float sparsity_ratio;            /**< Fraction of zero elements */
    float current_threshold;         /**< Current threshold value */
    float avg_positive_scale;        /**< Average positive scale */
    float avg_negative_scale;        /**< Average negative scale */
    float compression_ratio;         /**< Effective compression ratio */
} amp_ternary_stats_t;

/**
 * @brief Get default ternary precision configuration
 *
 * WHAT: Initialize ternary config with sensible defaults
 * WHY:  Easy setup for ternary training
 * HOW:  Based on Trained Ternary Quantization (Zhu et al.)
 *
 * DEFAULTS:
 * - threshold_ratio: 0.5 (ternarize at half of max value)
 * - use_ste: true (straight-through estimator)
 * - symmetric_thresholds: true
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
static inline int amp_ternary_default_config(amp_ternary_config_t* config) {
    if (!config) return -1;

    config->threshold_ratio = 0.5f;
    config->use_learned_threshold = true;
    config->positive_scale = 1.0f;
    config->negative_scale = 1.0f;
    config->symmetric_thresholds = true;
    config->use_ste = true;
    config->ste_clip_value = 1.0f;
    config->layer_wise_thresholds = true;
    config->channel_wise_thresholds = false;

    return 0;
}

/**
 * @brief Get ternary-optimized AMP configuration
 *
 * WHAT: Configuration optimized for ternary training
 * WHY:  Convenience for setting up ternary-based mixed precision
 * HOW:  Sets compute_dtype to TERNARY, configures scaling for stability
 *
 * @param config AMP configuration to initialize
 * @return 0 on success, negative on error
 */
int amp_ternary_config(amp_config_t* config);

/**
 * @brief Create ternary precision context
 *
 * WHAT: Allocate context for ternary precision training
 * WHY:  Manage ternary quantization state
 * HOW:  Create scaling parameters, threshold state
 *
 * @param amp_ctx Parent AMP context
 * @param ternary_config Ternary configuration
 * @return 0 on success, negative on error
 */
int amp_enable_ternary(
    amp_ctx_t* amp_ctx,
    const amp_ternary_config_t* ternary_config
);

/**
 * @brief Disable ternary precision mode
 *
 * WHAT: Return to standard mixed precision
 * WHY:  Allow switching between ternary and standard modes
 *
 * @param amp_ctx AMP context
 * @return 0 on success
 */
int amp_disable_ternary(amp_ctx_t* amp_ctx);

/**
 * @brief Check if ternary mode is enabled
 *
 * @param amp_ctx AMP context
 * @return true if ternary mode active
 */
bool amp_is_ternary(const amp_ctx_t* amp_ctx);

/**
 * @brief Ternarize tensor in-place
 *
 * WHAT: Convert FP32 tensor to ternary {-1, 0, +1} representation
 * WHY:  Apply extreme quantization for memory efficiency
 * HOW:  Apply threshold, quantize to ternary, store scales
 *
 * Algorithm (Trained Ternary Quantization):
 * 1. Compute threshold: t = threshold_ratio * max(|W|)
 * 2. Ternarize: w_t = +1 if w > t, -1 if w < -t, 0 otherwise
 * 3. Compute scales: s+ = mean(w where w > t), s- = mean(|w| where w < -t)
 * 4. Full-precision equivalent: W_approx = s+ * (w_t == +1) + s- * (w_t == -1)
 *
 * @param amp_ctx AMP context
 * @param tensor Tensor to ternarize (modified in place)
 * @param threshold_out Output: computed threshold
 * @param scale_pos_out Output: positive scale factor
 * @param scale_neg_out Output: negative scale factor
 * @return 0 on success, negative on error
 */
int amp_ternarize_tensor(
    amp_ctx_t* amp_ctx,
    nimcp_tensor_t* tensor,
    float* threshold_out,
    float* scale_pos_out,
    float* scale_neg_out
);

/**
 * @brief Ternarize weights array
 *
 * WHAT: Convert FP32 weights to ternary representation
 * WHY:  Memory-efficient weight storage
 * HOW:  Threshold-based ternarization with scaling
 *
 * @param amp_ctx AMP context
 * @param weights FP32 weights (modified in place to ternary values)
 * @param count Number of weights
 * @param threshold_out Output: computed threshold
 * @param scale_pos_out Output: positive scale
 * @param scale_neg_out Output: negative scale
 * @return 0 on success
 */
int amp_ternarize_weights(
    amp_ctx_t* amp_ctx,
    float* weights,
    size_t count,
    float* threshold_out,
    float* scale_pos_out,
    float* scale_neg_out
);

/**
 * @brief Dequantize ternary to full precision
 *
 * WHAT: Convert ternary {-1, 0, +1} back to scaled FP32
 * WHY:  Reconstruct approximate full-precision for computation
 * HOW:  Apply learned scales to ternary values
 *
 * @param amp_ctx AMP context
 * @param ternary_weights Ternary weights (as int8 or float {-1,0,+1})
 * @param output FP32 output buffer
 * @param count Number of weights
 * @param scale_pos Positive scale
 * @param scale_neg Negative scale
 * @return 0 on success
 */
int amp_dequantize_ternary(
    amp_ctx_t* amp_ctx,
    const float* ternary_weights,
    float* output,
    size_t count,
    float scale_pos,
    float scale_neg
);

/**
 * @brief Compute ternary gradients with STE
 *
 * WHAT: Backpropagate through ternary quantization
 * WHY:  Enable training of ternary networks
 * HOW:  Straight-Through Estimator passes gradients through
 *
 * STE Algorithm:
 * - Forward: apply ternarization
 * - Backward: gradient passes through unchanged (clipped if configured)
 *
 * @param amp_ctx AMP context
 * @param grad_output Gradient from upstream
 * @param original_weights Original FP32 weights before ternarization
 * @param grad_input Output gradient (same shape as grad_output)
 * @param count Number of elements
 * @param threshold Ternarization threshold
 * @return 0 on success
 */
int amp_ternary_backward(
    amp_ctx_t* amp_ctx,
    const float* grad_output,
    const float* original_weights,
    float* grad_input,
    size_t count,
    float threshold
);

/**
 * @brief Update ternary threshold
 *
 * WHAT: Adjust ternarization threshold based on training
 * WHY:  Learned thresholds improve accuracy
 * HOW:  Gradient-based update of threshold parameter
 *
 * @param amp_ctx AMP context
 * @param current_threshold Current threshold
 * @param gradient Gradient w.r.t. threshold
 * @param learning_rate Learning rate
 * @return Updated threshold value
 */
float amp_update_ternary_threshold(
    amp_ctx_t* amp_ctx,
    float current_threshold,
    float gradient,
    float learning_rate
);

/**
 * @brief Get optimal ternary threshold
 *
 * WHAT: Compute optimal threshold for given weight distribution
 * WHY:  Minimize quantization error
 * HOW:  Analytical solution based on weight statistics
 *
 * @param weights Weight array
 * @param count Number of weights
 * @param ratio Threshold ratio parameter
 * @return Optimal threshold value
 */
float amp_compute_optimal_ternary_threshold(
    const float* weights,
    size_t count,
    float ratio
);

/**
 * @brief Get ternary precision statistics
 *
 * WHAT: Retrieve ternary-specific training statistics
 * WHY:  Monitor sparsity and quantization quality
 *
 * @param amp_ctx AMP context
 * @param stats Output statistics
 * @return 0 on success
 */
int amp_get_ternary_stats(
    const amp_ctx_t* amp_ctx,
    amp_ternary_stats_t* stats
);

/**
 * @brief Reset ternary statistics
 *
 * @param amp_ctx AMP context
 */
void amp_reset_ternary_stats(amp_ctx_t* amp_ctx);

/**
 * @brief Pack ternary weights for storage
 *
 * WHAT: Compress ternary weights to 2 bits per weight
 * WHY:  Achieve 16x compression vs FP32
 * HOW:  Pack 16 ternary values per uint32
 *
 * Encoding: 00 = -1, 01 = 0, 10 = +1, 11 = reserved
 *
 * @param ternary_weights Ternary weights (as float {-1,0,+1})
 * @param count Number of weights
 * @param packed Output: packed representation
 * @param packed_size Size of packed buffer (must be >= (count + 15) / 16 * 4)
 * @return Number of bytes written
 */
size_t amp_pack_ternary(
    const float* ternary_weights,
    size_t count,
    uint32_t* packed,
    size_t packed_size
);

/**
 * @brief Unpack ternary weights
 *
 * WHAT: Decompress packed ternary to float {-1,0,+1}
 * WHY:  Restore ternary for computation
 *
 * @param packed Packed ternary weights
 * @param packed_size Size of packed buffer in bytes
 * @param output Output float buffer
 * @param count Number of weights to unpack
 * @return Number of weights unpacked
 */
size_t amp_unpack_ternary(
    const uint32_t* packed,
    size_t packed_size,
    float* output,
    size_t count
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MIXED_PRECISION_H */
