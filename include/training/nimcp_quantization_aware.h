/**
 * @file nimcp_quantization_aware.h
 * @brief Quantization-Aware Training (QAT) for NIMCP
 *
 * WHAT: Train models with simulated quantization for efficient deployment
 * WHY:  Enable INT8/INT4 inference with minimal accuracy loss
 * HOW:  Fake quantization during training, learn quantization-robust weights
 *
 * FRAMEWORK COMPARISON:
 * - PyTorch: torch.quantization.prepare_qat(), torch.ao.quantization
 * - JAX: AQT (Accurate Quantized Training)
 * - TensorFlow: tf.quantization, QAT with Keras
 *
 * NIMCP APPROACH:
 * - Integrates with tensor layer for quantized operations
 * - Bio-inspired precision adaptation per neuron type
 * - Supports SNN-specific spike quantization
 *
 * BIOLOGICAL GROUNDING:
 * - Neural precision varies by brain region and cell type
 * - Motor neurons: Low precision, fast response
 * - IT cortex: High precision for fine discrimination
 * - Spike coding naturally quantized (all-or-none)
 *
 * INTEGRATION POINTS:
 * - tensor: Quantized tensor operations
 * - gpu_neuron: Quantized neuron computation
 * - brain_factory: Per-region quantization config
 * - snn: Spike-based natural quantization
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#ifndef NIMCP_QUANTIZATION_AWARE_H
#define NIMCP_QUANTIZATION_AWARE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/error/nimcp_error_codes.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define QAT_DEFAULT_BITS              8        /**< Default quantization bits */
#define QAT_MAX_OBSERVERS             1024     /**< Maximum observer count */
#define QAT_DEFAULT_EMA_DECAY         0.9999f  /**< Default EMA decay for observers */
#define QAT_DEFAULT_CALIBRATION_BATCHES 100    /**< Default calibration batches */

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Quantization data types
 */
typedef enum {
    QAT_DTYPE_INT8 = 0,              /**< 8-bit signed integer */
    QAT_DTYPE_UINT8,                 /**< 8-bit unsigned integer */
    QAT_DTYPE_INT4,                  /**< 4-bit signed integer */
    QAT_DTYPE_INT2,                  /**< 2-bit (ternary) */
    QAT_DTYPE_INT1,                  /**< 1-bit (binary) */
    QAT_DTYPE_FP8_E4M3,              /**< 8-bit floating point e4m3 */
    QAT_DTYPE_FP8_E5M2,              /**< 8-bit floating point e5m2 */
    QAT_DTYPE_FP4,                   /**< 4-bit floating point */
    QAT_DTYPE_TERNARY,               /**< Ternary: {-1, 0, +1} values */
    QAT_DTYPE_COUNT
} qat_dtype_t;

/**
 * @brief Quantization scheme
 *
 * COMPARISON:
 * - PyTorch: per_tensor_affine, per_channel_affine
 * - TensorFlow: per_axis quantization
 */
typedef enum {
    QAT_SCHEME_SYMMETRIC = 0,        /**< Symmetric: zero_point = 0 */
    QAT_SCHEME_AFFINE,               /**< Affine: scale + zero_point */
    QAT_SCHEME_POWER_OF_TWO,         /**< Power-of-2 scale only */
    QAT_SCHEME_COUNT
} qat_scheme_t;

/**
 * @brief Quantization granularity
 */
typedef enum {
    QAT_GRANULARITY_TENSOR = 0,      /**< Per-tensor quantization */
    QAT_GRANULARITY_CHANNEL,         /**< Per-channel (weights) */
    QAT_GRANULARITY_GROUP,           /**< Per-group (e.g., 128 elements) */
    QAT_GRANULARITY_BLOCK,           /**< Block-wise (for sparsity) */
    QAT_GRANULARITY_COUNT
} qat_granularity_t;

/**
 * @brief Range observer method
 *
 * COMPARISON (PyTorch observers):
 * - MinMax: torch.quantization.MinMaxObserver
 * - MovingAverage: torch.quantization.MovingAverageMinMaxObserver
 * - Histogram: torch.quantization.HistogramObserver
 */
typedef enum {
    QAT_OBSERVER_MINMAX = 0,         /**< Track min/max values */
    QAT_OBSERVER_MOVING_AVG,         /**< Exponential moving average */
    QAT_OBSERVER_HISTOGRAM,          /**< Histogram-based (percentile) */
    QAT_OBSERVER_MSE,                /**< MSE-optimal thresholds */
    QAT_OBSERVER_ENTROPY,            /**< KL-entropy calibration */
    QAT_OBSERVER_ACIQ,               /**< Analytical optimal clipping */
    QAT_OBSERVER_COUNT
} qat_observer_t;

/**
 * @brief Fake quantization method
 */
typedef enum {
    QAT_FAKE_QUANT_STE = 0,          /**< Straight-Through Estimator */
    QAT_FAKE_QUANT_LSQ,              /**< Learned Step Size (LSQ) */
    QAT_FAKE_QUANT_PACT,             /**< Parameterized Clipping (PACT) */
    QAT_FAKE_QUANT_DSQ,              /**< Differentiable Soft Quantization */
    QAT_FAKE_QUANT_COUNT
} qat_fake_quant_t;

/**
 * @brief Quantization target
 */
typedef enum {
    QAT_TARGET_WEIGHTS = 0,          /**< Quantize weights */
    QAT_TARGET_ACTIVATIONS,          /**< Quantize activations */
    QAT_TARGET_BOTH,                 /**< Quantize both */
    QAT_TARGET_COUNT
} qat_target_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Observer configuration
 */
typedef struct {
    qat_observer_t method;           /**< Observer method */
    float ema_decay;                 /**< EMA decay (for moving average) */
    float percentile;                /**< Percentile (for histogram, 0.999) */
    uint32_t num_bins;               /**< Histogram bins */
    uint32_t calibration_batches;    /**< Calibration batches */
    bool symmetric;                  /**< Force symmetric range */
} qat_observer_config_t;

/**
 * @brief Fake quantization configuration
 */
typedef struct {
    qat_fake_quant_t method;         /**< Fake quant method */
    bool learn_scale;                /**< Learn scale (LSQ/PACT) */
    bool learn_zero_point;           /**< Learn zero point */
    float initial_scale;             /**< Initial scale (for LSQ) */
    float scale_lr_multiplier;       /**< LR multiplier for scale */
    float grad_scale_factor;         /**< Gradient scale in STE */
} qat_fake_quant_config_t;

/**
 * @brief Per-layer quantization configuration
 */
typedef struct {
    bool quantize_weights;           /**< Quantize weights */
    bool quantize_activations;       /**< Quantize activations */
    qat_dtype_t weight_dtype;        /**< Weight quantization dtype */
    qat_dtype_t activation_dtype;    /**< Activation quantization dtype */
    qat_scheme_t scheme;             /**< Quantization scheme */
    qat_granularity_t weight_granularity;  /**< Weight granularity */
    qat_granularity_t activation_granularity; /**< Activation granularity */
    uint32_t group_size;             /**< Group size (for GROUP granularity) */
} qat_layer_config_t;

/**
 * @brief Complete QAT configuration
 */
typedef struct {
    /* Default settings */
    qat_dtype_t default_weight_dtype;
    qat_dtype_t default_activation_dtype;
    qat_scheme_t default_scheme;
    qat_granularity_t default_granularity;

    /* Observer settings */
    qat_observer_config_t observer;

    /* Fake quantization settings */
    qat_fake_quant_config_t fake_quant;

    /* Training schedule */
    uint32_t warmup_epochs;          /**< FP32 warmup before QAT */
    uint32_t freeze_bn_epochs;       /**< Freeze BN after N epochs */
    bool start_with_calibration;     /**< Calibrate before training */

    /* Per-layer configs (optional) */
    qat_layer_config_t* layer_configs;
    uint32_t num_layer_configs;

    /* Integration */
    bool integrate_tensor_layer;     /**< Use NIMCP tensor quantized ops */
    bool integrate_brain_factory;    /**< Per-region quantization */

    /* Debugging */
    bool verbose;
    bool track_statistics;
} qat_config_t;

//=============================================================================
// Quantization Parameters
//=============================================================================

/**
 * @brief Quantization parameters for a tensor/layer
 */
typedef struct {
    float scale;                     /**< Quantization scale */
    int32_t zero_point;              /**< Zero point offset */
    qat_dtype_t dtype;               /**< Quantized dtype */
    qat_scheme_t scheme;             /**< Quantization scheme */
    qat_granularity_t granularity;   /**< Quantization granularity */

    /* Per-channel params (if CHANNEL granularity) */
    float* scales;                   /**< Per-channel scales */
    int32_t* zero_points;            /**< Per-channel zero points */
    uint32_t num_channels;           /**< Number of channels */

    /* Range statistics */
    float observed_min;              /**< Observed minimum */
    float observed_max;              /**< Observed maximum */
} qat_params_t;

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief QAT statistics
 */
typedef struct {
    uint64_t total_steps;            /**< Total training steps */
    uint64_t calibration_batches;    /**< Calibration batches processed */

    /* Quantization error stats */
    float avg_weight_mse;            /**< Average weight quantization MSE */
    float avg_activation_mse;        /**< Average activation quantization MSE */
    float max_weight_error;          /**< Maximum weight quantization error */
    float max_activation_error;      /**< Maximum activation quantization error */

    /* Range statistics */
    float avg_weight_range;          /**< Average weight dynamic range */
    float avg_activation_range;      /**< Average activation dynamic range */
    uint64_t outlier_count;          /**< Values clipped as outliers */
    float outlier_ratio;             /**< Ratio of outliers */

    /* Accuracy */
    float fp32_accuracy;             /**< Baseline FP32 accuracy */
    float qat_accuracy;              /**< QAT accuracy */
    float accuracy_delta;            /**< Accuracy loss from quantization */
} qat_stats_t;

//=============================================================================
// Opaque Types
//=============================================================================

/**
 * @brief QAT context (opaque)
 */
typedef struct qat_ctx_s qat_ctx_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default QAT configuration
 *
 * DEFAULTS:
 * - INT8 weights and activations
 * - Per-tensor symmetric quantization
 * - MinMax observer
 * - STE fake quantization
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int qat_default_config(qat_config_t* config);

/**
 * @brief Get INT4 configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int qat_int4_config(qat_config_t* config);

/**
 * @brief Get binary network configuration
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int qat_binary_config(qat_config_t* config);

/**
 * @brief Create QAT context
 *
 * @param config QAT configuration
 * @return QAT context or NULL on failure
 */
qat_ctx_t* qat_create(const qat_config_t* config);

/**
 * @brief Destroy QAT context
 *
 * @param ctx Context to destroy (NULL-safe)
 */
void qat_destroy(qat_ctx_t* ctx);

//=============================================================================
// Observer API
//=============================================================================

/**
 * @brief Register tensor for observation
 *
 * WHAT: Register a tensor to track its range statistics
 * WHY:  Determine optimal quantization parameters
 *
 * @param ctx QAT context
 * @param name Tensor name for identification
 * @param target Weight or activation
 * @return Observer ID or negative on error
 */
int qat_register_observer(
    qat_ctx_t* ctx,
    const char* name,
    qat_target_t target
);

/**
 * @brief Update observer with tensor values
 *
 * @param ctx QAT context
 * @param observer_id Observer ID
 * @param tensor Tensor to observe
 * @return 0 on success, negative on error
 */
int qat_observe(
    qat_ctx_t* ctx,
    int observer_id,
    const nimcp_tensor_t* tensor
);

/**
 * @brief Get computed quantization parameters
 *
 * @param ctx QAT context
 * @param observer_id Observer ID
 * @param params Output quantization parameters
 * @return 0 on success, negative on error
 */
int qat_get_params(
    qat_ctx_t* ctx,
    int observer_id,
    qat_params_t* params
);

/**
 * @brief Calibrate all observers
 *
 * WHAT: Run calibration pass over dataset
 * WHY:  Determine quantization ranges before training
 *
 * @param ctx QAT context
 * @return 0 on success, negative on error
 */
int qat_calibrate(qat_ctx_t* ctx);

/**
 * @brief Freeze observers (stop tracking)
 *
 * @param ctx QAT context
 * @return 0 on success, negative on error
 */
int qat_freeze_observers(qat_ctx_t* ctx);

//=============================================================================
// Fake Quantization API
//=============================================================================

/**
 * @brief Apply fake quantization to tensor
 *
 * WHAT: Quantize and dequantize tensor (simulate quantization)
 * WHY:  Train with quantization effects during forward pass
 * HOW:  q = round(x/scale) + zp; x' = (q - zp) * scale
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * x_fake = torch.fake_quantize_per_tensor_affine(x, scale, zp, qmin, qmax)
 * ```
 *
 * @param ctx QAT context
 * @param tensor Input/output tensor (modified in place)
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
int qat_fake_quantize(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    const qat_params_t* params
);

/**
 * @brief Fake quantize with learned parameters (LSQ)
 *
 * @param ctx QAT context
 * @param tensor Input/output tensor
 * @param observer_id Observer with learned params
 * @return 0 on success, negative on error
 */
int qat_fake_quantize_learned(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    int observer_id
);

/**
 * @brief Compute fake quantization gradient
 *
 * WHAT: Gradient for backward pass through fake quant
 * WHY:  Enable gradient flow (STE or learned)
 *
 * @param ctx QAT context
 * @param grad_output Upstream gradient
 * @param tensor Original tensor
 * @param params Quantization parameters
 * @param grad_input Output gradient (same shape as tensor)
 * @return 0 on success, negative on error
 */
int qat_fake_quantize_backward(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* grad_output,
    const nimcp_tensor_t* tensor,
    const qat_params_t* params,
    nimcp_tensor_t* grad_input
);

//=============================================================================
// Quantization Operations API
//=============================================================================

/**
 * @brief Quantize tensor to integer dtype
 *
 * @param ctx QAT context
 * @param input Input FP32 tensor
 * @param output Output quantized tensor
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
int qat_quantize(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    const qat_params_t* params
);

/**
 * @brief Dequantize tensor to FP32
 *
 * @param ctx QAT context
 * @param input Input quantized tensor
 * @param output Output FP32 tensor
 * @param params Quantization parameters
 * @return 0 on success, negative on error
 */
int qat_dequantize(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* input,
    nimcp_tensor_t* output,
    const qat_params_t* params
);

/**
 * @brief Quantized matrix multiply
 *
 * WHAT: Efficient quantized matmul
 * WHY:  INT8 gemm is 2-4x faster on supported hardware
 *
 * @param ctx QAT context
 * @param a First input (quantized)
 * @param b Second input (quantized)
 * @param c Output (dequantized FP32 or requantized)
 * @param a_params A quantization params
 * @param b_params B quantization params
 * @param c_params C params (NULL for FP32 output)
 * @return 0 on success, negative on error
 */
int qat_matmul(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b,
    nimcp_tensor_t* c,
    const qat_params_t* a_params,
    const qat_params_t* b_params,
    const qat_params_t* c_params
);

//=============================================================================
// Export API
//=============================================================================

/**
 * @brief Export quantized model parameters
 *
 * WHAT: Export trained model with quantization params
 * WHY:  Deploy to inference runtime
 *
 * @param ctx QAT context
 * @param weights FP32 weights to quantize
 * @param num_weights Number of weight tensors
 * @param params Output quantization params
 * @param filepath Output file path
 * @return 0 on success, negative on error
 */
int qat_export(
    qat_ctx_t* ctx,
    nimcp_tensor_t** weights,
    uint32_t num_weights,
    qat_params_t** params,
    const char* filepath
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to brain factory for per-region quantization
 *
 * BIOLOGICAL BASIS:
 * - Different brain regions need different precision
 * - Motor: Low precision (fast)
 * - IT: High precision (discrimination)
 *
 * @param ctx QAT context
 * @param brain_factory Brain factory
 * @return 0 on success, negative on error
 */
int qat_connect_brain_factory(qat_ctx_t* ctx, void* brain_factory);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get QAT statistics
 *
 * @param ctx QAT context
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int qat_get_stats(const qat_ctx_t* ctx, qat_stats_t* stats);

/**
 * @brief Reset QAT statistics
 *
 * @param ctx QAT context
 */
void qat_reset_stats(qat_ctx_t* ctx);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get dtype name
 */
const char* qat_dtype_name(qat_dtype_t dtype);

/**
 * @brief Get dtype bit width
 */
uint32_t qat_dtype_bits(qat_dtype_t dtype);

/**
 * @brief Get scheme name
 */
const char* qat_scheme_name(qat_scheme_t scheme);

/**
 * @brief Validate QAT configuration
 */
int qat_validate_config(const qat_config_t* config);

/**
 * @brief Compute quantization error (MSE)
 */
float qat_compute_mse(
    const nimcp_tensor_t* original,
    const nimcp_tensor_t* quantized
);

//=============================================================================
// Ternary Quantization API
//=============================================================================

/**
 * @brief Ternary quantization configuration
 *
 * WHAT: Configuration for ternary weight quantization
 * WHY:  Extreme compression with {-1, 0, +1} values
 * HOW:  Threshold-based ternarization with learned thresholds
 *
 * BIOLOGICAL BASIS:
 * - Synaptic weights discretize to LTD/STABLE/LTP states
 * - Binary/ternary coding in sensory systems
 * - Energy-efficient spiking with discrete states
 */
typedef struct {
    float threshold_ratio;           /**< Threshold as ratio of max (default: 0.7) */
    bool use_learned_threshold;      /**< Learn threshold during training */
    float initial_threshold;         /**< Initial threshold value */
    bool symmetric;                  /**< Use symmetric thresholds */
    bool use_ste;                    /**< Use straight-through estimator */
    float ste_gradient_scale;        /**< STE gradient scaling factor */
    bool normalize_weights;          /**< Normalize weights before ternarization */
} qat_ternary_config_t;

/**
 * @brief Ternary quantization parameters
 */
typedef struct {
    float positive_scale;            /**< Scale for +1 values */
    float negative_scale;            /**< Scale for -1 values */
    float threshold;                 /**< Current threshold value */
    float learned_threshold;         /**< Learned threshold (if enabled) */
    uint64_t n_positive;             /**< Count of +1 values */
    uint64_t n_zero;                 /**< Count of 0 values */
    uint64_t n_negative;             /**< Count of -1 values */
    float sparsity;                  /**< Ratio of zero values */
} qat_ternary_params_t;

/**
 * @brief Get default ternary configuration
 *
 * DEFAULTS:
 * - threshold_ratio: 0.7
 * - use_ste: true
 * - symmetric: true
 * - normalize_weights: true
 *
 * @param config Configuration to initialize
 * @return 0 on success, negative on error
 */
int qat_ternary_default_config(qat_ternary_config_t* config);

/**
 * @brief Create ternary QAT context
 *
 * @param ternary_config Ternary-specific configuration
 * @return QAT context configured for ternary, or NULL on failure
 */
qat_ctx_t* qat_ternary_create(const qat_ternary_config_t* ternary_config);

/**
 * @brief Ternarize tensor weights
 *
 * WHAT: Convert floating-point weights to ternary {-1, 0, +1}
 * WHY:  Extreme compression (1.58 bits/weight) with learned scaling
 * HOW:
 *   - Compute threshold from weight statistics
 *   - Map to: +1 if w > threshold, -1 if w < -threshold, 0 otherwise
 *   - Multiply by learned scale factors
 *
 * MATHEMATICAL FORMULATION:
 *   t = threshold_ratio * mean(|w|)
 *   w_ternary[i] = {
 *       +scale_p  if w[i] > t
 *       0         if |w[i]| <= t
 *       -scale_n  if w[i] < -t
 *   }
 *
 * @param ctx QAT context
 * @param tensor Input/output tensor (modified in place)
 * @param ternary_config Ternary configuration
 * @param params Output: computed ternary parameters
 * @return 0 on success, negative on error
 */
int qat_ternarize(
    qat_ctx_t* ctx,
    nimcp_tensor_t* tensor,
    const qat_ternary_config_t* ternary_config,
    qat_ternary_params_t* params
);

/**
 * @brief Compute ternary gradients with straight-through estimator
 *
 * WHAT: Gradient for backward pass through ternarization
 * WHY:  Enable gradient flow through discrete ternarization
 * HOW:  STE passes gradients through unchanged for ternarized values
 *
 * COMPARISON (PyTorch equivalent):
 * ```python
 * # Forward: w_tern = ternarize(w)
 * # Backward: grad_w = grad_tern (straight-through)
 * ```
 *
 * @param ctx QAT context
 * @param grad_output Upstream gradient
 * @param original_weights Original FP32 weights
 * @param ternary_config Ternary configuration
 * @param grad_input Output gradient (same shape as weights)
 * @return 0 on success, negative on error
 */
int qat_ternary_backward(
    qat_ctx_t* ctx,
    const nimcp_tensor_t* grad_output,
    const nimcp_tensor_t* original_weights,
    const qat_ternary_config_t* ternary_config,
    nimcp_tensor_t* grad_input
);

/**
 * @brief Compute optimal ternary threshold
 *
 * WHAT: Find threshold that minimizes quantization error
 * WHY:  Better threshold -> better accuracy
 * HOW:  Analytical solution based on weight distribution
 *
 * @param tensor Weight tensor
 * @param threshold_out Output: optimal threshold
 * @param scale_out Output: optimal scale factor
 * @return 0 on success, negative on error
 */
int qat_compute_optimal_ternary_threshold(
    const nimcp_tensor_t* tensor,
    float* threshold_out,
    float* scale_out
);

/**
 * @brief Get ternary quantization statistics
 *
 * @param ctx QAT context
 * @param params Ternary parameters with statistics
 * @return 0 on success, negative on error
 */
int qat_get_ternary_stats(
    const qat_ctx_t* ctx,
    qat_ternary_params_t* params
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_QUANTIZATION_AWARE_H */
